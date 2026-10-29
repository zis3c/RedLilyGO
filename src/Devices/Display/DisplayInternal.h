/**
 * @file DisplayInternal.h
 * @brief Shared state between the display translation units.
 *
 * DisplayDevice.cpp owns the panel, lifecycle and loop coordination.
 * DisplayScreens.cpp owns the per-mode renderers and the LED animation. Both
 * share the panel object and the display state machine through this header.
 */

#pragma once

#ifndef NO_TFT

#include "DisplayDevice.h"

#include <LovyanGFX.hpp>

#include <atomic>
#include <string>
#include <vector>

namespace DisplayInternal
{
  /** @brief Display state machine modes selected by button and idle activity. */
  enum ScreenMode
  {
    MODE_DASHBOARD,        ///< Default status dashboard.
    MODE_LOG_VIEWER,       ///< Recent in-memory log view.
    MODE_PAYLOAD_SELECTOR, ///< SD-backed Ducky payload selection.
    MODE_SCREENSAVER       ///< Idle matrix-style animation.
  };

  /** @brief Feature that currently owns the dashboard status label. */
  enum FeatureId
  {
    FEATURE_NONE,
    FEATURE_MARAUDER,
    FEATURE_DUCKY,
    FEATURE_VNC,
    FEATURE_HID
  };

  // If more board profiles use different ST7735 panels, keep their geometry and
  // offsets in separate LGFX configurations. The controller name alone does not
  // identify a panel's visible area.
  class LGFX_Panel : public lgfx::LGFX_Device
  {
      lgfx::Panel_ST7735S _panel_instance;
      lgfx::Bus_SPI _bus_instance;
      lgfx::Light_PWM _light_instance;

  public:
      LGFX_Panel(void)
      {
          {
              auto cfg = _bus_instance.config();
              cfg.spi_mode = 0;            // Use SPI mode 0 for this display.
              cfg.freq_write = 27000000;   // Write clock for the T-Dongle-S3 panel.
              cfg.freq_read = 16000000;    // Read clock for the T-Dongle-S3 panel.
              cfg.pin_sclk = DISPLAY_SCLK; // Board pin connected to SPI clock.
              cfg.pin_mosi = DISPLAY_MOSI; // Board pin used to send display data.
              cfg.pin_miso = DISPLAY_MISO; // Board pin used for reads, or -1 if unused.
              cfg.pin_dc = DISPLAY_DC;     // Board pin that selects command or display data.

  #ifdef ARDUINO_ARCH_ESP32
              cfg.spi_host = SPI3_HOST;          // SPI2 is reserved for the RGB LED.
              cfg.spi_3wire = true;              // Share MOSI for display reads and writes.
              cfg.use_lock = true;              // Serialize access to the shared SPI bus.
              cfg.dma_channel = SPI_DMA_CH_AUTO; // Let ESP32 select a compatible DMA channel.
  #endif
              _bus_instance.config(cfg);              // Apply the SPI pin and clock settings.
              _panel_instance.setBus(&_bus_instance); // Connect the configured SPI bus to the panel.
          }
          {
              auto cfg = _panel_instance.config(); // Start from the panel driver default settings.
              cfg.pin_cs = DISPLAY_CS;             // Board pin used for chip select, or -1 if unused.
              cfg.pin_rst = DISPLAY_RST;           // Board pin used to reset the panel, or -1 if unused.
              cfg.pin_busy = DISPLAY_BUSY;         // Board pin used for busy status, or -1 if unused.
              cfg.panel_width = DISPLAY_HEIGHT;    // Rotation swaps the panel width and height.
              cfg.panel_height = DISPLAY_WIDTH;    // Rotation swaps the panel width and height.
              cfg.offset_rotation = 1;             // Apply the panel orientation offset.
              cfg.readable = true;                 // The panel supports pixel reads.
              cfg.invert = true;
              cfg.rgb_order = false;
              cfg.dlen_16bit = false; // Pixel data lengths use the default 8-bit format.
              cfg.bus_shared = true;  // The SD card and display share this SPI bus.

              cfg.offset_x = 26;        // Align the visible area within controller memory.
              cfg.offset_y = 1;         // Align the visible area within controller memory.
              cfg.dummy_read_pixel = 8; // Discard leading bits before pixel data on reads.
              cfg.dummy_read_bits = 1;  // Discard the controller leading status bit.
              cfg.memory_width = 132;  // Controller memory width, including hidden columns.
              cfg.memory_height = 160; // Controller memory height, including hidden rows.
              _panel_instance.config(cfg);
          }
          {
              auto cfg = _light_instance.config();

              cfg.pin_bl = DISPLAY_LEDA; // Board pin connected to the backlight.
              cfg.invert = true;         // Invert PWM polarity for this panel.
              cfg.freq = 12000;          // Backlight PWM frequency in hertz.
              cfg.pwm_channel = 7;       // PWM channel assigned to the backlight.

              _light_instance.config(cfg);
              _panel_instance.setLight(&_light_instance);
          }
          setPanel(&_panel_instance);
      }
  };

  extern LGFX_Panel lcd;

  // State machine shared across the display loop and the renderers.
  extern std::atomic<bool> isScreenOff;
  extern std::atomic<bool> manualScreenOff;
  extern ScreenMode currentMode;
  extern bool screensaverEnabledState;
  extern bool bootScreenCleared;
  extern bool screensaverInitialized;
  extern unsigned long lastDashboardUpdate;
  extern unsigned long lastMatrixUpdate;
  extern unsigned long previousMillis;
  extern int drops[27];
  extern std::atomic<bool> screensaverEnabledAtomic;

  /** @brief Return true while the emergency stop-feedback banner is active. */
  bool stopFeedbackActive(uint32_t now);
  /** @brief Arm the stop-feedback banner deadline. */
  void armStopFeedback(uint32_t now);

  /** @brief Mark the payload selector list stale and reset the highlight. */
  void payloadSelectorReset();
  /** @brief Advance the payload highlight, wrapping at the end. */
  void payloadSelectorNext();
  /** @brief Run the highlighted payload, if any. Returns true when queued. */
  bool payloadSelectorRunSelected();

  /** @brief Draw the dashboard header and agent icon. */
  void renderDashboardHeader(bool isAgentConnected);
  /** @brief Draw the USB, SD and RAM rows on the left of the dashboard. */
  void renderDashboardSystemInfo();
  /** @brief Draw the feature-status box on the right of the dashboard. */
  void renderDashboardFeatureStatus(FeatureId currentFeature, bool isStopFeedback,
                                    bool isEvilPortalActive, const std::string &duckyStatus);
  /** @brief Render the recent-log screen. */
  void renderLogViewerFrame();
  /** @brief Render the payload selector screen and refresh its list if needed. */
  void renderPayloadSelectorFrame();
  /** @brief Advance and draw the screensaver animation. */
  void renderScreensaverFrame(unsigned long currentMillis);
  /** @brief Update the status LED breathing animation if its interval elapsed. */
  void updateLedBreathing(unsigned long currentMillis, bool stealth, bool isStopFeedback,
                          bool isMarauderRunning, bool isDuckyRunning, bool isHidActive,
                          bool isVncActive, const std::string &duckyStatus);
} // namespace DisplayInternal

#endif
