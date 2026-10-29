/** @file Marauder.cpp @brief Marauder Wi-Fi/BLE attack integration. */

#include "Marauder.h"

#define LOG_MARAUDER "ESP32M"
#include <string>
#include <deque>
#include <mutex>
#include "Diagnostics/Logging.h"
#include "../../Devices/Storage/StorageDevice.h"
#include "../../Comms/Web/WebServer.h"
#ifndef NO_TFT
#include "../../Devices/Display/DisplayDevice.h"
#endif

namespace Attacks
{
  ESP32Marauder Marauder;
}

ESP32Marauder::ESP32Marauder()
{
}

#ifndef NO_ESP_MARAUDER
#include <esp32_marauder/EvilPortal.h>
#include <esp32_marauder/CommandLine.h>
#include <esp32_marauder/SDInterface.h>
#include <esp32_marauder/WiFiScan.h>
#include <esp32_marauder/settings.h>
#include <esp32_marauder/Buffer.h>

// Defensive deauth detection state (blue-team). Reconstructed from the
// fragmented `sniffdeauth` output inside esp32m_println below.
static bool deauthSniffActive = false;
static uint32_t deauthTotalCount = 0;
static uint8_t deauthParseState = 0;
static std::string deauthPendingSrc;
static std::deque<std::string> deauthEvents;
static const size_t MAX_DEAUTH_EVENTS = 20;
std::recursive_mutex marauderMutex;

/**
 * @brief Forward C-string output to logger and deauth parser.
 * @param[in] msg NUL-terminated text; caller must provide non-null pointer.
 * @return Number of characters forwarded.
 * @note Serializes Marauder output and parser state with marauderMutex.
 */
size_t esp32m_println(const char* msg)
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
    const auto s = std::string(msg);

    // Defensive deauth detection (blue-team): while the `sniffdeauth` scan is
    // active, reconstruct the deauth sniffer's fragmented output
    // ("RSSI: " -> rssi -> " Ch: " -> ch -> " BSSID: " -> src -> " -> " -> dst)
    // and count real deauthentication frames with their src/dst addresses.
    if (deauthSniffActive)
    {
        switch (deauthParseState)
        {
            case 0:
                if (s == " BSSID: ") deauthParseState = 1;
                break;
            case 1:
                deauthPendingSrc = s;
                deauthParseState = 2;
                break;
            case 2:
                deauthParseState = (s == " -> ") ? 3 : 0;
                break;
            case 3:
                deauthTotalCount++;
                deauthEvents.push_back(deauthPendingSrc + " -> " + s);
                while (deauthEvents.size() > MAX_DEAUTH_EVENTS) deauthEvents.pop_front();
                deauthParseState = 0;
                break;
            default:
                deauthParseState = 0;
                break;
        }
    }

    // Keep the UI log focused on the attack name. The upstream Marauder
    // library appends the same stop hint to every long-running command.
    std::string logMessage = s;
    const size_t stopHint = logMessage.find(". Stop with ");
    if (stopHint != std::string::npos) logMessage.erase(stopHint);
    Debug::Log.info("ESP32M", logMessage);
    return s.length();
}

/** @brief Forward Arduino String output to common print sink. */
size_t esp32m_print(const String &s)
{
    return esp32m_println(s.c_str());
}

/** @brief Forward C-string output to common print sink. */
size_t esp32m_print(const char* msg)
{
    return esp32m_println(msg);
}

/** @brief Forward one character without ASCII-number promotion. */
size_t esp32m_print(const char m)
{
    // std::to_string has no char overload: the char would promote to int and
    // print its ASCII code ("A" -> "65"). Build a 1-char string instead.
    const auto strVal = std::string(1, m);
    return esp32m_println(strVal.c_str());
}

/** @brief Forward Arduino String line output to common print sink. */
size_t esp32m_println(const String &s)
{
    return esp32m_println(s.c_str());
}

/** @brief Forward unsigned numeric line output to common print sink. */
size_t esp32m_println(const size_t val)
{
    const auto strVal = std::to_string(val);
    return esp32m_println(strVal.c_str());
}

/** @brief Handle empty line output without emitting data. */
size_t esp32m_println()
{
  return 0;
}

/** @brief Handle compatibility error-print overload without extra output. */
size_t esp32m_println(esp_err_t&, int)
{
  return 0;
}

#ifdef USE_SD_INTERFACE
    // Use the Arduino SD library when this board profile selects SPI storage.
    #include "SD.h"
    using namespace fs;
    #define FILE_INTERFACE SD
#elif !defined(NO_SD)
  #include "../../Devices/Storage/ESP32/SDMmcFileSystem.h"
  using namespace fs;
  #define FILE_INTERFACE SD_MMC_2
#else
  #include <sd_defines.h>
#endif

WiFiScan wifi_scan_obj;
EvilPortal evil_portal_obj;
Buffer buffer_obj;
Settings settings_obj;
CommandLine cli_obj;
SDInterface sd_obj;

static uint32_t currentTime = 0;
static std::string nextMarauderCommandToRun;
static bool marauderActivated = false;
static bool marauderRadioActive = false;
// Portal and dashboard both listen on port 80. The dashboard is stopped before
// the portal command runs, but only outside marauderMutex: closing the web
// server while holding it deadlocks against dashboard requests that lock it.
// Atomic because startEvilPortal() runs on the AsyncTCP task.
static std::atomic<bool> portalListenerStopRequested{false};

static bool isRadioCommand(const std::string &command)
{
  const size_t start = command.find_first_not_of(" \t");
  if (start == std::string::npos) return false;
  const size_t end = command.find_first_of(" \t", start);
  const std::string verb = command.substr(start, end == std::string::npos ? std::string::npos : end - start);
  return verb == "attack" || verb.rfind("scan", 0) == 0 || verb.rfind("sniff", 0) == 0 ||
         verb == "blespam" || verb == "btwardrive" || verb == "wardrive" ||
         verb == "sigmon" || verb == "channelhop" || verb == "evilportal";
}

/**
 * @brief Initialize Marauder settings, storage lists, scanners, and portal.
 * @param[in,out] prefs Persistent firmware settings store.
 * @return None. running remains false when any required subsystem fails.
 * @note Blocking setup-time operation; serializes shared Marauder objects.
 */
void ESP32Marauder::begin(Preferences &prefs)
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  running = false;
  evilPortalActive = false;
  wifiCaptureActive = false;
  marauderActivated = false;
  marauderRadioActive = false;
  nextMarauderCommandToRun.clear();
  deauthSniffActive = false;
  deauthTotalCount = 0;
  deauthParseState = 0;
  deauthPendingSrc.clear();
  deauthEvents.clear();

  // SD_MMC calls below are unsafe when the storage mount failed. Keep the
  // board bootable and expose Marauder as unavailable instead of rebooting.
  if (!Devices::Storage.isRunning())
  {
    Debug::Log.error(LOG_MARAUDER, "Storage unavailable; Marauder startup skipped.");
    return;
  }

  buffer_obj.reset();
  if (sd_obj.sd_files != nullptr)
  {
    sd_obj.sd_files->clear();
  }
  sd_obj.supported = true;
#ifdef USE_SD_INTERFACE
  sd_obj.cardType = CARD_SD;
  sd_obj.cardSizeMB = FILE_INTERFACE.cardSize() / 1024 / 1024;
#elif !defined(NO_SD)
  sd_obj.cardType = FILE_INTERFACE.cardType();
  sd_obj.cardSizeMB = FILE_INTERFACE.cardSize() / 1024 / 1024;
#else
  sd_obj.cardType = CARD_SD;
  sd_obj.cardSizeMB = SPIFFS.totalBytes() / 1024 / 1024;
#endif
  sd_obj.card_sz = String(sd_obj.cardSizeMB);

  if (sd_obj.sd_files == nullptr)
  {
    sd_obj.sd_files = new LinkedList<String>();
    if (sd_obj.sd_files != nullptr) sd_obj.sd_files->add("Back");
  }
  if (sd_obj.sd_files == nullptr)
  {
    Debug::Log.error(LOG_MARAUDER, "Could not allocate Marauder SD file list.");
    return;
  }
  // "Back" is already added when the list is freshly allocated above; adding
  // it unconditionally here would duplicate the entry.
  else if (sd_obj.sd_files->size() == 0)
  {
    sd_obj.sd_files->add("Back");
  }
  (void)sd_obj.initSD();

  running = 
#ifdef NO_SD
  settings_obj.begin();
#else
  settings_obj.begin(FILE_INTERFACE, "/esp32m_settings.json");
#endif

  if (!running)
  {
    Debug::Log.info(LOG_MARAUDER, "Could not load settings.");
    return;
  }

  if (!wifi_scan_obj.RunSetup())
  {
    running = false;
    Debug::Log.error(LOG_MARAUDER, "Could not initialize Marauder WiFi lists.");
    return;
  }
  if (!evil_portal_obj.setup())
  {
    running = false;
    Debug::Log.error(LOG_MARAUDER, "Could not initialize Evil Portal.");
    return;
  }
  cli_obj.RunSetup();
}

/**
 * @brief Advance active Marauder scanner, buffer, settings, and command state.
 * @param[in,out] prefs Persistent settings passed to dependent services.
 * @return None.
 * @note Runs on Arduino loop and holds marauderMutex across library calls.
 */
void ESP32Marauder::loop(Preferences &prefs)
{
  if (portalListenerStopRequested.load())
  {
    portalListenerStopRequested.store(false);
    Comms::Web.end();
  }
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  // Defer Marauder library work until a command arrives, reducing idle startup
  // memory use and background processing.
  if (marauderActivated)
  {
    currentTime = millis();
    wifi_scan_obj.main(currentTime);

    // Save buffer to SD and/or serial
    buffer_obj.save();
    settings_obj.main(currentTime);

    // startEvilPortal() runs on the AsyncTCP task and holds marauderMutex while
    // it reads the template, so this loop can pass the stop check above, block
    // here on the mutex, and then run the queued portal command while the
    // dashboard still owns port 80. The portal bind would fail and its AP would
    // come up with no HTTP listener. Defer one iteration so the next pass
    // closes the dashboard outside the mutex before the portal binds.
    if (!nextMarauderCommandToRun.empty() && portalListenerStopRequested.load())
    {
      return;
    }

    if (!nextMarauderCommandToRun.empty())
    {
      const bool stopping = nextMarauderCommandToRun == "stopscan" || nextMarauderCommandToRun == "stop";
      const std::string command = nextMarauderCommandToRun;
      // A new radio mode must not be layered on top of the running one. The old
      // scan/attack/portal keeps promiscuous mode, sockets, or the AP alive and
      // the two modes then fight over the radio and web port 80. Stop it here,
      // on the loop task, before dispatching the replacement.
      if (!stopping && isRadioCommand(command) && wifi_scan_obj.scanning())
      {
        Debug::Log.info(LOG_MARAUDER, "Stopping previous radio mode before switching.");
        if (evil_portal_obj.isActive()) evil_portal_obj.stop();
        wifi_scan_obj.StopScan(wifi_scan_obj.currentScanMode);
        marauderRadioActive = false;
      }
      if (command.rfind("evilportal", 0) == 0)
      {
        // Saved settings are the default; payload -n/-p still wins for this run.
        if (!evil_portal_obj.hasExplicitAp())
          evil_portal_obj.setPortalSsid(prefs.getString("portal-ssid", "RedLilyGO-Portal"));
        if (!evil_portal_obj.hasExplicitPassword())
          evil_portal_obj.setPortalPassword(prefs.getString("portal-pwd", ""));
      }
      Debug::Log.info(LOG_MARAUDER, "Running Marauder command");
      cli_obj.runCommand(command.c_str());
      if (command.rfind("evilportal", 0) == 0)
        evil_portal_obj.clearExplicitPortalOverrides();
      buffer_obj.save();
      nextMarauderCommandToRun.clear();
      if (command.rfind("evilportal", 0) == 0)
      {
        const bool portalActive = evil_portal_obj.isActive();
        evilPortalActive.store(portalActive, std::memory_order_release);
        marauderRadioActive = portalActive;
        if (!portalActive) marauderActivated = false;
      }
      if (stopping)
      {
        marauderActivated = false;
        marauderRadioActive = false;
        evilPortalActive.store(false, std::memory_order_release);
        wifiCaptureActive.store(false, std::memory_order_release);
      }
      else if (command == "sniffraw")
      {
        wifiCaptureActive.store(true, std::memory_order_release);
      }
      else if (!marauderRadioActive && !isRadioCommand(command))
      {
        // One-shot/system commands must not keep TFT in MRD state.
        marauderActivated = false;
      }
      else if (isRadioCommand(command) && !wifi_scan_obj.scanning() && !evil_portal_obj.isActive())
      {
        // The command was rejected before it started a mode, for example an
        // attack without a selected target. Do not leave the UI/LED in MRD.
        marauderRadioActive = false;
        marauderActivated = false;
      }
      if (stopping)
      {
        Debug::Log.info(LOG_MARAUDER, "Marauder command stopped");
      }
      else if (!isRadioCommand(command))
      {
        Debug::Log.info(LOG_MARAUDER, "Marauder command finished");
      }
    }
  }
}

/**
 * @brief Queue one Marauder command for execution by loop().
 * @param[in] cmd Non-empty command line; ignored while not initialized.
 * @return None.
 * @note Also updates defensive deauth and captive-portal state flags.
 */
void ESP32Marauder::run(const std::string &cmd)
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running || cmd.empty())
  {
    Debug::Log.warning(LOG_MARAUDER, "Ignoring command while Marauder is not ready");
    return;
  }
  marauderActivated = true;
  if (isRadioCommand(cmd)) marauderRadioActive = true;
  nextMarauderCommandToRun = cmd;
#ifndef NO_TFT
  Devices::Display.wakeForActivity();
#endif

  // Track the deauth-sniff mode so the print sink only counts real deauth frames.
  if (cmd == "sniffdeauth")
  {
    deauthTotalCount = 0;
    deauthEvents.clear();
    deauthParseState = 0;
    deauthSniffActive = true;
  }
  else if (cmd == "stopscan" || cmd == "stop")
  {
    deauthSniffActive = false;
  }
  else if (cmd.rfind("evilportal", 0) == 0)
  {
    evilPortalActive.store(true, std::memory_order_release);
    portalListenerStopRequested.store(true);
  }
}

/** @brief Report whether Marauder is actively processing a command loop. */
bool ESP32Marauder::isActive() const
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  // Keep LED/UI state tied to the scanner too. The command flag can briefly
  // clear while BLE/WiFi cleanup or command dispatch is still active.
  return marauderRadioActive || wifi_scan_obj.scanning() || evil_portal_obj.isActive();
}

/** @brief Return packets captured since the previous status read. */
uint16_t ESP32Marauder::getPacketCount()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return 0;
  return buffer_obj.getPacketsSinceLastCheck();
}

/**
 * @brief Escape JSON string control characters and delimiters.
 * @param[in] s Input string.
 * @return JSON-safe string with quotes, slashes, and whitespace escaped.
 */
static std::string jsonEscape(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (const char c : s)
  {
    switch (c)
    {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

/** @brief Serialize live access-point scan results as JSON. */
std::string ESP32Marauder::getAccessPointsJson()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running || access_points == nullptr) return "[]";

  std::string out = "[";
  for (int i = 0; i < access_points->size(); i++)
  {
    const AccessPoint &ap = access_points->get(i);
    if (i != 0) out += ",";

    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
             ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);

    out += "{\"ssid\":\"" + jsonEscape(std::string(ap.essid.c_str())) + "\",\"channel\":" + std::to_string(ap.channel) +
           ",\"bssid\":\"" + std::string(bssid) + "\",\"rssi\":" + std::to_string((int)ap.rssi) +
           ",\"selected\":" + (ap.selected ? "true" : "false") + "}";
  }
  out += "]";
  return out;
}

/** @brief Serialize captured portal fields as JSON. */
std::string ESP32Marauder::getEvilPortalJson()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return "{\"active\":false,\"selectedPage\":\"\",\"submissions\":0,\"last\":null,\"history\":[]}";
  return evil_portal_obj.getStatusJson();
}

std::string ESP32Marauder::getEvilPortalFilesJson()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return "{\"files\":[]}";
  return evil_portal_obj.getFormFilesJson();
}

bool ESP32Marauder::startEvilPortal(const std::string &filename, const std::string &ssid, std::string &error)
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) { error = "marauder_not_ready"; return false; }
  if (evil_portal_obj.isActive()) { error = "portal_active"; return false; }
  if (!evil_portal_obj.setTargetHtml(String(filename.c_str()))) { error = "html_form_unavailable"; return false; }
  // Empty SSID keeps the saved portal-ssid setting.
  if (!ssid.empty() && !evil_portal_obj.setPortalSsid(String(ssid.c_str()))) { error = "portal_ssid_invalid"; return false; }
  marauderActivated = true;
  marauderRadioActive = true;
  // Claim port 80 before the queued command runs so WiFiDevice::loop stops the
  // dashboard first. Otherwise the portal AP starts but its HTTP server cannot bind.
  evilPortalActive.store(true, std::memory_order_release);
  portalListenerStopRequested.store(true);
  nextMarauderCommandToRun = "evilportal -c start";
#ifndef NO_TFT
  Devices::Display.wakeForActivity();
#endif
  return true;
}

void ESP32Marauder::stopEvilPortal()
{
  run("stopscan");
}

void ESP32Marauder::stopAll()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  nextMarauderCommandToRun.clear();
  marauderActivated = false;
  marauderRadioActive = false;
  deauthSniffActive = false;
  evilPortalActive.store(false, std::memory_order_release);
  wifiCaptureActive.store(false, std::memory_order_release);
  evil_portal_obj.stop();
  wifi_scan_obj.StopScan(wifi_scan_obj.currentScanMode);
  // StopScan only reaches shutdownBLE() for BLE scan modes. Call it again so a
  // BLE stack started by an attack is always released, even if the mode was
  // already reset. shutdownBLE() is a no-op when BLE was never initialized.
  wifi_scan_obj.shutdownBLE();
}

bool ESP32Marauder::clearEvilPortalHistory()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return false;
  return evil_portal_obj.clearSubmissionHistory();
}

/** @brief Return defensive deauthentication frame count. */
uint32_t ESP32Marauder::getDeauthCount()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return 0;
  return deauthTotalCount;
}

/** @brief Serialize bounded defensive deauthentication events as JSON. */
std::string ESP32Marauder::getDeauthEventsJson()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return "[]";
  std::string out = "[";
  size_t i = 0;
  for (const auto &ev : deauthEvents)
  {
    if (i++) out += ",";
    out += "\"" + jsonEscape(ev) + "\"";
  }
  out += "]";
  return out;
}

/** @brief Return configured channel-hop delay in seconds. */
uint8_t ESP32Marauder::getChannelHopDelay()
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  return wifi_scan_obj.getChannelHopDelay();
}

/**
 * @brief Set channel-hop delay with one-to-sixty-second bounds.
 * @param[in] seconds Requested delay; values outside range are clamped.
 * @return None.
 */
void ESP32Marauder::setChannelHopDelay(uint8_t seconds)
{
  const std::lock_guard<std::recursive_mutex> lock(marauderMutex);
  if (!running) return;
  if (seconds == 0) seconds = 1;
  if (seconds > 60) seconds = 60;
  wifi_scan_obj.setChannelHopDelay(seconds);
  Debug::Log.info(LOG_MARAUDER, std::string("Channel hop delay set to ") + std::to_string(seconds) + "s");
}

#else
void ESP32Marauder::begin(Preferences &prefs)
{

}

void ESP32Marauder::loop(Preferences &prefs)
{

}

bool ESP32Marauder::isActive() const
{
  return false;
}

void ESP32Marauder::run(const std::string &cmd)
{
  Debug::Log.info(LOG_MARAUDER, "ESP32Marauder is not supported on this device.");
}

uint16_t ESP32Marauder::getPacketCount()
{
  return 0;
}

std::string ESP32Marauder::getAccessPointsJson()
{
  return "[]";
}

std::string ESP32Marauder::getEvilPortalJson()
{
  return "{\"active\":false,\"selectedPage\":\"\",\"submissions\":0,\"last\":null,\"history\":[]}";
}

std::string ESP32Marauder::getEvilPortalFilesJson()
{
  return "{\"files\":[]}";
}

bool ESP32Marauder::startEvilPortal(const std::string &, const std::string &, std::string &error)
{
  error = "marauder_unavailable";
  return false;
}

void ESP32Marauder::stopEvilPortal()
{
}

void ESP32Marauder::stopAll()
{
}

bool ESP32Marauder::clearEvilPortalHistory()
{
  return false;
}

uint32_t ESP32Marauder::getDeauthCount()
{
  return 0;
}

std::string ESP32Marauder::getDeauthEventsJson()
{
  return "[]";
}

uint8_t ESP32Marauder::getChannelHopDelay()
{
  return 1;
}

void ESP32Marauder::setChannelHopDelay(uint8_t seconds)
{
}

#endif
