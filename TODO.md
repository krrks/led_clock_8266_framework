# TODO

## 显示

- [ ] 字体缺少 `-` 字形(标点字模只有冒号/感叹号/引号/句号/点/空格 6 个),导致日期 `09-15` 显示成 `0915`。需在 [FontData](src/FontData.h) 补充连字符字模。
- [ ] IP 显示从滚动改为**翻页轮播**:先显示 `192.168` 停留一段时间,再显示 `3.37`,如此循环(替代 drawScroll 滚动方式)。

## 已讨论确定的方案(2026-09-16,待实施)

### 1. 底部日期-星期指示点(仅 clock 模式)

- 第 8 行(逻辑 row 7,目前完全空闲)只亮**一颗灯**:
  - **位置 = 日期**:第 N 日亮第 N 列(N-1 列号),1 日→第 1 列,31 日→第 31 列;第 32 列备用
  - **颜色 = 星期**:周一/周二绿 0x00CC00,周三/周四蓝 0x0066FF,周五/周六/周日红 0xFF0000
  - 不显示月份
- 移除 [ClockDisplay.cpp](src/ClockDisplay.cpp) `drawClockFace` 中 26/28/30 三列信息条(日期条/星期条/天气条)
- 空出的 **26-31 列(6×7)用于天气图标**(仅 clock 模式):晴/少云/多云/阴/雨/雷/雪/雾/无数据 9 个字形,用主内容单色绘制

### 2. 按钮重映射 + 单色主题

- **BTN2 单击 = 亮度循环**(dim→med→bright→dim,替代原来的亮度+;长按 3s 强制 NTP+天气保持不变)
- **BTN3 单击 = 颜色循环**(长按 3s 天气开关保持不变)
- 颜色预设 **8 种单色**,循环切换:
  白 0xFFFFFF(默认)/ 暖黄 0xFFC800 / 红 0xFF0000 / 绿 0x00CC00 / 青 0x00CCCC / 蓝 0x0066FF / 橙 0xFF6600 / 紫 0x8800CC
- 主内容(时钟/日期/温度/IP/天气图标)统一使用所选单色;**底部星期指示点保持独立配色**
- 新增配置项 `colorIndex`(uint8_t,默认 0=白):[ConfigManager](src/config/ConfigManager.h) 持久化,网页设置页自动渲染(动态表单),BTN3 切换时 `configManager.save()`

### 实施要点

| 文件 | 改动 |
|---|---|
| FontData.h/.cpp | 补充 `-` 字模 |
| ClockDisplay.cpp | 删三列信息条;加日期-星期指示点(仅 clock);加天气图标字形;各 face 用主色 |
| main.cpp | BTN2/BTN3 重映射;mainColor 全局量 |
| AppState.h | 颜色预设表 + 星期配色常量 |
| ConfigManager.h/.cpp | `colorIndex` 字段 + 默认值 |
| docs/buttons.md, docs/display.md | 同步文档 |

> 状态:方案已与用户确认,按上述规格实施(尚未开始编码)。
