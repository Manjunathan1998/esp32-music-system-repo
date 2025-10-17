// Minimal audio logic for the github issue
// switching to the different audio modes: file player -> BT sink -> file player
// todo add wifi audio to the sequence

// things changed to fix the issue
// * AudioPlayer object creation without new
// * AudioSourceSPIFFS  creation without new
// *

#include <Arduino.h>
// #include "AudioTools.h"
// #include "AudioTools/Disk/AudioSourceSPIFFS.h"
// #include "AudioTools/AudioCodecs/CodecMP3Helix.h"
// #include "driver/i2s.h"
#include "esp_bt.h"

#include <Bounce2.h>
#include "BluetoothA2DPSink.h"
#include "utilities.h"
#include "globals.h"

// Audio objects
AudioInfo info(44100, 2, 16);
I2SStream i2s;

// mp3 startup sound setup
MP3DecoderHelix helix;
AudioSourceSPIFFS source("/", ".mp3");
AudioPlayer player(source, i2s, helix);
EncodedAudioStream out(&i2s, &helix); // output to decoder
BluetoothA2DPSink a2dp_sink(i2s);

// LVGL buffer
#if defined(NO_PSRAM)
static lv_color_t buf[TFT_W * 10]; // Single buffer: only 10 lines
#else
static lv_color_t *buf = NULL; // works with external SRAM
#endif

static lv_disp_draw_buf_t draw_buf;

// i2s callback for printing audio data from the file
void printMetaData(MetaDataType type, const char *str, int len)
{
    Serial.print("==> ");
    Serial.print(toStr(type));
    Serial.print(": ");
    Serial.println(str);
}

void playMp3File(int choice)
{
    if (!cbSet)
    {
        Serial.println("Init new player");
        player.setMetadataCallback(printMetaData);
        cbSet = true;
    }

    if (!player.begin(choice))
    {
        Serial.println("Failed to start player");
        return;
    }

    player.copyAll();
}

// Start BT sink. Call this function when user switches to Bluetooth menu
void startBtSink()
{
    Serial.println("Initializing Bluetooth sink...");

    if (!i2s.isActive())
    {
        Serial.println("i2s is not active, activating");
        auto cfg = i2s.defaultConfig(TX_MODE);
        cfg.pin_bck = I2S_BCK;
        cfg.pin_ws = I2S_WS;
        cfg.pin_data = I2S_DATA;
        cfg.copyFrom(info);
        i2s.begin(cfg);
    }

    a2dp_sink.set_auto_reconnect(true, 4); // Auto reconnect if disconnected
    a2dp_sink.start("ESP32 Music");        // Advertise device name

    Serial.println("Bluetooth sink init done");
}

void stopBtSink()
{
    Serial.println("Stopping A2DP sink...");
    a2dp_sink.disconnect();
    vTaskDelay(600 / portTICK_PERIOD_MS);
}

///////////// RTOS TASKS /////////////
void appTask(void *param)
{
    while (1)
    {
        playMp3File(1); // start sound
        vTaskDelay(600 / portTICK_PERIOD_MS);
        playMp3File(2); // bt pair sound
        i2s.end();
        vTaskDelay(600 / portTICK_PERIOD_MS);

        startBtSink();
        vTaskDelay(15000 / portTICK_PERIOD_MS); // 15 sec

        stopBtSink();
        vTaskDelay(600 / portTICK_PERIOD_MS);

        playMp3File(0); // bye sound
    }
}

void setup()
{
    Serial.begin(115200);

    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to mount SPIFFS");
    }

    // break?? works
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

    // break&& works

    // I2S and audio setup
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    Serial.println("Starting I2S...");
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.copyFrom(info);
    i2s.begin(cfg);

    // Tasks setup
    xTaskCreatePinnedToCore(appTask, "appTask", 4096, NULL, 2, NULL, 1);
}

void loop()
{
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(100));
}
