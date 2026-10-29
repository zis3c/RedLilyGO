/** @file Format.cpp @brief Physical-button SD recovery without automatic reboot loops. */

#ifndef NO_SD
#include "../Devices/Button/ButtonDevice.h"
#include "../Devices/Display/DisplayDevice.h"
#include "../Devices/Storage/StorageDevice.h"
#include "../Devices/WiFi/WiFiDevice.h"
#include "../Devices/USB/USBCore.h"
#include "../Attacks/Ducky/DuckyPayload.h"
#include "../Attacks/Marauder/Marauder.h"
#include "Diagnostics/Logging.h"

void AskFormatSD(Preferences &prefs)
{
#ifdef DISPLAY_WIDTH
    const float scale = Devices::Display.getTextSize() / 10.0;
    Devices::Display.clearScreen();
    Devices::Display.display(0, 0, "No SD card detected.");
    Devices::Display.display(0, 10 * scale, "Hold button to format.");

    // A missing card must not disable recovery access or restart forever.
    // Formatting still requires a physical long press during this prompt.
    for (int x = 0; x < 60 * 1000; ++x)
    {
        delay(1);
        Devices::Button.loop(prefs);
        if (!Devices::Button.hasButtonBeenPressed() || !Devices::Button.wasLastPressLong()) continue;
        Devices::Button.resetButtonPressedState();
        Devices::Wifi.end();
        Attacks::Ducky.stop();
        Attacks::Marauder.stopAll();
        Devices::USB::Core.end();
        Devices::Display.clearScreen();
        Devices::Display.display(0, 0, "Formatting");
        Devices::Display.display(0, 10 * scale, "SD card...");
        Devices::Storage.begin(prefs, true);

        Devices::Display.clearScreen();
        Devices::Display.display(0, 0, Devices::Storage.isRunning() ? "SD format complete" : "SD format failed.");
        delay(5000);
#ifdef ARDUINO_ARCH_ESP32
        ESP.restart();
#endif
        return;
    }
#endif
    Debug::Log.warning("FORMAT", "SD unavailable; continuing without storage.");
}
#endif // NO_SD
