/** @file NoIRDevice.cpp @brief Null infrared adapter for unsupported boards. */

#ifdef NO_IR
#include "IRDevice.h"

namespace Devices
{
    IRDevice IR;
}

IRDevice::IRDevice()
{
}

void IRDevice::begin(Preferences &prefs)
{
}

void IRDevice::loop(Preferences &prefs)
{
}
#endif
