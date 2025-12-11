#include <Arduino.h>
#include "globals.h"
#include "esp_bt.h"

#include "BluetoothA2DPSink.h"
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
	if (!cbSet)
	{
		Serial.println("Init new player");
		// player.setMetadataCallback(printMetaData);
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

	a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
	a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playback_status_changed);
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
		Serial.println("Sink stopped");
	}
	else
	{
		Serial.println("A2DP sink already stopped, doing nothing");
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
                        stopBtSink(); },
							  NULL);
				break;

			case CMD_SWITCH_TO_SCR_BT:

				playMp3File(2); // bt pair sound
				vTaskDelay(600 / portTICK_PERIOD_MS);
				i2s.end();
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
							  { switchToScreen(menu_screens[2]); },
							  NULL);
				break;
			case CMD_SWITCH_TO_SCR_SETTINGS:
				lv_async_call([](void *unused)
							  { switchToScreen(menu_screens[3]); },
							  NULL);
				break;
				// case CMD_BT_STOP:
				//   // todo add bt stop on encoder double click if curr. screen == bt
				//   Serial.println("Command BT stop");
				//   break;

			case CMD_SHUT_DOWN:
				playMp3File(0);
				Serial.println("Shutting down...");
				vTaskDelay(100 / portTICK_PERIOD_MS); // allow Serial flush
				esp_deep_sleep_start();
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
			// Button just pressed
			pressStartTime = millis();
			isPressed = true;
		}

		// Button held long enough for shutting down
		if (isPressed && (millis() - pressStartTime >= ENCODER_BTN_HOLD_TIME))
		{
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

	// I2S and audio setup
	Serial.printf("Free heap before i2s begin: %u bytes\n", esp_get_free_heap_size());
	AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
	Serial.println("Starting I2S...");
	auto cfg = i2s.defaultConfig(TX_MODE);
	cfg.copyFrom(info);
	cfg.buffer_count = 4;
	cfg.buffer_size = 64;
	i2s.begin(cfg);
	Serial.printf("Free heap after i2s begin: %u bytes\n", esp_get_free_heap_size());

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
	Serial.println("Focus group ready");

	// Tasks setup
	appCommandQueue = xQueueCreate(8, sizeof(AppCommand));
	xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(encoderTask, "EncoderTask", 4096, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(appTask, "appTask", 3072, NULL, 3, NULL, 1); // 3rd arg matters a lot, maybe find out optimal
	xTaskCreatePinnedToCore(serialTask, "serialTask", 1536, NULL, 1, NULL, 1);
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
