# TODO

## 启动

- [ ] 系统启动后、连接 WiFi 前播放**启动画面**(可为自检动画:LED 矩阵逐列扫描 / 跑马灯 / 版本号展示)。Recovery 模式不播放。实现位置:setup() 中 [LED] 初始化之后、WiFi 连接之前。

## 显示

- [x] 字体缺少 `-` 字形 → 已补 `PUNCT_DASH_INDEX` 字模([FontData.h](src/FontData.h) / [FontData.cpp](src/FontData.cpp))
- [x] IP 显示改**翻页轮播**:`192.168` → `3.37`,每页 3 秒([ClockDisplay.cpp](src/ClockDisplay.cpp) `drawIPFace`)

## 显示方案改造(2026-09-16 讨论,2026-09-17 已实施)

### 1. 底部日期-星期指示点(仅 clock 模式)

- [x] 第 8 行只亮一颗灯:列 = 日期(1 日→第 1 列,31 日→第 31 列),颜色 = 星期(周一周二绿 / 周三周四蓝 / 周五六日红)
- [x] 移除 26/28/30 三列信息条,26-31 列(6×7)绘制天气图标(9 字形:晴/少云/疏云/多云/阵雨/雨/雷/雪/雾),单色主题色

### 2. 按钮重映射 + 单色主题

- [x] BTN2 单击 = 亮度循环(dim→med→bright→dim);长按 3s 强制 NTP+天气不变
- [x] BTN3 单击 = 颜色循环(8 种单色预设:白/暖黄/红/绿/青/蓝/橙/紫);长按 3s 天气开关不变
- [x] 主内容(时钟/日期/温度/IP/天气图标)统一主题色,底部指示点独立配色
- [x] 配置项 `colorIndex` 持久化 + 网页设置页自动渲染

### 文档

- [x] [docs/display.md](docs/display.md) 重写:新布局、主题色表、图标映射
- [x] [docs/buttons.md](docs/buttons.md) 更新:BTN2/BTN3 新行为

> 构建验证通过(RAM 60.9% / Flash 45.3%),**尚未烧录**。
