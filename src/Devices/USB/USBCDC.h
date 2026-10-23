/**
 * @file USBCDC.h
 * @brief USB CDC command framing and debug transport interface.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

#include "../../Devices/USB/USBCore.h"
#include "../../RedLilyGOCapability.h"

enum HostCommand : uint8_t
{
    MIN = 0,                 ///< Reserved lower bound, not a command.
    Execute = 1,             ///< Execute a host-requested action.
    DebugMsg = 2,            ///< Send debug text to host.
    WSCONNECT = 3,           ///< WebSocket connection notification.
    WSDATA = 4,               ///< WebSocket data sent to host.
    WSDISCONNECT = 5,        ///< WebSocket disconnect notification.
    WSDATARECV = 6,          ///< WebSocket data received from host.
    RequestAgentStatus = 7,  ///< Request agent status from firmware.
    AgentStatus = 8,         ///< Agent status response.
    ExecuteResult = 9,       ///< Action result response.
    MicPcmData = 10,         ///< Microphone PCM frame.
    MAX = 11                 ///< Exclusive upper bound, not a command.
};

class USBCDCWrapper : RedLilyGOCapability {
public:
  /** @brief Construct CDC transport in stopped state. */
  USBCDCWrapper();

  /**
   * @brief Start serial transport at requested baud rate.
   * @param[in] baud Serial baud rate; ignored when USB mode is not Serial.
   */
  void begin(const unsigned long& baud);
  /** @brief Read and dispatch at most available framed CDC input. */
  virtual void loop(Preferences& prefs);
  /** @brief Load CDC settings and start transport at default baud. */
  virtual void begin(Preferences& prefs);
  /** @brief Stop serial transport and discard any partial frame. */
  virtual void end();
  /** @brief Report whether CDC framing can currently be used. */
  bool isReady() const;
  /**
   * @brief Register callback for one host command.
   * @param[in] tag Command identifier in (MIN, MAX).
   * @param[in] callback Handler receiving payload bytes and payload length.
   */
  void setCallback(const HostCommand&, std::function<void(uint8_t*, const size_t)>);
  /**
   * @brief Queue one bounded binary command frame; drops new frames if full.
   * @param[in] tag Command identifier in (MIN, MAX).
   * @param[in] buffer Payload; may be null only when size is zero.
   * @param[in] size Payload length, at most 2048 bytes.
   */
  void writeBinary(const HostCommand&, const uint8_t *, const size_t&);
  /** @brief Write debug text through raw or framed CDC transport. */
  void writeDebugString(const std::string &msg);
};

namespace Devices::USB
{
    extern USBCDCWrapper CDC;
}
