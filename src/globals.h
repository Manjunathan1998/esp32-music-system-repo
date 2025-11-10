#pragma once

#include <Arduino.h>
#include <Bounce2.h>
#include <lvgl.h>
#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "driver/i2s.h"
#include <ESP32Encoder.h>

#if defined(Sera) // Sergey pin config
#define I2S_BCK 27
#define I2S_WS 25
#define I2S_DATA 26

#define ENCODER_CLK_PIN 34
#define ENCODER_DT_PIN 4
#define ENCODER_BTN_PIN 35
#define ENCODER_BTN_HOLD_TIME 2000

#define TFT_BL_PIN -1 // 23
#define TFT_CS_PIN 23 // 15
#define TFT_RST_PIN 22
#define TFT_DC_PIN 21
#define TFT_SCLK_PIN 33 // 14 prev
#define TFT_MOSI_PIN 18 // 13
#define TFT_MISO_PIN -1

#elif (Manjun) // Manjunathan pin config
#define I2S_BCK 27
#define I2S_WS 25
#define I2S_DATA 26

#define ENCODER_CLK_PIN 34
#define ENCODER_DT_PIN 4
#define ENCODER_BTN_PIN 35
#define ENCODER_BTN_HOLD_TIME 2000

#define TFT_BL_PIN -1 // 23
#define TFT_CS_PIN 23 // 15
#define TFT_RST_PIN 22
#define TFT_DC_PIN 21
#define TFT_SCLK_PIN 33 // 14 prev
#define TFT_MOSI_PIN 18 // 13
#define TFT_MISO_PIN -1
#endif

#define LGFX_USE_V1 // needed only for lovyanGfx lib

#if defined(USE_ST7735)
#define TFT_W 160
#define TFT_H 128
#else
#define TFT_W 320
#define TFT_H 480
#endif

// GLOBAL VARS AND OBJECTS
bool cbSet = false;
bool startupDone = false;
bool btSinkActive = false;
static lv_obj_t *current_screen = NULL;
String batteryCharge = "0";
// String playbackStatus = "Stopped";
// const char *trackName;
// const char *artistName;

char artistName[128] = {0};
char trackName[128] = {0};
char playbackStatus[128] = {0};
bool metadata_updated = false;
volatile bool playback_status_updated = false;
uint16_t BTvolume = -1;

// All internal commands
enum AppCommand
{
    CMD_NONE,
    CMD_SWITCH_TO_SCR_MAIN,
    CMD_SWITCH_TO_SCR_BT,
    CMD_SWITCH_TO_SCR_WIFI_RADIO,
    CMD_SWITCH_TO_SCR_EQ,
    CMD_SWITCH_TO_SCR_SETTINGS,
    CMD_BT_RESTART,
    CMD_BT_STOP,
    CMD_BAT_UPDATE,
    CMD_SHUT_DOWN
};

// 1 - Hello sound; 0 - Bye sound; 2 - bt pair ready
enum SoundFile
{
    F_BYE_SND,
    F_HELLO_SND,
    F_BT_PAIR_SND
};

enum EncoderDirection
{
    ENC_INCREMENT,
    ENC_DECREMENT
};

QueueHandle_t appCommandQueue; // "transmits" the app commands

// Encoder variables
ESP32Encoder encoder;
volatile int64_t newEncoderPos, oldEncoderPos = 0;
lv_indev_t *enc_indev;
lv_group_t *focus_group;
lv_obj_t *menu_buttons[4];
lv_obj_t *menu_screens[4];

// Button variables
Bounce button = Bounce();
unsigned long pressStartTime = 0;
bool isPressed = false;
bool longPressTriggered = false;
unsigned long lastClickTime = 0;
bool firstClickDetected = false;
const unsigned long doubleClickThreshold = 600; // ms
bool waitingForSecondClick = false;