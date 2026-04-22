#!build
## v1.006 — 按钮重构 + 设置模式 + 低功耗 + 天气代码修正

### 按钮变更
- **移除双击检测**。每个按钮统一三级：单击 / 长按3秒 / 长按8秒
- 旧的双击等待窗口（500 ms）导致单击经常被误判为双击——完全移除后单击立即响应（松手即触发）
- 全新内联 `Btn` 状态机替换 `ButtonHandler`，消除静态变量复位 Bug

### 显示模式 (BTN_MODE 单击循环)
- 顺序改为：**CLOCK → DATE → TEMP → IP → CLOCK**

### 新增：设置模式
- **BTN_MODE 长按3秒** 进入设置模式
- BTN_UP / BTN_DOWN 单击对当前项 +1 / −1（亮度步长5）
- BTN_MODE 单击切换到下一个设置项；再次长按3秒或超时30秒自动保存并退出
- **NTP已同步时隐藏时间/日期项**（避免与自动时钟冲突）
- 设置项目（LED矩阵上以青色显示）：
  1. `H:` 小时 / `m:` 分钟 / `d:` 日 / `Mo:` 月 / `Y:` 年 / `Wd:` 星期 ← 仅在无NTP时
  2. `TZ:` 时区 — 在17个常见POSIX预设中循环（UTC、HKT、JST、EST…）
  3. `DM:` 暗档PWM值 / `MD:` 中档 / `BR:` 亮档（步长5，范围1-255）
  4. `Wx:ON/OFF` 天气开关
  5. `Wi:ON/OFF` WiFi开关

### BTN_MODE 长按8秒 → 恢复模式（原5秒改为8秒，与3秒设置模式区分）

### BTN_UP / BTN_DOWN 功能
- 单击：亮度 +1 / −1（DIM→MED→BRT 循环）
- 长按3秒：强制刷新NTP+天气 / 切换天气开关
- 长按8秒：进入恢复模式

### 低功耗
- 空闲时（30秒无操作 + 无WS客户端）`delay(50)` ≈ 20 Hz
- 活跃时 `delay(5)` ≈ 200 Hz，保证按钮响应灵敏
- wifiEnabled=false 时关闭WiFi无线（省电约70 mA）

### 配置新增字段（configuration.json）
- `brightDim` / `brightMed` / `brightBrt`：三档绝对PWM值（默认25/128/255）
- `wifiEnabled`：启动时是否连接WiFi
- `defaultWeather`：默认天气开关状态
- `timezone` 字段长度从32扩展为48，支持更长的POSIX时区字符串

> ⚠️ **配置结构变更**：首次刷新后EEPROM自动重置为默认值，需重新在Web界面配置。

### 立即刷新LED
- Web端或按键端修改配置后立即调用 `applyBrightness()` + `flushDisplay()`
- configSaveCallback 同时更新方向、天气开关、手动时间基准

### 天气代码修正（OWM group ranges）
- 2xx Thunderstorm (200-299) → 紫色
- 3xx Drizzle (300-399) → 浅蓝色
- 5xx Rain (500-599) → 蓝色
- 6xx Snow (600-699) → 冰蓝色
- 7xx Atmosphere/fog/mist (700-799) → 灰色
- 800 Clear → 黄色
- 801-802 Few/scattered clouds → 浅黄色
- 803-804 Broken/overcast → 中灰色
