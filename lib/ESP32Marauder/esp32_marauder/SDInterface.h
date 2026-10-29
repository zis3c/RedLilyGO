/** @file SDInterface.h @brief Marauder SD filesystem access API. */

#pragma once

#ifndef SDInterface_h
#define SDInterface_h

#include "configs.h"

#include "settings.h"
#ifdef USE_SD_MMC_INTERFACE
  #include "../../../src/Devices/Storage/ESP32/SDMmcFileSystem.h"
  #define SD  fs::SD_MMC_2
#elif USE_SD_INTERFACE
    // generic esp32 SD interface
    #include "SD.h"
    using namespace fs;
    #define FILE_INTERFACE SD
#elif USE_SPIFFS_INTERFACE
  #include <SPIFFS.h>
  #define SD SPIFFS
#else
  #include "SD.h"
#endif

#include "Buffer.h"
#ifdef HAS_SCREEN
  #include "Display.h"
#endif

extern Buffer buffer_obj;
extern Settings settings_obj;
#ifdef HAS_SCREEN
  extern Display display_obj;
#endif

#ifdef KIT
  #define SD_DET 4
#endif

class SDInterface {

  private:
#if defined(MARAUDER_M5STICKC)
  SPIClass *spiExt;
#endif
    bool checkDetectPin();

  public:
    uint8_t cardType;       ///< Detected Arduino SD card type.
    //uint64_t cardSizeBT;
    //uint64_t cardSizeKB;
    uint64_t cardSizeMB;    ///< Detected capacity in megabytes.
    //uint64_t cardSizeGB;
    bool supported = false; ///< Backend availability flag.

    String card_sz;         ///< Capacity formatted for UI output.

    /** @brief Initialize selected SD/filesystem backend. */
    bool initSD();

    LinkedList<String>* sd_files; ///< Allocated file listing used by Marauder UI.

    /** @brief List directory entries to serial/UI output. */
    void listDir(String str_dir);
    /** @brief Collect matching file names recursively. */
    void listDirToLinkedList(LinkedList<String>* file_names, String str_dir = "/", String ext = "");
    /** @brief Open file by path; return invalid File on failure. */
    File getFile(String path);
    /** @brief Service SD interface state. */
    void main();
    /** @brief Remove file by path and return backend result. */
    bool removeFile(String file_path);
};

#endif
