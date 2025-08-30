#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Bounce2.h>
#include <ESP32Encoder.h>
#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "driver/i2s.h"
#include "esp_bt.h"

#include "BluetoothA2DPSink.h"
#include "utilities.h"
#include "defines.h"
#include "./ui/ui.h"
#include "./ui/actions.h"

// the definition is in platformio.ini
#if defined(USE_ST7735)
#include "DisplayDrv_st7735.h"
LGFX_st7735 lcd;
#else
#include "DisplayDrv_st7796.h"
LGFX_st7796 lcd;
#endif

// GLOBAL VARS AND OBJECTS
bool startupDone = false;
bool btSinkActive = false;
static lv_obj_t *current_screen = NULL;
const char *batteryCharge = "0";

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
  CMD_BAT_UPDATE
};

// 1 - Hello sound; 0 - Bye sound; 2 - bt pair ready
enum SoundFile
{
  F_BYE_SND,
  F_HELLO_SND,
  F_BT_PAIR_SND
};

QueueHandle_t appCommandQueue; // "transmits" the app commands

// Encoder variables
ESP32Encoder encoder;
volatile int64_t encoderPos = 0;
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

// Audio objects
AudioInfo info(44100, 2, 16);
I2SStream i2s;

// mp3 startup sound setup
MP3DecoderHelix helix;
AudioSourceSPIFFS *source = nullptr;
AudioPlayer *player = nullptr;
EncodedAudioStream out(&i2s, &helix); // output to decoder
BluetoothA2DPSink a2dp_sink(i2s);

// LVGL buffer
#if defined(NO_PSRAM)
static lv_color_t buf[TFT_W * 10]; // Single buffer: only 10 lines
#else
static lv_color_t *buf = NULL; // works with external SRAM
#endif

static lv_disp_draw_buf_t draw_buf;

// LVGL input device callback. Allows to use encoder in lvgl UI
void encoderReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  static int64_t last = 0;
  int64_t pos = encoderPos;

  data->enc_diff = pos - last;
  // data->state = button.pressed() ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  last = pos;
}

// FOCUS GROUP SETUP
void setupEncoderFocusGroup()
{
  focus_group = lv_group_create();

  lv_group_add_obj(focus_group, objects.bluetooth);
  lv_group_add_obj(focus_group, objects.inet_radio);
  lv_group_add_obj(focus_group, objects.equalizer);
  lv_group_add_obj(focus_group, objects.settings);

  lv_obj_add_flag(objects.bluetooth, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(objects.inet_radio, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(objects.equalizer, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(objects.settings, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);

  lv_indev_set_group(enc_indev, focus_group);
  lv_group_focus_obj(objects.bluetooth); // focus the first item
}

// i2s callback for printind audio data from the file
void printMetaData(MetaDataType type, const char *str, int len)
{
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void freeI2S()
{
  if (player)
  {
    player->end();
    delete player;
    player = nullptr;
  }

  if (source)
  {
    delete source;
    source = nullptr;
  }

  i2s.flush(); // Flush pending audio frames (safe if I2S is still active)
  i2s.end();
  vTaskDelay(600 / portTICK_PERIOD_MS);
  Serial.println("I2S resources freed..maybe");
}

void playMp3File(int choice)
{
  Serial.println("Init new player");
  // Create a new AudioSourceSPIFFS for this file
  source = new AudioSourceSPIFFS("/", ".mp3");
  player = new AudioPlayer(*source, i2s, helix);
  player->setMetadataCallback(printMetaData);

  if (!player->begin(choice))
  {
    Serial.println("Failed to start player");
    return;
  }

  player->copyAll();
}

void switchToScreen(void *screen_ptr)
{
  lv_obj_t *target = (lv_obj_t *)screen_ptr;
  lv_disp_load_scr(target);
  current_screen = target; // save current active screen
}

// Start BT sink. Call this function when user switches to Bluetooth menu
void initBtSink()
{
  if (btSinkActive)
    return;

  Serial.println("Initializing Bluetooth sink...");

  // a2dp_sink = new BluetoothA2DPSink(i2s);

  if (!i2s.isActive())
  {
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCK;
    cfg.pin_ws = I2S_WS;
    cfg.pin_data = I2S_DATA;
    cfg.copyFrom(info);
    i2s.begin(cfg);
  }

  a2dp_sink.set_volume(50);              // Optional: set initial volume
  a2dp_sink.set_auto_reconnect(true, 5); // Auto reconnect if disconnected
  a2dp_sink.start("ESP32 Music");        // Advertise device name

  btSinkActive = true;
  Serial.println("Bluetooth sink init done");
}

void stopBtSink()
{
  if (!btSinkActive)
    return;

  Serial.println("Stopping A2DP sink...");
  a2dp_sink.disconnect();
  vTaskDelay(600 / portTICK_PERIOD_MS);
  a2dp_sink.end();
  vTaskDelay(1600 / portTICK_PERIOD_MS);

  Serial.println("Sink stopped");
  btSinkActive = false;
}

void playMp3FileTask(void *param)
{
  int fileNumber = (int)(intptr_t)param; // cast back to int safely
  playMp3File(fileNumber);
  vTaskDelete(NULL); // kill current task
}

void updateBatteryCharge()
{
  char buffer[10];
  sprintf(buffer, "%s%%", batteryCharge);
  lv_label_set_text(objects.charge, buffer);
  Serial.println("BAT charge updated");
}

///////////// RTOS TASKS /////////////
// main "flow" and events handling
void appTask(void *param)
{
  AppCommand cmd;
  while (1)
  {
    if (xQueueReceive(appCommandQueue, &cmd, portMAX_DELAY) == pdTRUE)
    {
      switch (cmd)
      {
      case CMD_SWITCH_TO_SCR_MAIN:
        lv_async_call([](void *unused)
                      {
                        switchToScreen(objects.main);
                        stopBtSink(); },
                      NULL);
        break;

      case CMD_SWITCH_TO_SCR_BT:
        lv_async_call([](void *unused)
                      {
                        switchToScreen(menu_screens[0]); 
                        // xTaskCreatePinnedToCore(playMp3FileTask, "bt pair snd", 4096, (void *)(intptr_t)F_BT_PAIR_SND, 2, NULL, 1);
                        freeI2S();
                        vTaskDelay(600 / portTICK_PERIOD_MS);
                      initBtSink(); },
                      NULL);
        break;
      case CMD_SWITCH_TO_SCR_WIFI_RADIO:
        lv_async_call([](void *unused)
                      { switchToScreen(menu_screens[1]); },
                      NULL);
        break;
      case CMD_SWITCH_TO_SCR_EQ:
        lv_async_call([](void *unused)
                      { switchToScreen(menu_screens[2]); },
                      NULL);
        break;
      case CMD_SWITCH_TO_SCR_SETTINGS:
        lv_async_call([](void *unused)
                      { switchToScreen(menu_screens[3]); },
                      NULL);
        break;
      case CMD_BT_STOP:
        // todo add bt stop on encoder double click if curr. screen == bt
        Serial.println("Command BT stop");
        break;

      case CMD_BAT_UPDATE:
        updateBatteryCharge();
        break;

      default:
        break;
      }
    }
  }
}

// Encoder AND button handling
void encoderTask(void *param)
{
  int16_t last_val = 0;
  while (1)
  {
    encoderPos = encoder.getCount() / 2;
    button.update(); // must be called repeatedly

    // button logic
    if (button.fell())
    {
      // Button just pressed
      pressStartTime = millis();
      isPressed = true;
      // longPressTriggered = false;
    }

    // Button held long enough
    if (isPressed && (millis() - pressStartTime >= ENCODER_BTN_HOLD_TIME))
    {
      playMp3File(0); // todo playMp3FileTask(0)
      Serial.println("Long press, shutting down...");

      vTaskDelay(100 / portTICK_PERIOD_MS); // allow Serial flush
      esp_deep_sleep_start();
    }

    if (button.rose())
    {
      unsigned long now = millis();
      Serial.println("Short press");

      // Detect double click, if waiting less than "doubleClickThreshold",
      if (waitingForSecondClick && (now - lastClickTime < doubleClickThreshold))
      {
        // switch to the main screen
        AppCommand cmd = CMD_SWITCH_TO_SCR_MAIN;
        xQueueSend(appCommandQueue, &cmd, 0);
        waitingForSecondClick = false;
      }
      else
      {
        waitingForSecondClick = true;
        lastClickTime = now;
      }
      isPressed = false;
    }

    if (waitingForSecondClick && (millis() - lastClickTime >= doubleClickThreshold))
    {
      // single click
      lv_obj_t *focused = lv_group_get_focused(focus_group);
      if (focused)
      {
        AppCommand cmd = CMD_NONE;
        for (int i = 0; i < 4; ++i)
        {
          if (focused == menu_buttons[i])
          {
            switch (i)
            {
            case 0:
              cmd = CMD_SWITCH_TO_SCR_BT;
              Serial.println("CMD_SWITCH_TO_SCR_BT");
              break;
            case 1:
              cmd = CMD_SWITCH_TO_SCR_WIFI_RADIO;
              Serial.println("CMD_SWITCH_TO_SCR_WIFI");
              break;
            case 2:
              cmd = CMD_SWITCH_TO_SCR_EQ;
              Serial.println("CMD_SWITCH_TO_SCR_EQ");
              break;
            case 3:
              cmd = CMD_SWITCH_TO_SCR_SETTINGS;
              Serial.println("CMD_SWITCH_TO_SCR_SETTINGS");
              break;
            }
            break;
          }
        }

        if (cmd != CMD_NONE)
        {
          xQueueSend(appCommandQueue, &cmd, 0);
        }
      }

      waitingForSecondClick = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// LVGL EEZ STUDIO UI loop
void uiTask(void *param)
{
  Serial.println("--> UI loop task start");
  while (1)
  {
    lv_timer_handler();
    ui_tick(); // This is important for EEZ-generated UI
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// parsing commands from serial port. Utility process
void serialTask(void *param)
{
  while (1)
  {
    if (Serial.available() > 0)
    {
      String input = Serial.readString();
      input.trim();
      Serial.println(input);

      // Split at first space
      int spaceIndex = input.indexOf(' ');
      String command = "";
      String value = "0";

      if (spaceIndex > 0)
      {
        command = input.substring(0, spaceIndex); // before space
        value = input.substring(spaceIndex + 1);  // after space
      }
      else
      {
        command = input; // no value, just a command
      }

      // Command handling
      if (command == "bat")
      {
        char buf[16]; // make sure it's large enough
        value.toCharArray(buf, sizeof(buf));
        batteryCharge = buf;
        Serial.print("Battery value received: ");
        Serial.println(value);
        AppCommand cmd = CMD_BAT_UPDATE;
        xQueueSend(appCommandQueue, &cmd, 42);
      }
    }
  }
}

///////////// RTOS TASKS end section /////////////

void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void touchscreen_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

void setup()
{
  Serial.begin(115200);

  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to mount SPIFFS");
  }

  size_t psramSize = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  Serial.printf("Total PSRAM available: %u bytes\n", psramSize);

  if (psramSize == 0)
  {
    Serial.println("Warning: PSRAM not detected!");
  }
  else
  {
    Serial.println("PSRAM is enabled and ready.");
  }

  // Encoder setup start
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENCODER_CLK_PIN, ENCODER_DT_PIN);
  xTaskCreatePinnedToCore(encoderTask, "EncoderTask", 6144, NULL, 1, NULL, 1);
  // Encoder button setup
  button.attach(ENCODER_BTN_PIN, INPUT_PULLUP);
  button.interval(10);
  Serial.println("Encoder initialized");

  Serial.printf("Free heap before BT start: %u bytes\n", esp_get_free_heap_size());

  // I2S and audio setup
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  Serial.println("Starting I2S...");
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCK;
  cfg.pin_ws = I2S_WS;
  cfg.pin_data = I2S_DATA;
  cfg.copyFrom(info);
  i2s.begin(cfg);

  Serial.printf("Free heap after BT start: %u bytes\n", esp_get_free_heap_size());

  xTaskCreatePinnedToCore(playMp3FileTask, "start sound", 4096, (void *)(intptr_t)F_HELLO_SND, 2, NULL, 1);

  // Initialize display
  lcd.init();
  lcd.setBrightness(255); // Set backlight (0-255)

  // draw test rectangle
  lcd.drawRect(0, 0, 20, 10, TFT_RED);
  lcd.drawRect(140, 0, 20, 10, TFT_RED);
  lcd.drawRect(0, 118, 20, 10, TFT_RED);
  lcd.drawRect(140, 118, 20, 10, TFT_RED);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  lv_init();

// Initialize display buffer
#if defined(NO_PSRAM)
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 10);
#else
  Serial.println("SRAM buf setup");
  buf = (lv_color_t *)heap_caps_malloc(TFT_W * 40 * sizeof(lv_color_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 40);
#endif

  // Check buffer allocation
  if (!buf)
  {
    Serial.println("Failed to allocate display buffer!");
    while (1)
      delay(1000);
  }

  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = TFT_W;
  disp_drv.ver_res = TFT_H;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // REGISTER TOUCHSCREEN INPUT
  // static lv_indev_drv_t touch_drv;
  // lv_indev_drv_init(&touch_drv);
  // touch_drv.type = LV_INDEV_TYPE_POINTER;
  // touch_drv.read_cb = touchscreen_read;
  // lv_indev_t *touch_indev = lv_indev_drv_register(&touch_drv);

  // REGISTER ENCODER AS LVGL INPUT
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_ENCODER;
  indev_drv.read_cb = encoderReadCb;
  enc_indev = lv_indev_drv_register(&indev_drv);

  Serial.println("Encoder registered with LVGL");

  ui_init();

  // SETUP FOCUS GROUP
  setupEncoderFocusGroup();
  lv_group_focus_obj(objects.bluetooth);

  menu_buttons[0] = objects.bluetooth;
  menu_buttons[1] = objects.inet_radio;
  menu_buttons[2] = objects.equalizer;
  menu_buttons[3] = objects.settings;

  menu_screens[0] = objects.bt_screen;
  menu_screens[1] = objects.wifi_radio_screen;
  menu_screens[2] = objects.equalizer_screen;
  menu_screens[3] = objects.settings_screen;
  Serial.println("Focus group ready");

  // Tasks setup
  appCommandQueue = xQueueCreate(8, sizeof(AppCommand));
  xTaskCreatePinnedToCore(appTask, "appTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(uiTask, "uiTask", 8096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(serialTask, "serialTask", 4096, NULL, 1, NULL, 1);
}

void loop()
{
  // Feed watchdog
  vTaskDelay(pdMS_TO_TICKS(100));

  // Monitor heap usage
  if (esp_get_free_heap_size() < 10000)
  {
    Serial.println("Low memory warning!");
  }
}

// LVGL callback
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.pushColors((uint16_t *)&color_p->full, w * h, true);
  lcd.endWrite();
  lv_disp_flush_ready(disp);
}

// void touchscreen_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
// {
//   uint16_t touchX, touchY;
//   bool touched = lcd.getTouch(&touchX, &touchY);
//   if (!touched)
//   {
//     data->state = LV_INDEV_STATE_REL;
//   }
//   else
//   {
//     data->state = LV_INDEV_STATE_PR;
//     data->point.x = touchX;
//     data->point.y = touchY;
//   }
// }
