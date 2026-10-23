/** @file WebWebSocket.cpp @brief Realtime WebSocket channels: VNC bridge, audio and HID input. */

#ifndef NO_WEB
#include "WebInternal.h"

#include <ArduinoJson.h>
#include <Adafruit_TinyUSB.h>

#include <cstring>
#include <unordered_set>

#include "../../Devices/Display/DisplayDevice.h"
#include "../../Devices/Microphone/MicrophoneDevice.h"
#include "../../Devices/USB/USBCore.h"
#include "../../Devices/USB/USBCDC.h"
#include "../../Devices/USB/USBHID.h"
#include "../../Attacks/Agent/Agent.h"
#include "Diagnostics/Logging.h"

#define LOG_WEB "WEB"

static std::atomic<uint32_t> inputOwnerId{0};
static std::atomic<uint32_t> vncClientCount{0};
static std::mutex vncClientsMutex;
static std::unordered_set<uint32_t> vncClientIds;
static std::atomic<uint32_t> audioClientCount{0};
static const char *remoteAddress = "127.0.0.1:7002";

void WebInternal::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                            void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    if (!Devices::USB::CDC.isReady())
    {
      Debug::Log.warning(LOG_WEB, "Remote client rejected: USB serial bridge is unavailable.");
      if (client != nullptr) client->close(1013, "USB serial unavailable");
      return;
    }
    if (!Attacks::Agent.isAgentConnected())
    {
      Debug::Log.warning(LOG_WEB, "Remote client rejected: Agent is offline.");
      if (client != nullptr) client->close(1013, "Agent unavailable");
      return;
    }
    bool firstClient = false;
    if (client != nullptr)
    {
      const std::lock_guard<std::mutex> lock(vncClientsMutex);
      firstClient = vncClientIds.insert(client->id()).second && vncClientIds.size() == 1;
      vncClientCount.store(static_cast<uint32_t>(vncClientIds.size()), std::memory_order_release);
      // Queue bridge commands under the ownership lock: a reconnect must
      // not overtake the previous session's final disconnect.
      if (firstClient)
      {
        const uint32_t remoteAddLen = strlen(remoteAddress);
        Devices::USB::CDC.writeBinary(HostCommand::WSCONNECT, (uint8_t *)remoteAddress, remoteAddLen);
      }
    }
    if (firstClient)
    {
#ifndef NO_TFT
      Devices::Display.wakeForActivity();
#endif
    }
  }
  else if (type == WS_EVT_DATA && data != nullptr && len != 0)
  {
    if (client != nullptr)
    {
      const std::lock_guard<std::mutex> lock(vncClientsMutex);
      if (vncClientIds.find(client->id()) != vncClientIds.end())
        Devices::USB::CDC.writeBinary(HostCommand::WSDATA, data, len);
    }
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    if (client != nullptr)
    {
      const std::lock_guard<std::mutex> lock(vncClientsMutex);
      const size_t erased = vncClientIds.erase(client->id());
      vncClientCount.store(static_cast<uint32_t>(vncClientIds.size()), std::memory_order_release);
      if (erased != 0 && vncClientIds.empty())
        Devices::USB::CDC.writeBinary(HostCommand::WSDISCONNECT, nullptr, 0);
    }
  }
  else
  {
    Devices::USB::CDC.writeDebugString("Got an unhandler websocket event: " + std::to_string(type));
  }
}

void WebInternal::onWsAudioEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                                 void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    const uint32_t previous = audioClientCount.fetch_add(1, std::memory_order_acq_rel);
    if (previous == 0) Devices::Mic.startCapture();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    uint32_t count = audioClientCount.load(std::memory_order_acquire);
    while (count != 0 && !audioClientCount.compare_exchange_weak(count, count - 1,
                                                                  std::memory_order_acq_rel,
                                                                  std::memory_order_acquire)) {}
    if (count == 1) Devices::Mic.stopCapture();
  }
}

void WebInternal::onInputEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                               void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    if (inputOwnerId.load(std::memory_order_acquire) != 0)
    {
      // A browser reload can connect before the old DISCONNECT callback runs.
      // Release the old HID state and let the newest controller take ownership.
      Debug::Log.warning(LOG_WEB, "USB HID already in use; replacing owner.");
      Devices::USB::HID.releaseAll();
    }
    const char *hidError = nullptr;
    if (!TinyUSBDevice.mounted())
    {
      hidError = "USB HID not mounted";
    }
    else if (Devices::USB::Core.currentDeviceType() != USBDeviceType::Serial ||
             Devices::USB::Core.currentClassType() != USBClassType::HID)
    {
      hidError = "Set USB mode: Serial + HID";
    }
    else if (!Devices::USB::HID.isReady())
    {
      hidError = "USB HID unavailable";
    }
    if (hidError != nullptr)
    {
      Debug::Log.warning(LOG_WEB, "Input client rejected: USB HID is unavailable.");
      if (client != nullptr) client->close(1013, hidError);
      return;
    }
    inputOwnerId.store(client != nullptr ? client->id() : 0, std::memory_order_release);
    Debug::Log.info("HID", "Connected");
#ifndef NO_TFT
    Devices::Display.wakeForActivity();
#endif
    return;
  }
  if (type == WS_EVT_DISCONNECT)
  {
    if (client != nullptr && client->id() == inputOwnerId.load(std::memory_order_acquire))
    {
      Devices::USB::HID.releaseAll();
      inputOwnerId.store(0, std::memory_order_release);
      Debug::Log.info("HID", "Disconnected");
    }
    return;
  }
  if (client == nullptr || client->id() != inputOwnerId.load(std::memory_order_acquire)) return;
  if (type != WS_EVT_DATA || data == nullptr || len == 0 || len > 512) return;
  AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
  if (info == nullptr || !info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;

  JsonDocument command;
  if (deserializeJson(command, data, len)) return;
  const char *kind = command["type"] | "";
  if (strcmp(kind, "move") == 0)
  {
    const int x = command["x"] | 0;
    const int y = command["y"] | 0;
    if (x >= -127 && x <= 127 && y >= -127 && y <= 127)
      Devices::USB::HID.mouseMove(static_cast<int8_t>(x), static_cast<int8_t>(y));
  }
  else if (strcmp(kind, "scroll") == 0)
  {
    const int vertical = command["vertical"] | 0;
    const int horizontal = command["horizontal"] | 0;
    if (vertical >= -127 && vertical <= 127 && horizontal >= -127 && horizontal <= 127)
      Devices::USB::HID.mouseScroll(static_cast<int8_t>(vertical), static_cast<int8_t>(horizontal));
  }
  else if (strcmp(kind, "button") == 0)
  {
    const int button = command["button"] | -1;
    const bool pressed = command["pressed"] | false;
    if (button >= 0 && button <= 2)
      Devices::USB::HID.mouseButton(static_cast<uint8_t>(button), pressed);
  }
  else if (strcmp(kind, "key") == 0)
  {
    const int key = command["key"] | 0;
    const int modifiers = command["modifiers"] | 0;
    if (key >= 0 && key <= 0xFF && modifiers >= 0 && modifiers <= 0xFF)
      Devices::USB::HID.keyboard_press(static_cast<uint8_t>(modifiers), static_cast<uint8_t>(key), 0, 0, 0, 0, 0);
  }
  else if (strcmp(kind, "release") == 0)
  {
    Devices::USB::HID.keyboard_release();
  }
  else if (strcmp(kind, "lock") == 0)
  {
    Debug::Log.info("HID", "Locked");
  }
  else if (strcmp(kind, "unlock") == 0)
  {
    Debug::Log.info("HID", "Unlocked");
  }
}

bool WebInternal::webVncActive()
{
  return vncClientCount.load(std::memory_order_acquire) != 0;
}

bool WebInternal::webHidActive()
{
  return inputOwnerId.load(std::memory_order_acquire) != 0;
}

void WebInternal::webResetRealtimeState()
{
  inputOwnerId.store(0, std::memory_order_release);
  audioClientCount.store(0, std::memory_order_release);
  const std::lock_guard<std::mutex> lock(vncClientsMutex);
  vncClientIds.clear();
  vncClientCount.store(0, std::memory_order_release);
}

#endif
