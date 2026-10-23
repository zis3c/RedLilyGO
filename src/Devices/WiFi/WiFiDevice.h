/**
 * @file WiFiDevice.h
 * @brief Wi-Fi AP or station lifecycle and status interface.
 *
 * Uses ESP32 WiFi radio and Preferences configuration. Reconnect and web
 * server ownership are serviced from the Arduino loop task.
 */

#pragma once

#include <string>

#include "../../RedLilyGOCapability.h"

class WiFiDevice : RedLilyGOCapability {
public:
  /** @brief Construct Wi-Fi adapter with radio initially controlled by begin. */
  WiFiDevice();

  /** @brief Service radio, reconnect, and web-server state. */
  virtual void loop(Preferences& prefs);
  /** @brief Load Wi-Fi settings and start requested AP or station mode. */
  virtual void begin(Preferences& prefs);
  /** @brief Disable radio and dependent web service. */
  virtual void end();
  /** @brief Set desired radio state for subsequent loop processing. */
  void setWiFi(const bool& state);
  /** @brief Report active AP or station connection state. */
  bool getState();
  /** @brief Return station RSSI in dBm, or zero outside station connection. */
  int signalStrength();
  /** @brief Return active AP or station IPv4 address, or empty when offline. */
  std::string currentIPAddress();
  /** @brief Return the configured AP host used for redirects and origin checks. */
  std::string apHostname();
};

namespace Devices
{
    extern WiFiDevice Wifi;
}
