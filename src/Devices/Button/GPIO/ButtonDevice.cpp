/** @file ButtonDevice.cpp @brief GPIO button input implementation. */

#ifndef NO_BUTTON
#ifndef LILYGO_T_WATCH_S3
#include <OneButton.h>

#include "../ButtonDevice.h"

static OneButton button(BTN_PIN, true);

namespace Devices
{
    ButtonDevice Button;
}

ButtonDevice::ButtonDevice()
{
}

void ButtonDevice::loop(Preferences& prefs)
{
    button.tick();
}

void ButtonDevice::begin(Preferences &prefs)
{
  resetButtonPressedState();
  resetButtonDoubleClickedState();
  resetButtonTripleClickedState();
  button.setPressMs(2000);

  button.attachClick([] {
    Devices::Button.setButtonPressState(true, button.isLongPressed());
  });

  button.attachLongPressStop([] {
    Devices::Button.setButtonPressState(true, true);
  });
  
  button.attachDoubleClick([] {
    Devices::Button.setButtonDoubleClickState(true);
  });

  button.attachMultiClick([] {
    if (button.getNumberClicks() == 3) {
      Devices::Button.setButtonTripleClickState(true);
    }
  });
}
#endif
#endif
