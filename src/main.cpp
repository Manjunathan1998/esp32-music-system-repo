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
#include "DisplayDriver.h"
#include "./ui/ui.h"
#include "./ui/actions.h"

// GLOBAL VARS AND OBJECTS
bool startupDone = false;
static lv_obj_t *current_screen = NULL;

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

// Display instance
LGFX_Display lcd;

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

// 1 - Hello sound; 0 - Bye sound
void sayHelloBye(bool choice)
{
  // free up the resources for audio stream
  // stop_audio_playback();

  // Create a new AudioSourceSPIFFS for this file
  source = new AudioSourceSPIFFS("/", ".mp3");
  player = new AudioPlayer(*source, i2s, helix);
  player->setMetadataCallback(printMetaData);

  if (!player->begin(choice)) //  hello.mp3 or bye.mp3
  {
    Serial.println("Failed to start player");
    return;
  }

  player->copyAll();
}

void stop_audio_playback()
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
  delay(100);
  Serial.println("Audio playback stopped, resources freed.");
}

void restartBT()
{
  esp_err_t err;

  err = btStop();
  if (err != ESP_OK)
  {
    Serial.printf("btStop failed: %d\n", err);
  }
  else
  {
    Serial.println("btStop success");
  }

  delay(100);

  err = btStart();
  if (err != ESP_OK)
  {
    Serial.printf("btStart failed: %d\n", err);
  }
  else
  {
    Serial.println("btStart success");
  }
}

// Start BT sink. Call this function when user switches to Bluetooth menu
void init_a2dp_sink()
{
  Serial.println("Initializing Bluetooth sink...");
  restartBT();

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCK;
  cfg.pin_ws = I2S_WS;
  cfg.pin_data = I2S_DATA;
  cfg.copyFrom(info);
  i2s.begin(cfg);

  a2dp_sink.set_volume(50);              // Optional: set initial volume
  a2dp_sink.set_auto_reconnect(true, 5); // Auto reconnect if disconnected
  a2dp_sink.start("ESP32 Music");        // Advertise device name

  Serial.println("Bluetooth sink started... maybe");
}

void switchToScreen(void *screen_ptr)
{
  lv_obj_t *target = (lv_obj_t *)screen_ptr;

  // deinit BT when leaving bt_screen
  if (current_screen == objects.bt_screen && target != objects.bt_screen)
  {
    Serial.println("Stopping A2DP sink...");
    a2dp_sink.end();
  }

  // Init BT when switching to bt_screen
  if (target == objects.bt_screen)
  {
    // todo add if bt is initialised
    Serial.println("Starting A2DP sink...");
    stop_audio_playback();
    init_a2dp_sink();
  }

  // load selected screen
  lv_disp_load_scr(target);
  current_screen = target; // save current active screen
}

///////////// RTOS TASKS /////////////

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
      longPressTriggered = false;
    }

    if (isPressed && !longPressTriggered && (millis() - pressStartTime >= ENCODER_BTN_HOLD_TIME))
    {
      // Button held long enough
      sayHelloBye(0);
      Serial.println("Long press detected. Shutting down...");
      longPressTriggered = true;

      vTaskDelay(100 / portTICK_PERIOD_MS); // allow Serial flush
      esp_deep_sleep_start();
    }

    if (button.rose())
    {
      // Button just released
      if (!longPressTriggered)
      {
        unsigned long now = millis();

        // Double Click, switch back to the main screen
        if (waitingForSecondClick && (now - lastClickTime < doubleClickThreshold))
        {
          lv_async_call(switchToScreen, objects.main);
          waitingForSecondClick = false;
        }
        else
        {
          waitingForSecondClick = true;
          lastClickTime = now;
        }
      }

      isPressed = false;
    }

    if (waitingForSecondClick && (millis() - lastClickTime >= doubleClickThreshold))
    {
      // single click
      lv_obj_t *focused = lv_group_get_focused(focus_group);
      if (focused)
      {
        for (int i = 0; i < 4; ++i)
        {
          if (focused == menu_buttons[i])
          {
            lv_async_call(switchToScreen, menu_screens[i]);
            break;
          }
        }
      }

      waitingForSecondClick = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void playStartSoundTask(void *param)
{
  sayHelloBye(1);
  startupDone = true;
  vTaskDelete(NULL); // kill current task
}

// LVGL EEZ STUDIO UI loop
void uiTask(void *param)
{
  Serial.println("--> UI loop task start");
  while (1)
  {
    lv_timer_handler();
    ui_tick(); // This is important for EEZ-generated UIs
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

///////////// RTOS TASKS end /////////////

// LVGL buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;

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

  // a2dp_sink.set_volume(50);
  // a2dp_sink.set_auto_reconnect(true, 5);
  // a2dp_sink.start("ESP32 Music");

  Serial.printf("Free heap after BT start: %u bytes\n", esp_get_free_heap_size());

  xTaskCreatePinnedToCore(playStartSoundTask, "start sound", 4096, NULL, 2, NULL, 1);

  // Initialize display
  lcd.init();
  lcd.setBrightness(255); // Set backlight (0-255)
  lv_init();

  // Initialize display buffer
  buf = (lv_color_t *)heap_caps_malloc(TFT_W * 40 * sizeof(lv_color_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  // Check buffer allocation
  if (!buf)
  {
    Serial.println("Failed to allocate display buffer!");
    while (1)
      delay(1000);
  }

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 40);

  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = TFT_W;
  disp_drv.ver_res = TFT_H;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // REGISTER TOUCHSCREEN INPUT
  static lv_indev_drv_t touch_drv;
  lv_indev_drv_init(&touch_drv);
  touch_drv.type = LV_INDEV_TYPE_POINTER;
  touch_drv.read_cb = touchscreen_read;
  lv_indev_t *touch_indev = lv_indev_drv_register(&touch_drv);

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

  xTaskCreatePinnedToCore(uiTask, "ui task", 4096, NULL, 1, NULL, 1);
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

void touchscreen_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  bool touched = lcd.getTouch(&touchX, &touchY);
  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
  }
}
