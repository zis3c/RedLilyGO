/** @file Agent.h @brief Host agent command and status interface. */

#pragma once

#include <string>
#include <atomic>
#include <mutex>

#include "../../RedLilyGOCapability.h"

class SerialAgent : RedLilyGOCapability {
public:
  /** @brief Construct serial agent state with connection cleared. */
  SerialAgent();

  /** @brief Service framed agent messages from the host. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize agent transport and protocol settings. */
  virtual void begin(Preferences& prefs);

  /** @brief Send command to connected host agent. */
  void run(const std::string& cmd);
  /** @brief Report whether host agent is connected. */
  bool isAgentConnected() { return m_agentIsConnected.load(std::memory_order_acquire); }
  /** @brief Return mutex-protected host machine name. */
  std::string machineName() { const std::lock_guard<std::mutex> lock(stateMutex); return m_machineName; }
  /** @brief Return mutex-protected agent version string. */
  std::string agentVersion() { const std::lock_guard<std::mutex> lock(stateMutex); return m_agentVersion; }
  /** @brief Report whether agent protocol version is supported. */
  bool protocolCompatible() { return m_protocolCompatible.load(std::memory_order_acquire); }
  /** @brief Report whether an agent command result awaits consumption. */
  bool hasAgentCmdResult() { return agentCmdResult.load(std::memory_order_acquire); }
  /** @brief Clear pending agent command result state. */
  void resetAgentCmdResultState() { agentCmdResult.store(false, std::memory_order_release); }

private:
  std::atomic<bool> m_agentIsConnected{false};
  std::atomic<bool> agentCmdResult{false};
  std::string m_machineName;
  std::string m_agentVersion;
  std::atomic<bool> m_protocolCompatible{false};
  mutable std::mutex stateMutex;
};

namespace Attacks
{
    extern SerialAgent Agent;
}
