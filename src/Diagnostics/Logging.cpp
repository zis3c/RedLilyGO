/** @file Logging.cpp @brief Thread-safe firmware logging and serial output. */

#include "Logging.h"
#include "../Devices/USB/USBCDC.h"
#include "../Devices/Storage/StorageDevice.h"
#include "../Attacks/Agent/Agent.h"

#include <mutex>
#include <deque>
#include <atomic>
#include <cstdio>

static std::vector<std::string> logMsg;
// logMsg is written from the async_tcp task (web handlers) and read/pruned
// from the main task: every access must hold this mutex or the vector's
// internal state gets corrupted.
static std::mutex logMsgMutex;
static constexpr size_t MAX_RUNTIME_LOGS = 30;

#define LOG_DELAY_UNTIL_DEBUGGER                    "loggerdelay"
#define LOG_DELAY_UNTIL_DEBUGGER_DEFAULT            true

static bool loggerDelayUntilDebuggerConnected = LOG_DELAY_UNTIL_DEBUGGER_DEFAULT;
static bool writtenPendingEntries = false;
static std::deque<std::string> pendingDiskLogs;
static std::mutex diskLogMutex;
static constexpr size_t MAX_PENDING_DISK_LOGS = 32;
static constexpr const char *SYSTEM_LOG_FILE = "/Logs/system.log";
static std::atomic<bool> diskLogWriteFailed{false};

namespace Debug
{
    Logging Log;
}

Logging::Logging()
{
}

static std::string uptimeTimestamp(const uint32_t elapsedMs)
{
    const uint32_t minutes = elapsedMs / 60000UL;
    const uint32_t seconds = (elapsedMs / 1000UL) % 60UL;
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu",
        static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
    return timestamp;
}

static std::string shortComponent(const std::string &component)
{
    if (component == "BOARD") return "BOOT";
    if (component == "WiFi") return "WIFI";
    if (component == "main" || component == "Main") return "SYS";
    if (component == "ESP32M" || component == "MARAUDER") return "MRD";
    if (component == "MMC" || component == "STORAGE") return "SD";
    if (component == "DISPLAY") return "TFT";
    return component;
}

static std::string shortMessage(const std::string &message)
{
    if (message.rfind("Reset reason code: ", 0) == 0) return "Reset " + message.substr(19);
    if (message == "WiFi mode: AP, password configured") return "AP + PW";
    if (message == "WiFi mode: Station, password configured") return "STA + PW";
    if (message == "Timeout waiting for USB to mount") return "Mount timeout";
    if (message == "Marauder will start when requested from web UI.") return "Marauder ready";
    if (message == "Starting Marauder on user request.") return "MRD start";
    if (message == "Marauder command queued") return "Marauder queued";
    if (message == "Running Marauder command") return "Running";
    if (message == "Marauder command stopped") return "Stopped";
    if (message == "Marauder command finished") return "Finished";
    if (message == "Starting web server.") return "Web server on";
    if (message == "System ready.") return "Ready";
    if (message == "Input client rejected: USB HID is unavailable.") return "HID unavailable";
    if (message.rfind("Version: ", 0) == 0) return "FW " + message.substr(9, 7);

    std::string compact = message;
    if (!compact.empty() && compact.back() == '.') compact.pop_back();
    return compact;
}

static std::string formatLogLine(const std::string &timestamp, const std::string &component, const std::string &message)
{
    return timestamp + " " + shortComponent(component) + " " + shortMessage(message);
}

std::vector<std::string> Logging::getLogs()
{
    const std::lock_guard<std::mutex> lock(logMsgMutex);
    return logMsg;
}

void Logging::appendLogsToJson(JsonArray array)
{
    const std::lock_guard<std::mutex> lock(logMsgMutex);
    for (const auto &entry : logMsg) array.add(entry);
}

void Logging::clearLogs()
{
    const std::lock_guard<std::mutex> lock(logMsgMutex);
    logMsg.clear();
}

static void writeDebugOut(const std::string& component, const std::string& msg)
{
    if (msg == "Marauder will start when requested from web UI." ||
        msg == "Starting Marauder on user request." ||
        msg == "Marauder command queued" ||
        msg == "Running Marauder command") return;
    const uint32_t elapsedMs = millis();
    const std::string timestamp = uptimeTimestamp(elapsedMs);
    const std::string boundedMessage = msg.size() > MAX_LOG_ENTRY_IN_CHARS ? "ERROR LOG MESSAGE TOO LARGE!" : msg;
    const std::string line = formatLogLine(timestamp, component, boundedMessage);
    {
        // Lock only covers the vector mutation: writeDebugString below can
        // recurse back into logging (e.g. short-write errors), which must not
        // happen while the mutex is held.
        const std::lock_guard<std::mutex> lock(logMsgMutex);
        if (logMsg.size() >= MAX_RUNTIME_LOGS) logMsg.erase(logMsg.begin());
        logMsg.emplace_back(line);
    }
    if (Devices::Storage.isRunning())
    {
        const std::string diskLine = line + "\n";
        // SD writes run in Logging::loop(), not inside async web callbacks. This
        // avoids taking the disk lock while another web request is reading SD.
        const std::lock_guard<std::mutex> lock(diskLogMutex);
        if (pendingDiskLogs.size() >= MAX_PENDING_DISK_LOGS) pendingDiskLogs.pop_front();
        pendingDiskLogs.push_back(diskLine);
    }
    if (loggerDelayUntilDebuggerConnected == false || (loggerDelayUntilDebuggerConnected && Attacks::Agent.isAgentConnected()))
    {
        Devices::USB::CDC.writeDebugString(std::string(component) + "|" + boundedMessage);
    }
}

void Logging::info(const char *component, const std::string &msg)
{
    writeDebugOut(component, msg);
}

void Logging::warning(const char *component, const std::string &msg)
{
    writeDebugOut(component, msg);
}

void Logging::error(const char *component, const std::string &msg)
{
    writeDebugOut(component, msg);
}

extern "C" void log_compat(const char* msg)
{
    std::string logMsg(msg);
    Debug::Log.info("C", logMsg);
}

void Logging::loop(Preferences &prefs)
{
    // Storage can log while holding its mutex. Never hold diskLogMutex while
    // entering storage, or concurrent SD errors can deadlock both tasks.
    if (!Devices::Storage.isRunning())
    {
        const std::lock_guard<std::mutex> lock(diskLogMutex);
        pendingDiskLogs.clear();
    }
    else for (size_t i = 0; i < MAX_PENDING_DISK_LOGS; ++i)
    {
        std::string line;
        {
            const std::lock_guard<std::mutex> lock(diskLogMutex);
            if (pendingDiskLogs.empty()) break;
            line = std::move(pendingDiskLogs.front());
            pendingDiskLogs.pop_front();
        }
        const bool written = Devices::Storage.appendFileQuiet(SYSTEM_LOG_FILE,
            reinterpret_cast<const uint8_t *>(line.data()), line.size());
        diskLogWriteFailed.store(!written);
        if (!written)
        {
            const std::lock_guard<std::mutex> lock(diskLogMutex);
            if (pendingDiskLogs.size() < MAX_PENDING_DISK_LOGS) pendingDiskLogs.push_front(std::move(line));
            break;
        }
    }
    std::vector<std::string> pendingEntries;
    {
        const std::lock_guard<std::mutex> lock(logMsgMutex);
        if (loggerDelayUntilDebuggerConnected && !writtenPendingEntries && Attacks::Agent.isAgentConnected())
        {
            pendingEntries = logMsg;
        }
    }

    if (!pendingEntries.empty())
    {
        for (const auto& msg : pendingEntries)
        {
            Devices::USB::CDC.writeDebugString("OLD|" + msg);
        }
        writtenPendingEntries = true;
    }
}

std::string Logging::systemLogStatus() const
{
    if (!Devices::Storage.isRunning()) return "unavailable";
    if (diskLogWriteFailed.load() && Devices::Storage.isFull()) return "full";
    if (diskLogWriteFailed.load()) return "write_failed";
    return "active";
}

void Logging::begin(Preferences &prefs)
{
    registerUserConfigurableSetting(CATEGORY_USB, LOG_DELAY_UNTIL_DEBUGGER, RedLilyGOCapability::SettingType::Bool, LOG_DELAY_UNTIL_DEBUGGER_DEFAULT);
    loggerDelayUntilDebuggerConnected = prefs.getBool(LOG_DELAY_UNTIL_DEBUGGER, LOG_DELAY_UNTIL_DEBUGGER_DEFAULT);
}
