/** @file ButtonDevice.h @brief Common debounced button input API. */

#pragma once

#include "../../RedLilyGOCapability.h"


class ButtonDevice : RedLilyGOCapability {
public:
  /** @brief Construct debounced button state adapter. */
  ButtonDevice();

  /** @brief Poll button hardware and update edge states. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize button hardware and settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Report pending single-press state. */
  bool hasButtonBeenPressed() { return buttonPressedState; }
  /** @brief Clear pending single-press state. */
  void resetButtonPressedState() { buttonPressedState = false; }
  /** @brief Set single-press and long-press state from hardware adapter. */
  void setButtonPressState(bool state, bool isLongPress) { buttonPressedState = state; buttonPressedStateWasLong = isLongPress; }
  /** @brief Report whether pending press was long. */
  bool wasLastPressLong() { return buttonPressedStateWasLong; }
  
  /** @brief Report pending double-click state. */
  bool hasButtonBeenDoubleClicked() { return buttonDoubleClickedState; }
  /** @brief Clear pending double-click state. */
  void resetButtonDoubleClickedState() { buttonDoubleClickedState = false; }
  /** @brief Set double-click state from hardware adapter. */
  void setButtonDoubleClickState(bool state) { buttonDoubleClickedState = state; }

  /** @brief Report pending triple-click state. */
  bool hasButtonBeenTripleClicked() { return buttonTripleClickedState; }
  /** @brief Clear pending triple-click state. */
  void resetButtonTripleClickedState() { buttonTripleClickedState = false; }
  /** @brief Set triple-click state from hardware adapter. */
  void setButtonTripleClickState(bool state) { buttonTripleClickedState = state; }
private:
  bool buttonPressedState = false;
  bool buttonPressedStateWasLong = false;
  bool buttonDoubleClickedState = false;
  bool buttonTripleClickedState = false;
};

namespace Devices
{
    extern ButtonDevice Button;
}
