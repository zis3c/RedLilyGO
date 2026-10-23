/**
 * @file main.cpp
 * @brief Arduino entry point and lifecycle coordinator for RedLilyGO.
 *
 * Coordinates board, display, storage, USB, network, attack, and auxiliary
 * components on the Arduino loop. Hardware initialization is synchronous in
 * setup(); all regular work runs cooperatively from the single loop task.
 * Components may create their own FreeRTOS tasks internally, but this file
 * does not access peripherals from an ISR.
 */

#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include <Adafruit_TinyUSB.h>

#include "Devices/Platform/BoardSupport.h"
#include "Utilities/TemperatureRead.h"
#include "Devices/Button/ButtonDevice.h"
#include "Devices/LED/LEDDevice.h"
#include "Devices/Display/DisplayDevice.h"
#include "Devices/Storage/StorageDevice.h"
#include "Devices/USB/USBCore.h"
#include "Devices/USB/USBNCM.h"
#include "Devices/WiFi/WiFiDevice.h"
#include "Devices/Microphone/MicrophoneDevice.h"
#include "Devices/IR/IRDevice.h"
#include "Devices/Touch/TouchDevice.h"

#include "Attacks/Marauder/Marauder.h"
#include "Attacks/Ducky/DuckyPayload.h"
#include "Attacks/Agent/Agent.h"

#include "Diagnostics/Logging.h"
#include "AuxiliaryComponent/Auxiliary.h"

#include "Utilities/Format.h"
#include "version.h"

#define TAG "main"

static Preferences prefs;
static Auxiliary aux;

static int currentLine = 0;
static bool thermalShutdown = false;
static unsigned long lastThermalCheck = 0;
static uint8_t thermalHighSamples = 0;
// ESP32-S3 internal temperature readings can spike while Wi-Fi TX is busy.
// Require a sustained, genuinely high reading before stopping a radio task.
static constexpr float THERMAL_LIMIT_C = 95.0f;
static constexpr float THERMAL_RECOVERY_C = 70.0f;
static constexpr uint8_t THERMAL_CONFIRMATION_SAMPLES = 5;

/** @brief Disable heat-producing subsystems after a sustained high reading. */
static void serviceThermalProtection()
{
  if (millis() - lastThermalCheck < 5000) return;
  lastThermalCheck = millis();

  const float temperatureC = readBoardTemperatureC();
  if (!std::isfinite(temperatureC) || temperatureC < -20.0f || temperatureC > 125.0f)
  {
    thermalHighSamples = 0;
    return;
  }

  if (thermalShutdown)
  {
    if (temperatureC <= THERMAL_RECOVERY_C)
    {
      thermalShutdown = false;
      Devices::Display.powerOn();
      Devices::Wifi.setWiFi(true);
      Devices::Wifi.begin(prefs);
      Debug::Log.info(TAG, "Thermal limit cleared. Wi-Fi recovery started.");
    }
    return;
  }

  if (temperatureC >= THERMAL_LIMIT_C)
  {
      if (++thermalHighSamples < THERMAL_CONFIRMATION_SAMPLES) return;
      thermalHighSamples = 0;
      thermalShutdown = true;
      Debug::Log.error(TAG, std::string("Thermal limit reached at ") + std::to_string(temperatureC) + " C. Wireless services disabled.");
      Attacks::Marauder.stopAll();
      Devices::Wifi.end();
      Devices::LED.changeLEDState(false, 0, 0, 0, 0);
      Devices::Display.showThermalWarning();
      delay(2000);
      Devices::Display.powerOff();
  }
  else
  {
    thermalHighSamples = 0;
  }
}

/**
 * @brief Draw one startup status line on the device display.
 * @param[in] heading NUL-terminated label; must not be null.
 * @param[in] value Optional NUL-terminated value shown beside the label.
 * @param[in] warning Selects warning colour when true.
 * @return None. Does nothing beyond advancing the startup line cursor.
 * @note Must run after display initialization and is intended for setup-time
 *       use only. The function is not re-entrant because it uses currentLine.
 */
static void displayMessage(const char* heading, const char* value = nullptr, bool warning = false)
{
  const auto WHITE = Devices::Display.convertStringToColor("WHITE");
  const auto LIGHTGREY = Devices::Display.convertStringToColor("LIGHTGREY");
  const auto RED = Devices::Display.convertStringToColor("RED");
  float scale = Devices::Display.getTextSize() / 10.0; 
  
  Devices::Display.setForegroundColor(LIGHTGREY);
  Devices::Display.display(0, static_cast<int>(std::round(currentLine * 8 * scale)), heading);

  if (value != nullptr)
  {
    Devices::Display.setForegroundColor(warning ? RED : WHITE);
    Devices::Display.display( static_cast<int>(std::round((strlen(heading) + 1) * 6 * scale)),
                          static_cast<int>(std::round(currentLine * 8 * scale)),
                          value
                        );
  }

  Devices::Display.setForegroundColor(WHITE);
  currentLine++;
}

/**
 * @brief Initialize persistent settings and all board subsystems.
 * @return None. Fatal storage conditions are reported and may restart ESP32.
 * @note Runs once in Arduino's main task. USB is disconnected first so the
 *       host observes a clean device re-enumeration after initialization.
 */
void setup()
{
  prefs.begin("usbarmyknife");

  // Start logging before devices so every component can report its startup state.
  Debug::Log.begin(prefs);

  // Initialize board-specific pins and shared hardware buses.
  Devices::Board.begin(prefs);

  if (prefs.getBool("powersave", false))
  {
    setCpuFrequencyMhz(80);
  }

  Devices::Storage.begin(prefs);
  // Marauder setup uses the Wi-Fi radio, so initialize the board Wi-Fi
  // service before starting Marauder's scanner and Bluetooth integration.
  Devices::Wifi.begin(prefs);
  Devices::Display.begin(prefs);
  Devices::LED.begin(prefs);
  if (prefs.getBool("stealth", false))
  {
    Devices::LED.changeLEDState(false, 0, 0, 0, 0);
  }
  Devices::Button.begin(prefs);
  Devices::Mic.begin(prefs);
  
  Devices::USB::Core.begin(prefs);
  Devices::IR.begin(prefs);
  Devices::Touch.begin(prefs);

  // Marauder startup is deferred until the board has been stable for 30s.
  // This keeps a Marauder/USB/SD init fault from creating an early reboot loop.
  Debug::Log.info(TAG, Devices::Board.isSafeMode()
    ? "Safe mode: Marauder disabled."
    : "Marauder will start when requested from web UI.");

  Attacks::Ducky.begin(prefs);
  Attacks::Agent.begin(prefs);

  // Start the dashboard only after every component has finished setup. This
  // avoids creating AsyncWebServer timers while Wi-Fi/Marauder are changing
  // the radio during boot, while still binding before the first loop cycle.
  Devices::Wifi.loop(prefs);

  if (!Devices::Storage.isRunning())
  {
#ifndef NO_SD
    AskFormatSD(prefs);
#endif
  }
  Devices::Display.showStartupLogo();
  displayMessage("Device now running");
  Debug::Log.info(TAG, "System ready.");

  if (Devices::USB::Core.currentDeviceType() == USBDeviceType::Serial)
  {
    displayMessage("USB MODE:", "Serial");
  }
  else if (Devices::USB::Core.currentDeviceType() == USBDeviceType::NCM)
  {
    displayMessage("USB MODE:", "NCM");
  }
  else
  {
    displayMessage("USB MODE:", "Disabled", true);
  }

  if (Devices::USB::Core.currentClassType() == USBClassType::HID)
  {
    displayMessage("USB CLASS:", "HID");
  }
  else if (Devices::USB::Core.currentClassType() == USBClassType::Storage)
  {
    displayMessage("USB CLASS:", "Storage");
  }
  else
  {
    displayMessage("USB CLASS:", "None", true);
  }

  Debug::Log.info(TAG, DEVICE_MAKE_MODEL);

  displayMessage("Version:", GIT_COMMIT_HASH);
  Debug::Log.info(TAG, std::string("Version: ")+GIT_COMMIT_HASH);

  aux.begin(prefs);
}

/**
 * @brief Service every initialized subsystem once per Arduino loop iteration.
 * @return None.
 * @note Runs cooperatively from the Arduino loop task. Each component owns its
 *       own timing and must return promptly so later services are not starved.
 */
void loop()
{
  Devices::Button.loop(prefs);
#ifndef NO_BUTTON
  if (Devices::Button.hasButtonBeenTripleClicked())
  {
    Debug::Log.info("main", "Triple click: stopping all features.");
    Attacks::Ducky.stop();
    Attacks::Marauder.stopAll();
    Devices::USB::NCM.stopPacketCollection();
    if (!thermalShutdown) Devices::Display.showStopFeedback();
    Devices::Button.resetButtonTripleClickedState();
  }
#endif

  serviceThermalProtection();
  if (thermalShutdown) { delay(1000); return; }

  Devices::Board.loop(prefs);

  Debug::Log.loop(prefs);

  Devices::USB::Core.loop(prefs);
  Devices::Wifi.loop(prefs);
  Devices::Storage.loop(prefs);
  Devices::LED.loop(prefs);
  Devices::Display.loop(prefs);
  Devices::Mic.loop(prefs);
  Devices::IR.loop(prefs);
  Devices::Touch.loop(prefs);

  Attacks::Ducky.loop(prefs);
  Attacks::Marauder.loop(prefs);
  Attacks::Agent.loop(prefs);

  aux.loop(prefs);
}

#endif /* ARDUINO_USB_MODE */
