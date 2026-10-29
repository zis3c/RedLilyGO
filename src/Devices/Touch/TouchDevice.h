/** @file TouchDevice.h @brief Touch controller lifecycle and coordinate API. */

#pragma once

#include "../../RedLilyGOCapability.h"

class TouchDevice : RedLilyGOCapability {
public:
  /** @brief Construct touch adapter with clear state. */
  TouchDevice();

  /** @brief Poll touch controller and update edge state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize touch controller and settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Report whether a new touch was observed. */
  bool hasBeenTouched() { return wasTouched; }
  /** @brief Clear pending touch edge state. */
  void resetTouchState() { wasTouched = false; }
  /** @brief Return last sampled X coordinate. */
  int16_t getXPos() { return xPos; }
  /** @brief Return last sampled Y coordinate. */
  int16_t getYPos() { return yPos; }
private:
  bool wasTouched = false;
  int16_t xPos = 0;
  int16_t yPos = 0;
};

namespace Devices
{
    extern TouchDevice Touch;
}
