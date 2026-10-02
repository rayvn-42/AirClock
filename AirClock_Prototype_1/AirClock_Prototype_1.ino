// Wifi and Http libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <esp_system.h>

// Display libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "font_renderer.h"

// Defaults for OLED Screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RST -1
#define SCREEN_ADDRESS 0x3C
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Open meteo url from where we get the weather info
const char *open_meteo_url = "http://api.open-meteo.com/v1/forecast?latitude=34.68&longitude=-1.91&current=temperature_2m&timezone=auto";

// These are to track how long last fetch was (so that it fetches every 1m = 1m * 60s/m * 1000ms/s = 60,000ms)
unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL = 60000;
bool firstFetchDone = false;

// Font settings
#define DOT_R 1
#define PITCH 4
#define SPACING 3

void showStatus(const char *line1, const char *line2 = "") {
  Serial.println(line1);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2[0]) display.println(line2);
  display.display();
}

bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("WiFi lost", "reconnecting...");
    WiFi.reconnect();
    return false;
  }

  HTTPClient http;
  http.setTimeout(10000);
  http.setConnectTimeout(10000);
  if (!http.begin(open_meteo_url)) {
    showStatus("http.begin failed");
    return false;
  }

  int code = http.GET();
  Serial.printf("HTTP code: %d\n", code);

  if (code != 200) {
    char msg[24];
    snprintf(msg, sizeof(msg), "HTTP error %d", code);
    showStatus(msg);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  int start = payload.indexOf('{');
  if (start > 0) payload = payload.substring(start);
  payload.trim();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("Parse failed: %s (len %d)\n", error.c_str(), payload.length());
    showStatus("JSON parse error", error.c_str());
    return false;
  }

  JsonVariant t = doc["current"]["temperature_2m"];
  if (!t.is<float>()) {
    Serial.println("temperature_2m missing or NaN");
    showStatus("No temperature");
    Serial.println(payload);
    return false;
  }

  float temperature = t.as<float>();
  Serial.printf("Temperature: %.1f\n", temperature);

  char buf[12];
  int len = snprintf(buf, sizeof(buf), "%d", (int)lroundf(temperature));
  buf[len++] = (char)GLYPH_DEGREE;
  buf[len++] = (char)GLYPH_CELSIUS;
  buf[len] = '\0';

  render_dot_string_centered(buf);
  return true;
}

void render_dot_string_centered(const char *text) {
  int n = strlen(text);
  int advance = 5 * PITCH + SPACING;
  int width = (n - 1) * advance + 4 * PITCH + 2 * DOT_R;
  int height = 6 * PITCH + 2 * DOT_R;
  int x = (SCREEN_WIDTH - width) / 2;
  int y = (SCREEN_HEIGHT - height) / 2;
  if (x < 0) x = 0;

  display.clearDisplay();
  render_dot_string(&display, text, x, y, DOT_R, PITCH, SPACING, SSD1306_WHITE);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\nReset reason: %d\n", (int)esp_reset_reason());

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;) delay(1000);
  }
  showStatus("Display Ok", "Starting WiFi...");
  delay(500);

  WiFi.mode(WIFI_STA);
  // First begin by getting wifi crendetials
  WiFiManager wm;
  //wm.resetSettings();
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);

  showStatus("Connecting WiFi", "or join AP:");
  if (!wm.autoConnect("ESP32_Config_Portal", "12345678")) {
    showStatus("WiFi failed", "restarting...");
    delay(3000);
    ESP.restart();
  }

  showStatus("WiFi connected", WiFi.localIP().toString().c_str());
  delay(1000);
  showStatus("Fetching weather...");
}

void loop() {
  if (!firstFetchDone || millis() - lastFetch >= FETCH_INTERVAL) {
    firstFetchDone = true;
    lastFetch = millis();
    getWeather();
  }
  delay(10);
}
