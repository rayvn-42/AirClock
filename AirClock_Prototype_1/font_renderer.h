#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

#define GLYPH_DEGREE       128
#define GLYPH_CELSIUS      129
#define GLYPH_FAHRENHEIT   130
#define GLYPH_COLON        131
#define ICON_SUN           132
#define ICON_CLOUD         133
#define ICON_PARTLY_CLOUDY 134
#define ICON_RAIN          135
#define ICON_STORM         136
#define ICON_SNOW          137

void render_dot_char(Adafruit_GFX *gfx, char ch, int x, int y, int dot_r, int pitch, uint16_t color = 0xFFFF);
void render_dot_string(Adafruit_GFX *gfx, const char *text, int x, int y, int dot_r, int pitch, int char_spacing, uint16_t color = 0xFFFF);