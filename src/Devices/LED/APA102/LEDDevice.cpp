/** @file LEDDevice.cpp @brief APA102 SPI LED implementation. */

#ifndef NO_LED
#if defined(LED_DI_PIN) && defined(LED_CI_PIN)
#include "../LEDDevice.h"
#include <APA102.h>
#ifndef NO_ESP_MARAUDER
#include "../../../Attacks/Marauder/Marauder.h"
#endif

namespace Devices
{
    LEDDevice LED;
}

static APA102<LED_DI_PIN, LED_CI_PIN> ledStrip;

// The board profile exposes one APA102 status LED.
const uint16_t ledCount = 1;

// APA102 expects one RGB color value for each LED.
static rgb_color colors[ledCount];

const uint8_t brightness = 10;

static rgb_color hsvToRgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint8_t f = (h % 60) * 255 / 60;
    uint8_t p = (255 - s) * (uint16_t)v / 255;
    uint8_t q = (255 - f * (uint16_t)s / 255) * (uint16_t)v / 255;
    uint8_t t = (255 - (255 - f) * (uint16_t)s / 255) * (uint16_t)v / 255;
    uint8_t r = 0, g = 0, b = 0;
    switch((h / 60) % 6){
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
    return rgb_color(r, g, b);
}

void LEDDevice::changeLEDState(bool on, uint16_t hue, uint8_t saturation, uint8_t lum, uint8_t brightness)
{
#ifndef NO_ESP_MARAUDER
  if (on && Attacks::Marauder.isActive() && hue != 120)
  {
    hue = 45; // MRD: orange, never let idle red win.
    saturation = 255;
    lum = 255;
  }
#endif
  if (!on)
  {
    ledStrip.write(colors, 1, 0);
  }
  else
  {
    // Correct the APA102 hue mapping so requested colors match the visible LED.
    if (hue == 100 && saturation == 100 && lum == 100) // green
    {
      hue = 120;     
    }

    // our brightness is between 0 - 255, need to rescale between 0-31
    brightness = (uint8_t) ((float) brightness * (31.f / 255.f));
    colors[0] = hsvToRgb(hue, saturation, lum);
    ledStrip.write(colors, 1, brightness);
  }
}

LEDDevice::LEDDevice()
{
}

void LEDDevice::loop(Preferences& prefs)
{
}

void LEDDevice::begin(Preferences &prefs)
{
  // Keep LED dark until the startup logo explicitly runs its red animation.
  changeLEDState(false, 0, 0, 0, 0);
}
#endif
#endif
