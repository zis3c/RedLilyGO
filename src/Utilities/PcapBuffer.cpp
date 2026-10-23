/** @file PcapBuffer.cpp @brief Bounded PCAP file writer used by capture paths. */

#include "PcapBuffer.h"

#include <string.h>

#define MAX_SIZE 15
#define MAX_PACKET_LEN 2324
#define MAX_FILENAME_ATTEMPTS 1000

static bool write(fs::File file, const uint8_t* buf, uint32_t len){
  return file.write(buf, len) == len;
}

static bool write(fs::File file, uint16_t n){
  uint8_t buf[2];
  buf[0] = n;
  buf[1] = n >> 8;
  return write(file, buf, 2);
}

static bool write(fs::File file, int32_t n){
  uint8_t buf[4];
  buf[0] = n;
  buf[1] = n >> 8;
  buf[2] = n >> 16;
  buf[3] = n >> 24;
  return write(file, buf,4);
}

static bool write(fs::File file, uint32_t n){
  uint8_t buf[4];
  buf[0] = n;
  buf[1] = n >> 8;
  buf[2] = n >> 16;
  buf[3] = n >> 24;
  return write(file, buf,4);
}

PcapBuffer::PcapBuffer(std::string baseName, fs::FS* fs, uint32_t dataLinkType)
    : m_blocks(), m_mutex(nullptr)
{
    // Must exist before any append/flush can run, including on early returns.
    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) return;

    if (fs == nullptr) return;
    fs->mkdir("/Captures");
    const size_t parentEnd = baseName.find_last_of('/');
    if (parentEnd != std::string::npos && parentEnd != 0)
    {
        fs->mkdir(baseName.substr(0, parentEnd).c_str());
    }
    uint32_t counter = 0;
    for (uint32_t attempts = 0; attempts < MAX_FILENAME_ATTEMPTS; ++attempts)
    {
        auto filename = (baseName.empty() || baseName.front() != '/' ? "/" : "") +
                        baseName + "_" + std::to_string(counter) + ".pcap";
        if (!fs->exists(filename.c_str()))
        {
            m_file = fs->open(filename.c_str(), "w");
            if (m_file)
            {
                break;
            }
        }
        counter++;
    }

    if (!m_file) return;

    m_writeFailed = !write(m_file, uint32_t(0xa1b2c3d4)) || // magic number
                    !write(m_file, uint16_t(2)) || // major version number
                    !write(m_file, uint16_t(4)) || // minor version number
                    !write(m_file, int32_t(0)) || // GMT to local correction
                    !write(m_file, uint32_t(0)) || // accuracy of timestamps
                    !write(m_file, uint32_t(MAX_PACKET_LEN)) || // max length
                    !write(m_file, uint32_t(dataLinkType)); // data link type
}

PcapBuffer::~PcapBuffer()
{
    // Persist whatever is still buffered rather than discarding it, so the
    // capture file does not silently end mid-stream.
    flushToDisk();

    if (m_mutex != nullptr)
    {
        // A final bounded flush may have failed to acquire the queue lock.
        // Do not delete it with heap-owned packet copies still queued.
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        while (!m_blocks.empty())
        {
            free(m_blocks.front().buffer);
            m_blocks.pop();
        }
        xSemaphoreGive(m_mutex);
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }

    if (m_file) m_file.close();
}

void PcapBuffer::flushToDisk()
{
    std::queue<PcapBufferBlock> toWrite;
    if (m_mutex == nullptr || xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    if (!m_file) {
        xSemaphoreGive(m_mutex);
        return;
    }
    // Keep queue and lifecycle state locked during SD I/O. Packet callbacks
    // use a nonblocking lock and safely drop while the file is being flushed.
    toWrite.swap(m_blocks);

    while (!toWrite.empty())
    {
        // Copy by value: pop() destroys the element, so a reference into it
        // would dangle.
        const PcapBufferBlock block = toWrite.front();
        toWrite.pop();

        uint32_t microSeconds = block.time;
        uint32_t seconds = (microSeconds/1000)/1000; // e.g. 45200400/1000/1000 = 45200 / 1000 = 45s
        microSeconds -= seconds*1000*1000; // e.g. 45200400 - 45*1000*1000 = 45200400 - 45000000 = 400us (because we only need the offset)

        const bool ok = write(m_file, seconds) && // ts_sec
                        write(m_file, microSeconds) && // ts_usec
                        write(m_file, block.length) && // incl_len
                        write(m_file, block.length) && // orig_len
                        write(m_file, block.buffer, block.length); // payload
        if (!ok)
        {
            m_writeFailed = true;
            free(block.buffer);
            while (!toWrite.empty())
            {
                free(toWrite.front().buffer);
                toWrite.pop();
            }
            break;
        }

        free(block.buffer);
    }

    m_file.flush();
    xSemaphoreGive(m_mutex);
}

bool PcapBuffer::append(uint8_t *packet, uint32_t len)
{
    if (packet == nullptr || len == 0 || len > MAX_PACKET_LEN || m_mutex == nullptr)
    {
        return false;
    }

    unsigned long curTime = micros();

    uint8_t* buffer = (uint8_t*) malloc(len);

    if (!buffer)
    {
        return false;
    }

    memcpy(buffer, packet, len);

    PcapBufferBlock block {
        .time = curTime,
        .buffer = buffer,
        .length = len
    };

    // append() can be invoked from the lwIP TCP/IP task while the main task
    // is flushing: never make the network stack wait on this queue.
    if (xSemaphoreTake(m_mutex, 0) != pdTRUE)
    {
        free(buffer);
        return false;
    }

    if (!m_file || m_writeFailed)
    {
        xSemaphoreGive(m_mutex);
        free(buffer);
        return false;
    }

    const bool full = m_blocks.size() >= MAX_SIZE;
    if (!full) m_blocks.push(block);
    xSemaphoreGive(m_mutex);

    if (full)
    {
        free(buffer);
        return false;
    }

    return true;
}
