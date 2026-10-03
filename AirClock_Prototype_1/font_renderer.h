// This is for the custom dot-style font

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

#define GLYPH_DEGREE '*'

enum WeatherIcon : uint8_t {
  ICON_SUN,
  ICON_MOON,
  ICON_PARTLY,
  ICON_CLOUD,
  ICON_FOG,
  ICON_RAIN,
  ICON_STORM,
  ICON_SNOW,
  ICON_COUNT
};

int dot_radius(int pitch);
int dot_text_width(const char *text, int pitch);
int dot_text_height(int pitch);
int dot_icon_size(int pitch);

void dot_text(Adafruit_GFX *gfx, const char *text, int x, int y, int pitch, uint16_t color);
void dot_icon(Adafruit_GFX *gfx, WeatherIcon icon, int x, int y, int pitch, uint16_t color_a, uint16_t color_b);

int dot_logo_width(int pitch);
int dot_logo_height(int pitch);
void dot_logo(Adafruit_GFX *gfx, int x, int y, int pitch, uint16_t color_cloud, uint16_t color_clock);
