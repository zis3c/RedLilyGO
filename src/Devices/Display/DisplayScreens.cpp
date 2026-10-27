/**
 * @file DisplayScreens.cpp
 * @brief Display renderers and LED animation split out of DisplayDevice.cpp.
 *
 * Owns the dashboard, log-viewer, payload-selector and screensaver rendering,
 * plus the status-LED breathing animation. Shares the panel and state machine
 * with DisplayDevice.cpp through DisplayInternal.h.
 */

#ifndef NO_TFT
#include "DisplayInternal.h"

#include "Diagnostics/Logging.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "../LED/LEDDevice.h"
#include "../USB/USBCore.h"
#ifndef NO_ESP_MARAUDER
#include "../../Attacks/Marauder/Marauder.h"
#endif

#include <algorithm>

using namespace DisplayInternal;

// LED breathing state; only this unit mutates it.
static int breathBrightness = 10;
static int breathDirection = 1;
static unsigned long lastLedUpdate = 0;

// Payload selector list state; only this unit mutates it.
static std::vector<std::string> payloadList;
static int selectedPayloadIdx = 0;
static bool payloadListNeedsRefresh = true;

void DisplayInternal::payloadSelectorReset()
{
  payloadListNeedsRefresh = true;
  selectedPayloadIdx = 0;
}

void DisplayInternal::payloadSelectorNext()
{
  if (payloadList.empty()) return;
  selectedPayloadIdx++;
  if (selectedPayloadIdx >= payloadList.size()) selectedPayloadIdx = 0;
}

bool DisplayInternal::payloadSelectorRunSelected()
{
  if (payloadList.empty() || selectedPayloadIdx >= payloadList.size()) return false;
  Attacks::Ducky.setPayload(payloadList[selectedPayloadIdx]);
  return true;
}

void DisplayInternal::renderDashboardHeader(bool isAgentConnected)
{
  // Draw the header background.
  lcd.fillRect(0, 0, DISPLAY_WIDTH, 14, lcd.color565(180, 0, 0));

  lcd.setTextSize(1);
  lcd.setTextColor(TFT_WHITE, lcd.color565(180, 0, 0));
  lcd.setCursor(4, 3);
  lcd.print("REDLILYGO");

  // Agent connection icon appears only while the host Agent is connected.
  if (isAgentConnected)
  {
    const int ax = 135;
    const int ay = 7;
    lcd.fillCircle(ax - 4, ay, 2, TFT_GREEN);
    lcd.fillCircle(ax + 4, ay, 2, TFT_GREEN);
    lcd.drawLine(ax - 4, ay, ax + 4, ay, TFT_GREEN);
  }

  // Clear the area that changes between display updates.
  lcd.fillRect(0, 15, DISPLAY_WIDTH, DISPLAY_HEIGHT - 15, TFT_BLACK);
}

void DisplayInternal::renderDashboardSystemInfo()
{
  // System information on the left side of the dashboard.
  lcd.setTextColor(TFT_DARKGREY);
  lcd.setCursor(5, 20);
  lcd.print("USB");
  lcd.setTextColor(TFT_WHITE);
  lcd.setCursor(26, 20);

  std::string usbMode = "OFF";
  if (Devices::USB::Core.currentDeviceType() == USBDeviceType::Serial)
    usbMode = Devices::USB::Core.currentClassType() == USBClassType::HID ? "SER+HID" : "SER";
  else if (Devices::USB::Core.currentDeviceType() == USBDeviceType::NCM)
    usbMode = Devices::USB::Core.currentClassType() == USBClassType::HID ? "NCM+HID" : "NCM";
  lcd.print(usbMode.c_str());

  // Show SD card usage and its progress bar.
  uint64_t sdUsedBytes = 0;
  uint64_t sdTotalBytes = 0;
  const bool sdMounted = Devices::Storage.isRunning();
  const bool sdUsageValid = Devices::Storage.getUsage(sdUsedBytes, sdTotalBytes);
  lcd.setTextColor(TFT_DARKGREY);
  lcd.setCursor(5, 32);
  lcd.print("SD");

  if (sdMounted)
  {
    lcd.setTextColor(TFT_WHITE);
    lcd.setCursor(20, 32);
    if (sdUsageValid)
    {
      const uint8_t sdPercent = static_cast<uint8_t>((sdUsedBytes * 100ULL) / sdTotalBytes);
      lcd.print(sdPercent > 100 ? 100 : sdPercent);
      lcd.print("%");
    }
    else
    {
      lcd.print("--");
    }

    // Render the storage usage bar.
    lcd.drawRect(42, 32, 40, 7, TFT_WHITE);
    if (sdUsageValid && sdUsedBytes > 0)
    {
      const int fillW = std::min(38, std::max(1,
        static_cast<int>((static_cast<double>(sdUsedBytes) * 38.0) / sdTotalBytes)));
      lcd.fillRect(43, 33, fillW, 5, TFT_RED);
    }
  }
  else
  {
    lcd.setTextColor(TFT_RED);
    lcd.setCursor(25, 32);
    lcd.print("ERR");
  }

  lcd.setTextColor(TFT_DARKGREY);
  lcd.setCursor(5, 44);
  lcd.print("RAM");
  lcd.setTextColor(TFT_WHITE);
  lcd.setCursor(26, 44);
  lcd.print(ESP.getFreeHeap() / 1024);
  lcd.print("KB");
}

void DisplayInternal::renderDashboardFeatureStatus(bool isStopFeedback, bool isMarauderRunning,
                                                   bool isEvilPortalActive, bool isDuckyRunning,
                                                   bool isHidActive, bool isVncActive,
                                                   const std::string &duckyStatus)
{
  // Current feature status on the right side of the dashboard.
  int activeColor = TFT_WHITE;
  std::string activeTitle = "IDLE";
  std::string bottomText = "> SYSTEM READY";
  int bottomColor = TFT_DARKGREEN;

  if (isStopFeedback)
  {
    activeTitle = "STOP";
    activeColor = TFT_GREEN;
    bottomText = "> ALL FEATURES STOPPED";
    bottomColor = TFT_GREEN;
  }
  else if (isMarauderRunning)
  {
    activeTitle = isEvilPortalActive ? "PORTAL" : "MRD";
    activeColor = TFT_ORANGE;
    if (isEvilPortalActive)
    {
      bottomText = "> PORTAL: 4.3.2.1";
    }
#ifndef NO_ESP_MARAUDER
    if (!isEvilPortalActive)
    {
      uint32_t deauths = Attacks::Marauder.getDeauthCount();
      if (deauths > 0)
      {
        bottomText = "> DEAUTH: " + std::to_string(deauths);
      }
      else
      {
        bottomText = "> SNIFF: " + std::to_string(Attacks::Marauder.getPacketCount()) + " PKT";
      }
    }
#else
    if (!isEvilPortalActive) bottomText = "> MARAUDER ACTIVE...";
#endif
    bottomColor = TFT_ORANGE;
  }
  else if (isDuckyRunning)
  {
    activeTitle = "DUCKY";
    activeColor = TFT_MAGENTA;
    bottomText = "> EXECUTING PAYLOAD...";
    bottomColor = TFT_MAGENTA;
  }
  else if (duckyStatus == "Error")
  {
    activeTitle = "ERR";
    activeColor = TFT_WHITE;
    bottomText = "> PAYLOAD ERROR";
    bottomColor = TFT_WHITE;
  }
  else if (isVncActive)
  {
    activeTitle = "VNC";
    activeColor = TFT_CYAN;
    bottomText = "> REMOTE DESKTOP";
    bottomColor = TFT_CYAN;
  }
  else if (isHidActive)
  {
    activeTitle = "HID";
    activeColor = TFT_YELLOW;
    bottomText = "> USB INPUT ACTIVE";
    bottomColor = TFT_YELLOW;
  }

  // Draw the target icon used for active assessment state.
  int cx = 100; int cy = 25;
  lcd.drawCircle(cx, cy, 4, TFT_DARKGREY);
  lcd.drawLine(cx - 6, cy, cx + 6, cy, TFT_DARKGREY);
  lcd.drawLine(cx, cy - 6, cx, cy + 6, TFT_DARKGREY);

  lcd.setTextColor(activeColor);
  lcd.setCursor(112, 21);
  lcd.print(activeTitle.c_str());

  // Frame the feature status and target icon.
  lcd.drawRoundRect(90, 16, 65, 18, 3, TFT_DARKGREY);

  // Draw the footer status and connection details.
  lcd.drawFastHLine(0, 68, DISPLAY_WIDTH, TFT_DARKGREY);
  lcd.setTextColor(bottomColor);
  lcd.setCursor(5, 71);
  lcd.print(bottomText.c_str());
}

void DisplayInternal::renderLogViewerFrame()
{
  // Render the recent log view.
  lcd.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, TFT_BLACK);

  // Header
  lcd.fillRect(0, 0, DISPLAY_WIDTH, 12, TFT_DARKGREY);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
  lcd.setCursor(2, 2);
  lcd.print("SYSTEM LOG (LIVE)");

  // Fetch and display logs (snapshot copy: the live log vector is
  // mutated by the async web task while this task renders)
  static std::vector<std::string> logSnapshot;
  static unsigned long logSnapshotAt = 0;
  const unsigned long now = millis();
  if (now - logSnapshotAt >= 500 || logSnapshotAt == 0)
  {
    logSnapshot = Debug::Log.getLogs();
    logSnapshotAt = now;
  }
  const auto &logs = logSnapshot;
  int logCount = logs.size();
  int startIdx = (logCount > 7) ? logCount - 7 : 0;

  lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  for (int i = 0; i < 7 && (startIdx + i) < logCount; i++)
  {
    std::string logLine = logs[startIdx + i];
    // Truncate to screen width (approx 26 chars at size 1)
    if (logLine.length() > 26)
    {
      logLine = logLine.substr(0, 24) + "..";
    }
    lcd.setCursor(2, 14 + (i * 9));
    lcd.print(logLine.c_str());
  }
  if (logCount == 0)
  {
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.setCursor(2, 20);
    lcd.print("No logs yet...");
  }
}

void DisplayInternal::renderPayloadSelectorFrame()
{
  // Render the payload browser.
  lcd.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, TFT_BLACK);

  // Header
  lcd.fillRect(0, 0, DISPLAY_WIDTH, 12, TFT_ORANGE);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_BLACK, TFT_ORANGE);
  lcd.setCursor(2, 2);
  lcd.print("PAYLOAD SELECTOR");

  // Load the SD file list only when the browser needs it.
  if (payloadListNeedsRefresh && Devices::Storage.isRunning())
  {
    payloadList.clear();
    std::vector<std::string> files = Devices::Storage.listFiles();
    for (auto &file : files)
    {
      if (file.find(".ds") != std::string::npos || file.find(".txt") != std::string::npos)
      {
        payloadList.push_back(file);
      }
    }
    if (payloadList.empty() || selectedPayloadIdx >= payloadList.size())
      selectedPayloadIdx = 0;
    payloadListNeedsRefresh = false;
  }

  if (!Devices::Storage.isRunning())
  {
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.setCursor(2, 20);
    lcd.print("SD CARD ERROR!");
  }
  else if (payloadList.empty())
  {
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.setCursor(2, 20);
    lcd.print("No .ds scripts found.");
  }
  else
  {
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.setCursor(2, 16);
    lcd.print("> Selected File:");

    std::string selFile = payloadList[selectedPayloadIdx];
    // Remove the leading slash from the path shown on the small display.
    if (selFile.length() > 0 && selFile[0] == '/')
    {
      selFile = selFile.substr(1);
    }

    // Draw the selected payload with its highlight color.
    lcd.fillRect(0, 26, DISPLAY_WIDTH, 12, lcd.color565(50, 50, 50));
    lcd.setTextColor(TFT_WHITE, lcd.color565(50, 50, 50));
    lcd.setCursor(5, 28);
    lcd.print(selFile.c_str());

    // Show the controls for browsing and selecting payloads.
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.setCursor(2, 45);
    lcd.print("2X Click: Next");
    lcd.setCursor(2, 55);
    lcd.print("Hold:     Execute");
    lcd.setCursor(2, 65);
    lcd.print("1X Click: Cancel");
  }
}

void DisplayInternal::renderScreensaverFrame(unsigned long currentMillis)
{
  if (!screensaverInitialized)
  {
    lcd.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, TFT_BLACK);
    for (int i = 0; i < 27; i++)
    {
      drops[i] = -(rand() % 10);
    }
    screensaverInitialized = true;
  }

  if (currentMillis - lastMatrixUpdate > 50)
  {
    lastMatrixUpdate = currentMillis;
    lcd.setTextSize(1);
    for (int i = 0; i < 26; i++)
    { // One column per 6-pixel-wide character.
      // Clear the previous character at this position.
      if (drops[i] > 5)
      {
        lcd.fillRect(i * 6, (drops[i] - 6) * 8, 6, 8, TFT_BLACK);
      }

      // Draw the next character in the scan line.
      if (drops[i] >= 0 && drops[i] < (DISPLAY_HEIGHT / 8) + 1)
      {
        char c = 33 + (rand() % 94); // Choose a printable character for this frame.
        lcd.setTextColor(lcd.color565(0, 255, 0), TFT_BLACK);
        lcd.setCursor(i * 6, drops[i] * 8);
        lcd.print(c);

        // Keep the animated characters green against the dark background.
      }

      drops[i]++;
      // Restart a character after it leaves the display.
      if (drops[i] * 8 > DISPLAY_HEIGHT && (rand() % 10 > 7))
      {
        drops[i] = 0;
      }
    }
  }
}

void DisplayInternal::updateLedBreathing(unsigned long currentMillis, bool stealth, bool isStopFeedback,
                                         bool isMarauderRunning, bool isDuckyRunning, bool isHidActive,
                                         bool isVncActive, const std::string &duckyStatus)
{
  // Update the status LED breathing animation every 30 ms.
  if (currentMillis - lastLedUpdate <= 30) return;
  lastLedUpdate = currentMillis;
  if (stealth) return;

  // Select a status color for the current device state.
  uint16_t hue = 0; // Status hue uses degrees.
  uint8_t saturation = 255;
  if (isStopFeedback)
  {
    hue = 120; // Stopped: green.
  }
  else if (isMarauderRunning)
  {
    hue = 30; // Marauder activity: orange.
  }
  else if (isDuckyRunning)
  {
    hue = 300; // Ducky payload activity: magenta.
  }
  else if (duckyStatus == "Error")
  {
    saturation = 0; // Error state: white.
  }
  else if (isVncActive)
  {
    hue = 180; // Remote desktop activity: cyan.
  }
  else if (isHidActive)
  {
    hue = 60; // USB HID activity: yellow.
  }

  breathBrightness += breathDirection > 0 ? 5 : -10;
  if (breathBrightness >= 255)
  {
    breathBrightness = 255;
    breathDirection = -1;
  }
  else if (breathBrightness <= 10)
  {
    breathBrightness = 10;
    breathDirection = 1;
  }

  Devices::LED.changeLEDState(true, hue, saturation, 128, breathBrightness);
}

#endif
