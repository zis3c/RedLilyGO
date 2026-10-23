/** @file WebAuth.cpp @brief Shared authentication, rate limiting and first-run nonce. */

#ifndef NO_WEB
#include "WebInternal.h"
#include "Diagnostics/Logging.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <esp_system.h>

#define LOG_WEB "WEB"

struct AuthFailureState
{
  uint8_t count = 0;
  unsigned long blockedUntil = 0;
};
static std::unordered_map<std::string, AuthFailureState> authFailures;
static std::mutex authFailuresMutex;
static String setupNonce;
static std::mutex setupNonceMutex;
static String csrfValue;
static std::mutex csrfMutex;

bool WebInternal::setupComplete()
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  return preferences != nullptr && preferences->getBool("web-auth", false);
}

String WebInternal::currentSetupNonce()
{
  const std::lock_guard<std::mutex> lock(setupNonceMutex);
  if (setupNonce.isEmpty())
  {
    char nonce[17];
    snprintf(nonce, sizeof(nonce), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    setupNonce = nonce;
  }
  return setupNonce;
}

void WebInternal::clearSetupNonce()
{
  const std::lock_guard<std::mutex> lock(setupNonceMutex);
  setupNonce = "";
}

String WebInternal::requestIp(AsyncWebServerRequest *request)
{
  return request != nullptr && request->client() != nullptr
    ? request->client()->remoteIP().toString() : String("unknown");
}

bool WebInternal::authRateLimited(const String &ip)
{
  const std::lock_guard<std::mutex> lock(authFailuresMutex);
  const auto it = authFailures.find(std::string(ip.c_str()));
  return it != authFailures.end() && it->second.count >= 5 &&
         static_cast<int32_t>(millis() - it->second.blockedUntil) < 0;
}

void WebInternal::recordAuthFailure(const String &ip)
{
  const std::lock_guard<std::mutex> lock(authFailuresMutex);
  const std::string key(ip.c_str());
  if (authFailures.size() >= 32 && authFailures.find(key) == authFailures.end())
    authFailures.erase(authFailures.begin());
  auto &state = authFailures[key];
  if (state.count >= 5 && static_cast<int32_t>(millis() - state.blockedUntil) >= 0)
  {
    state.count = 0;
    state.blockedUntil = 0;
  }
  if (++state.count >= 5) state.blockedUntil = millis() + 30000;
  Debug::Log.error(LOG_WEB, "Authentication failure");
}

void WebInternal::clearAuthFailures(const String &ip)
{
  const std::lock_guard<std::mutex> lock(authFailuresMutex);
  authFailures.erase(std::string(ip.c_str()));
}

// Shared authentication gate for request handlers. Upload callbacks run
// during multipart parsing, BEFORE webRequestHandler's auth gate executes,
// so handleUpload must call this itself or uploads bypass authentication.
bool WebInternal::requestAuthorized(AsyncWebServerRequest *request)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  if (request == nullptr) return false;
  if (preferences == nullptr || !preferences->getBool("web-auth", false)) return true;

  const String ip = requestIp(request);
  if (authRateLimited(ip)) return false;
  const String user = preferences->getString("web-user", "admin");
  const String pass = preferences->getString("web-pass", "redlilygo");
  if (!request->authenticate(user.c_str(), pass.c_str()))
  {
    recordAuthFailure(ip);
    return false;
  }
  clearAuthFailures(ip);
  return true;
}

bool WebInternal::isWebSocketAuthorized(AsyncWebServerRequest *request)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  if (request == nullptr) return false;
  const String origin = request->header("Origin");
  if (!origin.isEmpty())
  {
    const String host = request->header("Host");
    if (origin != String("http://") + host && origin != String("https://") + host) return false;
  }
  else if (!request->header("Sec-Fetch-Site").isEmpty())
  {
    const String site = request->header("Sec-Fetch-Site");
    if (!site.equalsIgnoreCase("same-origin") && !site.equalsIgnoreCase("none")) return false;
  }
  return requestAuthorized(request);
}

// Cross-site request guard for state-changing endpoints. Browsers replay
// cached Basic-auth credentials on simple cross-site GET navigations, so a
// malicious webpage could otherwise trigger destructive actions
// (format/reset/delete/run) against an authenticated device. Non-browser
// clients that send no Origin/Sec-Fetch headers are allowed through.
bool WebInternal::rejectCrossSiteMutation(AsyncWebServerRequest *request)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  if (request == nullptr) return true;
  const String origin = request->header("Origin");
  if (!origin.isEmpty())
  {
    const String host = request->header("Host");
    if (origin == String("http://") + host) return false;
  }
  else
  {
    const String site = request->header("Sec-Fetch-Site");
    // "none" means direct user navigation (typed URL/bookmark).
    if (site.isEmpty() || site.equalsIgnoreCase("same-origin") || site.equalsIgnoreCase("none")) return false;
  }
  Debug::Log.warning(LOG_WEB, "Blocked cross-site mutation attempt.");
  request->send(403);
  return true;
}

void WebInternal::resetAuthState()
{
  const std::lock_guard<std::mutex> lock(authFailuresMutex);
  authFailures.clear();
}

// Per-boot CSRF token. Cross-site JavaScript cannot read it (same-origin policy)
// and cannot attach the X-CSRF-Token header to a cross-origin request without a
// preflight the device never approves, so requiring it blocks browser CSRF even
// when cached Basic-auth credentials would otherwise be replayed.
String WebInternal::csrfToken()
{
  const std::lock_guard<std::mutex> lock(csrfMutex);
  if (csrfValue.isEmpty())
  {
    char token[17];
    snprintf(token, sizeof(token), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    csrfValue = token;
  }
  return csrfValue;
}

void WebInternal::resetCsrfToken()
{
  const std::lock_guard<std::mutex> lock(csrfMutex);
  csrfValue = "";
}

bool WebInternal::requestHasValidCsrf(AsyncWebServerRequest *request)
{
  if (request == nullptr) return false;
  const String provided = request->header("X-CSRF-Token");
  return !provided.isEmpty() && provided == csrfToken();
}

// State-changing routes must be POST and carry the CSRF token. This is the
// authoritative list for the mutation gate in webRequestHandler and the upload
// callback. /evilportal is dual-use: GET reads portal state, while the presence
// of an action parameter makes it a mutation.
bool WebInternal::requestNeedsMutationGuard(AsyncWebServerRequest *request)
{
  if (request == nullptr) return true;
  static const char *const kMutationRoutes[] = {
    "/clearlogs", "/runfile", "/rawinput", "/runagentcmd", "/showimage",
    "/marauder", "/capture", "/mic", "/system", "/delete", "/set", "/settings",
    "/channelhop", "/moveFile", "/uploadFile"
  };
  const String &url = request->url();
  if (url == "/evilportal") return request->hasParam("action");
  for (const char *route : kMutationRoutes)
  {
    if (url == route) return true;
  }
  return false;
}

#endif
