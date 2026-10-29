/**
 * @file MicrophoneDevice.h
 * @brief PCM microphone capture interface.
 *
 * Concrete profiles may use an I2S microphone and a FreeRTOS capture task;
 * callback consumers must treat supplied buffers as transient.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

#include "../../RedLilyGOCapability.h"
#include "../Storage/StorageDevice.h"

class MicrophoneDevice : RedLilyGOCapability {
public:
  /** @brief Construct microphone adapter with capture stopped. */
  MicrophoneDevice();

  /** @brief Service microphone state and callback delivery. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize microphone hardware and settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Register PCM callback; null callback disables delivery. */
  void setCallback(const std::function<bool(uint8_t *, const size_t)> &callback);
  /** @brief Report whether microphone hardware is configured. */
  bool isConfigured() const;
  /** @brief Request capture start. */
  void startCapture() { capture.store(true, std::memory_order_release); }
  /** @brief Request capture stop. */
  void stopCapture() { capture.store(false, std::memory_order_release); }
private:
  std::atomic<bool> capture{false};
  mutable std::mutex callbackMutex;
};

namespace Devices
{
    extern MicrophoneDevice Mic;
};
