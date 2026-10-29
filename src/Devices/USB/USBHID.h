/** @file USBHID.h @brief TinyUSB HID keyboard, consumer, and mouse API. */

#pragma once

#include "USBCore.h"
#include "../../RedLilyGOCapability.h"
#include <mutex>

class USBHID : RedLilyGOCapability {
public:
  /** @brief Construct HID queue service. */
  USBHID();

  /** @brief Drain queued reports to TinyUSB. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize HID interface and persisted settings. */
  virtual void begin(Preferences& prefs);

  /** @brief Report whether no HID reports are pending. */
  bool IsQueueEmpty();
  /** @brief Report whether the keyboard queue can accept `needed` more reports. */
  bool keyboardQueueHasSpace(size_t needed = 1) const;
  /** @brief Report whether HID interface is initialized and mounted. */
  bool isReady() const;
  /** @brief Remove all queued keyboard, mouse, and consumer reports. */
  void clearQueues();
  /** @brief Queue keyboard report with modifier and up to six keys. */
  void keyboard_press(const uint8_t modifiers, const uint8_t key1, const uint8_t key2, const uint8_t key3, const uint8_t key4, const uint8_t key5, const uint8_t key6);
  /** @brief Queue keyboard release report. */
  void keyboard_release();
  /** @brief Queue consumer-control key press. */
  void consumer_device_keypress(const uint16_t keyCode);
  /** @brief Queue relative mouse movement. */
  void mouseMove(int8_t xDelta, int8_t yDelta);
  /** @brief Queue relative mouse wheel movement. */
  void mouseScroll(int8_t vertical, int8_t horizontal = 0);
  /** @brief Queue mouse button press or release. */
  void mouseButton(uint8_t button, bool pressed);
  /** @brief Queue release reports for all active HID inputs. */
  void releaseAll();

private:
  void clearQueuesUnlocked();
  void queueReleaseReportsUnlocked();
  mutable std::mutex queueMutex;
};

namespace Devices::USB
{
    extern USBHID HID;
}
