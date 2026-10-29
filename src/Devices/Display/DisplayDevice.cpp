/**
 * @file DisplayDevice.cpp
 * @brief ST7735 SPI rendering, dashboard state machine, and screensaver.
 *
 * Uses LovyanGFX over the configured SPI pins and shared-bus transaction
 * locks. Rendering and button handling run on the Arduino loop task. Web-side
 * callers queue activity/image requests without touching the panel.
 */

#ifndef NO_TFT
#include "DisplayDevice.h"
#include "DisplayInternal.h"

#include "Diagnostics/Logging.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "../LED/LEDDevice.h"
#include "../WiFi/WiFiDevice.h"
#include "../Button/ButtonDevice.h"
#include "../USB/USBCore.h"
#include "../../Attacks/Agent/Agent.h"
#ifndef NO_WEB
#include "../../Comms/Web/WebServer.h"
#endif
#ifndef NO_ESP_MARAUDER
#include "../../Attacks/Marauder/Marauder.h"
#endif

#include <LovyanGFX.hpp>

#include <algorithm>
#include <unordered_map>
#include <mutex>

#define LOG_TFT "TFT"

static bool displayVariablesRegistered = false;
static std::mutex imageRequestMutex;
static std::string requestedImage;

#define TFT_TextSize "tftTextSize"
#define TFT_TextSize_Default 10
#define TFT_SCREENSAVER "screensaver"
#define TFT_SCREENSAVER_DEFAULT true

static int foregroundColor = TFT_WHITE;
static int backgroundColor = TFT_BLACK;

// The LGFX_Panel class is defined in DisplayInternal.h so DisplayScreens.cpp
// can render through the same panel object.

// Shared with DisplayScreens.cpp through DisplayInternal.h.
namespace DisplayInternal
{
  LGFX_Panel lcd;
  std::atomic<bool> isScreenOff{false};
  std::atomic<bool> manualScreenOff{false};
  ScreenMode currentMode = MODE_DASHBOARD;
  bool screensaverEnabledState = true;
  bool bootScreenCleared = false;
  bool screensaverInitialized = false;
  unsigned long lastDashboardUpdate = 0;
  unsigned long lastMatrixUpdate = 0;
  unsigned long previousMillis = 0;
  int drops[27] = {0};
}

// Referenced by the coordinator and renderers.
using DisplayInternal::lcd;
using DisplayInternal::isScreenOff;
using DisplayInternal::manualScreenOff;
using DisplayInternal::currentMode;
using DisplayInternal::bootScreenCleared;
using DisplayInternal::screensaverInitialized;
using DisplayInternal::lastDashboardUpdate;
using DisplayInternal::lastMatrixUpdate;
using DisplayInternal::previousMillis;

static unsigned long lastLedUpdate = 0;
static unsigned long lastActionTime = 0;

static unsigned long stopFeedbackUntil = 0;
static int breathBrightness = 10;
static int breathDirection = 1;



static std::unordered_map<std::string, int> colorLookup{
    {"BLACK", TFT_BLACK},
    {"NAVY", TFT_NAVY},
    {"DARKGREEN", TFT_DARKGREEN},
    {"DARKCYAN", TFT_DARKCYAN},
    {"MAROON", TFT_MAROON},
    {"PURPLE", TFT_PURPLE},
    {"OLIVE", TFT_OLIVE},
    {"LIGHTGREY", TFT_LIGHTGREY},
    {"LIGHTGRAY", TFT_LIGHTGRAY},
    {"DARKGREY", TFT_DARKGREY},
    {"DARKGRAY", TFT_DARKGRAY},
    {"BLUE", TFT_BLUE},
    {"GREEN", TFT_GREEN},
    {"CYAN", TFT_CYAN},
    {"RED", TFT_RED},
    {"MAGENTA", TFT_MAGENTA},
    {"YELLOW", TFT_YELLOW},
    {"WHITE", TFT_WHITE},
    {"ORANGE", TFT_ORANGE},
    {"GREENYELLOW", TFT_GREENYELLOW},
    {"PINK", TFT_PINK},
    {"BROWN", TFT_BROWN},
    {"GOLD", TFT_GOLD},
    {"SILVER", TFT_SILVER},
    {"SKYBLUE", TFT_SKYBLUE},
    {"VIOLET", TFT_VIOLET},
};

namespace Devices
{
    DisplayDevice Display;
}

void DisplayDevice::display(const int &x, const int &y, const std::string &str)
{
    lcd.startWrite();
    lcd.setCursor(x, y);
    lcd.setTextColor(foregroundColor, backgroundColor);
    lcd.println(str.c_str());
    lcd.display();
    lcd.endWrite();
}

void DisplayDevice::clearScreen()
{
    lcd.startWrite();
    lcd.clear(backgroundColor);
    lcd.display();
    lcd.endWrite();
}

void DisplayDevice::powerOff()
{
    lcd.clearDisplay();
    lcd.setBrightness(0);
    lcd.sleep();
}

void DisplayDevice::powerOn()
{
    if (isScreenOff) return;
    lcd.wakeup();
    lcd.setBrightness(100);
}

void DisplayDevice::showThermalWarning()
{
    lcd.startWrite();
    lcd.wakeup();
    lcd.setBrightness(100);
    lcd.fillScreen(TFT_BLACK);
    lcd.fillTriangle(DISPLAY_WIDTH / 2, 10, 18, DISPLAY_HEIGHT - 14,
                     DISPLAY_WIDTH - 18, DISPLAY_HEIGHT - 14, TFT_ORANGE);
    lcd.setTextColor(TFT_BLACK, TFT_ORANGE);
    lcd.setTextSize(3);
    lcd.setCursor((DISPLAY_WIDTH / 2) - 5, 28);
    lcd.print("!");
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setCursor(38, DISPLAY_HEIGHT - 10);
    lcd.print("THERMAL SHUTDOWN");
    lcd.display();
    lcd.endWrite();
}

void DisplayDevice::displayPng(StorageDevice &storage, const std::string &filename)
{
    String lowerFilename(filename.c_str());
    lowerFilename.toLowerCase();
    if (filename.empty() || filename.find("..") != std::string::npos ||
        filename.find('\\') != std::string::npos || !lowerFilename.endsWith(".png"))
    {
        Debug::Log.info(LOG_TFT, "Cannot display PNG: invalid filename");
        return;
    }
    if (!storage.isRunning())
    {
        Debug::Log.info(LOG_TFT, "Cannot display PNG: storage unavailable");
        return;
    }
    size_t size = 0;
    uint8_t *data = storage.readFileAsBinary(filename, size);

    if (data == NULL)
    {
        Debug::Log.info(LOG_TFT, "invalid data");
        return;
    }

    lcd.startWrite();
    lcd.drawPng((uint8_t *)data, size);
    lcd.endWrite();

    free(data);
}

void DisplayDevice::requestPng(const std::string &filename)
{
    const std::lock_guard<std::mutex> lock(imageRequestMutex);
    // One pending preview is enough; newer requests replace the older path.
    requestedImage = filename;
}

void DisplayDevice::setForegroundColor(int color)
{
    foregroundColor = color;
}

void DisplayDevice::setBackgroundColor(int color)
{
    backgroundColor = color;
}

int DisplayDevice::convertStringToColor(const std::string &color)
{
    // Resolve named colors before parsing hexadecimal RGB values.
    const auto &it = colorLookup.find(color);
    if (it != colorLookup.cend())
    {
        return it->second;
    }

    // Accept both prefixed and unprefixed 24-bit RGB values.
    std::string hexStr = color;
    if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
    {
        hexStr = hexStr.substr(2);
    }

    char* end;
    errno = 0;
    const unsigned long value = std::strtoul(hexStr.c_str(), &end, 16);

    if (errno != 0 || end == hexStr.c_str()) {
        return TFT_BLACK;
    }

    return lcd.color24to16(value);
}

void DisplayDevice::setTextSize(const int size)
{
    const float textSize = (float) size / 10.0;
    lcd.setTextSize(textSize);
}

int DisplayDevice::getTextSize()
{
    return  static_cast<int>(std::round(lcd.getTextSizeX() * 10));
}

DisplayDevice::DisplayDevice()
{
}

using DisplayInternal::ScreenMode;
using DisplayInternal::MODE_DASHBOARD;
using DisplayInternal::MODE_LOG_VIEWER;
using DisplayInternal::MODE_PAYLOAD_SELECTOR;
using DisplayInternal::MODE_SCREENSAVER;

bool DisplayInternal::stopFeedbackActive(uint32_t now)
{
    return static_cast<int32_t>(now - stopFeedbackUntil) < 0;
}

/** @brief Set the stop-feedback deadline used by the renderers. */
void DisplayInternal::armStopFeedback(uint32_t now)
{
    stopFeedbackUntil = now + 2500;
}

// States used to animate the startup screen.
static int bootAnimStep = 0;
static unsigned long lastBootAnimTime = 0;

static const char* bootLogText[] = {
    "Rst: 0x1, Boot: 0x8",
    "Loading RedLilyGO...",
    "[ OK ] Storage Mnt",
    "[ OK ] USB Core Init",
    "[ OK ] Agent Ready",
    "[ OK ] WiFi Spawner",
    "Starting UI..."
};

void DisplayDevice::loop(Preferences &prefs)
{
    unsigned long currentMillis = millis();

    const bool activityWakeRequested = wakeRequested.exchange(false);
    if (!manualScreenOff && (activityWakeRequested || (!screensaverEnabled && currentMode == MODE_SCREENSAVER)))
    {
        isScreenOff = false;
        currentMode = MODE_DASHBOARD;
        lastActionTime = currentMillis;
        bootScreenCleared = false;
        bootAnimStep = 7;
        screensaverInitialized = false;
        lastDashboardUpdate = 0;
        powerOn();
    }
    std::string image;
    {
        const std::lock_guard<std::mutex> lock(imageRequestMutex);
        image.swap(requestedImage);
    }
    if (!image.empty())
    {
        displayPng(Devices::Storage, image);
        return;
    }

    // Handle the button gestures used to control the display.
    if (!Attacks::Ducky.isWaitingForButton() && Devices::Button.hasButtonBeenPressed()) {
        lastActionTime = currentMillis; // Restart the display idle timer
        if (currentMode == MODE_SCREENSAVER) {
            currentMode = MODE_DASHBOARD;
            lcd.clear(TFT_BLACK);
            lastDashboardUpdate = 0;
            screensaverInitialized = false;
        } else if (!Devices::Button.wasLastPressLong()) {
            if (currentMode == MODE_PAYLOAD_SELECTOR) {
                // A short press leaves the payload menu and returns to the dashboard.
                currentMode = MODE_DASHBOARD;
                lcd.clear(TFT_BLACK);
                lastDashboardUpdate = 0;
            } else {
                manualScreenOff = !manualScreenOff;
                isScreenOff = static_cast<bool>(manualScreenOff);
                if (isScreenOff) {
                    powerOff();
                    // Turn off the LED immediately when the display is disabled.
                    Devices::LED.changeLEDState(false, 0, 0, 0, 0); 
                } else {
                    powerOn();
                    // Redraw the complete screen after waking from sleep.
                    bootScreenCleared = false;
                    lastDashboardUpdate = 0;
                    bootAnimStep = 7; // skip animation if woken up manually
                }
            }
        } else {
            // Handle a long press.
            if (!isScreenOff && bootAnimStep >= 7) {
                if (currentMode == MODE_PAYLOAD_SELECTOR) {
                    // Run the payload currently highlighted in the menu.
                    if (DisplayInternal::payloadSelectorRunSelected()) {
                        // Leave the payload menu and return to the dashboard.
                        currentMode = MODE_DASHBOARD;
                        lcd.clear(TFT_BLACK);
                    }
                } else {
                    // Switch between the dashboard and recent log view.
                    currentMode = (currentMode == MODE_DASHBOARD) ? MODE_LOG_VIEWER : MODE_DASHBOARD;
                    lcd.clear(TFT_BLACK);
                }
                lastDashboardUpdate = 0; // Force immediate update
            }
        }
        // Mark this button event as handled.
        Devices::Button.resetButtonPressedState();
    }

    // Handle double-click gestures.
    if (Devices::Button.hasButtonBeenDoubleClicked()) {
        lastActionTime = currentMillis; // Restart the display idle timer
        if (!isScreenOff && bootAnimStep >= 7) {
            if (currentMode != MODE_PAYLOAD_SELECTOR) {
                // Open the payload browser.
                currentMode = MODE_PAYLOAD_SELECTOR;
                DisplayInternal::payloadSelectorReset();
                lcd.clear(TFT_BLACK);
                screensaverInitialized = false;
            } else {
                // Move the highlight to the next payload.
                DisplayInternal::payloadSelectorNext();
            }
            lastDashboardUpdate = 0; // Force immediate update
        }
        Devices::Button.resetButtonDoubleClickedState();
    }

    if (isScreenOff) return;

    // Choose the display state from current subsystem activity.
    std::string duckyStatus = Attacks::Ducky.getPayloadRunningStatus();
    bool isDuckyRunning = (duckyStatus == "Running");
    bool isMarauderRunning = false;
#ifndef NO_ESP_MARAUDER
    isMarauderRunning = Attacks::Marauder.isActive();
#endif
    bool isEvilPortalActive = false;
#ifndef NO_ESP_MARAUDER
    isEvilPortalActive = Attacks::Marauder.isEvilPortalActive();
#endif
    bool isAgentConnected = Attacks::Agent.isAgentConnected();
    bool isVncActive = false;
    bool isHidActive = false;
#ifndef NO_WEB
    isVncActive = Comms::Web.isVncActive();
    isHidActive = Comms::Web.isHidActive();
#endif

    // Track which feature was activated most recently, so a combined session
    // (for example Marauder plus HID) shows the newest action on the dashboard.
    static uint32_t featureSequence = 0;
    static uint32_t marauderSeq = 0, duckySeq = 0, vncSeq = 0, hidSeq = 0;
    static bool wasMarauder = false, wasDucky = false, wasVnc = false, wasHid = false;
    if (isMarauderRunning && !wasMarauder) marauderSeq = ++featureSequence;
    if (isDuckyRunning && !wasDucky) duckySeq = ++featureSequence;
    if (isVncActive && !wasVnc) vncSeq = ++featureSequence;
    if (isHidActive && !wasHid) hidSeq = ++featureSequence;
    wasMarauder = isMarauderRunning;
    wasDucky = isDuckyRunning;
    wasVnc = isVncActive;
    wasHid = isHidActive;

    DisplayInternal::FeatureId currentFeature = DisplayInternal::FEATURE_NONE;
    uint32_t newestFeature = 0;
    if (isMarauderRunning && marauderSeq > newestFeature) { newestFeature = marauderSeq; currentFeature = DisplayInternal::FEATURE_MARAUDER; }
    if (isDuckyRunning && duckySeq > newestFeature) { newestFeature = duckySeq; currentFeature = DisplayInternal::FEATURE_DUCKY; }
    if (isVncActive && vncSeq > newestFeature) { newestFeature = vncSeq; currentFeature = DisplayInternal::FEATURE_VNC; }
    if (isHidActive && hidSeq > newestFeature) { newestFeature = hidSeq; currentFeature = DisplayInternal::FEATURE_HID; }

    // Prioritize interactive tasks on TFT dashboard when running alongside Marauder
    if (currentFeature == DisplayInternal::FEATURE_MARAUDER) {
        if (isHidActive) currentFeature = DisplayInternal::FEATURE_HID;
        else if (isDuckyRunning) currentFeature = DisplayInternal::FEATURE_DUCKY;
        else if (isVncActive) currentFeature = DisplayInternal::FEATURE_VNC;
    }

    // Restart the display idle timer while a long-running subsystem task is active.
    if (isDuckyRunning || isMarauderRunning || isVncActive || isHidActive) {
        lastActionTime = currentMillis;
    }

    // Show the screensaver after 60 seconds without user or subsystem activity.
    if (screensaverEnabled && currentMode != MODE_SCREENSAVER && bootAnimStep >= 7 && (currentMillis - lastActionTime > 60000)) {
        currentMode = MODE_SCREENSAVER;
    }

    static unsigned long loopStartTime = 0;
    if (loopStartTime == 0) loopStartTime = currentMillis;

    // Show the startup text before drawing the dashboard.
    if (currentMillis - loopStartTime < 7000 && bootAnimStep < 7) {
        // Leave the setup message visible briefly before starting the animation.
        if (currentMillis - loopStartTime > 2000) {
            if (currentMillis - lastBootAnimTime > 600) {
                lastBootAnimTime = currentMillis;
                
                lcd.startWrite();
                lcd.setTextSize(1);
                lcd.setTextColor(TFT_GREEN, TFT_BLACK);
                
                // Draw the next startup line after clearing the previous one.
                int y_pos = bootAnimStep * 11;
                lcd.fillRect(0, y_pos, DISPLAY_WIDTH, 11, TFT_BLACK);
                lcd.setCursor(2, y_pos + 1);
                lcd.print(bootLogText[bootAnimStep]);
                
                lcd.display();
                lcd.endWrite();
                
                bootAnimStep++;
            }
        }
        return; // Wait until the startup animation has finished.
    }

    if (!bootScreenCleared) {
        lcd.clear(TFT_BLACK);
        bootScreenCleared = true;
        lastDashboardUpdate = 0; // Force immediate update
    }

    const bool stopFeedback = DisplayInternal::stopFeedbackActive(millis());

    // Update the status LED breathing animation on its own cadence.
    DisplayInternal::updateLedBreathing(currentMillis, prefs.getBool("stealth", false),
                                        stopFeedback, isMarauderRunning, isDuckyRunning,
                                        isHidActive, isVncActive, duckyStatus);

    // Redraw the display once per second.
    if (currentMillis - lastDashboardUpdate > 1000) {
        lastDashboardUpdate = currentMillis;

        lcd.startWrite();
        if (currentMode == MODE_DASHBOARD) {
            DisplayInternal::renderDashboardHeader(isAgentConnected);
            DisplayInternal::renderDashboardSystemInfo();
            DisplayInternal::renderDashboardFeatureStatus(currentFeature, stopFeedback,
                                                          isEvilPortalActive, duckyStatus);
        } else if (currentMode == MODE_LOG_VIEWER) {
            DisplayInternal::renderLogViewerFrame();
        } else if (currentMode == MODE_PAYLOAD_SELECTOR) {
            DisplayInternal::renderPayloadSelectorFrame();
        } else if (currentMode == MODE_SCREENSAVER) {
            DisplayInternal::renderScreensaverFrame(currentMillis);
        }
        lcd.display();
        lcd.endWrite();
    }

#ifndef NO_TFT_HEARTBEAT
    if (currentMode == MODE_DASHBOARD) {
        if (currentMillis - previousMillis > 500)
        {
            previousMillis = currentMillis;
            static bool dotVisible = false;
            dotVisible = !dotVisible;

            lcd.startWrite();
            // Draw the small status indicator in the lower-right corner.
            if (dotVisible) {
                lcd.fillCircle(DISPLAY_WIDTH - 6, DISPLAY_HEIGHT - 6, 2, TFT_GREEN);
            } else {
                lcd.fillCircle(DISPLAY_WIDTH - 6, DISPLAY_HEIGHT - 6, 2, TFT_BLACK);
            }
            lcd.endWrite();
        }
    }
#endif
}

void DisplayDevice::showStopFeedback()
{
    manualScreenOff = false;
    isScreenOff = false;
    currentMode = MODE_DASHBOARD;
    bootScreenCleared = false;
    lastDashboardUpdate = 0;
    DisplayInternal::armStopFeedback(millis());
    powerOn();
}

void DisplayDevice::wakeForActivity()
{
    wakeRequested = true;
}

void DisplayDevice::setScreensaverEnabled(bool enabled)
{
    screensaverEnabled = enabled;
}

bool DisplayDevice::isScreensaverEnabled() const
{
    return screensaverEnabled;
}

void DisplayDevice::begin(Preferences &prefs)
{
    registerUserConfigurableSetting(CATEGORY_TFT, TFT_TextSize, RedLilyGOCapability::SettingType::Int16, (int16_t)TFT_TextSize_Default);
    registerUserConfigurableSetting(CATEGORY_TFT, TFT_SCREENSAVER, RedLilyGOCapability::SettingType::Bool, TFT_SCREENSAVER_DEFAULT);
    screensaverEnabled = prefs.getBool(TFT_SCREENSAVER, TFT_SCREENSAVER_DEFAULT);
    const int16_t configuredTextSize = prefs.getShort(TFT_TextSize, TFT_TextSize_Default);
    const int16_t textSizeValue = configuredTextSize < 1 ? TFT_TextSize_Default : configuredTextSize;
    float textSize = (float)textSizeValue / (float)10.0;
    lcd.init();
    lcd.setBrightness(128);
    lcd.clear(backgroundColor);
    lcd.display();
    lcd.setTextSize(textSize);
    if (!displayVariablesRegistered)
    {
        Attacks::Ducky.registerDynamicVariable([this]()
                                               { return std::pair("#_DISPLAY_WIDTH_", std::to_string(DISPLAY_WIDTH)); });
        Attacks::Ducky.registerDynamicVariable([this]()
                                               { return std::pair("#_DISPLAY_HEIGHT_", std::to_string(DISPLAY_HEIGHT)); });
        displayVariablesRegistered = true;
    }
}

void DisplayDevice::showStartupLogo()
{
    const float previousTextSize = getTextSize() / 10.0f;

    lcd.startWrite();
    lcd.clear(TFT_BLACK);
    
    lcd.setTextSize(4);
    
    // Center the logo using its approximate width at the selected text size.
    int y_pos = (DISPLAY_HEIGHT / 2) - 16;
    int x_pos = (DISPLAY_WIDTH / 2) - 72; // Approximate width of the six-character logo at this text size.
    if (x_pos < 0) x_pos = 0;
    
    lcd.setCursor(x_pos, y_pos);
    lcd.setTextColor(TFT_WHITE);
    lcd.print("LilyG");
    
    int cx = lcd.getCursorX();
    
    // Fill the center of the logo letter O.
    // Approximate dimensions of the letter O at text size 4.
    lcd.fillCircle(cx + 11, y_pos + 14, 10, TFT_RED);
    
    lcd.setCursor(cx, y_pos);
    lcd.setTextColor(TFT_WHITE);
    lcd.print("O");
    
    lcd.display();
    lcd.endWrite();

    // Fade the red LED in and out while idle.
    for (int i = 0; i <= 255; i += 5) {
        Devices::LED.changeLEDState(true, 0, 100, 100, i);
        delay(15);
    }
    for (int i = 255; i >= 0; i -= 5) {
        Devices::LED.changeLEDState(true, 0, 100, 100, i);
        delay(15);
    }
    Devices::LED.changeLEDState(false, 0, 0, 0, 0);

    // Allow the host panel to settle before the first normal frame.
    delay(500);
    lcd.startWrite();
    lcd.clear(backgroundColor);
    lcd.setTextSize(previousTextSize);
    lcd.display();
    lcd.endWrite();
}
#endif
