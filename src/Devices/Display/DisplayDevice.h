/**
 * @file DisplayDevice.h
 * @brief ST7735 display lifecycle and rendering interface.
 *
 * The active profile uses LovyanGFX over SPI and shares the bus with storage.
 * Drawing calls run from the Arduino loop task. Other tasks may queue wake
 * and image requests or update the atomic screensaver setting.
 */

#pragma once

#include "string"
#include <atomic>

#include "../../RedLilyGOCapability.h"
#include "../Storage/StorageDevice.h"

class DisplayDevice : RedLilyGOCapability {
public:
  /** @brief Construct display adapter with default colours and text scale. */
  DisplayDevice();

  /** @brief Refresh dashboard, log, payload, or screensaver state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize panel, backlight, and display variables. */
  virtual void begin(Preferences& prefs);
  /** @brief Decode and draw a validated PNG stored on filesystem. */
  void displayPng(StorageDevice& storage, const std::string& filename);
  /** @brief Queue a PNG preview from another task for the next display loop. */
  void requestPng(const std::string& filename);
  /** @brief Draw text at pixel coordinates using current colours. */
  void display(const int &x, const int &y, const std::string &str);
  /** @brief Set foreground RGB565 colour. */
  void setForegroundColor(int color);
  /** @brief Set background RGB565 colour. */
  void setBackgroundColor(int color);
  /** @brief Set display text scale in tenths. */
  void setTextSize(int size);
  /** @brief Return current display text scale in tenths. */
  int getTextSize();
  /** @brief Convert named or RGB888 hexadecimal colour to RGB565. */
  int convertStringToColor(const std::string& color);
  /** @brief Clear panel using current background colour. */
  void clearScreen();
  /** @brief Put panel into low-power sleep and disable backlight. */
  void powerOff();
  /** @brief Wake panel and restore backlight. */
  void powerOn();
  /** @brief Draw startup logo and LED fade sequence. */
  void showStartupLogo();
  /** @brief Show an emergency thermal warning before powering down. */
  void showThermalWarning();
  /** @brief Wake screen and show emergency stop feedback. */
  void showStopFeedback();
  /** @brief Request wake and live activity status on the next display loop. */
  void wakeForActivity();
  /** @brief Enable or disable automatic matrix screensaver. */
  void setScreensaverEnabled(bool enabled);
  /** @brief Report automatic matrix screensaver setting. */
  bool isScreensaverEnabled() const;
private:
  std::atomic<bool> screensaverEnabled{true};
  std::atomic<bool> wakeRequested{false};
};

namespace Devices
{
    extern DisplayDevice Display;
};
