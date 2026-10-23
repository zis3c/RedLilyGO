/** @file NoMicrophoneDevice.cpp @brief Null microphone adapter for headless profiles. */

#ifdef NO_MIC
#include "MicrophoneDevice.h"

namespace Devices
{
    MicrophoneDevice Mic;
}

MicrophoneDevice::MicrophoneDevice()
{
}

bool MicrophoneDevice::isConfigured() const
{
    return false;
}

void MicrophoneDevice::begin(Preferences &prefs)
{
}

void MicrophoneDevice::loop(Preferences &prefs)
{
}

void MicrophoneDevice::setCallback(const std::function<bool(uint8_t *, const size_t)> &callback)
{
}
#endif
