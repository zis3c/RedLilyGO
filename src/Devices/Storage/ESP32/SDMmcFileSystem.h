/** @file SDMmcFileSystem.h @brief ESP32 SD_MMC filesystem adapter. */

#pragma once

#ifndef NO_SD

#if ARDUINO_ARCH_ESP32
#include <SD_MMC.h>

namespace fs {

class SDMmcFileSystem : public SDMMCFS {
public:
  sdmmc_card_t* getCard() { return _card; }
};

extern fs::SDMmcFileSystem SD_MMC_2;
}
#endif
#endif
