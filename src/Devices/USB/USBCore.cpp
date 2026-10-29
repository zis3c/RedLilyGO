/**
 * @file USBCore.cpp
 * @brief USB mode and class lifecycle coordinator.
 *
 * Configures TinyUSB descriptors and dispatches CDC, NCM, HID, and MSC
 * services. USB callbacks may run in TinyUSB context; public lifecycle calls
 * run from the Arduino loop task and must not overlap a mode change.
 */

#include "USBCore.h"

#include "USBMSC.h"

#include <Adafruit_TinyUSB.h>

#include "USBNCM.h"
#include "USBCDC.h"
#include "USBHID.h"

#include "Diagnostics/Logging.h"
#define LOG_USB "USB"

#define USB_DeviceType "usbDeviceType"
#define USB_DeviceType_Default (uint16_t) USBDeviceType::Serial

#define USB_ClassType "usbClassType"
#define USB_ClassType_Default (uint16_t) USBClassType::HID

#define USB_DeviceVID "usbDeviceVID"
#define USB_DeviceVID_Default (uint16_t)0xcafe

#define USB_DevicePID "usbDevicePID"
#define USB_DevicePID_Default (uint16_t)0x403f

#define USB_Version "usbVersion"
#define USB_Version_Default (uint16_t)0x0101

#define USB_DeviceVersion "usbDevVersion"
#define USB_DeviceVersion_Default (uint16_t)0x0101

#define USB_DeviceManufacturer "usbDevMfr"
#define USB_DeviceManufacturer_Default "zis3c"

#define USB_DeviceProductDescriptor "usbDevProdDesc"
#define USB_DeviceProductDescriptor_Default "RedLilyGO"

std::string curUSBMode = "NONE";

extern Preferences prefs;

namespace Devices::USB
{
  USBCore Core;
  // devices
  USBCDCWrapper CDC;
  USBNCM NCM;

  // classes
  USBHID HID;
  USBMSC MSC;
}

USBCore::USBCore()
    : curDeviceType(USBDeviceType::None),
      curClassType(USBClassType::None)
{
}

/**
 * @brief Switch USB device mode and optional USB class combination.
 * @param[in] mode Requested Ducky USB mode flags.
 * @param[in] vidValue Vendor ID override, or zero to use Preferences.
 * @param[in] pidValue Product ID override, or zero to use Preferences.
 * @param[in] man Manufacturer descriptor; may be empty.
 * @param[in] prod Product descriptor; may be empty.
 * @param[in] serial Serial descriptor; may be empty.
 * @return None. Invalid class combinations are reduced to supported state.
 * @note Disconnects or reconfigures TinyUSB and may block while interfaces
 *       initialize. Call from the main control task only.
 */
void USBCore::changeUSBMode(DuckyInterpreter::USB_MODE &mode, const uint16_t &vidValue, const uint16_t &pidValue, const std::string &man, const std::string &prod, const std::string &serial)
{
  // Configure a USB device type at boot, or keep USB disabled until a payload
  // selects its requested mode.

  if (curDeviceType != USBDeviceType::None)
  {
    Debug::Log.warning(LOG_USB, "Device started with USB Device type NOT set to None, this is recommended when using ATTACKMODE command");
  }

  auto startingMode = curDeviceType;

  if (mode == DuckyInterpreter::USB_MODE::OFF)
  {
    Debug::Log.info(LOG_USB, "Changing USB mode to OFF");
    end();
    curDeviceType = USBDeviceType::None;
    curClassType = USBClassType::None;
    return;
  }
  const bool hidRequested = (mode & DuckyInterpreter::USB_MODE::HID) != 0;
  if (hidRequested)
  {
    Debug::Log.info(LOG_USB, "Changing USB mode to HID");
    curDeviceType = USBDeviceType::Serial;
    curClassType = USBClassType::HID;
  }
  if (mode & DuckyInterpreter::USB_MODE::STORAGE)
  {
    Debug::Log.info(LOG_USB, "Changing USB mode to STORAGE");
    curDeviceType = USBDeviceType::Serial;
    curClassType = USBClassType::Storage;
  }

  Preferences prefs;
  prefs.begin("usbarmyknife");

  // These begin() calls are safe when a USB mode change reuses the interfaces.
  Devices::USB::CDC.begin(115200);
  Devices::USB::NCM.begin(prefs);
  Devices::USB::HID.begin(prefs);
  Devices::USB::MSC.begin(prefs);

  if (startingMode == USBDeviceType::None && mode != DuckyInterpreter::USB_MODE::OFF)
  {
    vid = vidValue == 0 ? prefs.getUShort(USB_DeviceVID, USB_DeviceVID_Default) : vidValue;
    pid = pidValue == 0 ? prefs.getUShort(USB_DevicePID, USB_DevicePID_Default) : pidValue;

    manufacturer = man;
    product = prod;
    serialDescriptor = serial;

    // Apply the configured USB identity before the device reconnects.
    TinyUSBDevice.setID(vid, pid);
    TinyUSBDevice.setManufacturerDescriptor(manufacturer.c_str());
    TinyUSBDevice.setProductDescriptor(product.c_str());
    TinyUSBDevice.setSerialDescriptor(serialDescriptor.c_str());
  }

  if (mode & DuckyInterpreter::USB_MODE::STORAGE)
  {
    if (!Devices::USB::MSC.mountSD())
    {
      // HID may already be active in a combined ATTACKMODE request. A
      // failed SD mount must not tear down the working HID interface.
      curClassType = hidRequested ? USBClassType::HID : USBClassType::None;
    }
  }
}

/**
 * @brief Load USB settings and initialize supported TinyUSB interfaces.
 * @param[in,out] prefs Persistent USB configuration store.
 * @return None. Unsupported persisted enum values fall back to None.
 * @note Must run before USB traffic is enabled. Mount wait can last up to
 *       one second while the host enumerates the device.
 */
void USBCore::begin(Preferences &prefs)
{
  registerUserConfigurableSetting(CATEGORY_USB, USB_DeviceType, RedLilyGOCapability::SettingType::UInt16, USB_DeviceType_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_ClassType, RedLilyGOCapability::SettingType::UInt16, USB_ClassType_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_DeviceVID, RedLilyGOCapability::SettingType::UInt16, USB_DeviceVID_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_DevicePID, RedLilyGOCapability::SettingType::UInt16, USB_DevicePID_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_Version, RedLilyGOCapability::SettingType::UInt16, USB_Version_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_DeviceVersion, RedLilyGOCapability::SettingType::UInt16, USB_DeviceVersion_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_DeviceManufacturer, RedLilyGOCapability::SettingType::String, USB_DeviceManufacturer_Default);
  registerUserConfigurableSetting(CATEGORY_USB, USB_DeviceProductDescriptor, RedLilyGOCapability::SettingType::String, USB_DeviceProductDescriptor_Default);

  curDeviceType = (USBDeviceType)prefs.getUShort(USB_DeviceType, USB_DeviceType_Default);
  curClassType = (USBClassType)prefs.getUShort(USB_ClassType, USB_ClassType_Default);

  if (curDeviceType != USBDeviceType::None && curDeviceType != USBDeviceType::Serial && curDeviceType != USBDeviceType::NCM)
    curDeviceType = USBDeviceType::None;
  if (curClassType != USBClassType::None && curClassType != USBClassType::HID && curClassType != USBClassType::Storage)
    curClassType = USBClassType::None;

  // This firmware does not combine NCM with HID or mass storage, so clear the
  // class selection when NCM is the requested device type.
  if (curDeviceType == USBDeviceType::NCM)
  {
    curClassType = USBClassType::None;
  }

  vid = prefs.getUShort(USB_DeviceVID, USB_DeviceVID_Default);
  pid = prefs.getUShort(USB_DevicePID, USB_DevicePID_Default);

  Devices::USB::CDC.begin(115200);
  Devices::USB::NCM.begin(prefs);
  Devices::USB::HID.begin(prefs);
  Devices::USB::MSC.begin(prefs);

  // Set descriptors after interface initialization so TinyUSB does not replace
  // them with its defaults.
  if (curDeviceType != USBDeviceType::None)
  {
    TinyUSBDevice.setID(vid, pid); // USB_PID

    const uint16_t version = prefs.getUShort(USB_Version, USB_Version_Default);
    TinyUSBDevice.setVersion(version);

    const uint16_t deviceVersion = prefs.getUShort(USB_DeviceVersion, USB_DeviceVersion_Default);
    TinyUSBDevice.setDeviceVersion(deviceVersion);

    const String manufacturerArdString = prefs.getString(USB_DeviceManufacturer, USB_DeviceManufacturer_Default);
    manufacturer = std::string(manufacturerArdString.c_str());
    TinyUSBDevice.setManufacturerDescriptor(manufacturer.c_str());

    const String productArdString = prefs.getString(USB_DeviceProductDescriptor, USB_DeviceProductDescriptor_Default);
    product = std::string(productArdString.c_str());
    TinyUSBDevice.setProductDescriptor(product.c_str());
  }

  // now all our descriptors are set up we can now begin allowing the host OS to
  // talk to us. In "None" mode the device must stay off the bus: connecting
  // would enumerate us anyway and stall boot waiting for a mount that the
  // user explicitly asked not to happen.
  if (curDeviceType != USBDeviceType::None)
  {
    // CDC starts the ESP32 TinyUSB stack before HID/MSC are registered.
    // Always rebuild host enumeration after every interface is registered.
    TinyUSBDevice.detach();
    delay(50);
    tud_connect();

    // Allow up to one second for host enumeration, but keep battery-only boot
    // responsive when no USB host is connected.
    for (uint8_t counter = 0; counter < 10 && (!TinyUSBDevice.ready() || !TinyUSBDevice.mounted()); ++counter)
    {
      delay(100);
    }

    if (!TinyUSBDevice.ready() || !TinyUSBDevice.mounted())
    {
      Debug::Log.error(LOG_USB, "Timeout waiting for USB to mount");
    }
  }
}

/**
 * @brief Force TinyUSB disconnect and reconnect.
 * @return None.
 * @note Uses a 250 ms disconnect interval so host enumeration state clears.
 */
void USBCore::reset()
{
  if (!TinyUSBDevice.ready())
  {
    Debug::Log.error(LOG_USB, "USB is not ready. Continuing anyway.");
  }

  if (!TinyUSBDevice.mounted())
  {
    Debug::Log.error(LOG_USB, "USB not mounted, continuing");
  }

  tud_disconnect();
  delay(250);
  tud_connect();
  Debug::Log.info(LOG_USB, "USB reset");
}

/**
 * @brief Service all active USB interfaces.
 * @param[in,out] prefs Persistent settings passed to interface services.
 * @return None.
 * @note Must return promptly because it runs once per Arduino loop iteration.
 */
void USBCore::loop(Preferences &prefs)
{
#ifndef ARDUINO_ARCH_ESP32 
  // RP2040 profiles service TinyUSB from loop(); calling tud_task() here keeps
  // USB events moving even when no ESP32 background task exists.
  TinyUSBDevice.task();
#endif

  Devices::USB::NCM.loop(prefs);
  Devices::USB::HID.loop(prefs);
  Devices::USB::MSC.loop(prefs);
  Devices::USB::CDC.loop(prefs);
}

/**
 * @brief Disconnect and stop all USB interfaces.
 * @return None. Current device and class state become None.
 * @note Call before changing descriptors or USB mode.
 */
void USBCore::end()
{
  tud_disconnect();
  Devices::USB::CDC.end();
  Devices::USB::NCM.end();
  Devices::USB::HID.clearQueues();
  Devices::USB::MSC.end();
  curDeviceType = USBDeviceType::None;
  curClassType = USBClassType::None;
}
