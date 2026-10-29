/**
 * @file USBMSC.cpp
 * @brief TinyUSB mass-storage callbacks backed by device storage.
 *
 * USB host callbacks access storage sectors through the configured storage
 * backend. Callback code must stay bounded and preserve sector-size rules.
 */

#include "USBMSC.h"

#include <Adafruit_TinyUSB.h>

#include "../../Devices/Storage/StorageDevice.h"
#include "Diagnostics/Logging.h"
#define TAG_USB "USB"
/** USB MSC logical block size required by READ10/WRITE10 commands. */
#define LOGICAL_BLOCK_SIZE 512

static Adafruit_USBD_MSC usb_msc;
static File mscFile;
static std::recursive_mutex mscMutex;

/**
 * @brief Read logical blocks for a host READ10 request.
 * @param[in] lba First logical block address.
 * @param[out] buffer Host-provided destination; must be non-null and aligned
 *        as required by raw storage backend.
 * @param[in] bufsize Number of bytes; non-zero multiple of 512.
 * @return Bytes copied, or -1 on validation, seek, or storage failure.
 * @note Runs in TinyUSB callback context and serializes through mscMutex.
 */
static int32_t msc_read_cb(uint32_t lba, void *buffer, uint32_t bufsize)
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    if (!Devices::Storage.isRunning()) return -1;
    if (buffer == nullptr || bufsize == 0 || bufsize % LOGICAL_BLOCK_SIZE != 0) return -1;
    Devices::USB::MSC.setActivityStateState(true);

    if (mscFile)
    {
        // File seeks use byte offsets; raw SD uses sector addresses and can
        // access cards larger than the 32-bit file-offset range.
        if (lba > UINT32_MAX / LOGICAL_BLOCK_SIZE) return -1;
        const uint32_t offset = lba * LOGICAL_BLOCK_SIZE;
        if (bufsize > UINT32_MAX - offset) return -1;
        if (!mscFile.seek(offset))
        {
            Debug::Log.error(TAG_USB, "Disk seek error");
            return -1;
        }
        
        if (mscFile.readBytes((char *)buffer, bufsize) == bufsize)
        {
            // bufSize should always be a multiple of LOGICAL_BLOCK_SIZE so on read success return that
            return bufsize;
        }
        else
        {
            Debug::Log.error(TAG_USB, "Could not read data from file");
        }
    }
    else if (Devices::Storage.isRawAccessSupported())
    {
        const size_t sectorSize = Devices::Storage.sectorSize();
        if (sectorSize == 0 || bufsize == 0 || bufsize % sectorSize != 0) return -1;
        uint32_t sectors = (bufsize / sectorSize);
        return Devices::Storage.readRawSectors((uint8_t*)buffer, lba, sectors);
    }
    else
    {
        Debug::Log.error(TAG_USB, "No disk data to read");
    }
    return -1;
}

/**
 * @brief Write logical blocks for a host WRITE10 request.
 * @param[in] lba First logical block address.
 * @param[in] buffer Host-provided source bytes; must not be null.
 * @param[in] bufsize Number of bytes; non-zero multiple of 512.
 * @return Bytes written, or -1 when validation/backend write fails.
 * @note File-image writes are rejected; raw SD writes use storage backend.
 */
static int32_t msc_write_cb(uint32_t lba, uint8_t *buffer, uint32_t bufsize)
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    if (!Devices::Storage.isRunning()) return -1;
    if (buffer == nullptr || bufsize == 0 || bufsize % LOGICAL_BLOCK_SIZE != 0) return -1;
    Devices::USB::MSC.setActivityStateState(true);

    if (mscFile)
    {
        // This path supports raw-sector devices, not writes to disk-image files.
        return -1;
    }
    else if (Devices::Storage.isRawAccessSupported())
    {
        const size_t sectorSize = Devices::Storage.sectorSize();
        if (sectorSize == 0 || bufsize == 0 || bufsize % sectorSize != 0) return -1;
        uint32_t sectors = (bufsize / sectorSize);
        return Devices::Storage.writeRawSectors((uint8_t*)buffer, lba, sectors);
    }
    else
    {
        Debug::Log.error(TAG_USB, "Could not write");
    }
    return -1;
}

/**
 * @brief Flush pending MSC writes after host accepts WRITE10.
 * @return None.
 * @note Runs in TinyUSB callback context while mscMutex is held.
 */
static void msc_flush_cb()
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    if (mscFile)
    {
        mscFile.flush();
    }
    else
    {
        Devices::Storage.flush();
    }
}

/**
 * @brief Open a filesystem-backed MSC image.
 * @param[in] path NUL-terminated storage path.
 * @return Image size in bytes, or zero when open/size lookup fails.
 * @note Caller owns the resulting global mscFile until mount cleanup.
 */
static size_t open_msc(const char *path)
{
    mscFile = Devices::Storage.openFile(path, "r");
    if (!mscFile)
    {
        return 0;
    }

    const size_t fileSize = mscFile.size();
    if (fileSize == static_cast<size_t>(-1))
    {
        mscFile.close();
        return 0;
    }
    return fileSize;
}

bool USBMSC::mountSD()
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    mscActivity = false;

    if (!Devices::Storage.isRawAccessSupported())
    {
        Debug::Log.error(TAG_USB, "Raw SD card access is not supported on this device");
        return false;
    }

    if (mscStarted || mscFile)
    {
        Debug::Log.info(TAG_USB, "Device already mounted");
        return false;
    }

    usb_msc.setID("Adafruit", "Mass Storage", "1.0");

    const auto capacity = Devices::Storage.deviceCapacity();
    const auto sectorSize = Devices::Storage.sectorSize();

    if (sectorSize == 0 || capacity == 0)
    {
        Debug::Log.info(TAG_USB, "SD card size is invalid.");
        return false;
    }

    usb_msc.setCapacity(capacity, sectorSize);

    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

    // Report the logical unit as ready after validating the storage geometry.
    usb_msc.setUnitReady(true);

    // Reset USB descriptors before starting MSC so the host enumerates the new interface.
    Devices::USB::Core.reset();

    if (!usb_msc.begin())
    {
        usb_msc.setUnitReady(false);
        Debug::Log.info(TAG_USB, "Failed to start USB MSC");
        return false;
    }

    mscStarted = true;
    Debug::Log.info(TAG_USB, "SD card mounted.");
    return true;
}

USBMSC::USBMSC()
{
}

void USBMSC::begin(Preferences &prefs)
{
}

void USBMSC::loop(Preferences &prefs)
{
}

void USBMSC::end()
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    mscActivity = false;
    usb_msc.setUnitReady(false);
    if (mscFile)
    {
        mscFile.flush();
        mscFile.close();
        mscFile = File();
    }
    mscStarted = false;
}

bool USBMSC::mountDiskImage(const std::string &imageLocation, bool mountAsCD)
{
    const std::lock_guard<std::recursive_mutex> lock(mscMutex);
    mscActivity = false;
    if (mscStarted || mscFile)
    {
        Debug::Log.warning(TAG_USB, "USB MSC already mounted");
        return false;
    }
    const std::size_t size = open_msc(imageLocation.c_str());

    if (size != 0 && size % LOGICAL_BLOCK_SIZE == 0 &&
        size / LOGICAL_BLOCK_SIZE <= UINT32_MAX)
    {
        usb_msc.setCDROM(mountAsCD);

        if (mountAsCD)
        {
            usb_msc.setID("Adafruit", "USB CDROM", "1.0");
        }
        else
        {
            usb_msc.setID("Adafruit", "Mass Storage", "1.0");
        }

        usb_msc.setCapacity(size / LOGICAL_BLOCK_SIZE, LOGICAL_BLOCK_SIZE);

        usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

        // Report the logical unit as ready after validating the image size.
        usb_msc.setUnitReady(true);

        // Reset USB descriptors before starting MSC so the host enumerates the new interface.
        Devices::USB::Core.reset();

        if (!usb_msc.begin())
        {
            usb_msc.setUnitReady(false);
            mscFile.close();
            mscFile = File();
            mscStarted = false;
            Debug::Log.info(TAG_USB, "Failed to start USB MSC");
            return false;
        }

        mscStarted = true;
        Debug::Log.info(TAG_USB, mountAsCD ? "CDROM image mounted" : "Disk image mounted");
        return true;
    }
    else
    {
        if (mscFile) mscFile.close();
        Debug::Log.error(TAG_USB, "Could not load USB image");
        return false;
    }
}
