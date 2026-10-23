/** @file DuckyPayload.h @brief Ducky payload execution interface. */

#pragma once

#include <string>
#include <functional>

#include "../../RedLilyGOCapability.h"

#define AUTORUN_FILENAME "/autorun.ds"
#define LOG_DUCKY "DUCKY"

class DuckyPayload : RedLilyGOCapability {
public:
  /** @brief Construct payload interpreter in idle state. */
  DuckyPayload();

  /** @brief Load payload settings and register interpreter resources. */
  virtual void begin(Preferences& prefs);
  /** @brief Advance payload interpreter state machine. */
  virtual void loop(Preferences& prefs);

  /** @brief Set inline payload command text. */
  void setPayloadCmdLine(const std::string& cmdLine);
  /** @brief Select payload file path for execution. */
  void setPayload(const std::string& path);
  /** @brief Return current payload state label. */
  std::string getPayloadRunningStatus();
  /** @brief Return cumulative payload error count. */
  uint8_t getTotalErrors();
  /** @brief Reserve button events for a pending payload wait. */
  bool isWaitingForButton();
  /** @brief Stop current payload and release pending HID work. */
  void stop();
  /** @brief Register custom command extension callback. */
  void registerExtension(const std::string& command, std::function<int(const std::string&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, int>&)> callback);
  /** @brief Register dynamic variable resolver used during expansion. */
  void registerDynamicVariable(std::function<std::pair<std::string, std::string>()>);
  /** @brief Return initialized settings for extension commands. */
  Preferences* getPreferences();
};

namespace Attacks
{
    extern DuckyPayload Ducky;
}
