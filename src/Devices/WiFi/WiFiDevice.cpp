/**
 * @file WiFiDevice.cpp
 * @brief ESP32 Wi-Fi AP/station implementation and dashboard ownership.
 *
 * Uses the ESP32 radio, Preferences, and web server. All state transitions
 * occur in the Arduino loop task; Evil Portal temporarily owns port 80.
 */

#include "WiFiDevice.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_timer.h>

#include "Diagnostics/Logging.h"
#include "../../Attacks/Agent/Agent.h"
#include "../../Attacks/Marauder/Marauder.h"
#include "../../Comms/Web/WebServer.h"

#ifndef WIFI_POWER
  #define WIFI_POWER WIFI_POWER_21dBm // default power level is MAX
#endif

#ifndef WIFI_SLEEP
  #define WIFI_SLEEP false // keep the radio awake by default
#endif

#define LOG_WIFI "WiFi"

#define WIFI_AP_MODE                    "wifi-ap-mode"
#define WIFI_AP_MODE_DEFAULT            true

#define WIFI_AP_NAME                    "wifi-ap"
#define WIFI_AP_NAME_DEFAULT            "RedLilyGO"

#define WIFI_AP_PWD                     "wifi-pwd"
#define WIFI_AP_PWD_DEFAULT             "redlilygo"
// Configurable AP network address. The device host used by captive-portal
// redirects and CSP/Origin comparisons is derived from this value, so a
// deployment can move off the default 4.3.2.1 without patching firmware.
#define WIFI_AP_IP                      "wifi-ap-ip"
#define WIFI_AP_IP_DEFAULT              "4.3.2.1"
#define WEB_AUTH                         "web-auth"
#define WEB_AUTH_DEFAULT                false
#define WEB_USER                         "web-user"
#define WEB_USER_DEFAULT                "admin"
#define WEB_PASS                         "web-pass"
#define WEB_PASS_DEFAULT                "redlilygo"
#define WIFI_DESIRED_BOOT_STATE         "wifi-bootstate"
#define WIFI_DESIRED_BOOT_STATE_DEFAULT true

static bool webServerRunning = false;
static bool desiredWiFiState = true; // Requested radio state, restored from the saved boot setting.
static String wiFiApName = WIFI_AP_NAME_DEFAULT;
static String wiFiApPwd = WIFI_AP_PWD_DEFAULT;
static String wiFiApIp = WIFI_AP_IP_DEFAULT;
static bool wifiAPMode = WIFI_AP_MODE_DEFAULT;
static DNSServer captiveDns;
static bool captiveDnsRunning = false;

static uint64_t lastMinValueChecked = 0;
// The persisted boot state is read exactly once. loop() re-invokes begin()
// whenever the radio is down but wanted; re-reading the preference here would
// silently revert any runtime Devices::Wifi.setWiFi(true) call.
static bool wifiBootStateLoaded = false;

// Resolve the configured AP address, falling back to the default when the
// stored value is absent or malformed.
static IPAddress apAddress()
{
    IPAddress address;
    if (!address.fromString(wiFiApIp)) address = IPAddress(4, 3, 2, 1);
    return address;
}

std::string WiFiDevice::apHostname()
{
    return std::string(apAddress().toString().c_str());
}

namespace Devices
{
    WiFiDevice Wifi;
}

#ifndef NO_WIFI
WiFiDevice::WiFiDevice()
{
}

void WiFiDevice::setWiFi(const bool& state)
{
  desiredWiFiState = state;
}

bool WiFiDevice::getState()
{
  // AP.started() is transient during early ESP32 startup. The configured
  // interface and IP are the stable state used by the web-server lifecycle.
  return (wifiAPMode == true && WiFi.getMode() == WIFI_MODE_AP &&
          WiFi.softAPIP() == apAddress()) ||
         (wifiAPMode == false && WiFi.status() == WL_CONNECTED);
}

int WiFiDevice::signalStrength()
{
  if (!getState() || wifiAPMode) return 0;
  return WiFi.RSSI();
}

std::string WiFiDevice::currentIPAddress()
{
  if (!getState()) return std::string();
  if (wifiAPMode) {
    return std::string(WiFi.softAPIP().toString().c_str());
  }
  return std::string(WiFi.localIP().toString().c_str());
}

void WiFiDevice::begin(Preferences& prefs)
{
  registerUserConfigurableSetting(CATEGORY_WIFI, WIFI_AP_MODE, RedLilyGOCapability::SettingType::Bool, WIFI_AP_MODE_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WIFI_AP_NAME, RedLilyGOCapability::SettingType::String, WIFI_AP_NAME_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WIFI_AP_PWD, RedLilyGOCapability::SettingType::String, WIFI_AP_PWD_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WIFI_AP_IP, RedLilyGOCapability::SettingType::String, WIFI_AP_IP_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WEB_AUTH, RedLilyGOCapability::SettingType::Bool, WEB_AUTH_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WEB_USER, RedLilyGOCapability::SettingType::String, WEB_USER_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WEB_PASS, RedLilyGOCapability::SettingType::String, WEB_PASS_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, WIFI_DESIRED_BOOT_STATE, RedLilyGOCapability::SettingType::Bool, WIFI_DESIRED_BOOT_STATE_DEFAULT);
  registerUserConfigurableSetting(CATEGORY_WIFI, "portal-ssid", RedLilyGOCapability::SettingType::String, "RedLilyGO-Portal");
  registerUserConfigurableSetting(CATEGORY_WIFI, "portal-pwd", RedLilyGOCapability::SettingType::String, "");

  wifiAPMode = prefs.getBool(WIFI_AP_MODE, WIFI_AP_MODE_DEFAULT);
  wiFiApName = prefs.getString(WIFI_AP_NAME, WIFI_AP_NAME_DEFAULT);
  wiFiApPwd = prefs.getString(WIFI_AP_PWD, WIFI_AP_PWD_DEFAULT);
  wiFiApIp = prefs.getString(WIFI_AP_IP, WIFI_AP_IP_DEFAULT);
  if (wiFiApIp.isEmpty()) wiFiApIp = WIFI_AP_IP_DEFAULT;
  if (!wifiBootStateLoaded)
  {
    desiredWiFiState = prefs.getBool(WIFI_DESIRED_BOOT_STATE, WIFI_DESIRED_BOOT_STATE_DEFAULT);
    wifiBootStateLoaded = true;
  }

  Debug::Log.info(LOG_WIFI, wifiAPMode ? "WiFi mode: AP, password configured" : "WiFi mode: Station, password configured");

  if (desiredWiFiState == true)
  {
  if (wifiAPMode)
    {
      IPAddress apAddress;
      if (!apAddress.fromString(wiFiApIp))
      {
        Debug::Log.error(LOG_WIFI, "Invalid Wi-Fi AP address; using default.");
        apAddress = IPAddress(4, 3, 2, 1);
      }
      WiFi.mode(WIFI_AP);
      if (!WiFi.softAPConfig(apAddress, apAddress, IPAddress(255, 255, 255, 0)))
      {
        Debug::Log.error(LOG_WIFI, "Wi-Fi AP network configuration failed.");
        WiFi.mode(WIFI_OFF);
        return;
      }
      if (!WiFi.softAP(wiFiApName, wiFiApPwd))
      {
        Debug::Log.error(LOG_WIFI, "Wi-Fi AP startup failed.");
        WiFi.mode(WIFI_OFF);
      }
      else
      {
        captiveDns.start(53, "*", apAddress);
        captiveDnsRunning = true;
      }
    }
    else
    {
      WiFi.mode(WIFI_STA);
      WiFi.begin(wiFiApName,wiFiApPwd);
    }
    WiFi.setTxPower(WIFI_POWER);
    WiFi.setSleep(WIFI_SLEEP);
  } 
}

void WiFiDevice::loop(Preferences& prefs)
{
  // Evil Portal owns port 80. Handle it before generic Marauder activity,
  // because isActive() is also true while portal is running.
  if (Attacks::Marauder.isEvilPortalActive())
  {
    if (webServerRunning || Comms::Web.isRunning())
    {
      Debug::Log.info(LOG_WIFI, "Stopping dashboard while Evil Portal owns port 80.");
      Comms::Web.end();
      webServerRunning = false;
    }
    return;
  }

  // Marauder temporarily owns the radio while a scan or attack is active.
  // Do not let dashboard reconnect/maintenance calls change Wi-Fi state mid-command.
  // Keep servicing the web loop so stop commands, telemetry, and WebSockets remain
  // responsive while an attack is running.
  if (Attacks::Marauder.isActive())
  {
    if (captiveDnsRunning) captiveDns.processNextRequest();
    if (WiFi.getMode() != WIFI_OFF) Comms::Web.loop(prefs);
    return;
  }

  if (webServerRunning && !Comms::Web.isRunning())
  {
    webServerRunning = false;
  }
  if (webServerRunning && desiredWiFiState && !getState())
  {
    // AP status can be transient while a client associates. Keep the server
    // alive so a phone connection cannot make the dashboard disappear.
    if (!wifiAPMode || WiFi.getMode() != WIFI_MODE_AP)
    {
      Debug::Log.warning(LOG_WIFI, "Wi-Fi disconnected. Stopping web server until reconnect.");
      Comms::Web.end();
      webServerRunning = false;
    }
  }
  else if (!webServerRunning && desiredWiFiState && getState())
  {
    Debug::Log.info(LOG_WIFI, "Starting web server.");
    Comms::Web.begin(prefs);
    webServerRunning = true;
  }
  else if (WiFi.getMode() != WIFI_OFF && desiredWiFiState == false)
  {
    Debug::Log.info(LOG_WIFI, "Disabling Wi-Fi.");
    WiFi.mode(WIFI_OFF);
    Debug::Log.info(LOG_WIFI, "Wi-Fi disabled.");
    Comms::Web.end();
    Debug::Log.info(LOG_WIFI, "Web server stopped.");
    webServerRunning = false;
  }
  else if (WiFi.getMode() == WIFI_OFF && desiredWiFiState == true)
  {
    Debug::Log.info(LOG_WIFI, "Enabling WiFi");
    begin(prefs);
  }
  else if (!webServerRunning && desiredWiFiState && WiFi.getMode() != WIFI_OFF)
  {
    // Evil Portal disconnects the soft AP; bring the dashboard AP back.
    Debug::Log.info(LOG_WIFI, "Restoring dashboard AP after portal stop.");
    begin(prefs);
  }
  else if (WiFi.getMode() != WIFI_OFF)
  {
    if (captiveDnsRunning) captiveDns.processNextRequest();
    Comms::Web.loop(prefs);
  }

  // Log station connectivity only when the Agent can report it, and at most
  // once per minute to keep routine reconnects out of the log stream.
  const uint64_t uptimeMinute = static_cast<uint64_t>(esp_timer_get_time()) / 60000000;
  if (Attacks::Agent.isAgentConnected() && !wifiAPMode && WiFi.status() == WL_CONNECTED && uptimeMinute != lastMinValueChecked) {
    Debug::Log.info(LOG_WIFI, "WiFi connection established");
    lastMinValueChecked = uptimeMinute;
  }
}

void WiFiDevice::end()
{
  desiredWiFiState = false;
  if (captiveDnsRunning) {
    captiveDns.stop();
    captiveDnsRunning = false;
  }
  WiFi.mode(WIFI_OFF);
  Comms::Web.end();
  webServerRunning = false;
}
#else
WiFiDevice::WiFiDevice()
{
}

void WiFiDevice::setWiFi(const bool& state)
{
}

void WiFiDevice::begin(Preferences& prefs)
{
}

void WiFiDevice::loop(Preferences& prefs)
{
}

void WiFiDevice::end()
{
}

bool WiFiDevice::getState()
{
  return false;
}

int WiFiDevice::signalStrength()
{
  return 0;
}

std::string WiFiDevice::currentIPAddress()
{
  return std::string();
}

#endif
