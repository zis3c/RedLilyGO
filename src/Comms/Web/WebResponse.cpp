/** @file WebResponse.cpp @brief JSON response builders for telemetry, files and health. */

#ifndef NO_WEB
#include "WebInternal.h"
#include "WebServer.h"

#include <ArduinoJson.h>
#include "AsyncJson.h"
#include <esp_timer.h>
#include <esp_system.h>
#include <Adafruit_TinyUSB.h>

#include "../../Devices/Storage/StorageDevice.h"
#include "../../Devices/WiFi/WiFiDevice.h"
#include "../../Devices/Platform/BoardSupport.h"
#include "../../Devices/USB/USBCore.h"
#include "../../Devices/USB/USBHID.h"
#include "../../Devices/USB/USBNCM.h"
#include "../../Attacks/Agent/Agent.h"
#include "../../Attacks/Marauder/Marauder.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "Diagnostics/Logging.h"
#include "../../Utilities/Settings.h"
#include "../../version.h"

#include "../../Utilities/TemperatureRead.h"
using namespace WebRequestValidation;
using namespace WebInternal;

static constexpr uint32_t STORAGE_TELEMETRY_CACHE_MS = 5000;
static std::mutex telemetryStorageMutex;
static uint32_t storageTelemetryReadAt = 0;
static uint64_t storageTelemetryUsed = 0;
static uint64_t storageTelemetryTotal = 0;
static bool storageTelemetryAvailable = false;

static void refreshStorageTelemetryCache(uint64_t &used, uint64_t &total, bool &available)
{
  const std::lock_guard<std::mutex> lock(telemetryStorageMutex);
  if (!Devices::Storage.isRunning())
  {
    storageTelemetryReadAt = 0;
    storageTelemetryUsed = 0;
    storageTelemetryTotal = 0;
    storageTelemetryAvailable = false;
    used = 0;
    total = 0;
    available = false;
    return;
  }

  const uint32_t now = millis();
  if (storageTelemetryReadAt != 0 && now - storageTelemetryReadAt < STORAGE_TELEMETRY_CACHE_MS)
  {
    used = storageTelemetryUsed;
    total = storageTelemetryTotal;
    available = storageTelemetryAvailable;
    return;
  }
  storageTelemetryReadAt = now;
  storageTelemetryAvailable = Devices::Storage.getUsage(storageTelemetryUsed, storageTelemetryTotal);
  if (!storageTelemetryAvailable)
  {
    storageTelemetryUsed = 0;
    storageTelemetryTotal = 0;
  }
  used = storageTelemetryUsed;
  total = storageTelemetryTotal;
  available = storageTelemetryAvailable;
}

std::string WebInternal::prettyPrintUptime()
{
  const uint64_t totalMinutes = static_cast<uint64_t>(esp_timer_get_time()) / 60000000;
  const uint64_t days = totalMinutes / 1440;
  const uint64_t hours = (totalMinutes / 60) % 24;
  const uint64_t minutes = totalMinutes % 60;
  std::string ret;
  if (days != 0)
  {
    ret += std::to_string(days) + (days == 1 ? " day" : " days");
  }
  if (hours != 0)
  {
    if (!ret.empty()) ret += " ";
    ret += std::to_string(hours) + (hours == 1 ? " hour" : " hours");
  }
  if (minutes != 0)
  {
    if (!ret.empty()) ret += " ";
    ret += std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes");
  }
  if (ret.empty())
  {
    ret = "A few seconds";
  }

  return ret;
}

const char *WebInternal::resetReasonName()
{
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt_watchdog";
    case ESP_RST_TASK_WDT: return "task_watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

void WebInternal::sendTelemetryResponse(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json", 2048);
  if (response == nullptr)
  {
    request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
    return;
  }
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  const bool storageMounted = Devices::Storage.isRunning();
  uint64_t storageUsed = 0;
  uint64_t storageTotal = 0;
  bool storageAvailable = false;
  refreshStorageTelemetryCache(storageUsed, storageTotal, storageAvailable);
  JsonObject storage = root["storage"].to<JsonObject>();
  storage["mounted"] = storageMounted;
  storage["available"] = storageAvailable;
  storage["totalBytes"] = storageTotal;
  storage["usedBytes"] = storageUsed;
  storage["freeBytes"] = storageAvailable ? storageTotal - storageUsed : 0;
  storage["usedPercent"] = storageAvailable && storageTotal != 0
    ? (static_cast<double>(storageUsed) * 100.0 / static_cast<double>(storageTotal)) : 0.0;

  const uint32_t heapSize = ESP.getHeapSize();
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t minFreeHeap = ESP.getMinFreeHeap();
  const uint32_t heapUsed = freeHeap < heapSize ? heapSize - freeHeap : 0;
  root["freeHeap"] = freeHeap;
  root["minFreeHeap"] = minFreeHeap;
  root["heapSize"] = heapSize;
  root["heapUsagePc"] = heapSize > 0
    ? static_cast<int>((static_cast<uint64_t>(heapUsed) * 100ULL) / heapSize) : 0;
  root["uptime"] = prettyPrintUptime();
  root["temperatureC"] = readBoardTemperatureC();
  root["wifiSsid"] = preferences != nullptr ? preferences->getString("wifi-ap", "RedLilyGO") : String();
  root["wifiPasswordConfigured"] = preferences != nullptr && preferences->getString("wifi-pwd", "redlilygo").length() != 0;
  root["wifiPassword"] = preferences != nullptr ? preferences->getString("wifi-pwd", "redlilygo") : String();
  root["portalSsid"] = preferences != nullptr ? preferences->getString("portal-ssid", "RedLilyGO-Portal") : String();
  root["portalPassword"] = preferences != nullptr ? preferences->getString("portal-pwd", "") : String();
  root["wifiApMode"] = preferences == nullptr || preferences->getBool("wifi-ap-mode", true);
  root["status"] = Attacks::Ducky.getPayloadRunningStatus();
  root["marauderActive"] = Attacks::Marauder.isActive();
  root["errorCount"] = Attacks::Ducky.getTotalErrors();
  root["USBmode"] = Devices::USB::Core.getCurrentUSBMode();
  root["usbDeviceType"] = static_cast<uint16_t>(Devices::USB::Core.currentDeviceType());
  root["configuredUsbDeviceType"] = preferences != nullptr ? preferences->getUShort("usbDeviceType", 1) : 1;
  root["usbClassType"] = static_cast<uint16_t>(Devices::USB::Core.currentClassType());
  root["usbMounted"] = TinyUSBDevice.mounted();
  root["usbHidReady"] = Devices::USB::HID.isReady();
  root["safeMode"] = Devices::Board.isSafeMode();
  root["agentConnected"] = Attacks::Agent.isAgentConnected();
  root["machineName"] = Attacks::Agent.machineName();
  root["agentVersion"] = Attacks::Agent.agentVersion();
  root["agentProtocolCompatible"] = Attacks::Agent.protocolCompatible();
  root["stealth"] = preferences != nullptr && preferences->getBool("stealth", false);
  root["autorun"] = preferences != nullptr && preferences->getBool("autorun", true);
  root["powersave"] = preferences != nullptr && preferences->getBool("powersave", false);
  root["screensaver"] = preferences != nullptr && preferences->getBool("screensaver", true);
  root["channelHopDelay"] = Attacks::Marauder.getChannelHopDelay();
  auto capabilities = root["capabilities"].to<JsonArray>();
#ifndef NO_SD
  if (storageMounted) capabilities.add("SD");
#endif
#ifndef NO_WIFI
  if (Devices::Wifi.getState()) capabilities.add("WIFI");
#endif
#ifndef NO_TFT
  capabilities.add("TFT");
#endif
#ifndef NO_BUTTON
  capabilities.add("BUTTON");
#endif
#ifndef NO_LED
  capabilities.add("LED");
#endif
#ifndef NO_ESP_MARAUDER
  capabilities.add("MARAUDER");
#endif
  if (document.overflowed())
  {
    delete response;
    request->send(503, "application/json", "{\"error\":\"telemetry_response_overflow\"}");
    return;
  }
  serializeJson(document, *response);
  request->send(response);
}

void WebInternal::sendRuntimeLogsResponse(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json", 2048);
  if (response == nullptr)
  {
    request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
    return;
  }
  JsonDocument document;
  auto logs = document["logMessages"].to<JsonArray>();
  Debug::Log.appendLogsToJson(logs);
  if (document.overflowed())
  {
    delete response;
    request->send(503, "application/json", "{\"error\":\"logs_response_overflow\"}");
    return;
  }
  serializeJson(document, *response);
  request->send(response);
}

void WebInternal::sendFilesResponse(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json", 4096);
  if (response == nullptr)
  {
    request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
    return;
  }
  JsonDocument document;
  JsonObject root = document.to<JsonObject>();
  uint64_t storageUsed = 0;
  uint64_t storageTotal = 0;
  const bool mounted = Devices::Storage.isRunning();
  const bool available = Devices::Storage.getUsage(storageUsed, storageTotal);
  auto storage = root["storage"].to<JsonObject>();
  storage["mounted"] = mounted;
  storage["available"] = available;
  storage["totalBytes"] = storageTotal;
  storage["usedBytes"] = storageUsed;
  storage["freeBytes"] = available ? storageTotal - storageUsed : 0;
  auto details = root["fileDetails"].to<JsonArray>();

  if (request->hasParam("directory"))
  {
    const String requestedPath = request->getParam("directory")->value();
    const std::string directory = requestedPath.c_str();
    if (!Devices::Storage.isRunning())
    {
      delete response;
      request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
      return;
    }
    if (!StorageDevice::isSafeUserPath(directory))
    {
      delete response;
      request->send(400, "application/json", "{\"error\":\"invalid_directory\"}");
      return;
    }

    std::vector<std::string> directories;
    std::vector<std::string> files;
    bool directoriesTruncated = false;
    bool filesTruncated = false;
    if (!Devices::Storage.listDirectory(directory, directories, files, MAX_WEB_FILE_ENTRIES,
                                        directoriesTruncated, filesTruncated))
    {
      delete response;
      request->send(404, "application/json", "{\"error\":\"directory_not_found\"}");
      return;
    }

    root["directory"] = directory.c_str();
    auto directoryListing = root["directories"].to<JsonArray>();
    size_t directoryCount = 0;
    for (const auto &child : directories)
    {
      if (directoryCount++ >= MAX_WEB_FILE_ENTRIES) break;
      directoryListing.add(child.c_str());
    }
    size_t fileCount = 0;
    for (const auto &filename : files)
    {
      if (fileCount++ >= MAX_WEB_FILE_ENTRIES) break;
      const String path = filename.c_str();
      JsonObject detail = details.add<JsonObject>();
      detail["path"] = path;
      const int slash = path.lastIndexOf('/');
      detail["name"] = slash >= 0 ? path.substring(slash + 1) : path;
      detail["size"] = Devices::Storage.getFileSize(path.c_str());
      detail["type"] = fileTypeForPath(path);
      detail["directory"] = directory.c_str();
    }
    root["fileListingTruncated"] = filesTruncated;
    root["directoryListingTruncated"] = directoriesTruncated;
    if (document.overflowed())
    {
      delete response;
      request->send(503, "application/json", "{\"error\":\"files_response_overflow\"}");
      return;
    }
    serializeJson(document, *response);
    request->send(response);
    return;
  }

  const auto files = Devices::Storage.listFiles();
  size_t count = 0;
  for (const auto &filename : files)
  {
    if (count++ >= MAX_WEB_FILE_ENTRIES) break;
    const String path = filename.c_str();
    JsonObject detail = details.add<JsonObject>();
    detail["path"] = path;
    const int slash = path.lastIndexOf('/');
    detail["name"] = slash >= 0 ? path.substring(slash + 1) : path;
    detail["size"] = Devices::Storage.getFileSize(path.c_str());
    detail["type"] = fileTypeForPath(path);
    detail["directory"] = slash > 0 ? path.substring(0, slash) : "/";
  }
  root["fileListingTruncated"] = files.size() > MAX_WEB_FILE_ENTRIES;
  if (document.overflowed())
  {
    delete response;
    request->send(503, "application/json", "{\"error\":\"files_response_overflow\"}");
    return;
  }
  serializeJson(document, *response);
  request->send(response);
}

void WebInternal::sendDashboardResponse(AsyncWebServerRequest *request)
{
  // /data.json walks the SD tree. Serialize snapshots so the several UI
  // startup polls cannot access the filesystem concurrently.
  const std::lock_guard<std::mutex> snapshotLock(dataSnapshotMutex);

  // File page needs storage metadata only. Build this small response before
  // allocating the full dashboard document, otherwise VNC + file navigation
  // can exhaust heap and reset the web server.
  if (request->hasParam("files"))
  {
    AsyncJsonResponse *fileResponse = new AsyncJsonResponse();
    if (fileResponse == nullptr)
    {
      request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
      return;
    }
    JsonObject fileRoot = fileResponse->getRoot();
    uint64_t storageUsed = 0;
    uint64_t storageTotal = 0;
    const bool storageMounted = Devices::Storage.isRunning();
    const bool storageAvailable = Devices::Storage.getUsage(storageUsed, storageTotal);
    JsonObject storage = fileRoot["storage"].to<JsonObject>();
    storage["mounted"] = storageMounted;
    storage["available"] = storageAvailable;
    storage["totalBytes"] = storageTotal;
    storage["usedBytes"] = storageUsed;
    storage["freeBytes"] = storageAvailable ? storageTotal - storageUsed : 0;
    storage["usedPercent"] = storageAvailable
      ? (static_cast<double>(storageUsed) * 100.0 / static_cast<double>(storageTotal)) : 0.0;

    auto fileListing = fileRoot["fileListing"].to<JsonArray>();
    auto fileDetails = fileRoot["fileDetails"].to<JsonArray>();
    const auto files = Devices::Storage.listFiles();
    size_t fileCount = 0;
    for (const auto &filename : files)
    {
      if (fileCount++ >= MAX_WEB_FILE_ENTRIES) break;
      fileListing.add(filename);
      const String path = filename.c_str();
      JsonObject detail = fileDetails.add<JsonObject>();
      detail["path"] = path;
      const int slash = path.lastIndexOf('/');
      detail["name"] = slash >= 0 ? path.substring(slash + 1) : path;
      detail["size"] = Devices::Storage.getFileSize(path.c_str());
      detail["type"] = fileTypeForPath(path);
      detail["directory"] = slash > 0 ? path.substring(0, slash) : "/";
    }
    fileRoot["directories"].to<JsonArray>();
    fileRoot["fileListingTruncated"] = files.size() > MAX_WEB_FILE_ENTRIES;
    fileRoot["directoryListingTruncated"] = false;

    // No dynamic fields are added after this point, so the response length
    // is stable and can be emitted without copying JSON into another buffer.
    fileResponse->setLength();
    request->send(fileResponse);
    return;
  }

  AsyncJsonResponse *response = new AsyncJsonResponse();
  if (response == nullptr)
  {
    request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
    return;
  }
  JsonObject root = response->getRoot();

  uint64_t storageUsed = 0;
  uint64_t storageTotal = 0;
  const bool storageMounted = Devices::Storage.isRunning();
  const bool storageAvailable = Devices::Storage.getUsage(storageUsed, storageTotal);
  const bool storageHasCapacity = storageAvailable && storageTotal != 0;
  root["sdCardPercentFull"] = storageHasCapacity
    ? static_cast<uint8_t>((storageUsed * 100ULL) / storageTotal) : 0;
  auto storage = root["storage"].to<JsonObject>();
  storage["mounted"] = storageMounted;
  storage["available"] = storageAvailable;
  storage["totalBytes"] = storageTotal;
  storage["usedBytes"] = storageUsed;
  storage["freeBytes"] = storageAvailable ? storageTotal - storageUsed : 0;
  storage["usedPercent"] = storageHasCapacity
    ? (static_cast<double>(storageUsed) * 100.0 / static_cast<double>(storageTotal)) : 0.0;
  root["uptime"] = prettyPrintUptime();
  root["temperatureC"] = readBoardTemperatureC();
  const String wifiSsid = preferences != nullptr ? preferences->getString("wifi-ap", "RedLilyGO") : String();
  const String wifiPassword = preferences != nullptr ? preferences->getString("wifi-pwd", "redlilygo") : String();
  root["wifiSsid"] = wifiSsid;
  root["wifiPasswordConfigured"] = !wifiPassword.isEmpty();
  root["wifiPassword"] = wifiPassword;
  root["portalSsid"] = preferences != nullptr ? preferences->getString("portal-ssid", "RedLilyGO-Portal") : String();
  root["portalPassword"] = preferences != nullptr ? preferences->getString("portal-pwd", "") : String();
  root["wifiApMode"] = preferences == nullptr || preferences->getBool("wifi-ap-mode", true);

  const bool marauderReady = Attacks::Marauder.isRunning();
  const bool marauderActive = Attacks::Marauder.isActive();
  std::string currentStatus = Attacks::Ducky.getPayloadRunningStatus();
  if (Attacks::Marauder.isEvilPortalActive()) {
      currentStatus = "Evil Portal";
  } else if (marauderActive) {
      currentStatus = "Marauder radio";
  } else if (currentStatus == "Idle") {
      if (marauderReady) {
          currentStatus = "Ready";
      } else {
          currentStatus = "Off";
      }
  }
  root["status"] = currentStatus;
  root["marauderReady"] = marauderReady;
  root["marauderActive"] = marauderActive;
  root["wifiCaptureActive"] = Attacks::Marauder.isWifiCaptureActive();
  root["activeMode"] = Attacks::Marauder.isEvilPortalActive() ? "portal" :
                        (marauderActive ? "marauder" :
                         (currentStatus == "Running" ? "ducky" : "idle"));

  root["errorCount"] = Attacks::Ducky.getTotalErrors();
  root["USBmode"] = Devices::USB::Core.getCurrentUSBMode();
  root["wifiRssi"] = Devices::Wifi.signalStrength();
  root["usbDeviceType"] = static_cast<uint16_t>(Devices::USB::Core.currentDeviceType());
  root["configuredUsbDeviceType"] = preferences != nullptr ? preferences->getUShort("usbDeviceType", 1) : 1;
  root["usbClassType"] = static_cast<uint16_t>(Devices::USB::Core.currentClassType());
  root["usbMounted"] = TinyUSBDevice.mounted();
  root["usbHidReady"] = Devices::USB::HID.isReady();
  root["usbHidOwned"] = Comms::Web.isHidActive();
  root["vncActive"] = Comms::Web.isVncActive();
  root["usbNcmCapture"] = Devices::USB::NCM.isPacketCollectionActive();
  root["freeHeap"] = ESP.getFreeHeap();
  root["heapSize"] = ESP.getHeapSize();
  root["agentConnected"] = Attacks::Agent.isAgentConnected();
  root["machineName"] = Attacks::Agent.machineName();
  root["agentVersion"] = Attacks::Agent.agentVersion();
  root["agentProtocolCompatible"] = Attacks::Agent.protocolCompatible();
  root["stealth"] = preferences != nullptr && preferences->getBool("stealth", false);
  root["autorun"] = preferences != nullptr && preferences->getBool("autorun", true);
  root["powersave"] = preferences != nullptr && preferences->getBool("powersave", false);
  root["screensaver"] = preferences != nullptr && preferences->getBool("screensaver", true);
    root["version"] = GIT_COMMIT_HASH;

  root["channelHopDelay"] = Attacks::Marauder.getChannelHopDelay();
  root["deauthCount"] = Attacks::Marauder.getDeauthCount();

  const uint32_t heapSize = ESP.getHeapSize();
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t heapUsed = freeHeap < heapSize ? heapSize - freeHeap : 0;
  const int heapUsagePc = heapSize > 0
    ? static_cast<int>((static_cast<uint64_t>(heapUsed) * 100ULL) / heapSize)
    : 0;

  root["heapUsagePc"] = heapUsagePc > 100 ? 100 : heapUsagePc;
  root["numCores"] = ESP.getChipCores();
  root["chipModel"] = ESP.getChipModel();

  auto fileListing = root["fileListing"].to<JsonArray>();
  auto directories = root["directories"].to<JsonArray>();
  auto fileDetails = root["fileDetails"].to<JsonArray>();
  bool fileListingTruncated = false;
  bool directoryListingTruncated = false;
  root["fileListingTruncated"] = fileListingTruncated;
  root["directoryListingTruncated"] = directoryListingTruncated;
  auto logMessages = root["logMessages"].to<JsonArray>();
  Debug::Log.appendLogsToJson(logMessages);
  auto systemLog = root["systemLog"].to<JsonObject>();
  const String systemLogPath = Devices::Storage.doesFileExist("/Logs/system.log")
    ? "/Logs/system.log" : "/Captures/system.log";
  const bool systemLogAvailable = Devices::Storage.isRunning() &&
    Devices::Storage.doesFileExist(systemLogPath.c_str());
  systemLog["path"] = systemLogPath;
  systemLog["available"] = systemLogAvailable;
  systemLog["size"] = systemLogAvailable ? Devices::Storage.getFileSize(systemLogPath.c_str()) : 0;
  systemLog["status"] = Debug::Log.systemLogStatus().c_str();
  if (preferences != nullptr)
  {
    auto settingsCategories = root["settingCategories"].to<JsonArray>();
    enumerateSettingsAsJson(*preferences, settingsCategories);
  }

  auto capabilities = root["capabilities"].to<JsonArray>();
#ifndef NO_SD
  if (Devices::Storage.isRunning()) capabilities.add("SD");
#endif
#ifndef NO_WIFI
  if (Devices::Wifi.getState()) capabilities.add("WIFI");
#endif
#ifndef NO_TFT
  capabilities.add("TFT");
#endif
#ifndef NO_BUTTON
  capabilities.add("BUTTON");
#endif
#ifndef NO_LED
  capabilities.add("LED");
#endif
#ifndef NO_ESP_MARAUDER
  // Compile-time capability must not depend on the runtime active-command state.
  // Otherwise Tools and Defense cannot be opened to start Marauder.
  capabilities.add("MARAUDER");
#endif

  // File metadata is handled above, so this document is complete and its
  // length is stable. Send it directly without a second String copy.
  response->setLength();
  request->send(response);
}

void WebInternal::sendHealthResponse(AsyncWebServerRequest *request)
{
  AsyncJsonResponse *response = new AsyncJsonResponse();
  if (response == nullptr)
  {
    request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
    return;
  }
  JsonObject root = response->getRoot();
  const bool storageReady = Devices::Storage.isRunning();
  const bool wifiReady = Devices::Wifi.getState();
  const uint32_t freeHeap = ESP.getFreeHeap();
  root["overall"] = (!wifiReady || freeHeap < MIN_HEAP_SIZE) ? "failed" : (!storageReady ? "degraded" : "healthy");
  root["version"] = GIT_COMMIT_HASH;
  root["uptime"] = prettyPrintUptime();
  root["freeHeap"] = freeHeap;
  root["storage"] = storageReady ? "ready" : "unavailable";
  root["wifi"] = wifiReady ? "ready" : "unavailable";
  root["usb"] = Devices::USB::Core.getCurrentUSBMode();
  root["agentConnected"] = Attacks::Agent.isAgentConnected();
  root["agentVersion"] = Attacks::Agent.agentVersion();
  root["agentProtocolCompatible"] = Attacks::Agent.protocolCompatible();
  root["resetReason"] = resetReasonName();
  if (preferences != nullptr)
  {
    root["lastResetReasonCode"] = preferences->getUChar("last-reset-code", 0);
    root["lastBootCrash"] = preferences->getBool("last-rst-crash", false);
    root["lastBacktrace"] = preferences->getString("last-backtrace", "");
  }
  root["safeMode"] = Devices::Board.isSafeMode();
  response->setLength();
  request->send(response);
}

#endif
