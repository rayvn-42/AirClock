// include libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <sys/time.h>
// custom libraries
#include "config.h"
#include "ui.h"
// create the prefrences object
Preferences prefs;
// initial clock mode
ClockMode mode = MODE_BOTH;
WeatherData wx;
bool screenOn = true;
bool needFull = true;
bool wxDirty = false;
int lastMinute = -2;
long utcOffset = 0;
uint32_t nextSync = 0;

// function to turn url into valid format (for example half life! -> half%20life%21)
static String urlEncode(const String &s) {
  String out;
  char buf[4];
  for (size_t i = 0; i < s.length(); i++) {
    const unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      snprintf(buf, sizeof buf, "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

// send the request, fetch the response and clean up and end connection
static bool finishGet(HTTPClient &http, String &body, const char *tag) {
  const int code = http.GET();
  if (code == HTTP_CODE_OK) body = http.getString();
  http.end();
  Serial.printf("GET %s -> %d\n", tag, code);
  return code == HTTP_CODE_OK;
}

// I mean gets a value from a url
static bool httpGet(const String &hostAndPath, String &body) {
  {
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    if (http.begin("http://" + hostAndPath) && finishGet(http, body, "http")) return true;
  }

  WiFiClientSecure secure;
  secure.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  return http.begin(secure, "https://" + hostAndPath) && finishGet(http, body, "https");
}

// get a city's geocode (Latitude & Longitude)
static bool geocode(const String &city, float &lat, float &lon) {
  String body;
  const String url = "geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=" + urlEncode(city);
  if (!httpGet(url, body)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  JsonVariant hit = doc["results"][0];
  if (hit.isNull() || hit["latitude"].isNull() || hit["longitude"].isNull()) return false;

  lat = hit["latitude"].as<float>();
  lon = hit["longitude"].as<float>();
  return true;
}

// auto update Geocode when user sets new settings
static void resolveLocation() {
  const String city = prefs.getString("city", "");
  if (city.length() == 0 || city == prefs.getString("gcity", "")) return;

  float lat, lon;
  if (geocode(city, lat, lon)) {
    prefs.putFloat("lat", lat);
    prefs.putFloat("lon", lon);
    prefs.putString("gcity", city);
    Serial.printf("Location '%s' -> %.4f, %.4f\n", city.c_str(), lat, lon);
  } else {
    Serial.printf("Could not find '%s', keeping the old location\n", city.c_str());
  }
}

// pretty self explanatory, just gets the weather
static bool fetchWeather() {
  const float lat = prefs.getFloat("lat", DEFAULT_LAT);
  const float lon = prefs.getFloat("lon", DEFAULT_LON);

  char url[300];
  snprintf(url, sizeof url, "api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code,is_day&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto&temperature_unit=%s",lat, lon, USE_FAHRENHEIT ? "fahrenheit" : "celsius");

  String body;
  if (!httpGet(url, body)) return false;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }

  JsonVariant temp = doc["current"]["temperature_2m"];
  if (temp.isNull()) {
    Serial.println("No temperature in response for some reason ;)");
    return false;
  }

  wx.temp  = temp.as<float>();
  wx.code  = doc["current"]["weather_code"] | -1;
  wx.isDay = (doc["current"]["is_day"] | 1) != 0;

  JsonVariant hi = doc["daily"]["temperature_2m_max"][0];
  JsonVariant lo = doc["daily"]["temperature_2m_min"][0];
  wx.hasRange = !hi.isNull() && !lo.isNull();
  if (wx.hasRange) {
    wx.tmax = hi.as<float>();
    wx.tmin = lo.as<float>();
  }
  wx.valid = true;

  utcOffset = doc["utc_offset_seconds"] | 0L;
  Serial.printf("Weather: %.1f, code %d, day %d, utc offset %ld\n", wx.temp, wx.code, wx.isDay, utcOffset);
  return true;
}

// syncs the time
static bool syncTime() {
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  configTime(utcOffset, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  const uint32_t t0 = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && millis() - t0 < 8000) delay(50);

  struct tm t;
  const bool ok = getLocalTime(&t, 0);
  Serial.printf("NTP: %s\n", ok ? "ok" : "failed");
  return ok;
}

// initialize and connect to wifi
static bool wifiUp(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER);
  WiFi.begin();
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

// disconnect from wifi
static void wifiDown() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

// setup the wifi manager
static bool wifiManagerRun(bool forcePortal) {
  const String city = prefs.getString("city", "");

  WiFiManager wm;
  WiFiManagerParameter pCity("city", "City for the weather (e.g. New York)", city.c_str(), 40);
  wm.addParameter(&pCity);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);

  bool portalShown = false;
  wm.setAPCallback([&portalShown](WiFiManager *) {
    portalShown = true;
    ui_message("WIFI SETUP", "WiFi: " AP_NAME, "Pass: " AP_PASSWORD, "Open 192.168.4.1");
  });

  const bool ok = forcePortal ? wm.startConfigPortal(AP_NAME, AP_PASSWORD) : wm.autoConnect(AP_NAME, AP_PASSWORD);

  if (portalShown) {
    String c = pCity.getValue();
    c.trim();
    prefs.putString("city", c);
  }
  return ok;
}

// sync all data
static void doSync() {
  bool ok = false;
  if (wifiUp(15000)) {
    resolveLocation();
    const bool w = fetchWeather();
    const bool n = syncTime();
    ok = w && n;
  } else {
    Serial.println("WiFi connect failed");
  }
  wifiDown();

  nextSync = millis() + (ok ? SYNC_INTERVAL_MS : SYNC_RETRY_MS);
  wxDirty = true;
  if (NIGHT_DIM && wx.valid) ui_dim(!wx.isDay);
}

// poll if the button is pressed (debounced)
static BtnEvent pollButton() {
  static bool down = false, setupFired = false;
  static uint32_t t0 = 0;
  const uint32_t now = millis();
  const bool pressed = digitalRead(BTN_PIN) == LOW;

  if (pressed && !down) {
    down = true;
    setupFired = false;
    t0 = now;
  } else if (pressed && down) {
    if (!setupFired && now - t0 >= SETUP_PRESS_MS) {
      setupFired = true;
      return BTN_SETUP;
    }
  } else if (!pressed && down) {
    down = false;
    const uint32_t held = now - t0;
    if (setupFired || held < 30) return BTN_NONE;
    return held >= LONG_PRESS_MS ? BTN_LONG : BTN_SHORT;
  }
  return BTN_NONE;
}

// toggle the screen
static void setScreen(bool on) {
  screenOn = on;
  ui_screen(on);
  if (on) needFull = true;
}

// open the wifi manager portal
static void openPortal() {
  setScreen(true);
  ui_status("WIFI SETUP", "Starting...");
  wifiManagerRun(true);
  wifiDown();
  nextSync = millis();
  needFull = true;
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  Serial.printf("\nReset reason: %d\n", (int)esp_reset_reason());

  prefs.begin("wclock", false);
  const uint8_t m = prefs.getUChar("mode", MODE_BOTH);
  mode = m < MODE_COUNT ? (ClockMode)m : MODE_BOTH;

  pinMode(BTN_PIN, INPUT_PULLUP);
  const bool forcePortal = digitalRead(BTN_PIN) == LOW;

  if (!ui_begin()) {
    Serial.println("Display init failed.");
    for (;;) delay(1000);
  }

  ui_splash();
  const uint32_t splashStart = millis();

  ui_status("WEATHER CLOCK", "Connecting WiFi...");
  if (!wifiManagerRun(forcePortal)) {
    ui_status("WiFi setup failed", "Restarting...");
    delay(2500);
    ESP.restart();
  }

  ui_status("WEATHER CLOCK", "Getting time and", "weather...");
  doSync();
#if !DEBUG
  while (millis() - splashStart < 1500) delay(10);
#endif
  needFull = true;
}

void loop() {
  switch (pollButton()) {
    case BTN_SHORT:
      if (!screenOn) {
        setScreen(true);
      } else {
        mode = (ClockMode)((mode + 1) % MODE_COUNT);
        prefs.putUChar("mode", mode);
        needFull = true;
      }
      break;
    case BTN_LONG:
      setScreen(!screenOn);
      break;
    case BTN_SETUP:
      openPortal();
      break;
    default:
      break;
  }

  if ((int32_t)(millis() - nextSync) >= 0) doSync();

  struct tm t;
  const bool timeOk = getLocalTime(&t, 0);
  const int  minute = timeOk ? t.tm_min : -1;
  const bool minuteChanged = minute != lastMinute;

  if (screenOn && (needFull || wxDirty || minuteChanged)) {
    ui_render(mode, timeOk ? &t : nullptr, wx, needFull, minuteChanged, wxDirty);
    needFull = false;
    wxDirty = false;
  }
  lastMinute = minute;
  if (!screenOn) wxDirty = false;

  if (digitalRead(BTN_PIN) == LOW || needFull || wxDirty) {
    delay(10);
    return;
  }

  int32_t msToSync = (int32_t)(nextSync - millis());
  if (msToSync < 50) msToSync = 50;
  uint64_t us = (uint64_t)msToSync * 1000ULL;

  if (screenOn && timeOk) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    const uint64_t toMinute = (uint64_t)(60 - tv.tv_sec % 60) * 1000000ULL - tv.tv_usec + 20000ULL;
    if (toMinute < us) us = toMinute;
  } else if (screenOn) {
    if (us > 1000000ULL) us = 1000000ULL;
  }

#if USE_LIGHT_SLEEP
  Serial.printf("sleep %u ms\n", (unsigned)(us / 1000ULL));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(us);
  gpio_wakeup_enable((gpio_num_t)BTN_PIN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  const esp_err_t err = esp_light_sleep_start();
  Serial.printf("woke: cause %d, err %d\n", (int)esp_sleep_get_wakeup_cause(), (int)err);
  if (err != ESP_OK) delay(100);
#else
  delay(20);
#endif
}
