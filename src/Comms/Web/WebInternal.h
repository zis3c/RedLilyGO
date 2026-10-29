/** @file WebInternal.h @brief Shared state and helpers for the split web service. */

#pragma once

#ifndef NO_WEB

#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <mutex>
#include <string>

#include <ESPAsyncWebServer.h>

#include "RequestValidation.h"

/** @brief Embedded static asset descriptor. Must match the generated htmlFiles.cpp. */
struct StaticHtmlFile
{
  const uint8_t *data;
  size_t size;
  bool gzip;
};

// Owned by WebServer.cpp. The other web translation units reference these
// objects instead of duplicating the server or socket lifetimes.
extern Preferences *preferences;
extern std::recursive_mutex webLifecycleMutex;
extern AsyncWebSocket ws;
extern AsyncWebSocket audio;
extern AsyncWebSocket input;
extern std::mutex dataSnapshotMutex;

namespace WebInternal
{
  /** @brief Maximum number of SD entries returned in one JSON response. */
  constexpr size_t MAX_WEB_FILE_ENTRIES = 24;

  // Authentication and first-run setup (WebAuth.cpp).
  bool setupComplete();
  String currentSetupNonce();
  void clearSetupNonce();
  String requestIp(AsyncWebServerRequest *request);
  bool authRateLimited(const String &ip);
  void recordAuthFailure(const String &ip);
  void clearAuthFailures(const String &ip);
  bool requestAuthorized(AsyncWebServerRequest *request);
  bool isWebSocketAuthorized(AsyncWebServerRequest *request);
  bool rejectCrossSiteMutation(AsyncWebServerRequest *request);
  void resetAuthState();

  // Per-boot CSRF token and mutation method/credential guard (WebAuth.cpp).
  String csrfToken();
  void resetCsrfToken();
  bool requestHasValidCsrf(AsyncWebServerRequest *request);
  bool requestNeedsMutationGuard(AsyncWebServerRequest *request);

  // HTTP route dispatch (WebRoutes.cpp).
  bool handleInfoRoute(AsyncWebServerRequest *request);
  bool handlePayloadRoute(AsyncWebServerRequest *request);
  bool handleMediaRoute(AsyncWebServerRequest *request);
  bool handleMarauderRoute(AsyncWebServerRequest *request);
  bool handleStorageRoute(AsyncWebServerRequest *request);
  bool handleSystemRoute(AsyncWebServerRequest *request);
  bool handleStaticRoute(AsyncWebServerRequest *request);

  // JSON responses (WebResponse.cpp).
  std::string prettyPrintUptime();
  const char *resetReasonName();
  void sendTelemetryResponse(AsyncWebServerRequest *request);
  void sendRuntimeLogsResponse(AsyncWebServerRequest *request);
  void sendFilesResponse(AsyncWebServerRequest *request);
  void sendDashboardResponse(AsyncWebServerRequest *request);
  void sendHealthResponse(AsyncWebServerRequest *request);

  // Embedded static assets (WebStatic.cpp).
  StaticHtmlFile getStaticHtml(const String &url);
  const char *GetMimeType(const char *fname);

  // Multipart uploads (WebUpload.cpp).
  void handleUpload(AsyncWebServerRequest *request, String filename, size_t index,
                    uint8_t *data, size_t len, bool final);

  // Real-time WebSocket channels (WebWebSocket.cpp).
  void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                 void *arg, uint8_t *data, size_t len);
  void onWsAudioEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len);
  void onInputEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                    void *arg, uint8_t *data, size_t len);
  bool webVncActive();
  bool webHidActive();
  void webResetRealtimeState();
} // namespace WebInternal

#endif
