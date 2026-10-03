// this contains all the config

#pragma once

// I made it so that you can choose to use either an OLED or TFT
// so its better :)
#define DISPLAY_OLED
// #define DISPLAY_TFT

// OLED configs
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDRESS    0x3C
#define I2C_SDA         21
#define I2C_SCL         22

// TFT configs
#define TFT_CS          5
#define TFT_DC          27
#define TFT_RST         26
#define TFT_BL          25
#define TFT_ROTATION    1

// Button configs
#define BTN_PIN         33
#define LONG_PRESS_MS   1500
#define SETUP_PRESS_MS  6000

// AP (Access point) configs
#define AP_NAME         "WeatherClock"
#define AP_PASSWORD     "clock1234"
#define WIFI_TX_POWER   WIFI_POWER_11dBm

// Geocode defaults
#define DEFAULT_LAT     40.71f
#define DEFAULT_LON     -74.01f

// These are some configs to customize how everything is dusplayed
#define USE_FAHRENHEIT  0
#define USE_24H         1
#define NIGHT_DIM       1
#define NIGHT_CONTRAST  0x30
#define DAY_CONTRAST    0xCF
// If this is set (to 1) instead of the "AIRCLOCK" splash it shows status messages, used for debugging
#define DEBUG           0

// use the light sleep loop, more power efficient
#define USE_LIGHT_SLEEP   1

// These are intervals for how long to wait betwenn each sync & retry
#define SYNC_INTERVAL_MS  (15UL * 60UL * 1000UL)
#define SYNC_RETRY_MS     (2UL * 60UL * 1000UL)