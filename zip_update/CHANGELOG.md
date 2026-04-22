#!build
## v1.004 — 亮度 & 旋转重启后失效修复

### 问题根因

两个独立 bug 叠加，导致"重启后配置消失"的现象：

1. **亮度映射错误**
   - `brightness` 字段在配置 JSON 中是下拉选择，存储的是选项索引 `0 / 1 / 2`（对应 Dim / Medium / Bright）。
   - 旧代码：`FastLED.setBrightness(configManager.data.brightness)` 直接把索引值 1 或 2 传给 FastLED，亮度几乎为 0，视觉上等同于"消失"。
   - 新代码：通过 `BRIGHTNESS_MAP[3] = {25, 128, 255}` 将索引映射为实际 PWM 值。

2. **旋转设置从未生效**
   - `displayOrientation` 保存到 EEPROM 是正常的，但 `flushDisplay()` 从未读取过它，内容始终以 Normal 方向输出。
   - 新代码：在 `flushDisplay()` 内增加 `applyOrientation()` 函数，在蛇形映射前先对 `displayMatrix` 做像素重映射。

### 修复内容

- 新增 `BRIGHTNESS_MAP[3] = {25, 128, 255}` 查表
- `setup()` 及 `configSaveCallback` 均改用 `mapBrightness(idx)` 转换
- 新增 `applyOrientation(src, dst)` 函数，支持：
  - `0` = Normal（不变）
  - `2` = 180° 旋转
  - `4` = H-Flip（左右镜像）
  - `5` = V-Flip（上下镜像）
  - `1/3` = 90°/270°（非正方形屏不适合，近似为 Normal）
- `flushDisplay()` 调用链改为：`displayMatrix` → `applyOrientation()` → `convertToSnakeOrder()` → `FastLED.show()`
- Serial 日志新增：`[Boot] Brightness idx=N (raw=M) | Orientation=N` 和 `[Config saved] brightness idx=N orientation=N`

### 验证方法

1. 刷机后在 Web UI → Configuration 将 Brightness 改为 Bright，Orientation 改为 H-Flip，保存。
2. 重启（断电或 Web 重启按钮）。
3. 串口应输出 `Brightness idx=2 (raw=255) | Orientation=4`，显示屏应以最大亮度+镜像方向显示。
