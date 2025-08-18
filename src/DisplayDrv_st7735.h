#pragma once

#include <LovyanGFX.hpp>
#include "defines.h"

class LGFX_st7735 : public lgfx::LGFX_Device
{
public:
    LGFX_st7735(){
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
    cfg.panel_width = 128;
    cfg.panel_height = 160;
    cfg.offset_x = 0;
    cfg.offset_y = 2;
    cfg.offset_rotation = 3;
    cfg.rgb_order = true;

    _panel.config(cfg);
}

// Backlight configuration
{
    auto cfg = _light.config();
    cfg.pin_bl = TFT_BL_PIN; // TFT_BCKL
    cfg.invert = false;      // Set to true if LOW turns backlight ON
    cfg.freq = 44100;        // PWM frequency
    cfg.pwm_channel = 7;     // PWM channel
    _light.config(cfg);
    _panel.setLight(&_light);
}

setPanel(&_panel);
}
}
;

private:
lgfx::Panel_ST7735S _panel;
lgfx::Bus_SPI _bus;
lgfx::Light_PWM _light;
}
;
