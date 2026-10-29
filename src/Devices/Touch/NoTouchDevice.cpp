/** @file NoTouchDevice.cpp @brief Null touch adapter for unsupported boards. */

#ifdef NO_TOUCH
#include "TouchDevice.h"

namespace Devices
{
  TouchDevice Touch;
}

TouchDevice::TouchDevice()
{
}

void TouchDevice::loop(Preferences& prefs)
{
}

void TouchDevice::begin(Preferences &prefs)
{
}

#endif
