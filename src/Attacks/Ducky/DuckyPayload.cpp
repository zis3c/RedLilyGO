/** @file DuckyPayload.cpp @brief Ducky payload parsing and execution adapter. */

#include "DuckyPayload.h"

#include <DuckyParse.h>
#include <mutex>
#include <atomic>

#include "Diagnostics/Logging.h"

#include "../../Devices/Button/ButtonDevice.h"
#include "../../Devices/Touch/TouchDevice.h"
#include "../../Devices/Storage/StorageDevice.h"
#include "../../Devices/USB/USBCore.h"
#include "../../Devices/USB/USBHID.h"
#include "../../Devices/USB/USBMSC.h"
#include "../../Devices/USB/USBNCM.h"
#include "../../Devices/Display/DisplayDevice.h"
#include "../../Devices/LED/LEDDevice.h"
#include "../../Devices/WiFi/WiFiDevice.h"
#include "../../Devices/Platform/BoardSupport.h"

#include "../../Attacks/Marauder/Marauder.h"
#include "../../Attacks/Agent/Agent.h"

#ifdef DUCKY_CUSTOM_LOG
#warning Enabling DuckyParse logging increases build size
extern void DuckyInterpreterLog(uint8_t level, const char *format, ...)
{
    if (level >= 2) // Forward warnings and errors to the firmware log.
    {
        // Keep the formatted message within a fixed stack buffer.
        const int bufferSize = 1024;
        char buffer[bufferSize];

        // Collect the arguments supplied for the format string.
        va_list args;
        va_start(args, format);

        // Format the parser message before sending it to the logger.
        vsnprintf(buffer, bufferSize, format, args);

        // Release the argument-list state after formatting.
        va_end(args);

        // Pass an owned string to the logging API.
        std::string logMessage(buffer);
        Debug::Log.info("DUCKY", logMessage);
    }
}
#endif

static std::string currentlyExecutingFile;
static std::atomic<uint8_t> totalErrors{0};
static int lastExecutionResult = 0;
static bool firstRun = true;
static std::atomic<bool> requiresReset{false};
static ExtensionCommands extCommands;
static UserDefinedConstants consts;
static bool extensionsRegistered = false;
static std::string localCmdLineToExecute;
static int8_t lastButtonPressState = -1; // -1 means no press, 0 a short press, and 1 a long press.
static int lastSuccessfullyEvaluatedLine = 0;
static Preferences *preferences = nullptr;
static std::recursive_mutex payloadMutex;
static uint32_t payloadGeneration = 0;
static uint32_t executingGeneration = 0;
static bool executingInstruction = false;
static bool parserRestartRequested = false;
static std::string executingCmdLine;

enum class WaitKind { None, Delay, Button, AgentResult, MscActivity, MscIdle, Touch };
static WaitKind pendingWait = WaitKind::None;
static uint32_t waitStartedAt = 0;
static uint32_t waitTimeout = 0;
static uint32_t lastMscActivityAt = 0;

static void startWait(WaitKind kind, uint32_t timeout)
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (executingInstruction && executingGeneration != payloadGeneration) return;
    pendingWait = kind;
    waitStartedAt = millis();
    waitTimeout = timeout;
    lastMscActivityAt = waitStartedAt;
    if (kind == WaitKind::Button)
    {
        Devices::Button.resetButtonPressedState();
        lastButtonPressState = -1;
    }
    else if (kind == WaitKind::Touch) Devices::Touch.resetTouchState();
}

// Poll on the Arduino task, which also produces button/touch events. No wait
// task can outlive Stop, consume a later event, or clear a replacement wait.
static bool serviceWait()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (pendingWait == WaitKind::None) return false;
    const uint32_t now = millis();
    const bool expired = now - waitStartedAt >= waitTimeout;
    bool complete = false;
    switch (pendingWait)
    {
    case WaitKind::Delay:
        complete = expired;
        break;
    case WaitKind::Button:
        if (Devices::Button.hasButtonBeenPressed())
        {
            lastButtonPressState = Devices::Button.wasLastPressLong() ? 1 : 0;
            Devices::Button.resetButtonPressedState();
            complete = true;
        }
        else complete = waitTimeout != 0 && expired;
        break;
    case WaitKind::AgentResult:
        if (Attacks::Agent.hasAgentCmdResult())
        {
            Attacks::Agent.resetAgentCmdResultState();
            complete = true;
        }
        else if (!Attacks::Agent.isAgentConnected())
        {
            Debug::Log.error(LOG_DUCKY, "Agent disconnected while waiting for result");
            complete = true;
        }
        else if (expired)
        {
            Debug::Log.error(LOG_DUCKY, "Agent execution result timed out");
            complete = true;
        }
        break;
    case WaitKind::MscActivity:
    case WaitKind::MscIdle:
        if (Devices::USB::MSC.hasActivity())
        {
            Devices::USB::MSC.resetActivityState();
            lastMscActivityAt = now;
            complete = pendingWait == WaitKind::MscActivity;
        }
        complete = complete || expired ||
            (pendingWait == WaitKind::MscIdle && now - lastMscActivityAt >= 1000);
        if (complete) Debug::Log.info(LOG_DUCKY, "MSC wait complete.");
        break;
    case WaitKind::Touch:
        complete = Devices::Touch.hasBeenTouched() || expired;
        break;
    case WaitKind::None:
        break;
    }
    if (complete) pendingWait = WaitKind::None;
    return !complete;
}

namespace Attacks
{
    DuckyPayload Ducky;
}

static void reset()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (executingInstruction && executingGeneration != payloadGeneration) return;
    // Defer the reset until the next loop so queued HID reports can be flushed.
    requiresReset = true;
}

static std::string readLineFromFileOrCmdLine(const std::string &filename, const int lineNumber)
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (filename.length() == 0)
    {
        // Consume the next line from the inline command buffer.
        Debug::Log.info(LOG_DUCKY, "Reading command line " + std::to_string(lineNumber));

        auto temp = std::move(executingCmdLine);
        executingCmdLine.clear();

        return temp;
    }
    else
    {
        // Track the source line so parser errors can identify the file location.
        lastSuccessfullyEvaluatedLine = lineNumber;
        return Devices::Storage.readLineFromFile(filename, lineNumber);
    }
}

// A single STRING statement queues every character at once, but the HID report
// queue is bounded. Without draining here, reports past the cap are silently
// dropped and long payloads are truncated. Pump the normal HID loop until there
// is room so the script engine paces itself to the host instead of losing input.
static void waitForHidQueueSpace()
{
    if (preferences == nullptr) return;
    uint32_t guard = 0;
    while (!Devices::USB::HID.keyboardQueueHasSpace(1) &&
           Devices::USB::HID.isReady() && guard++ < 5000)
    {
        Devices::USB::HID.loop(*preferences);
    }
}

static void keyboard_press(const uint8_t modifiers, const uint8_t key1, const uint8_t key2, const uint8_t key3, const uint8_t key4, const uint8_t key5, const uint8_t key6)
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (executingInstruction && executingGeneration != payloadGeneration) return;
    waitForHidQueueSpace();
    Devices::USB::HID.keyboard_press(modifiers, key1, key2, key3, key4, key5, key6);
}

static void keyboard_release()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (executingInstruction && executingGeneration != payloadGeneration) return;
    waitForHidQueueSpace();
    Devices::USB::HID.keyboard_release();
}

static void changeLEDState(bool on, uint8_t hue, uint8_t saturation, uint8_t lum, uint8_t brightness)
{
    Devices::LED.changeLEDState(on, hue, saturation, lum, brightness);
}


static void waitForButtonWithTimeout(uint32_t timeout)
{
    startWait(WaitKind::Button, timeout);
}

static void waitForButton()
{
    waitForButtonWithTimeout(0);
}

static void changeUSBMode(DuckyInterpreter::USB_MODE &mode, const uint16_t &vid, const uint16_t &pid, const std::string &man, const std::string &prod, const std::string &serial)
{
    Devices::USB::Core.changeUSBMode(mode, vid, pid, man, prod, serial);
}

static void requestDelay(const uint32_t &time)
{
    Debug::Log.info(LOG_DUCKY, "Requesting wait for " + std::to_string(time));
    startWait(WaitKind::Delay, time);
}

static DuckyInterpreter duckyFileParser = DuckyInterpreter(
    requestDelay,
    readLineFromFileOrCmdLine,
    keyboard_press,
    keyboard_release,
    changeLEDState,
    waitForButton,
    changeUSBMode,
    reset);

#include "Extensions.h"

void DuckyPayload::registerExtension(const std::string& command, std::function<int(const std::string&, const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, int>&)> callback)
{
    if (command.empty() || !callback) return;
    extCommands[command] = callback;
}

void DuckyPayload::registerDynamicVariable(std::function<std::pair<std::string, std::string>()> func)
{
    if (!func) return;
    consts.emplace_back(func);
}

uint8_t DuckyPayload::getTotalErrors()
{
    return totalErrors;
}

bool DuckyPayload::isWaitingForButton()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    return pendingWait == WaitKind::Button;
}

std::string DuckyPayload::getPayloadRunningStatus()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    if (lastExecutionResult == DuckyInterpreter::SCRIPT_ERROR)
    {
        return "Error";
    }
    else if (!currentlyExecutingFile.empty() || !localCmdLineToExecute.empty() ||
             executingInstruction || pendingWait != WaitKind::None)
    {
        return "Running";
    }
    return "Ready";
}

void DuckyPayload::setPayload(const std::string &path)
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    ++payloadGeneration;
    pendingWait = WaitKind::None;
    if (getPayloadRunningStatus() == "Running")
    {
        Debug::Log.warning(LOG_DUCKY, "A payload is already running. Resetting it now.");
    }

    // Copy the requested path into the interpreter's owned string.
    std::string newFileToExecute(path.c_str(), path.length());
    currentlyExecutingFile = newFileToExecute;
    lastExecutionResult = 0;
    lastSuccessfullyEvaluatedLine = 0;
    parserRestartRequested = true;
    Debug::Log.info(LOG_DUCKY, "Payload selected");
    Devices::Display.wakeForActivity();
}

void DuckyPayload::setPayloadCmdLine(const std::string &cmdLine)
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    ++payloadGeneration;
    pendingWait = WaitKind::None;
    if (currentlyExecutingFile.empty()) parserRestartRequested = true;
    localCmdLineToExecute = std::string(cmdLine.c_str(), cmdLine.length());
    lastExecutionResult = 0;
    Devices::Display.wakeForActivity();
}

void DuckyPayload::stop()
{
    const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
    ++payloadGeneration;
    pendingWait = WaitKind::None;
    currentlyExecutingFile.clear();
    localCmdLineToExecute.clear();
    lastExecutionResult = 0;
    parserRestartRequested = true;
    requiresReset = false;
    Devices::USB::HID.releaseAll();
    Debug::Log.info(LOG_DUCKY, "Payload stopped by BOOT button.");
}

DuckyPayload::DuckyPayload()
{
}

void DuckyPayload::begin(Preferences &prefs)
{
    preferences = &prefs;
    if (!extensionsRegistered)
    {
        addDuckyScriptExtensions(extCommands, consts);
        extensionsRegistered = true;
    }
}

Preferences* DuckyPayload::getPreferences()
{
    return preferences;
}

void DuckyPayload::loop(Preferences &prefs)
{
    {
        const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
        if (parserRestartRequested)
        {
            // Only the Arduino task mutates the interpreter. Web requests
            // can replace/stop a payload while Execute is still in progress.
            duckyFileParser.Restart();
            parserRestartRequested = false;
        }
    }
    if (serviceWait())
    {
        // Other device services continue running while the interpreter waits.
    }
    else if (!Devices::USB::HID.IsQueueEmpty())
    {
        // Let queued HID reports drain before advancing the interpreter.
    }
    else if (requiresReset)
    {
        // Finish the pending reset after HID output has drained.
        Devices::USB::Core.end();
#ifdef ARDUINO_ARCH_ESP32
        ESP.restart();
#else
        watchdog_reboot(0, SRAM_END, 0);
#endif
    }
    else
    {
        std::string fileToExecute;
        bool executeFile = false;
        uint32_t generation;
        {
            const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
            fileToExecute = currentlyExecutingFile;
            executeFile = !fileToExecute.empty();
            if (fileToExecute.empty()) fileToExecute = localCmdLineToExecute;
            generation = payloadGeneration;
            executingGeneration = generation;
            executingInstruction = !fileToExecute.empty();
            if (!executeFile)
            {
                executingCmdLine = std::move(localCmdLineToExecute);
                localCmdLineToExecute.clear();
            }
        }
        if (fileToExecute.empty())
        {
            if (firstRun)
            {
                if (Devices::Storage.getFileSize(AUTORUN_FILENAME) != 0 &&
                    preferences != nullptr && preferences->getBool("autorun", true) &&
                    !Devices::Board.isSafeMode())
                    setPayload(AUTORUN_FILENAME);
                firstRun = false;
            }
            return;
        }

        if (!executeFile)
        {
            Debug::Log.info(LOG_DUCKY, "Executing web or USB command line");
        }

        const int result = duckyFileParser.Execute(executeFile ? fileToExecute : "", extCommands, consts);
        const std::lock_guard<std::recursive_mutex> lock(payloadMutex);
        executingInstruction = false;
        if (generation != payloadGeneration) return;
        lastExecutionResult = result;
        const bool executionHasCompleted = lastExecutionResult == DuckyInterpreter::SCRIPT_ERROR || lastExecutionResult == DuckyInterpreter::END_OF_FILE;

        if (lastExecutionResult == DuckyInterpreter::SCRIPT_ERROR)
        {
            Debug::Log.error(LOG_DUCKY, executeFile ? ("Script error near line " + std::to_string(lastSuccessfullyEvaluatedLine + 1)) : "Command execution failed.");
            totalErrors++;      
        }
        else if (lastExecutionResult == DuckyInterpreter::END_OF_FILE)
        {
            Debug::Log.info(LOG_DUCKY, "Script finished.");
        }

        if (executionHasCompleted)
        {
            // The generation check above preserves any replacement request.
            if (executeFile) currentlyExecutingFile.clear();
            lastSuccessfullyEvaluatedLine = 0;
            duckyFileParser.Restart();
        }
    }
}
