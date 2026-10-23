/**
 * @file USBHID.cpp
 * @brief TinyUSB HID report transport and input queue service.
 *
 * Targets TinyUSB-capable boards. Reports are queued between the Arduino
 * control loop and host USB callbacks; callers must use the class API.
 */

#include "USBHID.h"

#include <Adafruit_TinyUSB.h>
#include <queue>

#include "Diagnostics/Logging.h"
#define TAG_USB "USB"
#define HID_WAIT_TIME 10 ///< Inter-report delay preventing host report saturation.
static constexpr size_t MAX_PENDING_HID_EVENTS = 128;
static constexpr uint32_t HID_STALL_TIMEOUT_MS = 1500;
static bool hidWasMounted = false;
static uint32_t hidNotReadySince = 0;

enum
{
    RID_KEYBOARD = 1,    ///< Keyboard report identifier.
    RID_MOUSE,            ///< Relative mouse report identifier.
    RID_CONSUMER_CONTROL  ///< Consumer/media control report identifier.
};

// HID report descriptor using TinyUSB's templates. Each report carries its own ID.
uint8_t const desc_hid_report[] =
    {
        TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
        TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE)),
        TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER_CONTROL))};

struct KEY_EVENT
{
    uint8_t Keycode[6] = {0}; ///< Up to six simultaneous HID key usages.
    uint8_t Modifiers = 0;    ///< HID modifier bitmask.
    uint8_t ID = 0;           ///< Report ID sent to TinyUSB.
};

struct MOUSE_EVENT
{
    int8_t xDelta = 0;       ///< Relative horizontal movement.
    int8_t yDelta = 0;       ///< Relative vertical movement.
    int8_t vertical = 0;     ///< Vertical wheel delta.
    int8_t horizontal = 0;   ///< Horizontal wheel delta.
    uint8_t buttons = 0;     ///< Current button bitmask.
    bool buttonReport = false; ///< Select button report instead of movement.
    bool scrollReport = false; ///< Select wheel report instead of movement.
};

static std::queue<KEY_EVENT> eventsToProcess;
static std::queue<std::pair<uint8_t, uint16_t>> cdcEventsToProcess;
static std::queue<MOUSE_EVENT> mouseEventsToProcess;
static uint8_t mouseButtonMask = 0;
Adafruit_USBD_HID *usb_hid = nullptr;

USBHID::USBHID()
{
}

/**
 * @brief Initialize HID descriptor and report queues.
 * @param[in,out] prefs Persistent settings, reserved for profile consistency.
 * @return None. Initialization is skipped unless HID class is active.
 * @note Creates TinyUSB HID object once and clears stale reports on restart.
 */
void USBHID::begin(Preferences &prefs)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    hidWasMounted = false;
    hidNotReadySince = 0;
    if (Devices::USB::Core.currentDeviceType() == USBDeviceType::None || Devices::USB::Core.currentClassType() != USBClassType::HID)
    {
        clearQueuesUnlocked();
        return;
    }

    clearQueuesUnlocked();

    if (usb_hid == nullptr)
    {
        // Create the HID interface once, then reuse it for later USB mode changes.
        usb_hid = new Adafruit_USBD_HID(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
        if (usb_hid == nullptr)
        {
            Debug::Log.error("USB", "Could not allocate USB HID.");
            return;
        }
        // Configure the polling interval and report descriptor before registration.
        usb_hid->setPollInterval(2);
        usb_hid->setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
        usb_hid->setStringDescriptor("HID Composite");
        if (!usb_hid->begin())
        {
            Debug::Log.error("USB", "Could not register HID interface.");
            delete usb_hid;
            usb_hid = nullptr;
        }
    }
}

/** @brief Return true when all pending HID report queues are empty. */
bool USBHID::IsQueueEmpty()
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    return eventsToProcess.empty() && cdcEventsToProcess.empty() && mouseEventsToProcess.empty();
}

/** @brief Return true when the keyboard queue can accept `needed` more reports. */
bool USBHID::keyboardQueueHasSpace(size_t needed) const
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    return eventsToProcess.size() + needed <= MAX_PENDING_HID_EVENTS;
}

bool USBHID::isReady() const
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    return usb_hid != nullptr && TinyUSBDevice.mounted() &&
           Devices::USB::Core.currentDeviceType() == USBDeviceType::Serial &&
           Devices::USB::Core.currentClassType() == USBClassType::HID &&
           usb_hid->ready();
}

/** @brief Clear all pending reports and button state under queue lock. */
void USBHID::clearQueues()
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    clearQueuesUnlocked();
}

/** @brief Clear queues while caller already owns queueMutex. */
void USBHID::clearQueuesUnlocked()
{
    while (!eventsToProcess.empty()) eventsToProcess.pop();
    while (!cdcEventsToProcess.empty()) cdcEventsToProcess.pop();
    while (!mouseEventsToProcess.empty()) mouseEventsToProcess.pop();
    mouseButtonMask = 0;
}

/**
 * @brief Queue one keyboard report.
 * @param[in] modifiers HID modifier bitmask.
 * @param[in] key1 First usage code; remaining key parameters fill slots.
 * @param[in] key2 Second usage code.
 * @param[in] key3 Third usage code.
 * @param[in] key4 Fourth usage code.
 * @param[in] key5 Fifth usage code.
 * @param[in] key6 Sixth usage code.
 * @return None. Report is dropped when HID is unavailable or queue is full.
 */
void USBHID::keyboard_press(const uint8_t modifiers, const uint8_t key1, const uint8_t key2, const uint8_t key3, const uint8_t key4, const uint8_t key5, const uint8_t key6)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr)
    {
        return;
    }

    KEY_EVENT event;
    event.ID = RID_KEYBOARD;
    event.Keycode[0] = key1;
    event.Keycode[1] = key2;
    event.Keycode[2] = key3;
    event.Keycode[3] = key4;
    event.Keycode[4] = key5;
    event.Keycode[5] = key6;
    event.Modifiers = modifiers;

    if (eventsToProcess.size() < MAX_PENDING_HID_EVENTS) eventsToProcess.push(event);
}

/** @brief Queue keyboard release report; drops it when HID is unavailable. */
void USBHID::keyboard_release()
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr)
    {
        return;
    }

    KEY_EVENT event = {0};
    event.ID = RID_KEYBOARD;

    if (eventsToProcess.size() < MAX_PENDING_HID_EVENTS) eventsToProcess.push(event);
}

/** @brief Queue consumer key press followed by release. */
void USBHID::consumer_device_keypress(const uint16_t keyCode)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr)
    {
        return;
    }

    if (cdcEventsToProcess.size() + 2 <= MAX_PENDING_HID_EVENTS)
    {
        cdcEventsToProcess.push(std::pair<uint8_t, uint16_t>(RID_CONSUMER_CONTROL, keyCode));
        cdcEventsToProcess.push(std::pair<uint8_t, uint16_t>(RID_CONSUMER_CONTROL, 0));
    }
}

/** @brief Queue relative mouse movement, bounded by HID queue capacity. */
void USBHID::mouseMove(int8_t xDelta, int8_t yDelta)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr)
    {
        return;
    }

    MOUSE_EVENT event;
    event.xDelta = xDelta;
    event.yDelta = yDelta;

    if (mouseEventsToProcess.size() < MAX_PENDING_HID_EVENTS) mouseEventsToProcess.push(event);
}

/** @brief Queue non-zero vertical or horizontal wheel movement. */
void USBHID::mouseScroll(int8_t vertical, int8_t horizontal)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr || (vertical == 0 && horizontal == 0)) return;
    if (mouseEventsToProcess.size() >= MAX_PENDING_HID_EVENTS) return;

    MOUSE_EVENT event;
    event.vertical = vertical;
    event.horizontal = horizontal;
    event.scrollReport = true;
    mouseEventsToProcess.push(event);
}

/** @brief Queue button transition for button index 0 through 2. */
void USBHID::mouseButton(uint8_t button, bool pressed)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr || button > 2) return;
    if (mouseEventsToProcess.size() >= MAX_PENDING_HID_EVENTS) return;
    MOUSE_EVENT event;
    const uint8_t buttonMask = static_cast<uint8_t>(1u << button);
    if (pressed) mouseButtonMask |= buttonMask;
    else mouseButtonMask &= static_cast<uint8_t>(~buttonMask);
    event.buttons = mouseButtonMask;
    event.buttonReport = true;
    mouseEventsToProcess.push(event);
}

/** @brief Drop queued input and queue keyboard/mouse release reports. */
void USBHID::releaseAll()
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    clearQueuesUnlocked();
    queueReleaseReportsUnlocked();
}

void USBHID::queueReleaseReportsUnlocked()
{
    if (usb_hid == nullptr || !TinyUSBDevice.mounted()) return;
    KEY_EVENT keyRelease = {};
    keyRelease.ID = RID_KEYBOARD;
    eventsToProcess.push(keyRelease);

    MOUSE_EVENT mouseRelease = {};
    mouseRelease.buttonReport = true;
    mouseRelease.buttons = 0;
    mouseEventsToProcess.push(mouseRelease);
    cdcEventsToProcess.emplace(RID_CONSUMER_CONTROL, 0);
}

/**
 * @brief Send at most one queued HID report when host is ready.
 * @param[in,out] prefs Persistent settings, reserved for profile consistency.
 * @return None.
 * @note Runs from Arduino loop and waits HID_WAIT_TIME after successful send
 *       to respect host report pacing.
 */
void USBHID::loop(Preferences &prefs)
{
    const std::lock_guard<std::mutex> lock(queueMutex);
    if (usb_hid == nullptr)
    {
        return;
    }

    // not enumerated()/mounted() yet: nothing to do
    if (!TinyUSBDevice.mounted())
    {
        if (hidWasMounted)
        {
            clearQueuesUnlocked();
            hidWasMounted = false;
        }
        hidNotReadySince = 0;
        return;
    }

    hidWasMounted = true;

    if (!eventsToProcess.empty() || !cdcEventsToProcess.empty() || !mouseEventsToProcess.empty())
    {
        // Remote wakeup
        if (TinyUSBDevice.suspended())
        {
            // Wake the host only when it suspended the device and enabled
            // USB remote wakeup.
            TinyUSBDevice.remoteWakeup();
        }

        if (usb_hid->ready())
        {
            hidNotReadySince = 0;
            if (!cdcEventsToProcess.empty())
            {
                Debug::Log.info(TAG_USB, "CDC report");
                if (usb_hid->sendReport16(cdcEventsToProcess.front().first, cdcEventsToProcess.front().second))
                {
                    cdcEventsToProcess.pop();
                    delay(HID_WAIT_TIME);
                }
            }
            else if (!mouseEventsToProcess.empty())
            {
                auto report = mouseEventsToProcess.front();
                bool sent = report.scrollReport
                    ? usb_hid->mouseScroll(RID_MOUSE, report.vertical, report.horizontal)
                    : report.buttonReport
                    ? (report.buttons != 0
                        ? usb_hid->mouseButtonPress(RID_MOUSE, report.buttons)
                        : usb_hid->mouseButtonRelease(RID_MOUSE))
                    : usb_hid->mouseMove(RID_MOUSE, report.xDelta, report.yDelta);

                if (sent)
                {
                    mouseEventsToProcess.pop();
                    delay(HID_WAIT_TIME);
                }
                else
                {
                    Debug::Log.error(TAG_USB, "Send data mouse failed");
                }
            }
            else
            {
                bool sent = false;
                auto report = eventsToProcess.front();
                if (report.Modifiers == 0 && report.Keycode[0] == 0)
                {
                    sent = usb_hid->keyboardRelease(report.ID);
                }
                else
                {
                    sent = usb_hid->keyboardReport(report.ID, report.Modifiers, report.Keycode);
                }
                
                if (sent)
                {
                    eventsToProcess.pop();
                    delay(HID_WAIT_TIME);
                }
                else
                {
                    Debug::Log.error(TAG_USB, "Send data keyboard failed");
                }
            }
        }
        else
        {
            if (hidNotReadySince == 0) hidNotReadySince = millis();
            else if (millis() - hidNotReadySince >= HID_STALL_TIMEOUT_MS)
            {
                clearQueuesUnlocked();
                queueReleaseReportsUnlocked();
                hidNotReadySince = 0;
                Debug::Log.warning(TAG_USB, "HID queue cleared after host stall");
            }
        }
    }
}
