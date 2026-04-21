#!build
## v1.003 — Weather 401 fix

### Fix
- `fetchWeather()` now distinguishes HTTP 401/400/404 (API key / city
  config errors) from genuine network failures.
  - **401**: logs a clear multi-line message pointing to the web UI
    Configuration page; does NOT count toward the recovery-trigger
    fail counter.  Previously a bad key would silently accumulate
    failures and could eventually force recovery mode.
  - **400**: logs "bad request — check city name".
  - **404**: logs "city not found".
  - Other HTTP errors / timeouts still increment the fail counter
    and trigger recovery after WEATHER_FAIL_LIMIT (5) consecutive hits.
- `weatherFailCount` global tracks *network* failures only; reset to 0
  on every successful fetch.

### Notes
- A 401 from OpenWeatherMap means either the API key has not been
  entered yet, or a newly-created key hasn't activated (can take up
  to 2 hours on a free account).  Enter / re-enter the key in the
  web UI under **Configuration → API Key** and save.
