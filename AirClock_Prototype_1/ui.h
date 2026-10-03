// This has some datatypes, structures and functions useful in multiple files

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <time.h>
#include "config.h"
#include "font_renderer.h"

#if defined(DISPLAY_OLED) && defined(DISPLAY_TFT)
  #error "config.h: enable only one of DISPLAY_OLED / DISPLAY_TFT"
#endif

#if defined(DISPLAY_OLED)
  #include <Adafruit_SSD1306.h>
  static Adafruit_SSD1306 panel(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
  #define COL_BG    0
  #define COL_TIME  1
  #define COL_DATE  1
  #define COL_TEMP  1
  #define COL_SUB   1
  #define COL_LOGO_A 1
  #define COL_LOGO_B 1
#elif defined(DISPLAY_TFT)
  #include <SPI.h>
  #include <Adafruit_ILI9341.h>
  static Adafruit_ILI9341 panel(TFT_CS, TFT_DC, TFT_RST);
  #define COL_BG    0x0000
  #define COL_TIME  0xFFFF
  #define COL_DATE  0x8410
  #define COL_TEMP  0xFD20
  #define COL_SUB   0x8410
  #define COL_LOGO_A 0xFFFF
  #define COL_LOGO_B 0xFD20
#else
  #error "config.h: enable DISPLAY_OLED or DISPLAY_TFT"
#endif

enum ClockMode : uint8_t { MODE_TIME, MODE_WEATHER, MODE_BOTH, MODE_COUNT };

enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG, BTN_SETUP };

struct WeatherData {
  bool  valid    = false;
  float temp     = 0;
  bool  hasRange = false;
  float tmax     = 0;
  float tmin     = 0;
  int   code     = -1;
  bool  isDay    = true;
};

static WeatherIcon iconFor(int code, bool isDay) {
  if (code == 0 || code == 1)  return isDay ? ICON_SUN : ICON_MOON;
  if (code == 2)               return ICON_PARTLY;
  if (code == 3)               return ICON_CLOUD;
  if (code == 45 || code == 48) return ICON_FOG;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return ICON_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)   return ICON_SNOW;
  if (code >= 95 && code <= 99) return ICON_STORM;
  return ICON_CLOUD;
}

static const char *labelFor(WeatherIcon icon) {
  switch (icon) {
    case ICON_SUN:    return "CLEAR";
    case ICON_MOON:   return "CLEAR";
    case ICON_PARTLY: return "PARTLY CLOUDY";
    case ICON_CLOUD:  return "CLOUDY";
    case ICON_FOG:    return "FOG";
    case ICON_RAIN:   return "RAIN";
    case ICON_STORM:  return "STORM";
    case ICON_SNOW:   return "SNOW";
    default:          return "";
  }
}

static void iconColors(WeatherIcon icon, uint16_t &a, uint16_t &b) {
#if defined(DISPLAY_TFT)
  switch (icon) {
    case ICON_SUN:    a = 0xFFE0; b = 0xFFE0; break;
    case ICON_MOON:   a = 0xEF7D; b = 0xEF7D; break;
    case ICON_PARTLY: a = 0xC618; b = 0xFFE0; break;
    case ICON_CLOUD:  a = 0xC618; b = 0xC618; break;
    case ICON_FOG:    a = 0x8410; b = 0x8410; break;
    case ICON_RAIN:   a = 0xC618; b = 0x04FF; break;
    case ICON_STORM:  a = 0x9CD3; b = 0xFFE0; break;
    case ICON_SNOW:   a = 0xFFFF; b = 0xFFFF; break;
    default:          a = b = 0xFFFF;         break;
  }
#else
  (void)icon;
  a = b = COL_TEMP;
#endif
}

static inline void ui_flush() {
#if defined(DISPLAY_OLED)
  panel.display();
#endif
}

static bool ui_begin() {
#if defined(DISPLAY_OLED)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  if (!panel.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) return false;
#else
  #if TFT_BL >= 0
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  #endif
  panel.begin();
  panel.setRotation(TFT_ROTATION);
#endif
  panel.fillScreen(COL_BG);
  ui_flush();
  return true;
}

static void ui_screen(bool on) {
#if defined(DISPLAY_OLED)
  panel.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
#else
  #if TFT_BL >= 0
  digitalWrite(TFT_BL, on ? HIGH : LOW);
  #endif
#endif
}

static void ui_dim(bool dim) {
#if defined(DISPLAY_OLED)
  panel.ssd1306_command(SSD1306_SETCONTRAST);
  panel.ssd1306_command(dim ? NIGHT_CONTRAST : DAY_CONTRAST);
#else
  (void)dim;
#endif
}

static void ui_message(const char *l1, const char *l2 = "", const char *l3 = "", const char *l4 = "") {
  panel.fillScreen(COL_BG);
  panel.setTextColor(COL_TIME);
  panel.setTextSize(panel.width() >= 240 ? 2 : 1);
  panel.setCursor(4, 4);
  panel.println(l1);
  if (l2[0]) panel.println(l2);
  if (l3[0]) panel.println(l3);
  if (l4[0]) panel.println(l4);
  ui_flush();
}

static void ui_status(const char *l1, const char *l2 = "", const char *l3 = "", const char *l4 = "") {
#if DEBUG
  ui_message(l1, l2, l3, l4);
#else
  (void)l1; (void)l2; (void)l3; (void)l4;
#endif
}

static void ui_splash() {
  const char *name = "AIRCLOCK";
  const int W = panel.width(), H = panel.height();

  int lp = 1, tp = 1;
  for (int p = 20; p > 1; p--) {
    int t = p * 2 / 3;
    if (t < 1) t = 1;
    const int total = dot_logo_height(p) + p + 2 + dot_text_height(t);
    if (total <= H - 4 && dot_logo_width(p) <= W - 4 && dot_text_width(name, t) <= W - 4) {
      lp = p;
      tp = t;
      break;
    }
  }

  const int lh  = dot_logo_height(lp);
  const int th  = dot_text_height(tp);
  const int gap = lp + 2;
  const int y0  = (H - (lh + gap + th)) / 2;

  panel.fillScreen(COL_BG);
  dot_logo(&panel, (W - dot_logo_width(lp)) / 2, y0, lp, COL_LOGO_A, COL_LOGO_B);
  dot_text(&panel, name, (W - dot_text_width(name, tp)) / 2, y0 + lh + gap, tp, COL_TIME);
  ui_flush();
}

struct Rect { int x, y, w, h; };
struct Layout { Rect time, date, wx, sub, label; };

static Layout layoutFor(ClockMode m) {
  Layout L = {};
#if defined(DISPLAY_OLED)
  switch (m) {
    case MODE_TIME:
      L.time = {4,  4, 120, 34};
      L.date = {4, 44, 120, 16};
      break;
    case MODE_WEATHER:
      L.wx   = {4,  2, 120, 40};
      L.sub  = {4, 44, 120, 16};
      break;
    default:
      L.time = {4,  2, 120, 30};
      L.wx   = {4, 34, 120, 30};
      break;
  }
#else
  const int W = panel.width(), H = panel.height();
  const int mx = W / 32, fw = W - 2 * mx;
  auto band = [&](int yPct, int hPct) { return Rect{mx, H * yPct / 100, fw, H * hPct / 100}; };
  switch (m) {
    case MODE_TIME:
      L.time  = band(18, 38);
      L.date  = band(62, 18);
      break;
    case MODE_WEATHER:
      L.wx    = band(10, 50);
      L.sub   = band(66, 12);
      L.label = band(80, 12);
      break;
    default:
      L.time  = band(6, 42);
      L.wx    = band(55, 38);
      break;
  }
#endif
  return L;
}

static int fitText(const char *text, int maxW, int maxH, int maxPitch) {
  for (int p = maxPitch; p > 1; p--) {
    if (dot_text_height(p) <= maxH && dot_text_width(text, p) <= maxW) return p;
  }
  return 1;
}

static int fitRow(const char *text, int maxW, int maxH) {
  for (int p = 30; p > 1; p--) {
    const int is = dot_icon_size(p);
    if (is <= maxH && is + 2 * p + dot_text_width(text, p) <= maxW) return p;
  }
  return 1;
}

static void clearRect(const Rect &r) { panel.fillRect(r.x, r.y, r.w, r.h, COL_BG); }

static void drawCentered(const Rect &r, const char *text, uint16_t color, int maxPitch, int sx, int sy) {
  const int p = fitText(text, r.w - 2, r.h - 2, maxPitch);
  const int w = dot_text_width(text, p), h = dot_text_height(p);
  dot_text(&panel, text, r.x + (r.w - w) / 2 + sx, r.y + (r.h - h) / 2 + sy, p, color);
}

static void formatTemp(char *out, size_t n, const WeatherData &w) {
  const char unit = USE_FAHRENHEIT ? 'F' : 'C';
  if (w.valid) snprintf(out, n, "%d%c%c", (int)lroundf(w.temp), GLYPH_DEGREE, unit);
  else         snprintf(out, n, "--%c%c", GLYPH_DEGREE, unit);
}

static void drawWeatherRow(const Rect &r, const WeatherData &w, int sx, int sy) {
  char tb[16];
  formatTemp(tb, sizeof tb, w);
  const WeatherIcon ic = w.valid ? iconFor(w.code, w.isDay) : ICON_CLOUD;

  const int p     = fitRow(tb, r.w - 2, r.h - 2);
  const int isz   = dot_icon_size(p);
  const int gap   = 2 * p;
  const int tw    = dot_text_width(tb, p);
  const int th    = dot_text_height(p);
  const int x     = r.x + (r.w - (isz + gap + tw)) / 2 + sx;
  const int y     = r.y + (r.h - isz) / 2 + sy;

  uint16_t ca, cb;
  iconColors(ic, ca, cb);
  dot_icon(&panel, ic, x, y, p, ca, cb);
  dot_text(&panel, tb, x + isz + gap, y + (isz - th) / 2, p, COL_TEMP);
}

static void ui_render(ClockMode mode, const struct tm *t, const WeatherData &w, bool full, bool timeDirty, bool wxDirty) {
  int sx = 0, sy = 0;
#if defined(DISPLAY_OLED)
  full = true;
  if (t) {
    sx = (t->tm_min % 3) - 1;
    sy = ((t->tm_min / 3) % 3) - 1;
  }
#endif

  const Layout L = layoutFor(mode);
  if (full) panel.fillScreen(COL_BG);

  if (full || timeDirty) {
    char tbuf[8] = "--:--";
    char dbuf[24] = "";
    if (t) {
#if USE_24H
      snprintf(tbuf, sizeof tbuf, "%02d:%02d", t->tm_hour, t->tm_min);
#else
      int h12 = t->tm_hour % 12;
      if (h12 == 0) h12 = 12;
      snprintf(tbuf, sizeof tbuf, "%d:%02d", h12, t->tm_min);
#endif
      strftime(dbuf, sizeof dbuf, "%a %d %b", t);
#if !USE_24H
      size_t n = strlen(dbuf);
      snprintf(dbuf + n, sizeof dbuf - n, " %s", t->tm_hour >= 12 ? "PM" : "AM");
#endif
    }
    if (L.time.w) {
      if (!full) clearRect(L.time);
      drawCentered(L.time, tbuf, COL_TIME, 30, sx, sy);
    }
    if (L.date.w) {
      if (!full) clearRect(L.date);
      drawCentered(L.date, dbuf, COL_DATE, 12, sx, sy);
    }
  }

  if (full || wxDirty) {
    if (L.wx.w) {
      if (!full) clearRect(L.wx);
      drawWeatherRow(L.wx, w, sx, sy);
    }
    if (L.sub.w) {
      if (!full) clearRect(L.sub);
      if (w.valid && w.hasRange) {
        char sb[24];
        snprintf(sb, sizeof sb, "H%d L%d", (int)lroundf(w.tmax), (int)lroundf(w.tmin));
        drawCentered(L.sub, sb, COL_SUB, 12, sx, sy);
      }
    }
    if (L.label.w) {
      if (!full) clearRect(L.label);
      if (w.valid) drawCentered(L.label, labelFor(iconFor(w.code, w.isDay)), COL_SUB, 12, sx, sy);
    }
  }

  ui_flush();
}
