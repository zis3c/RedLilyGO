/** @file USBNCM.h @brief USB NCM networking and packet-capture API. */

#pragma once

#include "../../RedLilyGOCapability.h"
#include "USBCore.h"

class USBNCM : RedLilyGOCapability {
public:
  /** @brief Construct NCM adapter in stopped state. */
  USBNCM();

  /** @brief Service NCM network interface. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize NCM interface and persisted settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Stop NCM interface and packet capture. */
  virtual void end();

  /** @brief Start packet collection through USB NCM path. */
  bool startPacketCollection();
  /** @brief Stop packet collection and flush capture state. */
  void stopPacketCollection();
  /** @brief Return whether a PCAP buffer is currently active. */
  bool isPacketCollectionActive() const;
};

namespace Devices::USB
{
    extern USBNCM NCM;
}
