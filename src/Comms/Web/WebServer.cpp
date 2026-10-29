/** @file WebServer.cpp @brief HTTP lifecycle, request dispatch and WebSocket transport. */

#ifndef NO_WEB
#include "WebServer.h"
#include "WebInternal.h"

using namespace WebRequestValidation;
using namespace WebInternal;

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "esp32-hal-cpu.h"
#include "AsyncJson.h"
#include <ArduinoJson.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include <cctype>
#include <cstdlib>
#include <esp_system.h>
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
#include "Diagnostics/Logging.h"
#include "../../Attacks/Marauder/Marauder.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "../../Utilities/Settings.h"
#include "../../version.h"

#define LOG_WEB "WEB"

namespace Comms
{
  WebSite Web;
}

// Serve dashboard on standard HTTP port so relative API calls such as
// /data.json and /system work when the user opens the device host.
static AsyncWebServer controlInterfaceWebServer(80);
static bool defaultHeadersConfigured = false;
static bool webRoutesConfigured = false;
static bool webRealtimeConfigured = false;

// Shared with the other web translation units; declared in WebInternal.h.
Preferences *preferences = nullptr;
std::recursive_mutex webLifecycleMutex;
AsyncWebSocket ws("/websockify");
AsyncWebSocket audio("/audio");
AsyncWebSocket input("/input");
std::mutex dataSnapshotMutex;

/**
 * @brief Final request dispatcher for every route not handled by an explicit
 *        AsyncWebServer handler. Performs the shared security gates, then
 *        delegates to the focused route handlers in WebRoutes.cpp.
 */
static void webRequestHandler(AsyncWebServerRequest *request)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  const auto &url = request->url();
  // Multipart callbacks set a response before this final request callback.
  if (url == "/uploadFile" && request->getResponse() != nullptr) return;
  const String host = request->header("Host");
  if (host.indexOf("msftconnecttest.com") >= 0 ||
      host.indexOf("msftncsi.com") >= 0 ||
      host.indexOf("connectivitycheck.gstatic.com") >= 0 ||
      host.indexOf("captive.apple.com") >= 0)
  {
    request->redirect(String("http://") + Devices::Wifi.apHostname().c_str() + "/");
    return;
  }
  if (url == "/generate_204" || url == "/hotspot-detect.html" ||
      url == "/gen_204" || url == "/canonical.html" ||
      url == "/mobile/status.php" || url == "/check_network_status.txt" ||
      url == "/hotspotdetect.html" ||
      url == "/library/test/success.html" || url == "/ncsi.txt" ||
      url == "/connecttest.txt" || url == "/success.txt" ||
      url == "/redirect")
  {
    // Canonicalize captive requests to the device host. A relative redirect
    // keeps msftconnecttest.com as Host, making every later asset/API request
    // depend on captive DNS and producing inconsistent browser behaviour.
    request->redirect(String("http://") + Devices::Wifi.apHostname().c_str() + "/");
    return;
  }
  if (url == "/setup/status")
  {
    if (setupComplete())
      request->send(200, "application/json", "{\"required\":false}");
    else
      request->send(200, "application/json", String("{\"required\":true,\"nonce\":\"") + currentSetupNonce() + "\"}");
    return;
  }
  if (preferences != nullptr && preferences->getBool("web-auth", false))
  {
    const String ip = requestIp(request);
    if (authRateLimited(ip)) { request->send(429, "text/plain", "Too many authentication failures"); return; }
    if (!requestAuthorized(request))
    {
      request->requestAuthentication();
      return;
    }
  }

  // Every state-changing route must be a POST carrying the per-boot CSRF token.
  // This blocks browser cross-site requests and cached-Basic-auth GET prefetch
  // regardless of whether the client supplies Origin or Fetch Metadata headers.
  if (requestNeedsMutationGuard(request))
  {
    if (rejectCrossSiteMutation(request)) return;
    if (request->method() != HTTP_POST)
    {
      request->send(405, "application/json", "{\"error\":\"post_required\"}");
      return;
    }
    if (!requestHasValidCsrf(request))
    {
      Debug::Log.warning(LOG_WEB, "Blocked mutation without a valid CSRF token.");
      request->send(403, "application/json", "{\"error\":\"csrf_required\"}");
      return;
    }
  }

  if (handleInfoRoute(request)) return;
  if (handlePayloadRoute(request)) return;
  if (handleMediaRoute(request)) return;
  if (handleMarauderRoute(request)) return;
  if (handleStorageRoute(request)) return;
  if (handleSystemRoute(request)) return;
  handleStaticRoute(request);
}

WebSite::WebSite()
{
}

bool WebSite::isVncActive() const
{
  return webVncActive();
}

bool WebSite::isHidActive() const
{
  return webHidActive();
}

void WebSite::begin(Preferences &prefs)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  if (running)
  {
    return;
  }
  if (Devices::Wifi.getState() == false)
  {
    return;
  }

  preferences = &prefs;

  if (!defaultHeadersConfigured)
  {
    DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
    DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
    DefaultHeaders::Instance().addHeader("Referrer-Policy", "no-referrer");
    DefaultHeaders::Instance().addHeader("Content-Security-Policy", "default-src 'self' data: blob:; img-src 'self' data: blob: https://*.basemaps.cartocdn.com; script-src 'self' 'unsafe-inline' https://cdn.tailwindcss.com https://cdn.jsdelivr.net https://cdnjs.cloudflare.com https://unpkg.com; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com https://cdnjs.cloudflare.com https://unpkg.com; font-src 'self' data: https://fonts.gstatic.com https://cdnjs.cloudflare.com; connect-src 'self' ws: wss:; frame-ancestors 'none'");
    defaultHeadersConfigured = true;
  }

  if (!webRoutesConfigured)
  {
    controlInterfaceWebServer.onFileUpload(handleUpload);
    controlInterfaceWebServer.on("/setup/status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
      const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
      if (setupComplete())
        request->send(200, "application/json", "{\"required\":false}");
      else
        request->send(200, "application/json", String("{\"required\":true,\"nonce\":\"") + currentSetupNonce() + "\"}");
    });
    auto *setupHandler = new AsyncCallbackJsonWebHandler("/setup", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
    const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
    if (preferences == nullptr)
    {
      request->send(503, "application/json", "{\"error\":\"settings_unavailable\"}");
      return;
    }
      if (setupComplete())
    {
      request->send(409, "application/json", "{\"error\":\"Setup already completed\"}");
        return;
      }

      const String origin = request->header("Origin");
      const String host = request->header("Host");
      if (origin.isEmpty() || (origin != String("http://") + host && origin != String("https://") + host))
      {
        request->send(403, "application/json", "{\"error\":\"same_origin_required\"}");
        return;
      }

      const String ip = requestIp(request);
      if (authRateLimited(ip))
      {
        request->send(429, "application/json", "{\"error\":\"too_many_attempts\"}");
        return;
      }

      JsonObject body = json.as<JsonObject>();
      const String nonce = body["nonce"] | "";
      if (nonce.isEmpty() || nonce != currentSetupNonce())
      {
        recordAuthFailure(ip);
        request->send(403, "application/json", "{\"error\":\"invalid_setup_nonce\"}");
        return;
      }

    const String username = body["username"] | "";
    const String password = body["password"] | "";
    const String confirmation = body["confirmation"] | "";
      if (!validUsername(username) || password.length() < 12 || password != confirmation)
      {
        recordAuthFailure(ip);
        request->send(400, "application/json", "{\"error\":\"Invalid setup credentials\"}");
      return;
    }

      if (!setSettingValue(*preferences, "web-user", username.c_str()) ||
        !setSettingValue(*preferences, "web-pass", password.c_str()) ||
        !setSettingValue(*preferences, "web-auth", "1"))
    {
      request->send(500, "application/json", "{\"error\":\"Could not save setup credentials\"}");
      return;
      }
      clearSetupNonce();
      clearAuthFailures(ip);
      request->send(200, "application/json", "{\"ok\":true}");
    });
    if (setupHandler != nullptr)
    {
      controlInterfaceWebServer.addHandler(setupHandler);
    }
    else
    {
      Debug::Log.error(LOG_WEB, "Could not allocate setup handler.");
    }
    controlInterfaceWebServer.onNotFound(webRequestHandler);
    // Allow a later begin() to retry setup registration if allocation failed.
    webRoutesConfigured = setupHandler != nullptr;
  }

  if (!webRealtimeConfigured)
  {
    ws.onEvent(onWsEvent);
    audio.onEvent(onWsAudioEvent);
    input.onEvent(onInputEvent);
    ws.handleHandshake(isWebSocketAuthorized);
    audio.handleHandshake(isWebSocketAuthorized);
    input.handleHandshake(isWebSocketAuthorized);

    controlInterfaceWebServer.addHandler(&ws);
    controlInterfaceWebServer.addHandler(&audio);
    controlInterfaceWebServer.addHandler(&input);

    Devices::USB::CDC.setCallback(HostCommand::WSDATARECV, [](uint8_t *buffer, const size_t size) -> void
    {
      ws.binaryAll(buffer, size);
    });

    Devices::Mic.setCallback([](uint8_t *buffer, const size_t size) -> bool
    {
      const size_t freeHeap = ESP.getFreeHeap();
      if (size > freeHeap / 2)
      {
        Debug::Log.info(LOG_WEB, "Could not write data to /audio, not enough free mem");
        return true;
      }

      audio.binaryAll(buffer, size);
      return true;
    });
    webRealtimeConfigured = true;
  }
  controlInterfaceWebServer.begin();
  running = true;

}

void WebSite::loop(Preferences &prefs)
{
  if (preferences == nullptr)
  {
    return;
  }

  ws.cleanupClients();
  audio.cleanupClients();
  input.cleanupClients();
}

void WebSite::end()
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  resetAuthState();
  resetCsrfToken();
  Devices::Mic.stopCapture();
  ws.closeAll();
  audio.closeAll();
  input.closeAll();
  webResetRealtimeState();
  controlInterfaceWebServer.end();
  running = false;
  preferences = nullptr;
}

#endif
