#include <Arduino.h>
#include "globals.h"
#include "esp_bt.h"

#include "BluetoothA2DPSink.h"
#include "BluetoothDeviceManager.h"
#include "utilities.h"
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

// Audio objects
AudioInfo info(44100, 2, 16);
I2SStream i2s;

VolumeStream volume_stream(i2s);

// EQ setup
Equalizer3Bands *equalizer = nullptr;
// Initial EQ settings
float bassGain = 1.0;
float midGain = 1.0;
float trebleGain = 1.0;
float volumeLevel = 0.5;

// Audio callback - processes audio data with EQ (for Bluetooth)
void audio_data_callback(const uint8_t *data, uint32_t len)
{
	// Write audio data to equalizer, which processes and outputs to I2S
	equalizer->write(data, len);
}

// mp3 startup sound setup (initialized in setup() after equalizer)
MP3DecoderHelix helix;
AudioSourceSPIFFS source("/", ".mp3");
AudioPlayer *player = nullptr;	   // Will be initialized in setup()
EncodedAudioStream *out = nullptr; // Will be initialized in setup()
BluetoothA2DPSink a2dp_sink(i2s);

// Bluetooth device manager
BluetoothDeviceManager btDeviceManager;

// LVGL buffer
#if defined(NO_PSRAM)
static lv_color_t buf[TFT_W * 10]; // Single buffer: only 10 lines
#else
static lv_color_t *buf = NULL; // works with external SRAM
#endif

static lv_disp_draw_buf_t draw_buf;

// Functions declarations

void drawTestScreen()
{
	// draw test rectangle
	lcd.drawRect(0, 0, 20, 10, TFT_RED);
	lcd.drawRect(140, 0, 20, 10, TFT_RED);
	lcd.drawRect(0, 118, 20, 10, TFT_RED);
	lcd.drawRect(140, 118, 20, 10, TFT_RED);
	vTaskDelay(300 / portTICK_PERIOD_MS);
}

void switchToScreen(void *screen_ptr)
{
	lv_obj_t *target = (lv_obj_t *)screen_ptr;
	lv_disp_load_scr(target);
	current_screen = target; // save current active screen
}

// BT metadata print
void avrc_metadata_callback(uint8_t attr_id, const uint8_t *attr_text)
{
	// Serial.println("meta callback ");
	switch (attr_id)
	{
	case ESP_AVRC_MD_ATTR_TITLE:
		Serial.print("Title: ");
		strncpy(trackName, (const char *)attr_text, sizeof(trackName) - 1);
		trackName[sizeof(trackName) - 1] = '\0';
		break;
	case ESP_AVRC_MD_ATTR_ARTIST:
		Serial.print("Artist: ");
		strncpy(artistName, (const char *)attr_text, sizeof(artistName) - 1);
		artistName[sizeof(artistName) - 1] = '\0';
		break;
	default:
		// Serial.print("Other: ");
		break;
	}
	metadata_updated = true;
	// Serial.println((const char *)attr_text);
}

// BT playback status handling
const char *playbackStatusToStr(esp_avrc_playback_stat_t status)
{
	switch (status)
	{
	case ESP_AVRC_PLAYBACK_STOPPED:
		return "stopped";
	case ESP_AVRC_PLAYBACK_PLAYING:
		return "playing";
	case ESP_AVRC_PLAYBACK_PAUSED:
		return "paused";
	case ESP_AVRC_PLAYBACK_FWD_SEEK:
		return "forward seek";
	case ESP_AVRC_PLAYBACK_REV_SEEK:
		return "reverse seek";
	default:
		return "unknown";
	}
}

void avrc_playback_status_changed(esp_avrc_playback_stat_t playback)
{
	Serial.print("Playback status changed: ");
	Serial.println(playbackStatusToStr(playback));

	switch (playback)
	{
	case ESP_AVRC_PLAYBACK_PLAYING:
		strncpy(playbackStatus, "Playing", sizeof(playbackStatus) - 1);
		break;
	case ESP_AVRC_PLAYBACK_PAUSED:
		strncpy(playbackStatus, "Paused", sizeof(playbackStatus) - 1);
		break;
	case ESP_AVRC_PLAYBACK_STOPPED:
		strncpy(playbackStatus, "Stopped", sizeof(playbackStatus) - 1);
		break;
	default:
		strncpy(playbackStatus, "Unknown", sizeof(playbackStatus) - 1);
		break;
	}
	playbackStatus[sizeof(playbackStatus) - 1] = '\0'; // overflow protection
	playback_status_updated = true;
}

// Bluetooth connection state callback
void bt_connection_state_changed(esp_a2d_connection_state_t state, void *ptr)
{
	Serial.println("========================================");
	if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
	{
		Serial.println("✓ Bluetooth device CONNECTED!");

		// Get connected device address
		auto peer_addr = a2dp_sink.get_last_peer_address();
		if (peer_addr != nullptr)
		{
			Serial.printf("Device MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
						  (*peer_addr)[0], (*peer_addr)[1], (*peer_addr)[2],
						  (*peer_addr)[3], (*peer_addr)[4], (*peer_addr)[5]);
		}

		// Update and display bonded devices list
		Serial.println("\nUpdating bonded devices list...");
		btDeviceManager.updateBondedDevicesList();
		btDeviceManager.printBondedDevices();
	}
	else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
	{
		Serial.println("✗ Bluetooth device DISCONNECTED");
	}
	else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING)
	{
		Serial.println("⟳ Bluetooth device CONNECTING...");
	}
	else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
	{
		Serial.println("⟳ Bluetooth device DISCONNECTING...");
	}
	Serial.println("========================================");
}

// LVGL input device callback. Allows to use encoder with LVGL Menu
void encoderReadCb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
	static int64_t last = 0;
	int64_t pos = newEncoderPos;

	data->enc_diff = pos - last;
	last = pos;
}

void updateBatteryCharge()
{
	uint16_t charge = batteryCharge.toInt();

	if (charge > 69)
	{
		// main page
		lv_obj_set_style_bg_opa(objects.ions_left_20, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60_above, 200, LV_PART_MAIN);

		// BT page
		lv_obj_set_style_bg_opa(objects.ions_left_21, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_61, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_62, 200, LV_PART_MAIN);

		// AWS page
		lv_obj_set_style_bg_opa(objects.ions_left_22, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_63, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_64, 200, LV_PART_MAIN);

		// EQ page
		lv_obj_set_style_bg_opa(objects.ions_left_23, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_65, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_69, 200, LV_PART_MAIN);

		// Settings page
		lv_obj_set_style_bg_opa(objects.ions_left_24, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_67, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_68, 200, LV_PART_MAIN);
	}
	else if (charge > 23 && charge < 70)
	{
		// main page
		lv_obj_set_style_bg_opa(objects.ions_left_20, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60_above, 0, LV_PART_MAIN);

		// BT page
		lv_obj_set_style_bg_opa(objects.ions_left_21, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_61, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_62, 0, LV_PART_MAIN);

		// AWS page
		lv_obj_set_style_bg_opa(objects.ions_left_22, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_63, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_64, 0, LV_PART_MAIN);

		// EQ page
		lv_obj_set_style_bg_opa(objects.ions_left_23, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_65, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_69, 0, LV_PART_MAIN);

		// Settings page
		lv_obj_set_style_bg_opa(objects.ions_left_24, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_67, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_68, 0, LV_PART_MAIN);
	}
	else if (charge < 24)
	{
		// main page
		lv_obj_set_style_bg_opa(objects.ions_left_20, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_60_above, 0, LV_PART_MAIN);

		// BT page
		lv_obj_set_style_bg_opa(objects.ions_left_21, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_61, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_62, 0, LV_PART_MAIN);

		// AWS page
		lv_obj_set_style_bg_opa(objects.ions_left_22, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_63, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_64, 0, LV_PART_MAIN);

		// EQ page
		lv_obj_set_style_bg_opa(objects.ions_left_23, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_65, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_69, 0, LV_PART_MAIN);

		// Settings page
		lv_obj_set_style_bg_opa(objects.ions_left_24, 200, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_67, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(objects.ions_left_68, 0, LV_PART_MAIN);
	}

	Serial.println("BAT charge updated");
}

// bool encDir - true - increment, false - decrement
void handle_volume_control(bool encDir)
{
	BTvolume = a2dp_sink.get_volume();
	if (encDir)
	{
		a2dp_sink.set_volume(BTvolume + 1);
		Serial.print("BTvolume ");
		Serial.println(BTvolume + 1);
	}
	else
	{
		a2dp_sink.set_volume(BTvolume - 1);
		Serial.print("BTvolume ");
		Serial.println(BTvolume - 1);
	}
}

// FOCUS GROUP SETUP
void setupEncoderFocusGroup()
{
	focus_group = lv_group_create();

	lv_group_add_obj(focus_group, objects.a2dp_bluetooth);
	lv_group_add_obj(focus_group, objects.aws_sync);
	lv_group_add_obj(focus_group, objects.equalizer);
	lv_group_add_obj(focus_group, objects.settings);

	lv_obj_add_flag(objects.a2dp_bluetooth, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.aws_sync, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.equalizer, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.settings, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);

	lv_indev_set_group(enc_indev, focus_group);
	lv_group_focus_obj(objects.a2dp_bluetooth); // focus the first item
}

// FOCUS GROUP SETUP FOR EQUALIZER PAGE
void setupEncoderFocusGroupEQ()
{
	focus_group_eq = lv_group_create();
	lv_group_set_wrap(focus_group_eq, true); // Enable wrapping to cycle through all buttons

	lv_group_add_obj(focus_group_eq, objects.btn_theater);
	lv_group_add_obj(focus_group_eq, objects.btn_car);
	lv_group_add_obj(focus_group_eq, objects.btn_cinema);
	lv_group_add_obj(focus_group_eq, objects.btn_flat);

	lv_obj_add_flag(objects.btn_theater, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.btn_car, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.btn_cinema, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(objects.btn_flat, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);

	// Add focus styling for all buttons
	lv_obj_set_style_outline_width(objects.btn_theater, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
	lv_obj_set_style_outline_color(objects.btn_theater, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);

	lv_obj_set_style_outline_width(objects.btn_car, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
	lv_obj_set_style_outline_color(objects.btn_car, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);

	lv_obj_set_style_outline_width(objects.btn_cinema, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
	lv_obj_set_style_outline_color(objects.btn_cinema, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);

	lv_obj_set_style_outline_width(objects.btn_flat, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
	lv_obj_set_style_outline_color(objects.btn_flat, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);

	lv_group_focus_obj(objects.btn_theater); // focus the first button
}

void switchToEQFocusGroup()
{
	Serial.println(">>> switchToEQFocusGroup called");
	lv_indev_set_group(enc_indev, focus_group_eq);
	lv_group_focus_obj(objects.btn_theater);
	Serial.println(">>> EQ focus group active, btn_theater focused");
}

void switchToMainFocusGroup()
{
	Serial.println(">>> switchToMainFocusGroup called");
	lv_indev_set_group(enc_indev, focus_group);
	lv_group_focus_obj(objects.a2dp_bluetooth);
	Serial.println(">>> Main focus group active, a2dp_bluetooth focused");
}

void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

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
	if (!player)
	{
		Serial.println("ERROR: Player not initialized!");
		return;
	}

	if (!cbSet)
	{
		Serial.println("Init new player");
		// player->setMetadataCallback(printMetaData);
		cbSet = true;
	}

	if (!player->begin(choice))
	{
		Serial.println("Failed to start player");
		return;
	}

	player->copyAll();
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

	// Now that I2S is active, apply the stored EQ settings
	if (equalizer)
	{
		Serial.println("Applying EQ for BT mode...");
		Serial.printf("EQ values - Bass: %.1f, Mid: %.1f, Treble: %.1f\n", bassGain, midGain, trebleGain);
		equalizer->setAudioInfo(info);
		auto eq_cfg = equalizer->defaultConfig();
		eq_cfg.sample_rate = 44100;
		eq_cfg.channels = 2;
		eq_cfg.bits_per_sample = 16;
		eq_cfg.freq_low = 500;
		eq_cfg.freq_high = 3000;
		eq_cfg.gain_low = bassGain;
		eq_cfg.gain_medium = midGain;
		eq_cfg.gain_high = trebleGain;
		equalizer->begin(eq_cfg);
		Serial.println("EQ applied");
	}

	a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
	a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playback_status_changed);
	a2dp_sink.set_on_connection_state_changed(bt_connection_state_changed);

	a2dp_sink.set_stream_reader(audio_data_callback, false);

	a2dp_sink.set_auto_reconnect(true, 4); // Auto reconnect if disconnected
	a2dp_sink.start("ESP32 Music");		   // Advertise device name
	btSinkActive = true;
	Serial.println("Bluetooth sink init done");
}

void stopBtSink()
{
	if (btSinkActive)
	{
		Serial.println("Stopping A2DP sink...");
		a2dp_sink.disconnect();
		vTaskDelay(600 / portTICK_PERIOD_MS);
		btSinkActive = false;
		Serial.println("Sink stopped");
	}
	else
	{
		Serial.println("A2DP sink already stopped, doing nothing");
	}
}

void applyEq(float bassGain, float midGain, float trebleGain)
{
	Serial.println("aplyEq called");

	// Apply to equalizer in real-time
	if (equalizer)
	{
		// CRITICAL: Only apply if I2S is active (audio is playing)
		// Calling begin() on idle I2S corrupts equalizer state
		if (!i2s.isActive())
		{
			Serial.println("WARNING: I2S not active, storing EQ values but NOT applying");
			// Just store the values for later
			::bassGain = bassGain;
			::midGain = midGain;
			::trebleGain = trebleGain;
			return;
		}

		auto eq_cfg = equalizer->defaultConfig();
		eq_cfg.sample_rate = 44100;
		eq_cfg.channels = 2;
		eq_cfg.bits_per_sample = 16;
		eq_cfg.freq_low = 500;
		eq_cfg.freq_high = 3000;
		eq_cfg.gain_low = bassGain;
		eq_cfg.gain_medium = midGain;
		eq_cfg.gain_high = trebleGain;
		equalizer->begin(eq_cfg);
		Serial.println("EQ configuration applied (I2S active)");
	}
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
						switchToMainFocusGroup();
                        stopBtSink(); },
							  NULL);
				break;
			case CMD_SWITCH_TO_SCR_BT:

				// CRITICAL: Reinitialize equalizer before MP3/BT to ensure clean state
				// Menu EQ changes may leave equalizer in unstable state
				if (equalizer)
				{
					Serial.println("Reinitializing equalizer before BT mode...");
					equalizer->setAudioInfo(info);
					auto eq_cfg = equalizer->defaultConfig();
					eq_cfg.sample_rate = 44100;
					eq_cfg.channels = 2;
					eq_cfg.bits_per_sample = 16;
					eq_cfg.freq_low = 500;
					eq_cfg.freq_high = 3000;
					eq_cfg.gain_low = bassGain;
					eq_cfg.gain_medium = midGain;
					eq_cfg.gain_high = trebleGain;
					equalizer->begin(eq_cfg);
				}

				playMp3File(2); // bt pair sound
				vTaskDelay(600 / portTICK_PERIOD_MS);
				// i2s.end();
				vTaskDelay(600 / portTICK_PERIOD_MS);
				startBtSink();
				lv_async_call([](void *unused)
							  { switchToScreen(menu_screens[0]); }, NULL);

				break;

			case CMD_SWITCH_TO_SCR_WIFI_RADIO:
				lv_async_call([](void *unused)
							  { switchToScreen(menu_screens[1]); },
							  NULL);
				break;
			case CMD_SWITCH_TO_SCR_EQ:
				lv_async_call([](void *unused)
							  { switchToScreen(menu_screens[2]); 
								switchToEQFocusGroup(); },
							  NULL);
				break;
			case CMD_SWITCH_TO_SCR_SETTINGS:
				lv_async_call([](void *unused)
							  { switchToScreen(menu_screens[3]); },
							  NULL);
				break;

			case CMD_SHUT_DOWN:
				playMp3File(0);
				Serial.println("Shutting down...");
				vTaskDelay(100 / portTICK_PERIOD_MS); // allow Serial flush
				esp_deep_sleep_start();
				break;

			case CMD_BAT_UPDATE:
				updateBatteryCharge();
				break;

			case CMD_EQ_SET_THEATER:
				Serial.println(">>> Executing: CMD_EQ_SET_THEATER");
				applyEq(1.0, 1.0, 4.0);

				// Update button colors and maintain focus
				lv_async_call([](void *unused)
							  {
					// Set Theater button to green (active)
					lv_obj_set_style_bg_color(objects.btn_theater, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Reset other buttons to default color
					lv_obj_set_style_bg_color(objects.btn_car, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_cinema, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_flat, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Keep focus on Theater button
					lv_group_focus_obj(objects.btn_theater); },
							  NULL);
				break;

			case CMD_EQ_SET_CAR:
				Serial.println(">>> Executing: CMD_EQ_SET_CAR");
				applyEq(1.0, 1.0, 0.1);

				// Update button colors and maintain focus
				lv_async_call([](void *unused)
							  {
					// Set Car button to green (active)
					lv_obj_set_style_bg_color(objects.btn_car, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Reset other buttons to default color
					lv_obj_set_style_bg_color(objects.btn_theater, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_cinema, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_flat, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Keep focus on Car button
					lv_group_focus_obj(objects.btn_car); },
							  NULL);
				break;

			case CMD_EQ_SET_CINEMA:
				Serial.println(">>> Executing: CMD_EQ_SET_CINEMA");
				applyEq(1.5, 1.0, 1.2);

				// Update button colors and maintain focus
				lv_async_call([](void *unused)
							  {
					// Set Cinema button to green (active)
					lv_obj_set_style_bg_color(objects.btn_cinema, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Reset other buttons to default color
					lv_obj_set_style_bg_color(objects.btn_theater, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_car, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_flat, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Keep focus on Cinema button
					lv_group_focus_obj(objects.btn_cinema); },
							  NULL);
				break;

			case CMD_EQ_SET_FLAT:
				Serial.println(">>> Executing: CMD_EQ_SET_FLAT");
				applyEq(1.0, 1.0, 1.0);

				// Update button colors and maintain focus
				lv_async_call([](void *unused)
							  {
					// Set Flat button to green (active)
					lv_obj_set_style_bg_color(objects.btn_flat, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Reset other buttons to default color
					lv_obj_set_style_bg_color(objects.btn_theater, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_car, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_bg_color(objects.btn_cinema, lv_color_hex(0xff0292a0), LV_PART_MAIN | LV_STATE_DEFAULT);
					// Keep focus on Flat button
					lv_group_focus_obj(objects.btn_flat); },
							  NULL);
				break;

			default:
				Serial.print(">>> WARNING: Unknown command: ");
				Serial.println(cmd);
				break;
			}
		}
	}
}

// parsing commands from serial port. Utility process
void serialTask(void *param)
{
	while (1)
	{
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (Serial.available() > 0)
		{
			String input = Serial.readString();
			input.trim();
			Serial.println("input");

			Serial.println(input);
			// 	Split at first space
			int spaceIndex = input.indexOf(' ');
			String command = "";
			String cmdValue = "0";

			if (spaceIndex > 0)
			{
				command = input.substring(0, spaceIndex);	// before space
				cmdValue = input.substring(spaceIndex + 1); // after space
			}
			else
			{
				command = input; // no value, just a command
			}

			// Command handling
			if (command == "bat")
			{
				char buf[16]; // make sure it's large enough
				cmdValue.toCharArray(buf, sizeof(buf));
				batteryCharge = cmdValue;
				Serial.print("Battery value received: ");
				Serial.println(cmdValue);
				AppCommand cmd = CMD_BAT_UPDATE;
				xQueueSend(appCommandQueue, &cmd, pdMS_TO_TICKS(300));
			}
			// EQ control commands
			else if (command == "bass" || command == "low")
			{
				float newGain = cmdValue.toFloat();
				bassGain = newGain;
				// Serial.printf("Bass set to: %.1f dB\n", bassGain);

				// Apply immediately
				if (equalizer)
				{
					equalizer->setAudioInfo(info); // Ensure audio info is set
					auto eq_cfg = equalizer->defaultConfig();
					eq_cfg.sample_rate = 44100;
					eq_cfg.channels = 2;
					eq_cfg.bits_per_sample = 16;
					eq_cfg.freq_low = 500;
					eq_cfg.freq_high = 3000;
					eq_cfg.gain_low = bassGain;
					eq_cfg.gain_medium = midGain;
					eq_cfg.gain_high = trebleGain;
					equalizer->begin(eq_cfg);
					// Serial.println("EQ updated!");
				}
			}
			else if (command == "mid")
			{
				float newGain = cmdValue.toFloat();
				midGain = newGain;
				// Serial.printf("Mid set to: %.1f dB\n", midGain);

				// Apply immediately
				if (equalizer)
				{
					equalizer->setAudioInfo(info); // Ensure audio info is set
					auto eq_cfg = equalizer->defaultConfig();
					eq_cfg.sample_rate = 44100;
					eq_cfg.channels = 2;
					eq_cfg.bits_per_sample = 16;
					eq_cfg.freq_low = 500;
					eq_cfg.freq_high = 3000;
					eq_cfg.gain_low = bassGain;
					eq_cfg.gain_medium = midGain;
					eq_cfg.gain_high = trebleGain;
					equalizer->begin(eq_cfg);
					// Serial.println("EQ updated!");
				}
			}
			else if (command == "high" || command == "treble")
			{
				float newGain = cmdValue.toFloat();
				trebleGain = newGain;
				// Serial.printf("Treble set to: %.1f dB\n", trebleGain);

				// Apply immediately
				if (equalizer)
				{
					equalizer->setAudioInfo(info); // Ensure audio info is set
					auto eq_cfg = equalizer->defaultConfig();
					eq_cfg.sample_rate = 44100;
					eq_cfg.channels = 2;
					eq_cfg.bits_per_sample = 16;
					eq_cfg.freq_low = 500;
					eq_cfg.freq_high = 3000;
					eq_cfg.gain_low = bassGain;
					eq_cfg.gain_medium = midGain;
					eq_cfg.gain_high = trebleGain;
					equalizer->begin(eq_cfg);
					// Serial.println("EQ updated!");
				}
			}
			else if (command == "eq")
			{
				// Show current EQ settings
				Serial.println("===== Current EQ Settings =====");
				Serial.printf("  Bass:   %.1f dB\n", bassGain);
				Serial.printf("  Mid:    %.1f dB\n", midGain);
				Serial.printf("  Treble: %.1f dB\n", trebleGain);
				Serial.println("===============================");
				// Serial.println("Usage:");
				// Serial.println("  bass <value>   - Set bass gain (e.g., 'bass 6')");
				// Serial.println("  mid <value>    - Set mid gain (e.g., 'mid -2')");
				// Serial.println("  high <value>   - Set treble gain (e.g., 'high 4')");
				// Serial.println("  eq             - Show current settings");
			}
			// Bluetooth device management commands
			else if (command == "btlist" || command == "btdevices")
			{
				// List all bonded Bluetooth devices
				Serial.println("[DEBUG] Fetching bonded devices list...");
				btDeviceManager.updateBondedDevicesList();
				btDeviceManager.printBondedDevices();
				Serial.println("[DEBUG] List complete");
			}
			else if (command == "btcount")
			{
				// Get count of bonded devices
				btDeviceManager.updateBondedDevicesList();
				int count = btDeviceManager.getBondedDevicesCount();
				Serial.printf("Total bonded devices: %d\n", count);
			}
			else if (command == "btremove" || command == "btunpair")
			{
				// Remove a specific bonded device by index
				int index = cmdValue.toInt();
				if (index > 0)
				{
					Serial.printf("Removing device at index %d...\n", index);
					if (btDeviceManager.removeBondedDevice(index - 1))
					{
						Serial.println("Device removed successfully");
					}
					else
					{
						Serial.println("Failed to remove device");
					}
				}
				else
				{
					Serial.println("Usage: btremove <index> (e.g., 'btremove 1')");
				}
			}
			else if (command == "btclear" || command == "btclearall")
			{
				// Clear all bonded devices
				Serial.println("Clearing all bonded devices...");
				btDeviceManager.clearAllBondedDevices();
				Serial.println("All devices cleared");
			}
			else if (command == "bterase" || command == "btnvserase")
			{
				// Nuclear option - erase entire Bluetooth NVS
				Serial.println("WARNING: This will erase ALL Bluetooth data!");
				Serial.println("The device will need to restart after this operation.");
				if (btDeviceManager.eraseBluetoothNVS())
				{
					Serial.println("Bluetooth NVS erased. Restarting in 3 seconds...");
					vTaskDelay(3000 / portTICK_PERIOD_MS);
					ESP.restart();
				}
				else
				{
					Serial.println("Failed to erase Bluetooth NVS");
				}
			}
			else if (command == "help" || command == "?")
			{
				// Show available commands
				Serial.println("\n===== Available Serial Commands =====");
				Serial.println("EQ Commands:");
				Serial.println("  eq              - Show current EQ settings");
				Serial.println("  bass <value>    - Set bass gain (e.g., 'bass 6')");
				Serial.println("  mid <value>     - Set mid gain (e.g., 'mid -2')");
				Serial.println("  high <value>    - Set treble gain (e.g., 'high 4')");
				Serial.println("\nBluetooth Commands:");
				Serial.println("  btlist          - List all bonded BT devices");
				Serial.println("  btcount         - Show count of bonded devices");
				Serial.println("  btremove <num>  - Remove device by index (e.g., 'btremove 1')");
				Serial.println("  btclear         - Clear all bonded devices");
				Serial.println("  bterase         - Force erase BT NVS (for corrupted data, requires restart)");
				Serial.println("\nOther Commands:");
				Serial.println("  bat <value>     - Set battery value");
				Serial.println("  help            - Show this help message");
				Serial.println("====================================\n");
			}
		}
	}
}

void serialTask2(void *param)
{
	while (1)
	{
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (Serial.available() > 0)
		{
			String input = Serial.readString();
			input.trim();
			Serial.println("input");

			Serial.println(input);
		}
	}
}

// Encoder AND button handling
void encoderTask(void *param)
{
	int16_t last_val = 0;
	while (1)
	{
		newEncoderPos = encoder.getCount() / 2;

		if (current_screen == objects.bt_screen && newEncoderPos != oldEncoderPos)
		{
			// Serial.print("newEncoderPos ");
			// Serial.println(newEncoderPos);
			// Serial.print("oldEncoderPos ");
			// Serial.println(oldEncoderPos);

			if (oldEncoderPos < newEncoderPos)
			{
				handle_volume_control(true);
			}
			else if (oldEncoderPos > newEncoderPos)
			{
				handle_volume_control(false);
			}
		}
		oldEncoderPos = newEncoderPos;

		// button logic
		button.update(); // must be called repeatedly

		if (button.fell())
		{
			Serial.println("Button fell");
			// Button just pressed
			pressStartTime = millis();
			isPressed = true;
		}

		// Button held long enough for shutting down
		if (isPressed && (millis() - pressStartTime >= ENCODER_BTN_HOLD_TIME))
		{
			Serial.println("Long press");
			AppCommand cmd = CMD_SHUT_DOWN;
			xQueueSend(appCommandQueue, &cmd, 0);
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
			AppCommand cmd = CMD_NONE;

			// Check if we're on the EQ page first
			if (current_screen == objects.equalizer_page)
			{
				// We're on EQ page, check EQ buttons only
				lv_obj_t *focused = lv_group_get_focused(focus_group_eq);
				Serial.print("DEBUG: On EQ page, focused object = ");
				Serial.println((uint32_t)focused, HEX);

				if (focused)
				{
					for (int i = 0; i < 4; ++i)
					{
						if (focused == eq_buttons[i])
						{
							Serial.print("DEBUG: Matched eq_buttons[");
							Serial.print(i);
							Serial.println("]");

							switch (i)
							{
							case 0:
								cmd = CMD_EQ_SET_THEATER;
								Serial.println("CMD_EQ_SET_THEATER");
								break;
							case 1:
								cmd = CMD_EQ_SET_CAR;
								Serial.println("CMD_EQ_SET_CAR");
								break;
							case 2:
								cmd = CMD_EQ_SET_CINEMA;
								Serial.println("CMD_EQ_SET_CINEMA");
								break;
							case 3:
								cmd = CMD_EQ_SET_FLAT;
								Serial.println("CMD_EQ_SET_FLAT");
								break;
							}
							break;
						}
					}
				}
			}
			else
			{
				// We're on main menu, check main menu buttons
				lv_obj_t *focused = lv_group_get_focused(focus_group);
				if (focused)
				{
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
				}
			}

			if (cmd != CMD_NONE)
			{
				xQueueSend(appCommandQueue, &cmd, 0);
			}

			waitingForSecondClick = false;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

/* LVGL EEZ STUDIO UI
 * any custom UI updates happen here
 */
void uiTask(void *param)
{
	Serial.println("--> UI loop task start");
	while (1)
	{
		lv_timer_handler();
		ui_tick(); // This is important for EEZ-generated UI

		// custom UI updates
		if (metadata_updated)
		{
			lv_label_set_text(objects.track_name_placeholder, trackName);
			lv_label_set_text(objects.artist_name_placeholder, artistName);
			metadata_updated = false;
		}

		if (playback_status_updated)
		{
			// update UI
			lv_label_set_text(objects.play_status_placeholder, playbackStatus);
			playback_status_updated = false;
		}

		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

void setup()
{
	Serial.begin(115200);

	checkBoardMemory(); // Available RAM/ROM/Heap

	// Mount SPIFFS for audio files
	if (!SPIFFS.begin(true))
	{
		Serial.println("Failed to mount SPIFFS");
	}
	else
	{
		Serial.println("SPIFFS mounted successfully");
	}

	// I2S and audio setup
	Serial.printf("Free heap before i2s begin: %u bytes\n", esp_get_free_heap_size());
	AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
	Serial.println("Starting I2S...");
	auto cfg = i2s.defaultConfig(TX_MODE);
	cfg.pin_bck = I2S_BCK;
	cfg.pin_ws = I2S_WS;
	cfg.pin_data = I2S_DATA;
	cfg.channels = 2;
	cfg.bits_per_sample = 16;
	cfg.sample_rate = 44100;
	cfg.buffer_count = 8;  // can be adjusted to achieve smooth sound
	cfg.buffer_size = 256; // can be adjusted to achieve smooth sound
	cfg.copyFrom(info);
	i2s.begin(cfg);
	Serial.printf("Free heap after i2s begin: %u bytes\n", esp_get_free_heap_size());

	volume_stream.begin(cfg);
	volume_stream.setVolume(volumeLevel);

	// Configure equalizer
	equalizer = new Equalizer3Bands(volume_stream);

	// Set audio info first
	equalizer->setAudioInfo(info);

	auto eq_config = equalizer->defaultConfig();
	eq_config.sample_rate = 44100;
	eq_config.channels = 2;
	eq_config.bits_per_sample = 16;

	// Set EQ band frequencies (Hz)
	eq_config.freq_low = 500;	// Bass/Mid crossover
	eq_config.freq_high = 3000; // Mid/Treble crossover

	// Set EQ gains (dB)
	eq_config.gain_low = bassGain;
	eq_config.gain_medium = midGain;
	eq_config.gain_high = trebleGain;

	// Initialize equalizer
	equalizer->begin(eq_config);
	Serial.println("===================================");
	Serial.println("Equalizer initialized with TEST settings:");
	Serial.printf("  Bass:   %.1f dB (boosted)\n", bassGain);
	Serial.printf("  Mid:    %.1f dB (cut)\n", midGain);
	Serial.printf("  Treble: %.1f dB (boosted)\n", trebleGain);
	Serial.println("Expected sound: Heavy bass, reduced mids, bright highs");
	Serial.println("===================================");

	// Initialize MP3 player to route audio through equalizer
	player = new AudioPlayer(source, *equalizer, helix);
	out = new EncodedAudioStream(equalizer, &helix);
	Serial.println("MP3 player initialized - audio will route through EQ");

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
	// xTaskCreatePinnedToCore(encoderTask, "EncoderTask", 6144, NULL, 1, NULL, 1);
	// Encoder button setup
	button.attach(ENCODER_BTN_PIN, INPUT_PULLUP);
	button.interval(10);
	Serial.println("Encoder initialized");

	// UI Init
	// Initialize display
	lcd.init();
	lcd.setBrightness(255); // Set backlight (0-255)

	// drawTestScreen();
	lv_init();

// Initialize display buffer
#if defined(NO_PSRAM)
	lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 10);
#else
	Serial.println("SRAM buf setup");
	buf = (lv_color_t *)heap_caps_malloc(TFT_W * 20 * sizeof(lv_color_t),
										 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 20);
#endif

	// Check buffer allocation
	if (!buf)
	{
		Serial.println("Failed to allocate display buffer!");
	}

	// Initialize display driver
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = TFT_W;
	disp_drv.ver_res = TFT_H;
	disp_drv.flush_cb = display_flush;
	disp_drv.draw_buf = &draw_buf;
	lv_disp_drv_register(&disp_drv);

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
	lv_group_focus_obj(objects.a2dp_bluetooth);

	menu_buttons[0] = objects.a2dp_bluetooth;
	menu_buttons[1] = objects.aws_sync;
	menu_buttons[2] = objects.equalizer;
	menu_buttons[3] = objects.settings;

	menu_screens[0] = objects.bt_screen;
	menu_screens[1] = objects.aws_connect;
	menu_screens[2] = objects.equalizer_page;
	menu_screens[3] = objects.settings_page;

	eq_buttons[0] = objects.btn_theater;
	eq_buttons[1] = objects.btn_car;
	eq_buttons[2] = objects.btn_cinema;
	eq_buttons[3] = objects.btn_flat;

	Serial.println("Focus group ready");

	// Setup EQ page focus group
	setupEncoderFocusGroupEQ();
	Serial.println("EQ focus group ready");

	// Play startup sound
	Serial.println("Playing startup sound...");
	vTaskDelay(200 / portTICK_PERIOD_MS); // Longer delay to ensure I2S is fully stabilized
	playMp3File(1);						  // Play hello.mp3
	vTaskDelay(100 / portTICK_PERIOD_MS); // Brief delay after sound playback
	Serial.println("Startup sound playback completed");

	// Check for existing bonded Bluetooth devices
	Serial.println("\n[BOOT] Checking for bonded Bluetooth devices...");
	btDeviceManager.updateBondedDevicesList();
	int bondedCount = btDeviceManager.getBondedDevicesCount();
	if (bondedCount > 0)
	{
		Serial.printf("[BOOT] Found %d bonded device(s):\n", bondedCount);
		for (int i = 0; i < bondedCount; i++)
		{
			Serial.printf("  [%d] %s\n", i + 1, btDeviceManager.getMacAddressString(i).c_str());
		}
	}
	else
	{
		Serial.println("[BOOT] No bonded devices found");
	}
	Serial.println("[BOOT] Use 'btlist' command to view bonded devices anytime");
	Serial.println("");

	// Tasks setup
	appCommandQueue = xQueueCreate(8, sizeof(AppCommand));
	xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(encoderTask, "EncoderTask", 4096, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(appTask, "appTask", 3072, NULL, 3, NULL, 1); // 3rd arg matters a lot, maybe find out optimal
	xTaskCreatePinnedToCore(serialTask, "serialTask", 4096, NULL, 1, NULL, 1);
	// xTaskCreatePinnedToCore(serialTask2, "serialTask2", 1536, NULL, 1, NULL, 1);  // for testing|debugging
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
