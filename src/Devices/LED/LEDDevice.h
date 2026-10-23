/** @file LEDDevice.h @brief LED status and effect interface. */

#pragma once

#include "../../RedLilyGOCapability.h"

class LEDDevice : RedLilyGOCapability {
public:
  /** @brief Construct LED adapter. */
  LEDDevice();

  /** @brief Service LED effects from the Arduino loop. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize LED hardware and settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Set LED power, HSV colour, and brightness. */
  void changeLEDState(bool on, uint16_t hue, uint8_t saturation, uint8_t lum, uint8_t brightness);
};

namespace Devices
{
    extern LEDDevice LED;
}
