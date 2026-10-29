/** @file SDMmcFileSystem.cpp @brief ESP32 SD_MMC bus and filesystem adapter. */

#ifndef NO_SD
#if ARDUINO_ARCH_ESP32

#include "SDMmcFileSystem.h"
using namespace fs;

#include "vfs_api.h"

SDMmcFileSystem fs::SD_MMC_2 = SDMmcFileSystem(FSImplPtr(new VFSImpl()));
#endif
#endif
