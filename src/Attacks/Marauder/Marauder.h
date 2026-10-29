/** @file Marauder.h @brief Wi-Fi/BLE Marauder attack lifecycle interface. */

#pragma once

#include "../../RedLilyGOCapability.h"
#include <string>
#include <atomic>

class ESP32Marauder : RedLilyGOCapability {
public:
  /** @brief Construct Marauder integration in stopped state. */
  ESP32Marauder();

  /** @brief Service Marauder command and attack state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize Marauder Wi-Fi/BLE resources. */
  virtual void begin(Preferences& prefs);

  /** @brief Dispatch one authorized Marauder command line. */
  void run(const std::string& cmd);
  /** @brief Report whether captive portal owns web service. */
  bool isEvilPortalActive() const { return evilPortalActive.load(std::memory_order_acquire); }
  /** @brief Report whether the Marauder service initialized successfully. */
  bool isRunning() const { return running.load(std::memory_order_acquire); }
  /** @brief Report whether Marauder has an active command loop. */
  bool isActive() const;
  /** @brief Report whether Wi-Fi raw capture is active. */
  bool isWifiCaptureActive() const { return wifiCaptureActive.load(std::memory_order_acquire); }
  /** @brief Return current captured packet count. */
  uint16_t getPacketCount();
  /** @brief Return current access points as JSON. */
  std::string getAccessPointsJson();
  /** @brief Return captive portal status as JSON. */
  std::string getEvilPortalJson();
  /** @brief Return selectable captive portal HTML files as JSON. */
  std::string getEvilPortalFilesJson();
  /** @brief Queue captive portal startup for a validated HTML template. */
  bool startEvilPortal(const std::string &filename, const std::string &ssid, std::string &error);
  /** @brief Queue captive portal shutdown. */
  void stopEvilPortal();
  /** @brief Immediately stop Marauder radio and captive-portal activity. */
  void stopAll();
  /** @brief Clear local captive portal submission history. */
  bool clearEvilPortalHistory();
  /** @brief Return current deauthentication frame count. */
  uint32_t getDeauthCount();
  /** @brief Return deauthentication events as JSON. */
  std::string getDeauthEventsJson();
  /** @brief Return channel-hop delay in seconds. */
  uint8_t getChannelHopDelay();
  /** @brief Set channel-hop delay in seconds. */
  void setChannelHopDelay(uint8_t seconds);
private:
  std::atomic<bool> running{false};
  std::atomic<bool> evilPortalActive{false};
  std::atomic<bool> wifiCaptureActive{false};
};

namespace Attacks
{
    extern ESP32Marauder Marauder;
}
