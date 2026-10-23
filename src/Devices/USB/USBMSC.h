/** @file USBMSC.h @brief TinyUSB mass-storage lifecycle API. */

#pragma once

#include <string.h>
#include <atomic>

#include "../../RedLilyGOCapability.h"
#include "USBCore.h"
#include <mutex>

class USBMSC : RedLilyGOCapability {
public:
  /** @brief Construct mass-storage adapter in stopped state. */
  USBMSC();

  /** @brief Service mass-storage state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize mass-storage settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Stop mass-storage interface. */
  virtual void end();

  /** @brief Mount image file as disk or read-only CD media. */
  bool mountDiskImage(const std::string&, bool mountAsCD);
  /** @brief Mount configured SD storage as host-visible media. */
  bool mountSD();
  /** @brief Report whether host has accessed media since last reset. */
  bool hasActivity() { return mscActivity.load(); }
  /** @brief Clear host media activity flag. */
  void resetActivityState() { mscActivity.store(false); }
  /** @brief Set host media activity flag. */
  void setActivityStateState(bool state) { mscActivity.store(state); }
private:
  std::atomic<bool> mscActivity{false};
  bool mscStarted = false;
};

namespace Devices::USB
{
    extern USBMSC MSC;
}
