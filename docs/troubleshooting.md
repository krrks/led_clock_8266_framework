# Troubleshooting — 运行数小时后"卡死"(蓝灯闪 + 矩阵无输出)

> 记录:Claude 诊断建议。症状"主板蓝灯持续闪烁 + LED 矩阵无输出"有明确含义——
> **不是死机,而是设备自动进入了 Recovery(恢复)模式**。

## 症状对应关系

[main.cpp](../src/main.cpp) 中的状态灯逻辑:

```cpp
static void updateStatusLED() {
    if (appMode == AM_RECOVERY) {
        unsigned long now = millis();
        if (now - tLedBlink >= 1000UL) {   // 1秒切换一次 = 慢闪
            tLedBlink = now; ledBlinkState = !ledBlinkState;
            digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH);
        }
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH);   // 正常模式常灭
    }
}
```

`docs/recovery.md` 也写明:"Recovery mode → Slow blink (1s)"。而一旦进入 Recovery,`loop()` 会直接 return,不再执行矩阵渲染:

```cpp
void loop() {
    RecoveryManager::get().loop();
    if (RecoveryManager::get().isActive()) {
        updateStatusLED();
        delay(50);
        return;   // ← 后面所有正常逻辑(含 flushDisplay)全部跳过
    }
    ...
}
```

当前的 `RecoveryManager.cpp` 里**完全没有驱动 NeoPixelBus/矩阵的代码**(旧版会滚动显示 "RECOVERY" 文字,那部分逻辑还留在 `tools/trash/DisplayManager.cpp` 里,但已被排除出编译)。所以矩阵会保持进入 Recovery 前的最后一帧,画面"卡住不动"。

## Recovery 是怎么被触发的

`RecoveryManager::begin()` 只在开机时判断一次是否要进入 Recovery,触发条件三选一:

1. **开机窗口按住 BTN1(GPIO5)3秒**
2. **上次复位原因是 Exception 或 Watchdog**(即固件真的崩溃过)
3. **RTC 内存里的软件触发标志**——目前代码里唯一会调用 `RecoveryManager::get().trigger()` 的地方是 [WeatherFetch.cpp](../src/WeatherFetch.cpp):

```cpp
if (weatherFails >= WEATHER_FAIL_MAX) {   // 连续5次网络失败
    triggerRecovery();                     // 写RTC标志 + 重启
}
```

因为症状是"**运行一段时间后**才卡住",可以排除开机按键触发(那个只在上电瞬间生效)。真正的嫌疑是后两种:

### 嫌疑 A:固件崩溃(Exception/Watchdog)导致自动重启进入 Recovery

这是最需要重点排查的,因为代码里有几处长期运行容易导致**堆内存碎片化/耗尽**的写法:

- `printHeartbeat()` 每 60 秒(`HEARTBEAT_MS`)用 `String` 拼接一次,长期运行是 ESP8266 上经典的堆碎片来源
- `Dashboard::_push()` 只要有网页仪表盘连着,每 5 秒(`DASH_INT_MS`)就新建 `JsonDocument` + `String json`,一天下来是上万次分配/释放
- `WebServer.cpp` 里几乎所有 API 处理函数(`/api/status`、`/api/pins`、`/api/config` 等)都用未指定容量的 `JsonDocument doc;`(ArduinoJson v7 动态分配),配合 `String` 拼接
- `fetchWeather()` 每小时一次 HTTP 请求 + JSON 解析,也会周期性占用堆

这些叠加起来,在 ESP8266 只有 ~40KB 可用堆的情况下,跑几小时到几天后出现内存碎片化甚至 OOM,进而触发看门狗复位或 Exception,是很典型的表现——正好符合"运行一段时间后卡住"。

### 嫌疑 B:天气请求连续失败 5 次,主动触发 Recovery

`WEATHER_INT_MS = 3600000UL`(每小时一次),如果 WiFi 信号不稳定或 OWM 接口偶发超时(注意 `http.setTimeout(8000)` 只有 8 秒,弱网环境容易超时算作"失败"),连续 5 小时失败就会主动重启进入 Recovery——这也完全符合"跑了一段时间(通常是几个小时)之后"的时间特征。

## 建议的排查步骤

1. **先看崩溃日志**:固件已内置崩溃记录机制——设备进入 Recovery 后,打开 `http://<设备IP或AP的192.168.4.1>` → Files 标签页,看有没有 `/crash.log`,里面的 `reset=` 字段会直接告诉你是 Exception 还是 Watchdog,以及崩溃时的剩余堆大小。

2. **看串口心跳日志里 heap 的变化趋势**(`[Heart] ... heap=xxxxx ...`,每分钟一条),如果是持续走低到几千字节才崩,基本可以确认是堆碎片/泄漏问题。

3. **看是不是天气触发的**:串口日志会打印 `[Weather] network fail X / 5`,如果崩溃前能看到这个计数一路涨到 5,那就是嫌疑 B,而不是真崩溃。

4. 如果确认是堆碎片问题,优化方向包括:减少/延长 Dashboard 推送频率、给 `JsonDocument` 指定固定容量(用 `StaticJsonDocument<N>`)、减少心跳/API 里 `String` 拼接、限制同时连接的 WebSocket 客户端数。

先看一下 `/crash.log` 和心跳里的 heap 曲线,基本就能定位是哪一种了。
