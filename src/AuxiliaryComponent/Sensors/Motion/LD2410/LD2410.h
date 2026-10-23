/** @file LD2410.h @brief LD2410 UART motion sensor interface. */

#pragma once

#include "../../../../RedLilyGOCapability.h"

class LD2410MotionSensorExtension : RedLilyGOCapability {
public:
  LD2410MotionSensorExtension();

  virtual void loop(Preferences& prefs);
  virtual void begin(Preferences& prefs);
};
