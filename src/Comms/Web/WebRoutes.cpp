/** @file WebRoutes.cpp @brief HTTP route handlers grouped by concern. */

#ifndef NO_WEB
#include "WebInternal.h"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <esp_system.h>
#include <esp_timer.h>
#include <mutex>

#include <WiFi.h>

#include "../../Devices/Display/DisplayDevice.h"
#include "../../Devices/Storage/StorageDevice.h"
#include "../../Devices/LED/LEDDevice.h"
#include "../../Devices/WiFi/WiFiDevice.h"
#include "../../Devices/Platform/BoardSupport.h"
#include "../../Devices/Microphone/MicrophoneDevice.h"
#include "../../Devices/USB/USBCore.h"
#include "../../Devices/USB/USBCDC.h"
#include "../../Devices/USB/USBHID.h"
#include "../../Devices/USB/USBNCM.h"
#include "../../Attacks/Agent/Agent.h"
#include "../../Attacks/Marauder/Marauder.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "../../Utilities/Settings.h"
#include "Diagnostics/Logging.h"

#define LOG_WEB "WEB"

using namespace WebRequestValidation;
using namespace WebInternal;

// Marauder startup is deferred until a supported command requests it.
static bool ensureMarauderReady()
{
  if (Attacks::Marauder.isRunning()) return true;
  if (preferences == nullptr || Devices::Board.isSafeMode()) return false;
  Debug::Log.info(LOG_WEB, "Starting Marauder on user request.");
  Attacks::Marauder.begin(*preferences);
  return Attacks::Marauder.isRunning();
}

// Read-only telemetry, health, listing, log and status routes.
bool WebInternal::handleInfoRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/wpad.dat")
  {
    request->send(404);
  }
  else if (url == "/assets/css/redlilygo.css")
  {
    // Older cached pages still request this file. Custom CSS is bundled into
    // styles.min.css, so keep the old URL working with the correct MIME type.
    request->redirect(String("http://") + Devices::Wifi.apHostname().c_str() + "/assets/css/styles.min.css");
  }
  else if (url == "/favicon.ico" || url == "/apple-touch-icon.png" || url == "/apple-touch-icon-precomposed.png")
  {
    request->send(204);
  }
  else if (url == "/telemetry")
  {
    // Telemetry reads atomics/settings only. Do not wait behind SD snapshots.
    sendTelemetryResponse(request);
  }
  else if (url == "/runtime-logs")
  {
    sendRuntimeLogsResponse(request);
  }
  else if (url == "/csrf-token")
  {
    request->send(200, "application/json", String("{\"token\":\"") + csrfToken() + "\"}");
  }
  else if (url == "/files")
  {
    const std::lock_guard<std::mutex> snapshotLock(dataSnapshotMutex);
    sendFilesResponse(request);
  }
  else if (url == "/data.json")
  {
    sendDashboardResponse(request);
  }
  else if (url == "/health")
  {
    sendHealthResponse(request);
  }
  else if (url == "/systemlog")
  {
    if (!Devices::Storage.isRunning())
    {
      request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
      return true;
    }
    size_t tail = 200;
    if (request->hasParam("tail"))
    {
      const String value = request->getParam("tail")->value();
      const int requested = value.toInt();
      if (requested < 1 || requested > 2000 || String(requested) != value)
      {
        request->send(400, "application/json", "{\"error\":\"invalid_tail\"}");
        return true;
      }
      tail = static_cast<size_t>(requested);
    }
    const char *systemLogPath = Devices::Storage.doesFileExist("/Logs/system.log")
      ? "/Logs/system.log" : "/Captures/system.log";
    const std::string log = Devices::Storage.readFileTail(systemLogPath, tail);
    request->send(200, "text/plain; charset=utf-8", String(log.c_str()));
  }
  else if (url == "/wifi/scan")
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    if (!Devices::Wifi.getState())
    {
      request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return true;
    }
    if (!Attacks::Marauder.isRunning())
    {
      request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}");
      return true;
    }
    const std::string json = Attacks::Marauder.getAccessPointsJson();
    request->send(200, "application/json", String(json.c_str()));
#endif
  }
  else
  {
    return false;
  }
  return true;
}

// Payload and command queueing routes.
bool WebInternal::handlePayloadRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/runfile" && request->hasParam("filename"))
  {
    const String filename = request->getParam("filename")->value();
    if (!validStoragePath(filename) || !hasExtensionIgnoreCase(filename, ".ds")) { request->send(400, "text/plain", "Invalid payload path"); return true; }
    if (!Devices::Storage.isRunning()) { request->send(503, "text/plain", "Storage unavailable"); return true; }
    if (!Devices::Storage.doesFileExist(filename.c_str())) { request->send(404, "text/plain", "Payload not found"); return true; }
    Debug::Log.info(LOG_WEB, "Executing payload from web UI");

    Attacks::Ducky.setPayload(filename.c_str());
    request->send(200, "application/json", "{\"ok\":true,\"status\":\"payload_queued\"}");
  }
  else if (url == "/runfile")
  {
    request->send(400, "application/json", "{\"error\":\"missing_filename\"}");
  }
  else if (url == "/rawinput" && request->hasParam("rawCommand"))
  {
    const String cmdline = request->getParam("rawCommand")->value();
    if (!validCommand(cmdline)) { request->send(400, "text/plain", "Invalid command"); return true; }
    if (isMarauderCommand(cmdline))
    {
      if (!Devices::Wifi.getState()) { request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}"); return true; }
      if (!ensureMarauderReady()) { request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}"); return true; }
      Debug::Log.info(LOG_WEB, "Marauder command queued");
      Attacks::Marauder.run(cmdline.c_str());
      request->send(200, "application/json", "{\"ok\":true,\"kind\":\"marauder\",\"status\":\"marauder_radio_queued\",\"message\":\"Marauder radio command queued\"}");
      return true;
    }
    Debug::Log.info(LOG_WEB, "Running Ducky command from raw input");
    Attacks::Ducky.setPayloadCmdLine(cmdline.c_str());
    request->send(200, "application/json", "{\"ok\":true,\"status\":\"command_queued\"}");
  }
  else if (url == "/rawinput")
  {
    request->send(400, "application/json", "{\"error\":\"missing_rawCommand\"}");
  }
  else if (url == "/runagentcmd" && request->hasParam("rawCommand"))
  {
    const String cmdline = request->getParam("rawCommand")->value();
    if (!validCommand(cmdline)) { request->send(400, "text/plain", "Invalid command"); return true; }
    if (!Attacks::Agent.isAgentConnected()) { request->send(503, "text/plain", "Agent unavailable"); return true; }
    if (!Attacks::Agent.protocolCompatible()) { request->send(503, "text/plain", "Agent protocol incompatible"); return true; }
    Debug::Log.info(LOG_WEB, "Running agent command from web UI");

    Attacks::Ducky.setPayloadCmdLine(std::string("AGENT_RUN ") + cmdline.c_str());
    request->send(200, "application/json", "{\"ok\":true,\"status\":\"agent_command_queued\"}");
  }
  else if (url == "/runagentcmd")
  {
    request->send(400, "application/json", "{\"error\":\"missing_rawCommand\"}");
  }
  else if (url == "/showimage" && request->hasParam("filename"))
  {
#ifdef NO_TFT
    request->send(503, "text/plain", "Display unavailable");
#else
    const String filename = request->getParam("filename")->value();
    if (!validStoragePath(filename) || !hasExtensionIgnoreCase(filename, ".png")) { request->send(400, "text/plain", "Invalid image path"); return true; }
    if (!Devices::Storage.isRunning()) { request->send(503, "text/plain", "Storage unavailable"); return true; }
    const std::string imagePath = Devices::Storage.resolveImagePath(filename.c_str());
    if (imagePath.empty()) { request->send(404, "text/plain", "Image not found"); return true; }
    Debug::Log.info(LOG_WEB, "Displaying image from web UI");

    Devices::Display.requestPng(imagePath);

    request->redirect("/index.html");
#endif
  }
  else if (url == "/showimage")
  {
    request->send(400, "application/json", "{\"error\":\"missing_filename\"}");
  }
  else
  {
    return false;
  }
  return true;
}

// Microphone and capture control routes.
bool WebInternal::handleMediaRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/capture" && request->hasParam("source") && request->hasParam("action"))
  {
    const String source = request->getParam("source")->value();
    const String action = request->getParam("action")->value();
    if (action != "start" && action != "stop")
    {
      request->send(400, "application/json", "{\"error\":\"invalid_capture_action\"}");
      return true;
    }
    if (source == "wifi")
    {
#ifdef NO_ESP_MARAUDER
      request->send(503, "application/json", "{\"error\":\"wifi_capture_unavailable\"}");
#else
      if (action == "start" && Attacks::Marauder.isActive())
      {
        request->send(409, "application/json", "{\"error\":\"marauder_busy\"}");
        return true;
      }
      if (!Devices::Wifi.getState() || !ensureMarauderReady())
      {
        request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}");
        return true;
      }
      Attacks::Marauder.run(action == "start" ? "sniffraw" : "stopscan");
      request->send(200, "application/json", action == "start"
        ? "{\"ok\":true,\"status\":\"wifi_capture_started\"}"
        : "{\"ok\":true,\"status\":\"wifi_capture_stopped\"}");
#endif
      return true;
    }
    if (source == "usb")
    {
      if (Devices::USB::Core.currentDeviceType() != USBDeviceType::NCM)
      {
        request->send(503, "application/json", "{\"error\":\"usb_ncm_required\"}");
        return true;
      }
      if (!Devices::Storage.isRunning())
      {
        request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
        return true;
      }
      if (action == "start" && !Devices::USB::NCM.startPacketCollection())
      {
        request->send(503, "application/json", "{\"error\":\"usb_capture_start_failed\"}");
        return true;
      }
      if (action == "stop") Devices::USB::NCM.stopPacketCollection();
      request->send(200, "application/json", action == "start"
        ? "{\"ok\":true,\"status\":\"usb_capture_started\"}"
        : "{\"ok\":true,\"status\":\"usb_capture_stopped\"}");
      return true;
    }
    request->send(400, "application/json", "{\"error\":\"unknown_capture_source\"}");
  }
  else if (url == "/capture")
  {
    request->send(400, "application/json", "{\"error\":\"missing_capture_parameters\"}");
  }
  else if (url == "/mic" && request->hasParam("enabled"))
  {
#ifdef NO_MIC
    request->send(503, "text/plain", "Microphone unavailable");
#else
    const String cmd = request->getParam("enabled")->value();
    if (!cmd.equalsIgnoreCase("true") && !cmd.equalsIgnoreCase("false"))
    {
      request->send(400, "text/plain", "enabled must be true or false");
      return true;
    }
    if (!Devices::Mic.isConfigured())
    {
      request->send(503, "text/plain", "Microphone unavailable");
      return true;
    }
    if (cmd.equalsIgnoreCase("true"))
    {
      Debug::Log.info(LOG_WEB, "Starting Mic capture");
      Devices::Mic.startCapture();
    }
    else
    {
      Debug::Log.info(LOG_WEB, "Stopping Mic capture");
      Devices::Mic.stopCapture();
    }
    request->send(200);
#endif
  }
  else if (url == "/mic")
  {
    request->send(400, "application/json", "{\"error\":\"missing_enabled\"}");
  }
  else
  {
    return false;
  }
  return true;
}

// Marauder, portal and radio routes.
bool WebInternal::handleMarauderRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/marauder" && request->hasParam("marauderCmd"))
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    const String cmd = request->getParam("marauderCmd")->value();
    if (!validCommand(cmd)) { request->send(400, "text/plain", "Invalid command"); return true; }
    if (!Devices::Wifi.getState()) { request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}"); return true; }
    if (!ensureMarauderReady())
    {
      request->send(503, "application/json", Devices::Board.isSafeMode()
        ? "{\"error\":\"marauder_not_ready\",\"safeMode\":true}"
        : "{\"error\":\"marauder_not_ready\"}");
      return true;
    }
    Debug::Log.info(LOG_WEB, "Marauder command queued");
    Attacks::Marauder.run(cmd.c_str());
    if (request->header("Sec-Fetch-Mode") == "navigate")
    {
      request->redirect("/index.html");
      return true;
    }
    request->send(200, "application/json", "{\"ok\":true}");
#endif
  }
  else if (url == "/marauder")
  {
    request->send(400, "application/json", "{\"error\":\"missing_marauderCmd\"}");
  }
  else if (url == "/evilportal/files")
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    if (!Devices::Storage.isRunning()) { request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}"); return true; }
    if (!ensureMarauderReady()) { request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}"); return true; }
    const std::string json = Attacks::Marauder.getEvilPortalFilesJson();
    request->send(200, "application/json", String(json.c_str()));
#endif
  }
  else if (url == "/evilportal")
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    const String action = request->hasParam("action") ? request->getParam("action")->value() : "";
    if (action == "clear")
    {
      if (!Attacks::Marauder.clearEvilPortalHistory()) { request->send(503, "application/json", "{\"error\":\"history_unavailable\"}"); return true; }
      request->send(200, "application/json", "{\"ok\":true,\"status\":\"cleared\"}");
      return true;
    }
    if (!Devices::Wifi.getState()) { request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}"); return true; }
    if (action == "start" && !ensureMarauderReady()) { request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}"); return true; }
    if (action == "start")
    {
      if (!request->hasParam("file")) { request->send(400, "application/json", "{\"error\":\"missing_file\"}"); return true; }
      std::string error;
      const String filename = request->getParam("file")->value();
      const String ssid = request->hasParam("ssid") ? request->getParam("ssid")->value() : String("");
      if (!Attacks::Marauder.startEvilPortal(filename.c_str(), ssid.c_str(), error))
      {
        request->send(400, "application/json", String("{\"error\":\"") + error.c_str() + "\"}");
        return true;
      }
      request->send(200, "application/json", String("{\"ok\":true,\"status\":\"starting\",\"selectedPage\":\"") + filename + "\"}");
      return true;
    }
    if (action == "stop")
    {
      Attacks::Marauder.stopEvilPortal();
      request->send(200, "application/json", "{\"ok\":true,\"status\":\"stopped\"}");
      return true;
    }
    const std::string json = Attacks::Marauder.getEvilPortalJson();
    request->send(200, "application/json", String(json.c_str()));
#endif
  }
  else if (url == "/deauth")
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    if (!Devices::Wifi.getState()) { request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}"); return true; }
    if (!ensureMarauderReady()) { request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}"); return true; }
    const std::string json = "{\"count\":" + std::to_string(Attacks::Marauder.getDeauthCount()) +
                             ",\"events\":" + Attacks::Marauder.getDeauthEventsJson() + "}";
    request->send(200, "application/json", String(json.c_str()));
#endif
  }
  else if (url == "/channelhop" && request->hasParam("delay"))
  {
#ifdef NO_ESP_MARAUDER
    request->send(503, "application/json", "{\"error\":\"marauder_unavailable\"}");
    return true;
#else
    if (!Devices::Wifi.getState()) { request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}"); return true; }
    if (!ensureMarauderReady()) { request->send(503, "application/json", "{\"error\":\"marauder_not_ready\"}"); return true; }
    const String value = request->getParam("delay")->value();
    const int delay = value.toInt();
    if (delay < 1 || delay > 60 || String(delay) != value) { request->send(400, "application/json", "{\"error\":\"invalid_delay\"}"); return true; }
    Attacks::Marauder.setChannelHopDelay(static_cast<uint8_t>(delay));
    request->send(200, "application/json", String("{\"ok\":true,\"delay\":") + Attacks::Marauder.getChannelHopDelay() + "}");
#endif
  }
  else if (url == "/channelhop")
  {
    request->send(400, "application/json", "{\"error\":\"missing_delay\"}");
  }
  else
  {
    return false;
  }
  return true;
}

// Storage download, delete, move and log-clear routes.
bool WebInternal::handleStorageRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/download" && request->hasParam("filename"))
  {
    const String filename = request->getParam("filename")->value();
    if (!validStoragePath(filename)) { request->send(400, "text/plain", "Invalid filename"); return true; }
    if (!Devices::Storage.isRunning()) { request->send(503, "text/plain", "Storage unavailable"); return true; }
    const size_t fileSize = Devices::Storage.getFileSize(filename.c_str());
    if (Devices::Storage.doesFileExist(filename.c_str()))
    {
      AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", fileSize,
        [filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
        {
          return Devices::Storage.readFileChunk(filename.c_str(), index, buffer, maxLen);
        });
      if (response == nullptr)
      {
        request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
        return true;
      }
      response->addHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
      request->send(response);
    }
    else
    {
      Debug::Log.info(LOG_WEB, std::string("File missing ") + filename.c_str());
      request->send(404);
    }
  }
  else if (url == "/download")
  {
    request->send(400, "application/json", "{\"error\":\"missing_filename\"}");
  }
  else if (url == "/delete" && request->hasParam("filename"))
  {
    const String filename = request->getParam("filename")->value();
    if (!validStoragePath(filename)) { request->send(400, "text/plain", "Invalid filename"); return true; }

    Debug::Log.info(LOG_WEB, "File deletion requested from web UI");

    if (!Devices::Storage.isRunning()) { request->send(503, "text/plain", "Storage unavailable"); return true; }
    if (!Devices::Storage.doesFileExist(filename.c_str()))
    {
      request->send(404, "text/plain", "File not found");
      return true;
    }
    if (hasExtensionIgnoreCase(filename, ".ds"))
    {
      Attacks::Ducky.setPayload("");
    }
    if (!Devices::Storage.deleteFile(filename.c_str()))
    {
      request->send(500, "text/plain", "File could not be deleted");
      return true;
    }

    request->redirect("/index.html");
  }
  else if (url == "/delete")
  {
    request->send(400, "application/json", "{\"error\":\"missing_filename\"}");
  }
  else if (url == "/moveFile")
  {
    if (!request->hasParam("from") || !request->hasParam("to"))
    {
      request->send(400, "application/json", "{\"error\":\"missing_move_paths\"}");
      return true;
    }
    const String source = request->getParam("from")->value();
    const String destination = request->getParam("to")->value();
    if (!validStoragePath(source) || !validStoragePath(destination) ||
        destination.substring(destination.lastIndexOf('/') + 1) != source.substring(source.lastIndexOf('/') + 1))
    {
      request->send(400, "application/json", "{\"error\":\"invalid_move_path\"}");
      return true;
    }
    if (!Devices::Storage.isRunning())
    {
      request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
      return true;
    }
    if (!Devices::Storage.moveFile(source.c_str(), destination.c_str()))
    {
      request->send(409, "application/json", "{\"error\":\"move_failed_or_destination_exists\"}");
      return true;
    }
    request->send(200, "application/json", String("{\"ok\":true,\"path\":\"") + destination + "\"}");
  }
  else if (url == "/clearlogs")
  {
    Debug::Log.clearLogs();
    request->send(200, "application/json", "{\"ok\":true,\"status\":\"logs_cleared\"}");
  }
  else
  {
    return false;
  }
  return true;
}

// System control, settings and the upload fallback route.
bool WebInternal::handleSystemRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  if (url == "/system" && request->hasParam("cmd"))
  {
    const String cmd = request->getParam("cmd")->value();
    if (cmd == "reboot")
    {
      Debug::Log.info(LOG_WEB, "Rebooting from web UI.");
      request->send(200, "text/plain", "Rebooting...");
      delay(100);
      ESP.restart();
      return true;
    }
    else if (cmd == "reset")
    {
      Debug::Log.info(LOG_WEB, "Factory reset from web UI.");
      if (preferences != nullptr) resetSettings(*preferences);
      request->send(200, "text/plain", "Resetting...");
      delay(100);
      ESP.restart();
      return true;
    }
    else if (cmd == "format")
    {
      if (!Devices::Storage.isRunning())
      {
        request->send(503, "text/plain", "Storage unavailable");
        return true;
      }
      Debug::Log.info(LOG_WEB, "Formatting SD card from web UI.");
      // Stop every subsystem that may hold an open SD file or raw-sector
      // callback before StorageDevice unmounts and formats the backend.
      Attacks::Ducky.stop();
      Attacks::Marauder.stopAll();
      Devices::USB::Core.end();
      if (preferences != nullptr) Devices::Storage.begin(*preferences, true);
      if (!Devices::Storage.isRunning())
      {
        request->send(500, "text/plain", "SD format failed");
        return true;
      }
      request->send(200, "text/plain", "Formatting SD...");
      delay(100);
      ESP.restart();
      return true;
    }
    request->send(400, "text/plain", "Unknown command");
  }
  else if (url == "/system")
  {
    request->send(400, "application/json", "{\"error\":\"missing_cmd\"}");
  }
  else if (url == "/settings" && preferences != nullptr)
  {
    if (request->hasParam("stealth"))
    {
      const String value = request->getParam("stealth")->value();
      if (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false")) { request->send(400, "text/plain", "Invalid stealth value"); return true; }
      const bool stealth = value.equalsIgnoreCase("true");
      if (preferences->putBool("stealth", stealth) == 0 || preferences->getBool("stealth", !stealth) != stealth) { request->send(500, "text/plain", "Could not save stealth setting"); return true; }
      Devices::LED.changeLEDState(!stealth, 0, 0, 0, 0);
      Debug::Log.info(LOG_WEB, std::string("Stealth mode ") + (stealth ? "enabled" : "disabled"));
      request->send(200, "text/plain", stealth ? "stealth on" : "stealth off");
    }
    else if (request->hasParam("autorun"))
    {
      const String value = request->getParam("autorun")->value();
      if (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false")) { request->send(400, "text/plain", "Invalid autorun value"); return true; }
      const bool autorun = value.equalsIgnoreCase("true");
      if (preferences->putBool("autorun", autorun) == 0 || preferences->getBool("autorun", !autorun) != autorun) { request->send(500, "text/plain", "Could not save autorun setting"); return true; }
      Debug::Log.info(LOG_WEB, std::string("Auto-run ") + (autorun ? "enabled" : "disabled"));
      request->send(200, "text/plain", autorun ? "autorun on" : "autorun off");
    }
    else if (request->hasParam("powersave"))
    {
      const String value = request->getParam("powersave")->value();
      if (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false")) { request->send(400, "text/plain", "Invalid powersave value"); return true; }
      const bool ps = value.equalsIgnoreCase("true");
      if (preferences->putBool("powersave", ps) == 0 || preferences->getBool("powersave", !ps) != ps) { request->send(500, "text/plain", "Could not save powersave setting"); return true; }
      setCpuFrequencyMhz(ps ? 80 : 240);
      Debug::Log.info(LOG_WEB, std::string("Power saving ") + (ps ? "enabled" : "disabled"));
      request->send(200, "text/plain", ps ? "powersave on" : "powersave off");
    }
    else if (request->hasParam("screensaver"))
    {
      const String value = request->getParam("screensaver")->value();
      if (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false")) { request->send(400, "text/plain", "Invalid screensaver value"); return true; }
      const bool enabled = value.equalsIgnoreCase("true");
      if (preferences->putBool("screensaver", enabled) == 0 || preferences->getBool("screensaver", !enabled) != enabled) { request->send(500, "text/plain", "Could not save screensaver setting"); return true; }
      Devices::Display.setScreensaverEnabled(enabled);
      request->send(200, "text/plain", enabled ? "screensaver on" : "screensaver off");
    }
    else if (request->hasParam("portal"))
    {
      const String portalSsid = request->hasParam("ssid") ? request->getParam("ssid")->value() : String();
      const String portalPass = request->hasParam("pass") ? request->getParam("pass")->value() : String();
      const bool portalOpen = request->getParam("open") != nullptr &&
                              request->getParam("open")->value().equalsIgnoreCase("true");
      if (!validWifiText(portalSsid, 29) ||
          (!portalOpen && !portalPass.isEmpty() && (!validWifiText(portalPass, 63) || portalPass.length() < 8)))
      {
        request->send(400, "text/plain", "Invalid portal configuration");
        return true;
      }
      bool saved = setSettingValue(*preferences, "portal-ssid", portalSsid.c_str());
      if (portalOpen)
      {
        saved = setSettingValue(*preferences, "portal-pwd", "") && saved;
      }
      else if (!portalPass.isEmpty())
      {
        saved = setSettingValue(*preferences, "portal-pwd", portalPass.c_str()) && saved;
      }
      if (!saved)
      {
        request->send(500, "text/plain", "Could not save portal configuration");
        return true;
      }
      Debug::Log.info(LOG_WEB, "Evil Portal AP configuration saved.");
      request->send(200, "text/plain", portalOpen ? "Portal AP saved (open network)." : "Portal AP saved.");
    }
    else if (request->hasParam("network"))
    {
      const String apmode = request->hasParam("apmode") ? request->getParam("apmode")->value() : String();
      const String ssid = request->hasParam("ssid") ? request->getParam("ssid")->value() : String();
      const String pass = request->hasParam("pass") ? request->getParam("pass")->value() : String();
      const bool apModeRequested = apmode.equalsIgnoreCase("true");
      const String currentPassword = preferences->getString("wifi-pwd", "");
      const bool keepingCurrentPassword = pass.isEmpty() && currentPassword.length() >= 8;
      if ((!apmode.equalsIgnoreCase("true") && !apmode.equalsIgnoreCase("false")) ||
          ((!apModeRequested || request->hasParam("ssid")) && !validWifiText(ssid, 32)) ||
          (!pass.isEmpty() && !validWifiText(pass, 63)) ||
          (apModeRequested && pass.length() < 8 && !keepingCurrentPassword))
      {
        request->send(400, "text/plain", "Invalid Wi-Fi configuration");
        return true;
      }

      bool saved = true;
      if (request->hasParam("ssid"))
      {
        saved = setSettingValue(*preferences, "wifi-ap", ssid.c_str()) && saved;
      }
      if (request->hasParam("pass") && !pass.isEmpty())
      {
        saved = setSettingValue(*preferences, "wifi-pwd", pass.c_str()) && saved;
      }
      if (request->hasParam("apmode"))
      {
        saved = setSettingValue(*preferences, "wifi-ap-mode", apmode.equalsIgnoreCase("true") ? "1" : "0") && saved;
      }
      if (!saved)
      {
        request->send(500, "text/plain", "Could not save Wi-Fi configuration");
        return true;
      }
      Debug::Log.info(LOG_WEB, "WiFi configuration saved. Rebooting to apply.");
      request->send(200, "text/plain", "Wi-Fi configuration saved. Rebooting...");
      delay(100);
      ESP.restart();
      return true;
    }
    else
    {
      request->send(400, "text/plain", "missing parameter");
    }
  }
  else if (url == "/settings")
  {
    request->send(503, "application/json", "{\"error\":\"settings_unavailable\"}");
  }
  else if (url == "/set" && preferences != nullptr && request->hasParam("name"))
  {
    const String settingName = request->getParam("name")->value();
    if (settingName.isEmpty())
    {
      request->send(400, "application/json", "{\"error\":\"empty_name\"}");
      return true;
    }

    if (request->hasParam("value"))
    {
      const String settingValue = request->getParam("value")->value();

      if (settingName == "usbDeviceType")
      {
        const char *rawValue = settingValue.c_str();
        char *parsedEnd = nullptr;
        const unsigned long deviceType = std::strtoul(rawValue, &parsedEnd, 0);
        if (!settingValue.startsWith("0x") || parsedEnd == rawValue || *parsedEnd != '\0' || deviceType > 2)
        {
          request->send(400, "text/plain", "USB device type must be 0x0000 (None), 0x0001 (Serial), or 0x0002 (NCM)");
          return true;
        }
      }

      if (settingName == "web-pass" && (settingValue.length() < 12 || settingValue.length() > 64))
      {
        request->send(400, "text/plain", "Invalid web password length");
        return true;
      }
      if (settingName == "wifi-ap" && !validWifiText(settingValue, 32))
      {
        request->send(400, "text/plain", "Invalid Wi-Fi SSID");
        return true;
      }
      if (settingName == "wifi-pwd" && (!settingValue.isEmpty() && !validWifiText(settingValue, 63)))
      {
        request->send(400, "text/plain", "Invalid Wi-Fi password");
        return true;
      }
      if (settingName == "web-user" && !validUsername(settingValue))
      {
        request->send(400, "text/plain", "Invalid web username");
        return true;
      }
      if (settingName == "web-auth" && (settingValue == "1" || settingValue.equalsIgnoreCase("true")))
      {
        const String currentPassword = preferences->getString("web-pass", "");
        if (currentPassword.length() < 12 || currentPassword.length() > 64)
        {
          request->send(400, "text/plain", "Set a valid web password before enabling authentication");
          return true;
        }
      }

      Debug::Log.info(LOG_WEB, "Setting update requested from web UI");

      if (setSettingValue(*preferences, settingName.c_str(), settingValue.c_str()))
      {
        Debug::Log.info(LOG_WEB, "New setting has been set");
      }
      else
      {
        Debug::Log.info(LOG_WEB, "Setting could not be set");
        request->send(400, "text/plain", "Invalid setting value");
        return true;
      }
    }
    else
    {
      if ((settingName == "web-pass" || settingName == "web-user") &&
          preferences->getBool("web-auth", false))
      {
        request->send(400, "text/plain", "Disable web authentication before removing credentials");
        return true;
      }
      if (!preferences->isKey(settingName.c_str()))
      {
        request->send(404, "application/json", "{\"error\":\"setting_not_found\"}");
        return true;
      }
      Debug::Log.info(LOG_WEB, "Removing setting");
      if (!preferences->remove(settingName.c_str()))
      {
        request->send(500, "text/plain", "Setting could not be removed");
        return true;
      }
    }
    request->redirect("/index.html");
  }
  else if (url == "/set")
  {
    request->send(preferences == nullptr ? 503 : 400, "application/json",
                  preferences == nullptr ? "{\"error\":\"settings_unavailable\"}" : "{\"error\":\"missing_name\"}");
  }
  else if (url == "/uploadFile")
  {
    request->send(400, "application/json", "{\"error\":\"missing_upload\"}");
  }
  else
  {
    return false;
  }
  return true;
}

// Embedded static assets and the final 404 fallback. Always handles the request.
bool WebInternal::handleStaticRoute(AsyncWebServerRequest *request)
{
  const auto &url = request->url();
  StaticHtmlFile data;
  const char *unknown = "application/octet-stream";

  if (url.isEmpty() || url.length() == 1)
  {
    data = getStaticHtml("/index.html");
  }
  else
  {
    data = getStaticHtml(url);
  }

  if (data.data != nullptr)
  {
    String mimeUrl = url;
    if (mimeUrl.isEmpty() || mimeUrl == "/") mimeUrl = "/index.html";
    else if (mimeUrl == "/remote" || mimeUrl == "/remote/input") mimeUrl = "/vnc/index.html";

    const char *mime = GetMimeType(mimeUrl.c_str());

    // AsyncProgmemResponse streams directly from embedded flash and sends the
    // exact compressed length. Chunked callbacks caused browser asset
    // truncation/reset errors on the larger JS bundles.
    AsyncWebServerResponse *response = request->beginResponse(
      200, mime != nullptr ? mime : unknown, data.data, data.size);
    if (response == nullptr)
    {
      request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
      return true;
    }
    if (data.gzip) response->addHeader("Content-Encoding", "gzip");
    // Cache large embedded assets to avoid repeatedly reloading them over the
    // ESP32 AP. Keep the HTML shell fresh after firmware updates.
    const bool isJavaScriptAsset = url.startsWith("/assets/js/");
    response->addHeader("Cache-Control", isJavaScriptAsset
      ? "no-cache, no-store, must-revalidate"
      : (url.startsWith("/assets/") ? "public, max-age=86400"
                                    : "no-store, no-cache, must-revalidate"));
    request->send(response);
    return true;
  }

  if (url != "/chat") Debug::Log.error(LOG_WEB, std::string("404 ") + url.c_str());
  StaticHtmlFile page404 = getStaticHtml("/404.html");
  if (page404.data != nullptr)
  {
    AsyncWebServerResponse *response = request->beginResponse(404, "text/html", page404.data, page404.size);
    if (response == nullptr)
    {
      request->send(503, "application/json", "{\"error\":\"response_allocation_failed\"}");
      return true;
    }
    if (page404.gzip) response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=6000");
    request->send(response);
  }
  else
  {
    request->send(404);
  }
  return true;
}

#endif
