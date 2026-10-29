/**
 * @file USBCore.h
 * @brief Public USB mode, descriptor, and interface lifecycle API.
 *
 * Targets TinyUSB-capable MCUs and coordinates USB CDC, NCM, HID, and MSC
 * interfaces from the Arduino control loop.
 */

#pragma once

#include <DuckyParse.h>
#include <cstdint>

#include "../../RedLilyGOCapability.h"

enum class USBDeviceType : uint16_t
{
  None = 0,   ///< USB disconnected from host bus.
  Serial = 1, ///< USB serial device mode.
  NCM = 2     ///< USB network-control model mode.
};

enum class USBClassType : uint16_t
{
  None = 0,    ///< No secondary USB class enabled.
  HID = 1,     ///< Human Interface Device class.
  Storage = 2  ///< Mass-storage class backed by device storage.
};

class USBCore : RedLilyGOCapability
{
public:
  /** @brief Construct core with default serial and HID state. */
  USBCore();

  virtual void loop(Preferences& prefs);
  virtual void begin(Preferences &prefs);
  virtual void end();

  /** @brief Return current USB device mode. */
  USBDeviceType currentDeviceType() { return curDeviceType; }
  /** @brief Return current secondary USB class. */
  USBClassType currentClassType() { return curClassType; }
  /** @brief Return active USB vendor ID. */
  uint16_t getVID() { return vid; }
  /** @brief Return active USB product ID. */
  uint16_t getPID() { return pid; }


  void changeUSBMode(DuckyInterpreter::USB_MODE &mode, const uint16_t &vid, const uint16_t &pid, const std::string &man, const std::string &prod, const std::string &serial);
  void reset();
  
  std::string getCurrentUSBMode()
  {
    if (curDeviceType == USBDeviceType::None)
    {
      return "None";
    }
    else
    {
      std::string ret = curDeviceType == USBDeviceType::Serial ? "Serial" : "NCM";

      if (curClassType != USBClassType::None)
      {
        ret += curClassType == USBClassType::HID ? " + HID" : " + Storage";
      }
      return ret;
    }
  }
private:
  USBDeviceType curDeviceType = USBDeviceType::Serial;
  USBClassType curClassType = USBClassType::HID;
  uint16_t vid = 0;
  uint16_t pid = 0;
  std::string manufacturer = "";
  std::string product = "";
  std::string serialDescriptor = "";
};

namespace Devices::USB
{
  extern USBCore Core;
}
