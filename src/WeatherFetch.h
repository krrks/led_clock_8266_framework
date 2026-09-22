#pragma once
// WeatherFetch.h — OWM HTTP fetch + recovery trigger

#include <Arduino.h>

// Fetch current weather from OpenWeatherMap (plain HTTP, free tier).
// Reads city + apiKey from configManager.
// Increments weatherFails on network error; weather failures NEVER trigger
// recovery — they are only logged and retried on the hourly schedule.
// Config errors (401/404/400) and OWM server issues (429/5xx) do NOT increment the counter.
void fetchWeather();

// Write recovery flag to RTC memory and reboot.
void triggerRecovery();
