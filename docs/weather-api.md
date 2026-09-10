# OpenWeatherMap 天气 API 参考

> 整理自 OpenWeatherMap 官方文档,含 2026-09-10 实测返回数据。校验工具见 [tools/fetch_weather_test.py](../tools/fetch_weather_test.py)。

## 1. 接口与参数

```
GET https://api.openweathermap.org/data/2.5/weather
```

| 参数 | 必填 | 说明 |
|---|---|---|
| `q` | ✓ | 城市名,如 `Guangzhou,CN`(固件直接传用户配置的城市字符串) |
| `appid` | ✓ | API Key |
| `units` | 可选 | `standard`(K)/ `metric`(°C)/ `imperial`(°F),固件用 `metric` |
| `lang` | 可选 | 描述语言,如 `zh_cn`。固件**未传此参数**,描述为英文 |

> 注意:`weather` 是数组,一个地点可能同时出现多种天气,**数组第一项为主天气**。

## 2. JSON 响应格式

| 路径 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `coord.lon` / `coord.lat` | float | ✓ | 经度 / 纬度 |
| `weather[].id` | int | ✓ | 天气条件代码(见第 4 节) |
| `weather[].main` | string | ✓ | 参数分组:Rain / Snow / Clouds 等 |
| `weather[].description` | string | ✓ | 描述文本,可用 `lang` 指定语言 |
| `weather[].icon` | string | ✓ | 图标 id,如 `10d` |
| `base` | string | ✓ | 内部参数(`stations`) |
| `main.temp` | float | ✓ | 温度(metric 下为 °C) |
| `main.feels_like` | float | ✓ | 体感温度 |
| `main.pressure` | float | ✓ | 海平面气压,hPa |
| `main.humidity` | int | ✓ | 湿度,% |
| `main.temp_min` / `temp_max` | float | ✓ | 当前观测的最低 / 最高温 |
| `main.sea_level` / `grnd_level` | float | — | 海平面 / 地面气压(where available) |
| `visibility` | int | ✓ | 能见度,米(最大 10000) |
| `wind.speed` | float | ✓ | 风速,m/s |
| `wind.deg` | float | ✓ | 风向,气象度数 |
| `wind.gust` | float | — | 阵风,m/s(where available) |
| `clouds.all` | int | ✓ | 云量,% |
| `rain.1h` | float | — | 过去 1 小时降水量,mm/h(仅降水时返回) |
| `snow.1h` | float | — | 过去 1 小时降雪量,mm/h(仅降雪时返回) |
| `dt` | int | ✓ | 数据计算时间,unix UTC 秒 |
| `sys.type` / `sys.id` / `sys.message` | — | — | 内部参数 |
| `sys.country` | string | ✓ | 国家代码,如 `CN` |
| `sys.sunrise` / `sys.sunset` | int | ✓ | 日出 / 日落,unix UTC 秒 |
| `timezone` | int | ✓ | 相对 UTC 的秒数偏移(广州 = 28800) |
| `id` | int | ✓ | 城市 ID |
| `name` | string | ✓ | 城市名 |
| `cod` | int | ✓ | 内部参数,200 = 成功 |

### 实测返回示例(广州,2026-09-10 17:23 本地时间)

```json
{
  "coord": { "lon": 113.25, "lat": 23.1167 },
  "weather": [
    { "id": 803, "main": "Clouds", "description": "broken clouds", "icon": "04d" }
  ],
  "base": "stations",
  "main": {
    "temp": 31.52, "feels_like": 34.18,
    "temp_min": 31.52, "temp_max": 31.52,
    "pressure": 1011, "humidity": 53,
    "sea_level": 1011, "grnd_level": 1010
  },
  "visibility": 10000,
  "wind": { "speed": 4.63, "deg": 30, "gust": 5 },
  "clouds": { "all": 65 },
  "dt": 1789032197,
  "sys": { "country": "CN", "sunrise": 1788991920, "sunset": 1789036601 },
  "timezone": 28800,
  "id": 1809858,
  "name": "Guangzhou",
  "cod": 200
}
```

校验结果:必填字段缺失 0、类型不符 0,**与官方文档格式一致**。`lang=zh_cn` 时 `description` 返回中文(如上例为 `"多云"`),其余字段不变。

## 3. 天气图标

图标 URL 模板(标准地址):

```
https://openweathermap.org/img/wn/{icon}@2x.png
```

例如 light rain 的图标 id 为 `10d` → `https://openweathermap.org/img/wn/10d@2x.png`

| 日间 | 夜间 | 描述 |
|---|---|---|
| 01d | 01n | clear sky 晴 |
| 02d | 02n | few clouds 少云 |
| 03d | 03n | scattered clouds 疏云 |
| 04d | 04n | broken clouds 多云 |
| 09d | 09n | shower rain 阵雨 |
| 10d | 10n | rain 雨 |
| 11d | 11n | thunderstorm 雷暴 |
| 13d | 13n | snow 雪 |
| 50d | 50n | mist 薄雾 |

## 4. 天气条件代码

### 2xx — 雷暴 Thunderstorm(图标 11d)

| 代码 | 描述 |
|---|---|
| 200 | thunderstorm with light rain |
| 201 | thunderstorm with rain |
| 202 | thunderstorm with heavy rain |
| 210 | light thunderstorm |
| 211 | thunderstorm |
| 212 | heavy thunderstorm |
| 221 | ragged thunderstorm |
| 230 | thunderstorm with light drizzle |
| 231 | thunderstorm with drizzle |
| 232 | thunderstorm with heavy drizzle |

### 3xx — 毛毛雨 Drizzle(图标 09d)

| 代码 | 描述 |
|---|---|
| 300 | light intensity drizzle |
| 301 | drizzle |
| 302 | heavy intensity drizzle |
| 310 | light intensity drizzle rain |
| 311 | drizzle rain |
| 312 | heavy intensity drizzle rain |
| 313 | shower rain and drizzle |
| 314 | heavy shower rain and drizzle |
| 321 | shower drizzle |

### 5xx — 雨 Rain(图标 10d)

| 代码 | 描述 |
|---|---|
| 500 | light rain |
| 501 | moderate rain |
| 502 | heavy intensity rain |
| 503 | very heavy rain |
| 504 | extreme rain |
| 511 | freezing rain(图标 13d) |
| 520 | light intensity shower rain(图标 09d) |
| 521 | shower rain(图标 09d) |
| 522 | heavy intensity shower rain(图标 09d) |
| 531 | ragged shower rain(图标 09d) |

### 6xx — 雪 Snow(图标 13d)

| 代码 | 描述 |
|---|---|
| 600 | light snow |
| 601 | snow |
| 602 | heavy snow |
| 611 | sleet |
| 612 | light shower sleet |
| 613 | shower sleet |
| 615 | light rain and snow |
| 616 | rain and snow |
| 620 | light shower snow |
| 621 | shower snow |
| 622 | heavy shower snow |

### 7xx — 大气现象 Atmosphere(图标 50d)

| 代码 | 描述 |
|---|---|
| 701 | mist |
| 711 | smoke |
| 721 | haze |
| 731 | sand/dust whirls |
| 741 | fog |
| 751 | sand |
| 761 | dust |
| 762 | volcanic ash |
| 771 | squalls |
| 781 | tornado |

### 800 — 晴 Clear(图标 01d/01n)

| 代码 | 描述 |
|---|---|
| 800 | clear sky |

### 80x — 云 Clouds

| 代码 | 描述 | 图标 |
|---|---|---|
| 801 | few clouds: 11-25% | 02d / 02n |
| 802 | scattered clouds: 25-50% | 03d / 03n |
| 803 | broken clouds: 51-84% | 04d / 04n |
| 804 | overcast clouds: 85-100% | 04d / 04n |

## 5. 固件使用情况

[src/WeatherFetch.cpp](../src/WeatherFetch.cpp) 请求 URL:

```
http://api.openweathermap.org/data/2.5/weather?q={city}&appid={key}&units=metric
```

解析字段(与本文档格式一致,已实测验证):

| 固件全局变量 | 来源字段 |
|---|---|
| `weatherCode` | `weather[0].id` |
| `weatherTemp` | `main.temp` |
| `weatherDesc` | `weather[0].description` |

天气代码 → LED 颜色映射见 [ClockDisplay.cpp](../src/ClockDisplay.cpp) `wxColor()`(活动代码路径):

| 代码范围 | 颜色 | 含义 |
|---|---|---|
| 200–299 | 0xA000C8 紫 | 雷暴 |
| 300–399 | 0x508CFF 浅蓝 | 毛毛雨 |
| 500–599 | 0x003CFF 蓝 | 雨 |
| 600–699 | 0xA0C8FF 冰蓝 | 雪 |
| 700–799 | 0x787878 灰 | 雾 / 霾 |
| 800 | 0xFFC800 黄 | 晴 |
| 801–802 | 0xB4B43C 淡黄 | 少云 / 疏云 |
| 803–804 | 0x8C8C8C 灰 | 多云 / 阴 |
| 其他 | 0xFFFFFF 白 | 未知 |

> 旧实现 `WeatherService::_codeToColor()`(见 [tools/trash/WeatherService.cpp](../tools/trash/WeatherService.cpp))已被上述 `wxColor()` 取代。WeatherService / DisplayManager / ClockFace / InfoColumns / ButtonHandler / StatusLED / TimeManager 为重构后遗留的死代码,已移入 `tools/trash/`,不参与编译。

> 如需 LED 钟显示中文描述,在请求 URL 追加 `&lang=zh_cn` 即可。
