#pragma once

#include <LovyanGFX.hpp>
#include "defines.h"

class LGFX_Display : public lgfx::LGFX_Device
{
public:
    LGFX_Display();

private:
    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;
    lgfx::Touch_FT5x06 _touch;
};
