/** @file IRDevice.h @brief Infrared peripheral lifecycle interface. */

#pragma once

#include <string>
#include <vector>
#include <functional>

#include "../../RedLilyGOCapability.h"

class IRDevice : RedLilyGOCapability {
public:
  /** @brief Construct IR adapter. */
IRDevice();

  /** @brief Service IR receive/transmit state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize IR hardware and settings. */
  virtual void begin(Preferences& prefs);
};

namespace Devices
{
    extern IRDevice IR;
};
