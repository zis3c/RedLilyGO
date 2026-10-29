/** @file Agent.cpp @brief Host agent command transport and execution lifecycle. */

#include "Agent.h"

#include "Diagnostics/Logging.h"
#include "../../Devices/USB/USBCDC.h"

namespace Attacks
{
    SerialAgent Agent;
}

#define LOG_AGENT "Agent"

#define USB_AGENT_POLLING_ENABLED "agentPolling"
#define USB_AGENT_POLLING_ENABLED_DEFAULT true

#define USB_AGENT_POLLING_TIME_IN_SEC "agentPollSec"
#define USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT 15
#define AGENT_PROTOCOL_VERSION "2"

static bool agentPollingEnabled = USB_AGENT_POLLING_ENABLED_DEFAULT;
static uint16_t agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT;
static unsigned long previousMillis = 0; 
static unsigned long lastAgentStatusMillis = 0;

SerialAgent::SerialAgent()
{
}

void SerialAgent::begin(Preferences &prefs)
{
    const std::lock_guard<std::mutex> stateLock(stateMutex);
    m_agentIsConnected = false;
    m_protocolCompatible = false;
    m_machineName.clear();
    m_agentVersion.clear();
    agentCmdResult = false;
    previousMillis = 0;
    lastAgentStatusMillis = 0;
    registerUserConfigurableSetting(CATEGORY_USB, USB_AGENT_POLLING_ENABLED, RedLilyGOCapability::SettingType::Bool, USB_AGENT_POLLING_ENABLED_DEFAULT);
    registerUserConfigurableSetting(CATEGORY_USB, USB_AGENT_POLLING_TIME_IN_SEC, RedLilyGOCapability::SettingType::Int16, (int16_t)USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT);

    agentPollingEnabled = prefs.getBool(USB_AGENT_POLLING_ENABLED, USB_AGENT_POLLING_ENABLED_DEFAULT);
    const int16_t configuredPollingInterval = prefs.getShort(USB_AGENT_POLLING_TIME_IN_SEC, USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT);
    agentPollingIntervalInSec = configuredPollingInterval < 1 ? USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT :
        (configuredPollingInterval > 3600 ? 3600 : static_cast<uint16_t>(configuredPollingInterval));

    Devices::USB::CDC.setCallback(HostCommand::AgentStatus, [this](uint8_t *buffer, const size_t size) -> void
    {
        if (buffer == nullptr || size == 0) return;
        const std::lock_guard<std::mutex> stateLock(stateMutex);
        const bool wasConnected = m_agentIsConnected;
        m_agentIsConnected = true;
        lastAgentStatusMillis = millis();
        if (!wasConnected) Debug::Log.info(LOG_AGENT, "Agent has responded with status");
        const std::string status((char*)buffer, size);
        const size_t separator = status.find('\n');
        m_machineName = separator == std::string::npos ? status : status.substr(0, separator);
        if (separator == std::string::npos)
        {
            m_agentVersion = "unknown";
            m_protocolCompatible = false;
        }
        else
        {
            const size_t protocolSeparator = status.find('\n', separator + 1);
            m_agentVersion = status.substr(separator + 1,
                protocolSeparator == std::string::npos ? std::string::npos : protocolSeparator - separator - 1);
            std::string protocol = protocolSeparator == std::string::npos ? "" : status.substr(protocolSeparator + 1);
            if (!protocol.empty() && protocol.back() == '\r') protocol.pop_back();
            m_protocolCompatible = protocol == AGENT_PROTOCOL_VERSION;
        }
    });
    Devices::USB::CDC.setCallback(HostCommand::ExecuteResult, [this](uint8_t *buffer, const size_t size) -> void
    {
        Debug::Log.info(LOG_AGENT, "Agent execution result received");

        agentCmdResult.store(true, std::memory_order_release);
    });
}

void SerialAgent::loop(Preferences &prefs)
{
    if (!Devices::USB::CDC.isReady())
    {
        bool wasConnected = false;
        {
            const std::lock_guard<std::mutex> stateLock(stateMutex);
            wasConnected = m_agentIsConnected.load(std::memory_order_acquire);
            m_agentIsConnected.store(false, std::memory_order_release);
            m_protocolCompatible.store(false, std::memory_order_release);
            lastAgentStatusMillis = 0;
        }
        if (wasConnected)
        {
            Debug::Log.warning(LOG_AGENT, "Agent disconnected: USB mode changed.");
        }
        return;
    }

    unsigned long currentMillis = millis();
    const unsigned long heartbeatTimeout = std::max(30000UL, agentPollingIntervalInSec * 3000UL);
    bool timedOut = false;
    {
        const std::lock_guard<std::mutex> stateLock(stateMutex);
        if (m_agentIsConnected.load(std::memory_order_acquire) && lastAgentStatusMillis != 0 &&
            currentMillis - lastAgentStatusMillis >= heartbeatTimeout)
        {
            m_agentIsConnected.store(false, std::memory_order_release);
            m_protocolCompatible.store(false, std::memory_order_release);
            timedOut = true;
        }
    }
    if (timedOut)
    {
        Debug::Log.warning(LOG_AGENT, "Agent heartbeat timed out. Reconnecting.");
    }

    // The host responds to status requests; it does not send heartbeats on
    // its own. Keep polling established sessions so they do not time out.
    if (agentPollingEnabled && Devices::USB::CDC.isReady())
    {
        if (currentMillis - previousMillis >= agentPollingIntervalInSec * 1000) {
            Devices::USB::CDC.writeBinary(HostCommand::RequestAgentStatus, nullptr, 0);
            previousMillis = currentMillis;
        }
    }
}

void SerialAgent::run(const std::string& cmd)
{
    if (cmd.empty())
    {
        Debug::Log.error(LOG_AGENT, "Cannot run agent command. Command is empty.");
    }
    else if (!Devices::USB::CDC.isReady())
    {
        Debug::Log.error(LOG_AGENT, "Cannot run agent command. Device is not in serial mode.");
    }
    else if (!m_agentIsConnected)
    {
        Debug::Log.error(LOG_AGENT, "Cannot run agent command. Agent is not connected.");
    }
    else if (!m_protocolCompatible)
    {
        Debug::Log.error(LOG_AGENT, "Cannot run agent command. Protocol is incompatible.");
    }
    else
    {
        agentCmdResult = false;
        Devices::USB::CDC.writeBinary(HostCommand::Execute, (uint8_t*)cmd.c_str(), cmd.length());
    }
}
