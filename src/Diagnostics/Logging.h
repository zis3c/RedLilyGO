/** @file Logging.h @brief Firmware logging interface and log storage. */

#pragma once

#ifdef __cplusplus
#include "../RedLilyGOCapability.h"
#include <ArduinoJson.h>

#include <string>
#include <vector>

#define MAX_LOG_ENTRY_IN_CHARS 350

class Logging : RedLilyGOCapability {
public:
  Logging();

  virtual void loop(Preferences& prefs);
  virtual void begin(Preferences& prefs);

  void info(const char* component, const std::string& msg);
  void warning(const char* component, const std::string& msg);
  void error(const char* component, const std::string& msg);
  std::string systemLogStatus() const;
  // Returns a snapshot copy: the live vector is mutated from multiple
  // FreeRTOS tasks (async web handlers + main loop), so handing out a
  // reference invites iterator invalidation / heap corruption.
  std::vector<std::string> getLogs();
  void appendLogsToJson(JsonArray array);
  void clearLogs();
};

namespace Debug
{
    extern Logging Log;
}

extern "C"
{
#endif

    void log_compat(const char*);

#ifdef __cplusplus
 }
#endif
