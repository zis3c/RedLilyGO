/**
 * @file USBNCM.cpp
 * @brief USB Network Control Model transport and packet capture bridge.
 *
 * Uses TinyUSB networking and the PCAP buffer path. Packet callbacks and the
 * Arduino loop share bounded state protected by the module's synchronization.
 */

#ifdef ARDUINO_ARCH_ESP32

#include "USBNCM.h"
#include "Diagnostics/Logging.h"

#include <Adafruit_TinyUSB.h>
#include <usb_ncm.h>
#include <mutex>

#ifdef NO_SD
    #include <SPIFFS.h>
    #define FILE_INTERFACE SPIFFS
#elif USE_SD_INTERFACE
    // Use the Arduino SD library when this board profile selects SPI storage.
    #include "SD.h"
    using namespace fs;
    #define FILE_INTERFACE SD
#else
    #include "../../Devices/Storage/ESP32/SDMmcFileSystem.h"
    using namespace fs;
    #define FILE_INTERFACE SD_MMC_2
#endif

#include "../../Utilities/PcapBuffer.h"
#include "../../Devices/Storage/StorageDevice.h"

static Adafruit_USBD_NET usbNetworking;
static bool readyForPackets = false;
static PcapBuffer* buf = nullptr;
// Serializes the buf pointer between createPcap() (packet task) and
// start/stopPacketCollection() (main/async web task). Without it, stopping a
// capture can delete the buffer while the lwIP task is inside append().
static SemaphoreHandle_t pcapMutex = nullptr;
static std::mutex pcapInitMutex;

#define USBNCM_PCAPONSTART  "pcapNcmOnStart"
#define USBNCM_PCAPONSTART_DEFAULT  false

USBNCM::USBNCM()
{
}

/**
 * @brief Lazily create mutex protecting PCAP buffer lifetime.
 * @return true when mutex exists, otherwise false after allocation failure.
 * @note Must run before packet handler registration.
 */
static bool ensurePcapMutex()
{
    const std::lock_guard<std::mutex> lock(pcapInitMutex);
    if (pcapMutex == nullptr)
    {
        pcapMutex = xSemaphoreCreateMutex();
    }
    return pcapMutex != nullptr;
}

/**
 * @brief Append one received or transmitted packet to active PCAP capture.
 * @param[in] buffer Packet bytes; ignored when null.
 * @param[in] len Packet length; ignored when zero.
 * @param[in] received Direction flag supplied by NCM stack.
 * @return None. Packet is dropped when lock is busy or capture is inactive.
 * @note Called from packet polling and network tasks; never blocks on pcapMutex.
 */
extern "C" void createPcap(uint8_t *buffer, uint32_t len, bool received)
{
    if (pcapMutex == nullptr || buffer == nullptr || len == 0)
    {
        return;
    }

    // Never block packet processing waiting for the lock. Drop packet instead.
    if (xSemaphoreTake(pcapMutex, 0) != pdTRUE)
    {
        return;
    }

    if (buf != nullptr && buffer != nullptr && len != 0)
    {
        buf->append(buffer, len);
    }
    xSemaphoreGive(pcapMutex);
}

/**
 * @brief Start PCAP capture when NCM mode and storage are available.
 * @return true when handler and output buffer are ready, otherwise false.
 * @note May block on the PCAP mutex while synchronizing with lwIP callback.
 */
bool USBNCM::startPacketCollection()
{
    if (!ensurePcapMutex())
    {
        Debug::Log.error("NCM", "Could not allocate PCAP lock");
        return false;
    }

    if (Devices::USB::Core.currentDeviceType() == USBDeviceType::NCM &&
        Devices::Storage.isRunning())
    {
        xSemaphoreTake(pcapMutex, portMAX_DELAY);
        if (buf != nullptr)
        {
            xSemaphoreGive(pcapMutex);
            Debug::Log.error("NCM", "Error starting PCAP, already running");
            return false;
        }
        auto newBuf = new PcapBuffer(std::string("/Captures/Other/usbncm"), &FILE_INTERFACE);
        if (newBuf == nullptr)
        {
            xSemaphoreGive(pcapMutex);
            Debug::Log.error("NCM", "Could not allocate PCAP buffer");
            return false;
        }
        if (!newBuf->isReady())
        {
            Debug::Log.error("NCM", "Could not open PCAP output file");
            delete newBuf;
            xSemaphoreGive(pcapMutex);
            return false;
        }
        // Publish the buffer while holding the lock so no createPcap() call
        // can observe a half-constructed object.
        buf = newBuf;
        set_packet_handler(&createPcap);
        xSemaphoreGive(pcapMutex);
        Debug::Log.info("NCM", "Started PCAP");
        return true;
    }
    else
    {
        Debug::Log.error("NCM", "Error starting PCAP, incorrect mode or missing SD");
        return false;
    }
}

/**
 * @brief Detach packet callback and destroy active PCAP buffer safely.
 * @return None.
 * @note Waits for in-flight lwIP append before freeing buffer.
 */
void USBNCM::stopPacketCollection()
{
    Debug::Log.info("NCM", "Stopping PCAP");
    if (!ensurePcapMutex())
    {
        return;
    }
    // Serialize detachment with start so an overlapping start cannot restore
    // the callback after stop has already decided capture is inactive.
    xSemaphoreTake(pcapMutex, portMAX_DELAY);
    set_packet_handler(NULL);
    if (buf != nullptr)
    {
        buf->flushToDisk();
        delete buf;
        buf = nullptr;
    }
    xSemaphoreGive(pcapMutex);
}

bool USBNCM::isPacketCollectionActive() const
{
    if (pcapMutex == nullptr) return false;
    xSemaphoreTake(pcapMutex, portMAX_DELAY);
    const bool active = buf != nullptr;
    xSemaphoreGive(pcapMutex);
    return active;
}

/**
 * @brief Initialize USB NCM packet transport.
 * @param[in,out] prefs Persistent NCM configuration store.
 * @return None. Initialization is skipped outside NCM mode.
 * @note Runs on control task; packet callbacks may begin after TinyUSB init.
 */
void USBNCM::begin(Preferences &prefs)
{
    registerUserConfigurableSetting(CATEGORY_USB, USBNCM_PCAPONSTART, RedLilyGOCapability::SettingType::Bool, USBNCM_PCAPONSTART_DEFAULT);

    if (Devices::USB::Core.currentDeviceType() != USBDeviceType::NCM)
    {
        if (isPacketCollectionActive()) stopPacketCollection();
        readyForPackets = false;
        return;
    }

    if (Devices::USB::Core.currentDeviceType() == USBDeviceType::NCM)
    {
        if (readyForPackets == false && usb_ncm_init(&usbnet_hasNewPacket, &usbnet_getPacket, &usbnet_releasePacket, &usbnet_transmitPacket) == ESP_OK)
        {
            bool pcapOnStart = prefs.getBool(USBNCM_PCAPONSTART, USBNCM_PCAPONSTART_DEFAULT);
            if (pcapOnStart)
            {
                startPacketCollection();
            }

            Debug::Log.info("NCM", "NCM Init OK");
            readyForPackets = true;
            TinyUSB_Device_Init(0, true);
        }
        else
        {
            Debug::Log.error("NCM", "NCM Init error");
        }
    }
}

/**
 * @brief Poll NCM packets and flush active PCAP data.
 * @param[in,out] prefs Persistent settings passed to dependent services.
 * @return None.
 * @note Shares pcapMutex with lwIP callback and stop path.
 */
void USBNCM::loop(Preferences &prefs)
{
    if (Devices::USB::Core.currentDeviceType() == USBDeviceType::NCM && readyForPackets)
    {
        poll_for_packets();
    }

    if (!ensurePcapMutex()) return;
    xSemaphoreTake(pcapMutex, portMAX_DELAY);
    const bool captureActive = buf != nullptr;
    xSemaphoreGive(pcapMutex);
    if (captureActive)
    {
        if (!Devices::Storage.isRunning())
        {
            Debug::Log.warning("NCM", "Storage unavailable, stopping PCAP");
            stopPacketCollection();
            return;
        }
        // Most applications need to use the protocol layer only in one task.
        // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html
        // Hold the same lock as createPcap()/stopPacketCollection() so a
        // concurrent stop from another task cannot delete buf mid-flush.
        xSemaphoreTake(pcapMutex, portMAX_DELAY);
        if (buf != nullptr)
        {
            buf->flushToDisk();
        }
        xSemaphoreGive(pcapMutex);
    }
}

/**
 * @brief Stop packet capture and mark NCM transport unavailable.
 * @return None.
 */
void USBNCM::end()
{
    if (isPacketCollectionActive()) stopPacketCollection();
    readyForPackets = false;
}
#endif
