#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fetch current weather for Guangzhou from OpenWeatherMap and verify
the JSON response against the official documented field list.

Matches the firmware's request style (q=<city>, units=metric) but uses
HTTPS. API key comes from --api-key, OPENWEATHER_API_KEY, tools/key, or a
prompt.

Usage:
  python tools/fetch_weather_test.py
  python tools/fetch_weather_test.py --api-key <KEY>
  python tools/fetch_weather_test.py --city "Guangzhou,CN" --lang zh_cn
  python tools/fetch_weather_test.py --raw     # print response JSON only
"""
import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API_URL = "https://api.openweathermap.org/data/2.5/weather"

_MISSING = object()

# Documented schema: dotted path -> (expected type, required).
# "weather[0]" means the first element of the weather array.
SCHEMA = {
    "coord.lon":         ((int, float), True),
    "coord.lat":         ((int, float), True),
    "weather[0].id":     (int,          True),
    "weather[0].main":   (str,          True),
    "weather[0].description": (str,     True),
    "weather[0].icon":   (str,          True),
    "base":              (str,          True),
    "main.temp":         ((int, float), True),
    "main.feels_like":   ((int, float), True),
    "main.pressure":     ((int, float), True),
    "main.humidity":     ((int, float), True),
    "main.temp_min":     ((int, float), True),
    "main.temp_max":     ((int, float), True),
    "main.sea_level":    ((int, float), False),  # only on land stations
    "main.grnd_level":   ((int, float), False),
    "visibility":        ((int, float), True),
    "wind.speed":        ((int, float), True),
    "wind.deg":          ((int, float), True),
    "wind.gust":         ((int, float), False),  # where available
    "clouds.all":        ((int, float), True),
    "rain.1h":           ((int, float), False),  # where available
    "snow.1h":           ((int, float), False),  # where available
    "dt":                (int,          True),
    "sys.type":          (int,          False),  # internal
    "sys.id":            (int,          False),  # internal
    "sys.message":       (str,          False),  # internal
    "sys.country":       (str,          True),
    "sys.sunrise":       (int,          True),
    "sys.sunset":        (int,          True),
    "timezone":          (int,          True),
    "id":                (int,          True),
    "name":              (str,          True),
    "cod":               ((int, str),   True),
}

# Fields the ESP8266 firmware (WeatherFetch.cpp) actually parses
FIRMWARE_FIELDS = ("weather[0].id", "main.temp", "weather[0].description")


def get_path(obj, path):
    """Walk a dotted path into a nested dict; weather[0] selects array
    element 0, keys like rain.1h work as-is."""
    cur = obj
    for part in path.split("."):
        if "[" in part and part.endswith("]"):
            name, idx = part[:-1].split("[", 1)
            try:
                cur = cur[name][int(idx)]
            except (KeyError, IndexError, TypeError):
                return _MISSING
        else:
            if not isinstance(cur, dict) or part not in cur:
                return _MISSING
            cur = cur[part]
    return cur


def load_key_from_file():
    """Parse tools/key (next to this script). Supports:
    - key = value lines: returns the value of 'weather_api_key'
    - bare line (legacy): the line itself is the key
    """
    key_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "key")
    try:
        with open(key_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    k, v = line.split("=", 1)
                    if k.strip() == "weather_api_key" and v.strip():
                        return v.strip()
                else:
                    return line
    except OSError:
        pass
    return None


def fetch(city, api_key, units, lang):
    params = {"q": city, "appid": api_key, "units": units}
    if lang:
        params["lang"] = lang
    url = API_URL + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "led-clock-weather-check/1.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return resp.status, json.loads(resp.read().decode("utf-8"))
    return None, None  # unreachable; kept for clarity


def type_name(t):
    return t.__name__


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--city", default="Guangzhou,CN",
                    help='city query for OWM geocoder (default: "Guangzhou,CN")')
    ap.add_argument("--api-key", default=os.environ.get("OPENWEATHER_API_KEY"),
                    help="OpenWeatherMap API key (env: OPENWEATHER_API_KEY)")
    ap.add_argument("--units", default="metric", choices=["standard", "metric", "imperial"])
    ap.add_argument("--lang", default=None, help='language, e.g. zh_cn (default: English)')
    ap.add_argument("--raw", action="store_true",
                    help="print the raw JSON response only")
    args = ap.parse_args()

    api_key = args.api_key or load_key_from_file()
    if not api_key:
        api_key = input("API key: ").strip()
    if not api_key:
        sys.exit("no API key — pass --api-key, set OPENWEATHER_API_KEY, or create tools/key")

    try:
        status, data = fetch(args.city, api_key, args.units, args.lang)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")
        print(f"HTTP {e.code}: {body}")
        sys.exit(1)
    except urllib.error.URLError as e:
        sys.exit(f"network error: {e.reason}")

    if args.raw:
        print(json.dumps(data, ensure_ascii=False, indent=2))
        return

    print(f"HTTP {status}  city={args.city}  units={args.units}"
          + (f"  lang={args.lang}" if args.lang else ""))

    # ── Field-by-field comparison against the documented schema ─────────────
    print(f"\n{'field':<24} {'expected':<12} {'status':<13} value")
    print("-" * 78)
    missing = type_bad = 0
    for path, (types, required) in SCHEMA.items():
        v = get_path(data, path)
        if v is _MISSING:
            mark = "OPT-ABSENT" if not required else "** MISSING **"
            if required:
                missing += 1
            print(f"{path:<24} {type_name(types[0]) if isinstance(types, tuple) else type_name(types):<12}"
                  f" {mark:<13}")
            continue
        ok = isinstance(v, types) and not isinstance(v, bool)
        if not ok:
            type_bad += 1
        mark = "OK" if ok else f"** TYPE {type_name(type(v))} **"
        print(f"{path:<24} {type_name(types[0]) if isinstance(types, tuple) else type_name(types):<12}"
              f" {mark:<13} {v!r}")

    # ── Summary ─────────────────────────────────────────────────────────────
    print("-" * 78)
    verdict = "MATCHES DOCS" if not missing and not type_bad else "MISMATCH"
    print(f"summary: {verdict}  (missing required={missing}, type mismatches={type_bad})")

    # fields the docs don't mention (error payloads, e.g. message)
    tops = {p.split(".")[0].split("[")[0] for p in SCHEMA}
    extra = sorted(set(data) - tops)
    if extra:
        print(f"extra top-level keys not in docs: {extra}")

    # ── Firmware-relevant fields ────────────────────────────────────────────
    print("\nfirmware (WeatherFetch.cpp) reads:")
    for path in FIRMWARE_FIELDS:
        print(f"  {path} = {get_path(data, path)!r}")

    # ── Human-readable extras ───────────────────────────────────────────────
    tz = get_path(data, "timezone")
    dt = get_path(data, "dt")
    if tz is not _MISSING and dt is not _MISSING:
        local = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(dt + tz))
        print(f"\nobservation time (local): {local}")
    if get_path(data, "sys.country") is not _MISSING:
        print(f"location: {get_path(data, 'name')}, {get_path(data, 'sys.country')}")

    print("\nraw JSON:")
    print(json.dumps(data, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
