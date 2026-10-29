/** @file Buffer.h @brief Marauder packet and text buffer declarations. */

#pragma once

#ifndef Buffer_h
#define Buffer_h

#include "Arduino.h"
#include "FS.h"
#include "settings.h"
#include "esp_wifi_types.h"
#include <mutex>

#define BUF_SIZE 3 * 1024 ///< Double-buffer capacity reduced to fit MCU RAM.
#define SNAP_LEN 2324     ///< Maximum captured 802.11 frame bytes.

//extern bool useSD;

extern Settings settings_obj;

class Buffer {
  public:
    /** @brief Allocate packet buffers and initialize capture state. */
    Buffer();
    /** @brief Flush pending data and release packet buffers. */
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    /** @brief Clear buffers and counters without changing backend selection. */
    void reset();
    /** @brief Open PCAP output destination. */
    void pcapOpen(String file_name, fs::FS* fs, bool serial);
    /** @brief Open text log output destination. */
    void logOpen(String file_name, fs::FS* fs, bool serial);
    /** @brief Append captured packet bytes to active buffer. */
    void append(wifi_promiscuous_pkt_t *packet, int len);
    /** @brief Append one log line to active buffer. */
    void append(String log);
    /** @brief Persist buffered data to filesystem and/or serial. */
    void save();
    /** @brief Flush caller-owned capture state and close its output file. */
    void close();
    /** @brief Return and reset packet count atomically. */
    uint16_t getPacketsSinceLastCheck() {
      const std::lock_guard<std::recursive_mutex> lock(bufferMutex);
      uint16_t temp = packetsCounter;
      packetsCounter = 0;
      return temp; 
    }
  private:
    bool createFile(String name, bool is_pcap);
    void open(bool is_pcap);
    void openFile(String file_name, fs::FS* fs, bool serial, bool is_pcap);
    void add(const uint8_t* buf, uint32_t len, bool is_pcap);
    void write(int32_t n);
    void write(uint32_t n);
    void write(uint16_t n);
    void write(const uint8_t* buf, uint32_t len);
    void saveFs();
    void saveSerial();
    
    uint8_t* bufA; ///< First packet accumulation buffer.
    uint8_t* bufB; ///< Second buffer used while first is being saved.

    uint32_t bufSizeA = 0;
    uint32_t bufSizeB = 0;

    uint16_t packetsCounter = 0;

    bool writing = false; ///< Accepting writes into active buffer.
    bool useA = true;     ///< Selects bufA when true, otherwise bufB.
    bool saving = false;  ///< Save operation currently owns inactive buffer.

    String fileName = "/0.pcap";
    File file;
    fs::FS* fs;
    bool serial;
    mutable std::recursive_mutex bufferMutex;
};

#endif
