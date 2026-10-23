/** @file Auxiliary.h @brief Auxiliary component lifecycle interface. */

#pragma once

#include "../RedLilyGOCapability.h"

class Auxiliary : RedLilyGOCapability {
public:
  Auxiliary();

  virtual void loop(Preferences& prefs);
  virtual void begin(Preferences& prefs);
};
