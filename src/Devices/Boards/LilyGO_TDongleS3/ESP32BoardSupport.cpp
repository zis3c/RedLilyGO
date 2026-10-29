/**
 * @file ESP32BoardSupport.cpp
 * @brief ESP32-S3 board lifecycle and crash tracking.
 *
 * Uses ESP reset metadata plus the board Preferences namespace.
 * Startup and periodic work run from the Arduino loop task; no ISR accesses
 * the state in this module.
 */

#ifdef ARDUINO_ARCH_ESP32
#include "ESP32BoardSupport.h"

#include <backtrace_saver.hpp>

#include "Diagnostics/Logging.h"
#include "../../Display/DisplayDevice.h"
#define TAG "BOARD"

static unsigned long previousMillis = 0;
static bool hasHadCrash = false;
static bool emittedCrashDump = false;
static bool safeMode = false;
static bool crashStreakCleared = false;

static std::string savedBacktrace(const backtrace_saver::re_restart_debug_t &debugInfo)
{
    std::string trace;
    char pcBuffer[16] = {0};
    for (int i = 0; i < CONFIG_RESTART_DEBUG_STACK_DEPTH && debugInfo.backtracePC[i] != 0; ++i)
    {
        snprintf(pcBuffer, sizeof(pcBuffer), "%x", debugInfo.backtracePC[i]);
        if (!trace.empty()) trace += ' ';
        trace += pcBuffer;
    }
    return trace;
}

namespace Devices
{
    ESP32BoardSupport Board;
}

ESP32BoardSupport::ESP32BoardSupport()
{
}

/**
 * @brief Initialize board support and classify the previous reset.
 * @param[in,out] prefs Persistent settings store; must be opened by caller.
 * @return None. Crash streak is capped at 255 and safe mode starts at three.
 * @note Runs once during setup before normal component tasks are started.
 */
void ESP32BoardSupport::begin(Preferences &prefs)
{
    BoardSupport::begin(prefs);

    backtrace_saver::init();
    const auto &resetReason = esp_reset_reason();
    const auto debugInfo = backtrace_saver::debugGet();
    // ESP_RST_EXT / ESP_RST_USB / ESP_RST_JTAG are produced by normal board,
    // USB, or debugger resets, not application crashes.
    hasHadCrash = resetReason != ESP_RST_UNKNOWN && resetReason != ESP_RST_POWERON && resetReason != ESP_RST_SW &&
                  resetReason != ESP_RST_EXT && resetReason != ESP_RST_USB && resetReason != ESP_RST_JTAG;

    const std::string trace = hasHadCrash ? savedBacktrace(debugInfo) : std::string();
    prefs.putUChar("last-reset-code", static_cast<uint8_t>(resetReason));
    prefs.putBool("last-rst-crash", hasHadCrash);
    prefs.putString("last-backtrace", trace.c_str());

    Debug::Log.info(TAG, std::string("Reset reason code: ") + std::to_string(static_cast<int>(resetReason)));
    if (hasHadCrash)
    {
        Debug::Log.error(TAG, "Previous boot crashed. Persistent backtrace saved.");
        if (!trace.empty()) Debug::Log.error(TAG, trace);
    }

    uint8_t crashStreak = prefs.getUChar("crash-streak", 0);
    if (hasHadCrash && crashStreak < 255)
    {
        crashStreak++;
        prefs.putUChar("crash-streak", crashStreak);
    }
    safeMode = crashStreak >= 3;
    if (safeMode)
    {
        Debug::Log.error(TAG, "Safe mode enabled after repeated crash boots.");
    }
}

/**
 * @brief Report whether previous boot ended with a crash-class reset.
 * @return true when reset reason indicates a crash; otherwise false.
 * @note Read-only after begin().
 */
bool ESP32BoardSupport::hasCrashed()
{
    return hasHadCrash;
}

/**
 * @brief Report whether repeated crashes selected safe mode.
 * @return true when crash streak is at least three; otherwise false.
 */
bool ESP32BoardSupport::isSafeMode() const
{
    return safeMode;
}

/**
 * @brief Service crash reporting and streak recovery.
 * @param[in,out] prefs Persistent settings store used to clear stable-boot
 *        crash state.
 * @return None.
 * @note Called from the cooperative Arduino loop. Stable-boot actions wait
 *       30 seconds before clearing the persistent crash streak.
 */
void ESP32BoardSupport::loop(Preferences &prefs)
{
    BoardSupport::loop(prefs);

    if (!crashStreakCleared && millis() >= 30000)
    {
        prefs.putUChar("crash-streak", 0);
        crashStreakCleared = true;
    }

    if (hasHadCrash && !emittedCrashDump)
    {
        const auto RED = Devices::Display.convertStringToColor("RED");
        const auto BLACK = Devices::Display.convertStringToColor("BLACK");
        const auto WHITE = Devices::Display.convertStringToColor("WHITE");
        const auto LIGHTGREY = Devices::Display.convertStringToColor("LIGHTGREY");

        Devices::Display.clearScreen();
        Devices::Display.setBackgroundColor(RED);
        Devices::Display.setForegroundColor(BLACK);
        Devices::Display.display(0, 0, "System crash. Please report this bug.");
        Devices::Display.setBackgroundColor(BLACK);
        Devices::Display.setForegroundColor(WHITE);

        const backtrace_saver::re_restart_debug_t current_debug_info = backtrace_saver::debugGet();
        char pcBuffer[16] = {0};

        std::string stackTrace;
        stackTrace.reserve(200);
        uint8_t lineNumber = 2;

        for (int i = 0; i < CONFIG_RESTART_DEBUG_STACK_DEPTH && current_debug_info.backtracePC[i] != 0; i++)
        {
            snprintf(pcBuffer, sizeof(pcBuffer), "%x", current_debug_info.backtracePC[i]);
            const auto& pcBufStr = std::string(pcBuffer);
            stackTrace += pcBufStr + " ";

#ifdef DISPLAY_HEIGHT
            if (lineNumber * 10 < DISPLAY_HEIGHT)
            {
                Devices::Display.setForegroundColor(i % 2 == 0 ? WHITE : LIGHTGREY);

                Devices::Display.display(
                    i % 2 == 0 ? 0 : 60,
                    lineNumber * 10, pcBufStr);
            }

            lineNumber = i % 2 != 0 ? lineNumber + 1 : lineNumber;
#endif
        }

        // addr2line -pfiaC -e .pio/build/esp32/firmware.elf 400d1ba7 400d575f 40082e1d 400846c8 400829c5 400891ad
        Debug::Log.info(TAG, stackTrace);

        emittedCrashDump = true;
    }
}
#endif
