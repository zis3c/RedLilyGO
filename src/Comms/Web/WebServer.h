/** @file WebServer.h @brief Embedded HTTP/WebSocket service interface. */

#pragma once

#include "../../RedLilyGOCapability.h"
#include <atomic>

class WebSite : RedLilyGOCapability {
public:
  /** @brief Construct web service in stopped state. */
  WebSite();

  /** @brief Service asynchronous web and WebSocket state. */
  virtual void loop(Preferences& prefs);
  /** @brief Register routes and start web service. */
  virtual void begin(Preferences& prefs);
  /** @brief Stop web service and release listeners. */
  virtual void end();
  /** @brief Report whether web service is listening. */
  bool isRunning() const { return running.load(std::memory_order_acquire); }
  /** @brief Report whether a VNC client currently owns the remote bridge. */
  bool isVncActive() const;
  /** @brief Report whether a browser currently owns the HID input channel. */
  bool isHidActive() const;
private:
  std::atomic<bool> running{false};
};

namespace Comms
{
    extern WebSite Web;
}
