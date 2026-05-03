#pragma once
// WeatherFetch.h — OWM HTTP fetch + recovery trigger

#include <Arduino.h>

// Fetch current weather from OpenWeatherMap (plain HTTP, free tier).
// Reads city + apiKey from configManager.
// Increments weatherFails on network error; triggers recovery after WEATHER_FAIL_MAX.
// Config errors (401/404/400) do NOT increment the counter.
void fetchWeather();

// Set error state and switch to AM_RECOVERY — no reboot.
// (Legacy: previously wrote RTC flag and rebooted.)
void triggerRecovery();
