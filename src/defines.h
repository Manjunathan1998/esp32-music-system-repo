#ifndef PINS_H
#define PINS_H

#define I2S_BCK 27
#define I2S_WS 14
#define I2S_DATA 26

#define ENCODER_CLK_PIN 25
#define ENCODER_DT_PIN 33
#define ENCODER_BTN_PIN 32
#define ENCODER_BTN_HOLD_TIME 2000

#define LGFX_USE_V1 // needed only for lovyanGfx lib

#if defined(USE_ST7735)
#define TFT_W 160
#define TFT_H 128
#else
#define TFT_W 320
#define TFT_H 480
#endif

#define TFT_BL_PIN -1 // display backlight pin
#define TFT_CS_PIN 15
#define TFT_RST_PIN 4
#define TFT_DC_PIN 2
#define TFT_SCLK_PIN 18
#define TFT_MOSI_PIN 23
#define TFT_MISO_PIN -1

#endif
