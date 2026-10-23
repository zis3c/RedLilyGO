/**
 * @file USBCDC.cpp
 * @brief Framed USB CDC transport for host commands and debug output.
 *
 * Uses TinyUSB/HardwareSerial and a fixed receive buffer. Transport state is
 * mutex-protected; callbacks run outside that lock on the Arduino loop task.
 */

#include <HardwareSerial.h>
#include <unordered_map>
#include <mutex>
#include <algorithm>

#include "USBCDC.h"
#include "Diagnostics/Logging.h"

#include <Adafruit_TinyUSB.h>

#define MAX_SERIAL_BUFFER_SIZE 2048 ///< Maximum accepted payload.
#define SERIAL_RX_QUEUE_SIZE 8192    ///< Buffer VNC bursts larger than the core's 256-byte default.
#define SERIAL_FRAME_TIMEOUT_MS 1000
#define SIZEOF_TAG 1                ///< One-byte HostCommand field.
#define SIZEOF_LENGTH 4             ///< Little-endian uint32 payload length field.
#define LOG_CDC "CDC"

// Only loop() writes the backing storage. end() cannot invalidate a payload
// being delivered outside cdcMutex. Callbacks must not retain the pointer.
static uint8_t receiveBuffer[MAX_SERIAL_BUFFER_SIZE];
static uint8_t *serialPortRecvBuffer = nullptr;
static uint8_t frameHeader[SIZEOF_TAG + SIZEOF_LENGTH];
static size_t headerRead = 0, payloadRead = 0;
static uint32_t payloadLength = 0, frameStartedAt = 0;
static bool discardPending = false;
static std::unordered_map<HostCommand, std::function<void(uint8_t *, size_t)>> callbacks;

#define USB_SERIALRAW "usbSerialRaw"
#define USB_SERIALRAW_DEFAULT false
static bool usbRawMode = USB_SERIALRAW_DEFAULT;
static bool usbInitCalled = false;
static bool noSerialDebugErrorPrinted = false;
static std::recursive_mutex cdcMutex;
// NOTE: the fixed 4 KiB queue drops whole new frames when the host stalls.
// Add caller backpressure if sustained traffic needs guaranteed delivery.
static uint8_t transmitBuffer[4096];
static size_t transmitHead = 0, transmitSize = 0;

static void flushOutput()
{
    if (!tud_cdc_n_connected(0))
    {
        transmitHead = transmitSize = 0;
        return;
    }
    const size_t count = std::min(transmitSize, sizeof(transmitBuffer) - transmitHead);
    // The installed Arduino Serial.write spins while a connected host stops
    // reading. TinyUSB's FIFO write returns immediately, including short writes.
    if (count != 0)
    {
        const size_t written = tud_cdc_n_write(0, transmitBuffer + transmitHead, count);
        transmitHead = (transmitHead + written) % sizeof(transmitBuffer);
        transmitSize -= written;
        tud_cdc_n_write_flush(0);
    }
}

static void enqueueOutput(const uint8_t *data, size_t size)
{
    if (size == 0) return;
    const size_t tail = (transmitHead + transmitSize) % sizeof(transmitBuffer);
    const size_t first = std::min(size, sizeof(transmitBuffer) - tail);
    memcpy(transmitBuffer + tail, data, first);
    memcpy(transmitBuffer, data + first, size - first);
    transmitSize += size;
}

static void resetFrame()
{
    headerRead = payloadRead = payloadLength = 0;
}

/** @brief Construct CDC transport in stopped state. */
USBCDCWrapper::USBCDCWrapper()
{
}

/**
 * @brief Register framed host-command callback.
 * @param[in] tag Command identifier strictly between MIN and MAX.
 * @param[in] callback Handler receiving payload pointer and byte count.
 * @return None. Invalid tags or empty callbacks are ignored.
 * @note Callback executes outside cdcMutex on the Arduino loop task.
 */
void USBCDCWrapper::setCallback(const HostCommand &tag, std::function<void(uint8_t *, const size_t)> callback)
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    if (tag <= HostCommand::MIN || tag >= HostCommand::MAX || !callback) return;
    callbacks[tag] = callback;
}

/**
 * @brief Start CDC serial framing at requested baud.
 * @param[in] baud Hardware serial baud rate.
 * @return None.
 * @note Idempotent and serialized by cdcMutex.
 */
void USBCDCWrapper::begin(const unsigned long &baud)
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    registerUserConfigurableSetting(CATEGORY_USB, USB_SERIALRAW, RedLilyGOCapability::SettingType::Bool, USB_SERIALRAW_DEFAULT);

    // ensure this can be called multiple times without error
    if (Devices::USB::Core.currentDeviceType() != USBDeviceType::Serial)
    {
        if (serialPortRecvBuffer != nullptr || usbInitCalled)
        {
            end();
        }
        return;
    }

    if (usbInitCalled == false)
    {
#ifdef ARDUINO_ARCH_ESP32 
        TinyUSB_Device_Init(0, false);
#endif
        usbInitCalled = true;
    }

    serialPortRecvBuffer = receiveBuffer;
    resetFrame();
    discardPending = false;
    transmitHead = transmitSize = 0;
    Serial.setRxBufferSize(SERIAL_RX_QUEUE_SIZE);
    Serial.begin(baud);
    noSerialDebugErrorPrinted = false;
}

/**
 * @brief Load raw-mode setting and start CDC at 115200 baud.
 * @param[in,out] prefs Persistent USB settings store.
 * @return None.
 */
void USBCDCWrapper::begin(Preferences &prefs)
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    usbRawMode = prefs.getBool(USB_SERIALRAW, USB_SERIALRAW_DEFAULT);

    begin(115200);
}

/** @brief Return true when framed CDC transport can accept work. */
bool USBCDCWrapper::isReady() const
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    return serialPortRecvBuffer != nullptr && !usbRawMode &&
           Devices::USB::Core.currentDeviceType() == USBDeviceType::Serial;
}

/**
 * @brief Advance at most one CDC frame without waiting for incoming bytes.
 * @param[in,out] prefs Persistent settings, reserved for interface contract.
 * @return None. Invalid or expired frames discard currently buffered input.
 * @note Runs only from Arduino loop. Callbacks execute without cdcMutex.
 */
void USBCDCWrapper::loop(Preferences &prefs)
{
    std::unique_lock<std::recursive_mutex> lock(cdcMutex);
    if (serialPortRecvBuffer == nullptr || Devices::USB::Core.currentDeviceType() != USBDeviceType::Serial) return;
    flushOutput();
    if (usbRawMode) return;

    const uint32_t now = millis();
    if (headerRead != 0 && now - frameStartedAt >= SERIAL_FRAME_TIMEOUT_MS)
    {
        resetFrame();
        discardPending = true;
        Debug::Log.error(LOG_CDC, "Serial frame timed out.");
    }
    if (discardPending)
    {
        // A continuously sending peer must not keep the Arduino task here.
        const size_t available = Serial.available();
        if (available != 0) Serial.read(receiveBuffer, std::min(available, sizeof(receiveBuffer)));
        discardPending = Serial.available() != 0;
        return;
    }

    if (headerRead == 0 && Serial.available()) frameStartedAt = now;
    if (headerRead < sizeof(frameHeader))
    {
        const size_t available = Serial.available();
        if (available == 0) return;
        headerRead += Serial.read(frameHeader + headerRead,
                                  std::min(available, sizeof(frameHeader) - headerRead));
        if (headerRead != sizeof(frameHeader)) return;
        memcpy(&payloadLength, frameHeader + SIZEOF_TAG, sizeof(payloadLength));
        if (frameHeader[0] <= HostCommand::MIN || frameHeader[0] >= HostCommand::MAX ||
            payloadLength > MAX_SERIAL_BUFFER_SIZE)
        {
            resetFrame();
            discardPending = Serial.available() != 0;
            Debug::Log.error(LOG_CDC, "Invalid serial frame header (tag=" + std::to_string(frameHeader[0]) +
                                        ", length=" + std::to_string(payloadLength) + ").");
            return;
        }
    }

    const size_t available = Serial.available();
    if (available != 0 && payloadRead != payloadLength)
        payloadRead += Serial.read(receiveBuffer + payloadRead,
                                   std::min(available, static_cast<size_t>(payloadLength) - payloadRead));
    if (payloadRead != payloadLength) return;

    // Consume unhandled frames too, preserving the next frame's boundary.
    const auto it = callbacks.find(static_cast<HostCommand>(frameHeader[0]));
    const auto callback = it == callbacks.cend() ? std::function<void(uint8_t *, size_t)>{} : it->second;
    const size_t length = payloadLength;
    resetFrame();
    lock.unlock();
    if (callback) callback(receiveBuffer, length);
}

/**
 * @brief Queue one bounded command frame to host without waiting for USB space.
 * @param[in] tag Command identifier strictly between MIN and MAX.
 * @param[in] buffer Payload bytes; null allowed only for zero size.
 * @param[in] size Payload length at most MAX_SERIAL_BUFFER_SIZE.
 * @return None. A disconnected host or full queue drops the whole new frame.
 * @note Holds cdcMutex and must not be called recursively through logging.
 */
void USBCDCWrapper::writeBinary(const HostCommand &tag, const uint8_t *buffer, const size_t &size)
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    if (tag <= HostCommand::MIN || tag >= HostCommand::MAX ||
        size > MAX_SERIAL_BUFFER_SIZE || (size != 0 && buffer == nullptr))
    {
        Debug::Log.error(LOG_CDC, "Cannot write binary data. Invalid buffer.");
    }
    else if (Devices::USB::Core.currentDeviceType() != USBDeviceType::Serial)
    {
        if (!noSerialDebugErrorPrinted)
        {
            noSerialDebugErrorPrinted = true;
            Debug::Log.warning(LOG_CDC, "Cannot write to serial. USB mode is not set to serial.");
        }
    }
    else if (usbRawMode)
    {
        Debug::Log.error(LOG_CDC, "Cannot write binary data. USB mode is incorrect.");
    }
    else if (serialPortRecvBuffer == nullptr)
    {
        Debug::Log.error(LOG_CDC, "Cannot write binary data. USB CDC is not running.");
    }
    else
    {
        flushOutput();
        if (!tud_cdc_n_connected(0) || size + SIZEOF_TAG + SIZEOF_LENGTH > sizeof(transmitBuffer) - transmitSize) return;
        const uint8_t command = tag;
        const uint32_t length = size;
        enqueueOutput(&command, SIZEOF_TAG);
        enqueueOutput(reinterpret_cast<const uint8_t *>(&length), SIZEOF_LENGTH);
        enqueueOutput(buffer, size);
        flushOutput();
    }
}

/**
 * @brief Write debug text using raw or framed serial mode.
 * @param[in] msg Text payload; may be empty.
 * @return None. Output is skipped when serial transport is unavailable.
 * @note Logging recursion is intentionally avoided because logger uses CDC.
 */
void USBCDCWrapper::writeDebugString(const std::string &msg)
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    // be careful about calling debug log here as you could get into a loop

    if (Devices::USB::Core.currentDeviceType() != USBDeviceType::Serial)
    {
        if (!noSerialDebugErrorPrinted)
        {
            noSerialDebugErrorPrinted = true;
            Debug::Log.warning(LOG_CDC, "Cannot write debug output. USB mode is not set to serial.");
        }
    }
    else if (serialPortRecvBuffer == nullptr)
    {
        return;
    }
    else if (usbRawMode)
    {
        flushOutput();
        if (!tud_cdc_n_connected(0) || msg.size() > sizeof(transmitBuffer) - 2 ||
            msg.size() + 2 > sizeof(transmitBuffer) - transmitSize) return;
        enqueueOutput(reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
        enqueueOutput(reinterpret_cast<const uint8_t *>("\r\n"), 2);
        flushOutput();
    }
    else
    {
        writeBinary(HostCommand::DebugMsg, (uint8_t *)msg.c_str(), msg.length());
    }
}

/**
 * @brief Stop CDC transport and discard any partial frame.
 * @return None.
 * @note Holds cdcMutex while closing Serial and resetting receive state.
 */
void USBCDCWrapper::end()
{
    const std::lock_guard<std::recursive_mutex> lock(cdcMutex);
    if (serialPortRecvBuffer != nullptr)
    {
        Serial.end();
        serialPortRecvBuffer = nullptr;
    }
    resetFrame();
    discardPending = false;
    transmitHead = transmitSize = 0;
    noSerialDebugErrorPrinted = false;
    usbRawMode = USB_SERIALRAW_DEFAULT;
    usbInitCalled = false;
}
