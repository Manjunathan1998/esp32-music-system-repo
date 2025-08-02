#include "DisplayDriver.h"

// Create a display panel instance with manual settings for ST7796
LGFX_Display::LGFX_Display(){
		{{auto cfg = _bus.config();
// SPI pins
cfg.spi_host = HSPI_HOST;
cfg.pin_sclk = TFT_SCLK_PIN;
cfg.pin_mosi = TFT_MOSI_PIN;
cfg.pin_miso = TFT_MISO_PIN; // unused
cfg.pin_dc = TFT_DC_PIN;
cfg.freq_write = 60000000;
_bus.config(cfg);
_panel.setBus(&_bus);
}

// Panel configuration
{
	auto cfg = _panel.config();
	cfg.pin_cs = TFT_CS_PIN;
	cfg.pin_rst = TFT_RST_PIN;
	cfg.pin_busy = -1; // Busy (-1 = unused)

	// Display parameters - adjust based on your display specs
	cfg.panel_width = TFT_W;
	cfg.panel_height = TFT_H;
	cfg.offset_x = 0;
	cfg.offset_y = 0;
	_panel.config(cfg);
}

// Backlight configuration
{
	auto cfg = _light.config();
	cfg.pin_bl = TFT_BL_PIN; // TFT_BCKL
	cfg.invert = false;			 // Set to true if LOW turns backlight ON
	cfg.freq = 44100;				 // PWM frequency
	cfg.pwm_channel = 7;		 // PWM channel
	_light.config(cfg);
	_panel.setLight(&_light);
}

// Touch configuration (I2C)
{
	auto cfg = _touch.config();
	cfg.x_min = 0;
	cfg.x_max = 319;
	cfg.y_min = 0;
	cfg.y_max = 479;
	cfg.pin_sda = 18;		 // TOUCH_SDA
	cfg.pin_scl = 19;		 // TOUCH_SCL
	cfg.i2c_addr = 0x38; // I2C_TOUCH_ADDRESS
	cfg.i2c_port = 1;		 // I2C port number
	cfg.freq = 400000;	 // I2C frequency
	_touch.config(cfg);
	_panel.setTouch(&_touch);
}

setPanel(&_panel);
}
}
;