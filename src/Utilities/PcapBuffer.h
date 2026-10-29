/** @file PcapBuffer.h @brief Bounded PCAP capture writer interface. */

#pragma once

#ifdef NO_SD
  #include <SPIFFS.h>
#else
  #include <SD_MMC.h>
#endif

#include <string>
#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct PcapBufferBlock {
  unsigned long time; ///< Capture timestamp in device microsecond units.
  uint8_t* buffer;    ///< Heap-owned packet bytes queued for disk write.
  uint32_t length;    ///< Number of valid bytes in buffer.
};

class PcapBuffer {
  public:
    /** @brief Open PCAP output and initialize queue synchronization. */
    PcapBuffer(std::string file_name, fs::FS* fs, uint32_t dataLinkType = 1);
    /** @brief Release queued packets, mutex, and output file. */
    ~PcapBuffer();
    /** @brief Report output readiness and prior write failure state. */
    bool isReady() const { return static_cast<bool>(m_file) && !m_writeFailed; }
    /** @brief Queue one packet for asynchronous disk flush. */
    bool append(uint8_t *packet, uint32_t len);
    /** @brief Write queued packets to filesystem. */
    void flushToDisk();
  private:
    fs::File m_file;
    std::queue<PcapBufferBlock> m_blocks;
    // Guards m_blocks: append() runs on the lwIP TCP/IP task while
    // flushToDisk()/the destructor run on other tasks.
    SemaphoreHandle_t m_mutex;
    bool m_writeFailed = false;
};
