#pragma once
// WeatherFetch.h — OWM HTTP fetch + recovery trigger

#include <Arduino.h>

// Fetch current weather from OpenWeatherMap (plain HTTP, free tier).
// Reads city + apiKey from configManager.
// Increments weatherFails on network error; triggers recovery after WEATHER_FAIL_MAX.
// Config errors (401/404/400) do NOT increment the counter.
void fetchWeather();

// Write recovery flag to RTC memory and reboot.
void triggerRecovery();
