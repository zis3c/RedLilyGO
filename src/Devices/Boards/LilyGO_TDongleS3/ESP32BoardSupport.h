/**
 * @file ESP32BoardSupport.h
 * @brief ESP32-S3 board lifecycle and crash-recovery interface.
 */

#pragma once
#include "../../Platform/BoardSupportBase.h"

class ESP32BoardSupport : public BoardSupport {
public:
  /** @brief Construct board support with crash state unset. */
  ESP32BoardSupport();

  virtual void loop(Preferences& prefs) override;
  virtual void begin(Preferences& prefs) override;
  bool hasCrashed();
  bool isSafeMode() const;
};
