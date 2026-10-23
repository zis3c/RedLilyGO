"""Run firmware logic on the host with in-memory device doubles. Requires g++."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index("\n}", start) + 2]


def web_source():
    """Concatenate the split web translation units so extraction survives refactors."""
    web_dir = ROOT / "src" / "Comms" / "Web"
    return "\n".join(
        (web_dir / name).read_text(encoding="utf-8")
        for name in sorted(path.name for path in web_dir.glob("*.cpp"))
    )


def display_source():
    """Concatenate the split display translation units so extraction survives refactors."""
    display_dir = ROOT / "src" / "Devices" / "Display"
    return "\n".join(
        (display_dir / name).read_text(encoding="utf-8")
        for name in sorted(path.name for path in display_dir.glob("*.cpp"))
    )


def web_function(text, signature):
    """Extract a WebInternal member and flatten its namespace for host doubles."""
    return function(text, signature).replace("WebInternal::", "")


def logging_check():
    logging = source("src/Diagnostics/Logging.cpp")
    logging = re.sub(r'^#include .*$', '', logging, flags=re.MULTILINE)
    logging = logging[:logging.index("void Logging::begin")]
    # Track mutex ownership deterministically, without scheduling races.
    logging = logging.replace("std::mutex", "CheckedMutex")
    return r'''
#include <cassert>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <utility>
struct CheckedMutex {
    bool locked = false;
    void lock() { assert(!locked); locked = true; }
    void unlock() { assert(locked); locked = false; }
};
struct Preferences {};
struct JsonArray { void add(const std::string&) {} };
uint32_t millis() { return 1234; }
#define MAX_LOG_ENTRY_IN_CHARS 350
class Logging {
public:
    Logging();
    std::vector<std::string> getLogs();
    void appendLogsToJson(JsonArray);
    void clearLogs();
    void info(const char*, const std::string&);
    void warning(const char*, const std::string&);
    void error(const char*, const std::string&);
    void loop(Preferences&);
    std::string systemLogStatus() const;
};
namespace Devices {
    struct StorageMock {
        bool available = true, fail = false;
        std::vector<std::string> written;
        bool appendFileQuiet(const char*, const uint8_t*, size_t);
        bool isRunning() { return available; }
        bool isFull() { return false; }
    } Storage;
    namespace USB { struct { void writeDebugString(const std::string&) {} } CDC; }
}
namespace Attacks { struct { bool isAgentConnected() { return false; } } Agent; }
''' + logging + r'''
bool Devices::StorageMock::appendFileQuiet(const char*, const uint8_t* data, size_t size) {
    assert(!diskLogMutex.locked && "SD I/O while holding disk log mutex can deadlock");
    if (!available || fail) return false;
    written.emplace_back(reinterpret_cast<const char*>(data), size);
    return true;
}
int main() {
    Preferences prefs;
    Debug::Log.info("SD", "first");
    Devices::Storage.fail = true;
    Debug::Log.loop(prefs);
    assert(Debug::Log.systemLogStatus() == "write_failed");
    Debug::Log.info("SD", "second");
    Devices::Storage.fail = false;
    Debug::Log.loop(prefs);
    assert(Debug::Log.systemLogStatus() == "active");
    assert(Devices::Storage.written.size() == 2);
    assert(Devices::Storage.written[0].find("first") != std::string::npos);
    assert(Devices::Storage.written[1].find("second") != std::string::npos);
    assert(pendingDiskLogs.empty());
    for (int i = 0; i < 1000; ++i) Debug::Log.info("SD", std::to_string(i));
    assert(Debug::Log.getLogs().size() <= 30);
    assert(pendingDiskLogs.size() <= 256);
    assert(Debug::Log.getLogs().back().find("999") != std::string::npos);
    Debug::Log.warning("SD", std::string(10000, 'x'));
    Debug::Log.error("SD", std::string(10000, 'x'));
    for (const auto& line : Debug::Log.getLogs()) assert(line.size() < 400);
    Debug::Log.loop(prefs);
    assert(pendingDiskLogs.empty());
    puts("PASS logging lock order, retry, recovery and memory bounds");
}
'''


def web_security_check():
    web = web_source()
    functions = "\n".join(web_function(web, signature) for signature in [
        "bool WebInternal::authRateLimited", "void WebInternal::recordAuthFailure", "void WebInternal::clearAuthFailures",
        "bool WebInternal::requestAuthorized", "bool WebInternal::isWebSocketAuthorized", "bool WebInternal::rejectCrossSiteMutation",
        "String WebInternal::csrfToken", "void WebInternal::resetCsrfToken", "bool WebInternal::requestHasValidCsrf",
        "bool WebInternal::requestNeedsMutationGuard"])
    return r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& s) : std::string(s) {}
    bool isEmpty() const { return empty(); }
    bool equalsIgnoreCase(const char* text) const {
        std::string value = *this;
        for (auto& c : value) c = std::tolower(static_cast<unsigned char>(c));
        return value == text;
    }
};
uint32_t now = 0;
uint32_t millis() { return now; }
struct AuthFailureState { uint8_t count = 0; uint32_t blockedUntil = 0; };
std::unordered_map<std::string, AuthFailureState> authFailures;
std::mutex authFailuresMutex;
std::recursive_mutex webLifecycleMutex;
struct Preferences {
    bool enabled = true;
    bool getBool(const char*, bool) { return enabled; }
    String getString(const char*, const char* fallback) { return fallback; }
} prefs;
Preferences* preferences = &prefs;
struct AsyncWebServerRequest {
    std::unordered_map<std::string, String> headers;
    std::unordered_map<std::string, bool> params;
    String requestUrl = "/data.json";
    bool valid = true;
    int status = 0, authCalls = 0;
    String header(const char* key) { return headers[key]; }
    String url() { return requestUrl; }
    bool hasParam(const char* name) { return params.count(name) > 0; }
    bool authenticate(const char*, const char*) { ++authCalls; return valid; }
    void send(int code) { status = code; }
};
String csrfValue;
std::mutex csrfMutex;
uint32_t esp_random() { static uint32_t next = 0x12345678u; return next += 0x01010101u; }
String requestIp(AsyncWebServerRequest*) { return "client"; }
namespace Debug { struct {
    void error(const char*, const char*) {}
    void warning(const char*, const char*) {}
} Log; }
#define LOG_WEB "WEB"
''' + functions + r'''
int main() {
    assert(!requestAuthorized(nullptr));
    assert(!isWebSocketAuthorized(nullptr));
    assert(rejectCrossSiteMutation(nullptr));
    AsyncWebServerRequest request;
    request.valid = false;
    for (int i = 0; i < 5; ++i) assert(!requestAuthorized(&request));
    assert(authRateLimited("client"));
    request.valid = true;
    assert(!requestAuthorized(&request) && request.authCalls == 5);
    now = 29999;
    assert(authRateLimited("client"));
    now = 30000;
    assert(requestAuthorized(&request) && authFailures.empty());
    for (uint32_t start : {UINT32_MAX - 10000, UINT32_MAX - 29999}) {
        now = start;
        for (int i = 0; i < 5; ++i) recordAuthFailure("rollover");
        assert(authRateLimited("rollover"));
        now = start + 29999;
        assert(authRateLimited("rollover"));
        now = start + 30000;
        assert(!authRateLimited("rollover"));
        recordAuthFailure("rollover");
        assert(authFailures.at("rollover").count == 1);
        clearAuthFailures("rollover");
    }
    for (int i = 0; i < 100; ++i) recordAuthFailure(String(std::to_string(i)));
    assert(authFailures.size() == 32);
    authFailures.clear();
    request.headers["Host"] = "4.3.2.1";
    request.headers["Origin"] = "http://4.3.2.1";
    assert(isWebSocketAuthorized(&request) && !rejectCrossSiteMutation(&request));
    for (const char* origin : {"http://foreign.example", "null", "http://4.3.2.1.evil.example"}) {
        request.headers["Origin"] = origin;
        assert(!isWebSocketAuthorized(&request));
        assert(rejectCrossSiteMutation(&request) && request.status == 403);
    }
    request.headers["Origin"] = "";
    for (const char* site : {"cross-site", "same-site"}) {
        request.headers["Sec-Fetch-Site"] = site;
        assert(!isWebSocketAuthorized(&request) && rejectCrossSiteMutation(&request));
    }
    for (const char* site : {"same-origin", "none", ""}) {
        request.headers["Sec-Fetch-Site"] = site;
        assert(isWebSocketAuthorized(&request) && !rejectCrossSiteMutation(&request));
    }
    request.headers["Origin"] = "";
    request.headers["Sec-Fetch-Site"] = "";
    request.requestUrl = "/marauder";
    assert(requestNeedsMutationGuard(&request));
    request.requestUrl = "/data.json";
    assert(!requestNeedsMutationGuard(&request));
    request.requestUrl = "/evilportal";
    assert(!requestNeedsMutationGuard(&request));
    request.params["action"] = true;
    assert(requestNeedsMutationGuard(&request));
    request.params.clear();
    const String token = csrfToken();
    assert(!token.isEmpty());
    request.headers["X-CSRF-Token"] = token;
    assert(requestHasValidCsrf(&request));
    request.headers["X-CSRF-Token"] = "wrong-token";
    assert(!requestHasValidCsrf(&request));
    assert(!requestHasValidCsrf(nullptr));
    resetCsrfToken();
    prefs.enabled = false;
    request.valid = false;
    assert(requestAuthorized(&request));
    puts("PASS real web auth, bounded lockout, timer rollover, HTTP/WebSocket origin guards and CSRF mutation guard");
}
'''


def web_mutation_gate_check():
    """Drive the dispatcher's POST+token decision matrix through a host double."""
    web = web_source()
    functions = "\n".join(web_function(web, signature) for signature in [
        "String WebInternal::csrfToken", "void WebInternal::resetCsrfToken", "bool WebInternal::requestHasValidCsrf",
        "bool WebInternal::requestNeedsMutationGuard"])
    return r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& s) : std::string(s) {}
    bool isEmpty() const { return empty(); }
};
enum HttpMethod { HTTP_GET, HTTP_POST, HTTP_ANY, HTTP_OPTIONS };
static const int MUTATION_GUARD = 1;
struct AsyncWebServerRequest {
    std::unordered_map<std::string, String> headers;
    std::unordered_map<std::string, bool> params;
    String requestUrl = "/data.json";
    HttpMethod httpMethod = HTTP_GET;
    int status = 0;
    String header(const char* key) { return headers[key]; }
    String url() { return requestUrl; }
    HttpMethod method() { return httpMethod; }
    bool hasParam(const char* name) { return params.count(name) > 0; }
    void send(int code) { status = code; }
};
String csrfValue;
std::mutex csrfMutex;
uint32_t esp_random() { static uint32_t next = 0x0badf00du; return next += 0x9e3779b9u; }
''' + functions + r'''
// Mirror the dispatcher gate order exactly: cross-site -> method -> token.
static int dispatchGate(AsyncWebServerRequest& request, bool crossSite) {
    if (!requestNeedsMutationGuard(&request)) { request.status = 0; return request.status; }
    if (crossSite) { request.send(403); return MUTATION_GUARD; }
    if (request.method() != HTTP_POST) { request.send(405); return MUTATION_GUARD; }
    if (!requestHasValidCsrf(&request)) {
        request.send(request.headers["X-CSRF-Token"].empty() ? 403 : 403);
        return MUTATION_GUARD;
    }
    return 0;
}
int main() {
    const String token = csrfToken();
    assert(!token.isEmpty());
    const String other = csrfToken();
    assert(token == other && "token must be stable within a boot");

    // Read routes never enter the mutation gate regardless of method.
    for (const char* url : {"/data.json", "/telemetry", "/health", "/files",
                            "/download", "/systemlog", "/wifi/scan", "/deauth",
                            "/evilportal/files", "/csrf-token"}) {
        AsyncWebServerRequest r; r.requestUrl = url; r.httpMethod = HTTP_GET;
        assert(!requestNeedsMutationGuard(&r));
        assert(dispatchGate(r, false) == 0);
    }

    // /evilportal is read-only without an action and guarded once action is set.
    {
        AsyncWebServerRequest r; r.requestUrl = "/evilportal"; r.httpMethod = HTTP_GET;
        assert(!requestNeedsMutationGuard(&r) && dispatchGate(r, false) == 0);
        r.params["action"] = true;
        assert(requestNeedsMutationGuard(&r) && dispatchGate(r, false) == MUTATION_GUARD && r.status == 405);
    }

    // Every guarded route rejects GET with 405 and a tokenless POST with 403.
    for (const char* url : {"/clearlogs", "/runfile", "/rawinput", "/runagentcmd",
                            "/showimage", "/marauder", "/capture", "/mic", "/system",
                            "/delete", "/set", "/settings", "/channelhop", "/moveFile",
                            "/uploadFile"}) {
        AsyncWebServerRequest r; r.requestUrl = url; r.httpMethod = HTTP_GET;
        assert(requestNeedsMutationGuard(&r));
        assert(dispatchGate(r, false) == MUTATION_GUARD && r.status == 405);
        r.httpMethod = HTTP_POST;
        assert(dispatchGate(r, false) == MUTATION_GUARD && r.status == 403);
        r.headers["X-CSRF-Token"] = token;
        assert(dispatchGate(r, false) == 0);
        // A stale/bogus token is rejected even with POST.
        r.headers["X-CSRF-Token"] = "stale";
        assert(dispatchGate(r, false) == MUTATION_GUARD && r.status == 403);
        // Cross-site POST with a valid token is still blocked before the token check.
        r.headers["X-CSRF-Token"] = token;
        assert(dispatchGate(r, true) == MUTATION_GUARD && r.status == 403);
    }

    // Token rotates on reset: an old token must stop being accepted.
    resetCsrfToken();
    const String rotated = csrfToken();
    assert(rotated != token && "reset must issue a new boot token");
    {
        AsyncWebServerRequest r; r.requestUrl = "/delete"; r.httpMethod = HTTP_POST;
        r.headers["X-CSRF-Token"] = token;
        assert(dispatchGate(r, false) == MUTATION_GUARD && r.status == 403);
        r.headers["X-CSRF-Token"] = rotated;
        assert(dispatchGate(r, false) == 0);
    }
    puts("PASS dispatcher POST-only + CSRF token matrix, read-route exemption and boot rotation");
}
'''


def websocket_check():
    web = web_source()
    return r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>
enum AwsEventType { WS_EVT_CONNECT, WS_EVT_DATA, WS_EVT_DISCONNECT, WS_EVT_ERROR };
enum class HostCommand { WSCONNECT, WSDATA, WSDISCONNECT };
struct AsyncWebSocket {};
struct AsyncWebSocketClient {
    uint32_t identifier;
    int closeCode = 0;
    uint32_t id() { return identifier; }
    void close(int code, const char*) { closeCode = code; }
};
std::atomic<uint32_t> vncClientCount{0};
struct CheckedMutex {
    bool locked = false;
    void lock() { assert(!locked); locked = true; }
    void unlock() { assert(locked); locked = false; }
} vncClientsMutex;
std::unordered_set<uint32_t> vncClientIds;
const char* remoteAddress = "127.0.0.1:7002";
struct Frame { HostCommand kind; std::vector<uint8_t> bytes; };
namespace Devices {
    namespace USB { struct {
        bool ready = true;
        std::vector<Frame> frames;
        bool isReady() { return ready; }
        void writeBinary(HostCommand kind, uint8_t* data, size_t size) {
            assert(vncClientsMutex.locked && "bridge commands must be serialized with ownership changes");
            std::vector<uint8_t> bytes;
            if (size) bytes.assign(data, data + size);
            frames.push_back({kind, bytes});
        }
        void writeDebugString(const std::string&) {}
    } CDC; }
    struct { int wakes = 0; void wakeForActivity() { ++wakes; } } Display;
}
namespace Attacks { struct {
    bool connected = true;
    bool isAgentConnected() { return connected; }
} Agent; }
namespace Debug { struct { void warning(const char*, const char*) {} } Log; }
#define LOG_WEB "WEB"
''' + web_function(web, "void WebInternal::onWsEvent").replace("std::lock_guard<std::mutex>", "std::lock_guard<CheckedMutex>") + r'''
void event(AsyncWebSocketClient* client, AwsEventType type, uint8_t* data = nullptr, size_t size = 0) {
    onWsEvent(nullptr, client, type, nullptr, data, size);
}
int main() {
    AsyncWebSocketClient a{1}, b{2}, unknown{99};
    auto& bridge = Devices::USB::CDC;
    bridge.ready = false;
    event(&a, WS_EVT_CONNECT);
    assert(a.closeCode == 1013 && vncClientCount == 0 && bridge.frames.empty());
    bridge.ready = true;
    Attacks::Agent.connected = false;
    event(&b, WS_EVT_CONNECT);
    assert(b.closeCode == 1013 && vncClientCount == 0 && bridge.frames.empty());
    Attacks::Agent.connected = true;
    event(nullptr, WS_EVT_CONNECT);
    event(&a, WS_EVT_CONNECT);
    event(&a, WS_EVT_CONNECT);
    event(&b, WS_EVT_CONNECT);
    assert(vncClientCount == 2 && bridge.frames.size() == 1 && Devices::Display.wakes == 1);
    assert(bridge.frames[0].kind == HostCommand::WSCONNECT);
    assert(std::string(bridge.frames[0].bytes.begin(), bridge.frames[0].bytes.end()) == remoteAddress);
    uint8_t bytes[] = {0, 1, 255};
    event(&unknown, WS_EVT_DATA, bytes, sizeof(bytes));
    event(nullptr, WS_EVT_DATA, bytes, sizeof(bytes));
    event(&a, WS_EVT_DATA, nullptr, sizeof(bytes));
    event(&a, WS_EVT_DATA, bytes, 0);
    assert(bridge.frames.size() == 1);
    event(&a, WS_EVT_DATA, bytes, sizeof(bytes));
    assert(bridge.frames.size() == 2 && bridge.frames.back().kind == HostCommand::WSDATA);
    assert(bridge.frames.back().bytes == std::vector<uint8_t>({0, 1, 255}));
    event(&unknown, WS_EVT_DISCONNECT);
    event(&a, WS_EVT_DISCONNECT);
    event(&a, WS_EVT_DISCONNECT);
    assert(vncClientCount == 1 && bridge.frames.size() == 2);
    event(&a, WS_EVT_DATA, bytes, sizeof(bytes));
    assert(bridge.frames.size() == 2);
    event(&b, WS_EVT_DISCONNECT);
    event(&b, WS_EVT_DISCONNECT);
    event(nullptr, WS_EVT_DISCONNECT);
    assert(vncClientCount == 0 && vncClientIds.empty() && bridge.frames.size() == 3);
    assert(bridge.frames.back().kind == HostCommand::WSDISCONNECT);
    event(&a, WS_EVT_CONNECT);
    assert(vncClientCount == 1 && bridge.frames.size() == 4 && Devices::Display.wakes == 2);
    event(&a, WS_EVT_DISCONNECT);
    puts("PASS WebSocket bridge readiness, ownership, exact binary data and first/last client lifecycle");
}
'''


def upload_check():
    web = web_source()
    validation = source("src/Comms/Web/RequestValidation.h")
    storage = source("src/Devices/Storage/StorageDevice.cpp")
    functions = "\n".join(function(validation, signature) for signature in [
        "inline bool validStoragePath", "inline bool validUploadFilename", "inline bool hasExtensionIgnoreCase",
        "inline bool validUsername", "inline bool validCommand", "inline bool validWifiText", "inline String fileTypeForPath"])
    functions = re.search(r'static constexpr size_t MAX_COMMAND_LENGTH = \d+;', validation).group(0) + "\n" + functions
    functions += "\n" + storage[storage.index("namespace {"):storage.index("#ifdef NO_SD")]
    functions += "\n" + "\n".join(function(storage, signature) for signature in [
        "std::string StorageDevice::directoryForFilename", "bool StorageDevice::isSafeUserPath",
        "std::string StorageDevice::resolveUploadPath", "bool StorageDevice::commitUpload"])
    return (r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
struct CheckedRecursiveMutex {
    unsigned depth = 0;
    void lock() { ++depth; }
    void unlock() { assert(depth); --depth; }
};
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& s) : std::string(s) {}
    bool isEmpty() const { return empty(); }
    int indexOf(const char* s) const { auto i = find(s); return i == npos ? -1 : int(i); }
    int indexOf(char c) const { auto i = find(c); return i == npos ? -1 : int(i); }
    int lastIndexOf(char c) const { auto i = rfind(c); return i == npos ? -1 : int(i); }
    String substring(size_t start) const { return substr(start); }
    void toLowerCase() { for (auto& c : *this) c = std::tolower(static_cast<unsigned char>(c)); }
    bool endsWith(const char* suffix) const { String s(suffix); return size() >= s.size() && compare(size()-s.size(), s.size(), s) == 0; }
};
static std::map<std::string, std::string> files;
static std::set<std::string> directories;
static bool failWrite = false, failCommit = false;
class File {
    std::string name;
    bool opened = false;
    size_t offset = 0;
public:
    File() = default;
    File(std::string path) : name(path), opened(true) {}
    explicit operator bool() const { return opened; }
    void close() { opened = false; }
    bool isDirectory() const { return directories.count(name); }
    const char* path() const { return name.c_str(); }
    size_t position() const { return offset; }
    size_t write(const uint8_t* data, size_t size) {
        if (failWrite || !opened) return 0;
        files[name].append(reinterpret_cast<const char*>(data), size);
        offset += size;
        return size;
    }
};
struct Filesystem {
    bool exists(const char* name) { return files.count(name) || directories.count(name); }
    File open(const char* name, const char* mode) {
        if (*mode == 'w') files[name].clear();
        return exists(name) ? File(name) : File();
    }
    bool remove(const char* name) { return files.erase(name); }
    bool rename(const char* from, const char* to) {
        if (exists(to) || !files.count(from)) return false;
        if (failCommit && String(from).endsWith(".tmp")) return false;
        files[to] = files.at(from); files.erase(from); return true;
    }
} filesystem;
#define FILE_INTERFACE filesystem
class StorageDevice {
    bool running = true;
    std::recursive_mutex storageMutex;
public:
    static std::string directoryForFilename(const std::string&);
    static std::string resolveUploadPath(const std::string&, const std::string&);
    static bool isSafeUserPath(const std::string&);
    bool commitUpload(const std::string&, const std::string&, bool);
    bool isRunning() { return running; }
    bool doesFileExist(const std::string& name) { return filesystem.exists(name.c_str()); }
    File openFile(const std::string& name, const char* mode) { return filesystem.open(name.c_str(), mode); }
    bool deleteFile(const std::string& name) { return filesystem.remove(name.c_str()); }
    bool createEmptyFile(const std::string& name) { files[name].clear(); return true; }
    bool writeFileChunk(const std::string& name, const uint8_t* data, size_t len, bool append) {
        if (!append) files[name].clear();
        files[name].append(reinterpret_cast<const char*>(data), len); return true;
    }
    void ensureStandardDirectories() {}
    void refreshCache() {}
    std::recursive_mutex& lifecycleMutex() { return storageMutex; }
};
namespace Devices { StorageDevice Storage; }
namespace Attacks { struct {
    void setPayload(const char*) {
        assert(Devices::Storage.lifecycleMutex().depth == 0 && "payload lock must precede storage lock");
    }
} Ducky; }
namespace Debug { struct {
    void info(const char*, const std::string&) {}
    void error(const char*, const std::string&) {}
} Log; }
struct Parameter { String text; const String& value() { return text; } };
struct AsyncWebServerRequest {
    File _tempFile;
    std::map<std::string, Parameter> params;
    std::function<void()> disconnect;
    int status = 0;
    bool authorized = true, crossSite = false;
    String response;
    ~AsyncWebServerRequest() { if (disconnect) disconnect(); }
    String url() { return "/uploadFile"; }
    int* getResponse() { return status ? &status : nullptr; }
    bool hasParam(const char* name) { return params.count(name); }
    Parameter* getParam(const char* name) { return &params.at(name); }
    void send(int code, const char* = "", const String& body = "") { status = code; response = body; }
    void requestAuthentication() { send(401); }
    bool authenticate(const char*, const char*) { return authorized; }
    void onDisconnect(std::function<void()> callback) { disconnect = callback; }
};
static std::recursive_mutex webLifecycleMutex;
static constexpr size_t MAX_UPLOAD_BYTES = 4 * 1024 * 1024;
#define LOG_WEB "WEB"
uint32_t esp_random() { static uint32_t next = 0; return ++next; }
struct Preferences {
    bool enabled = true;
    bool getBool(const char*, bool) { return enabled; }
    String getString(const char*, const char* fallback) { return fallback; }
} authPreferences;
Preferences* preferences = &authPreferences;
bool authBlocked = false;
int authFailures = 0, authSuccesses = 0;
String requestIp(AsyncWebServerRequest*) { return "127.0.0.1"; }
bool authRateLimited(const String&) { return authBlocked; }
void recordAuthFailure(const String&) { ++authFailures; }
void clearAuthFailures(const String&) { ++authSuccesses; }
bool rejectCrossSiteMutation(AsyncWebServerRequest* r) { if (r->crossSite) r->send(403); return r->crossSite; }
bool requestHasValidCsrf(AsyncWebServerRequest*) { return true; }
''' + functions + "\n" + web_function(web, "bool WebInternal::requestAuthorized") + "\n" + web_function(web, "void WebInternal::handleUpload") + r'''
void chunk(AsyncWebServerRequest& r, const char* filename, size_t offset, const char* data, bool final) {
    handleUpload(&r, filename, offset, reinterpret_cast<uint8_t*>(const_cast<char*>(data)), std::string(data).size(), final);
}
int main() {
    assert(!requestAuthorized(nullptr));
    {
        AsyncWebServerRequest r;
        r.authorized = false;
        assert(!requestAuthorized(&r) && authFailures == 1);
        r.authorized = true;
        assert(requestAuthorized(&r) && authSuccesses == 1);
        authBlocked = true;
        assert(!requestAuthorized(&r));
        authBlocked = false;
        authPreferences.enabled = false;
        r.authorized = false;
        assert(requestAuthorized(&r));
        authPreferences.enabled = true;
    }
    assert(validUsername("admin_01") && validUsername(String(32, 'a')));
    for (const String& value : {String("ab"), String(33, 'a'), String("a b"), String("ab\n")}) assert(!validUsername(value));
    assert(validCommand("scanap") && validCommand(String(MAX_COMMAND_LENGTH, 'x')));
    for (const String& value : {String(""), String("   "), String("scan\n"), String("cmd\x7f"), String(MAX_COMMAND_LENGTH + 1, 'x')}) assert(!validCommand(value));
    assert(validWifiText("my wifi", 32) && validWifiText(String(63, 'x'), 63));
    assert(!validWifiText("   ", 32) && !validWifiText("bad\r", 32) && !validWifiText(String(33, 'x'), 32));
    assert(validStoragePath("/Scripts/good.ds") && validUploadFilename("good.ds"));
    for (const String& value : {String("relative.ds"), String("/../bad"), String("/bad\\path"), String("/bad\"path"), String("/bad\n")}) assert(!validStoragePath(value));
    for (const String& value : {String("../bad.ds"), String("folder/bad.ds"), String("bad\x7f")}) assert(!validUploadFilename(value));
    assert(hasExtensionIgnoreCase("/Scripts/TEST.DS", ".ds"));
    assert(fileTypeForPath("/Logs/test.JSONL") == "capture" && fileTypeForPath("/Scripts/a.DS") == "ducky");
    assert(fileTypeForPath("/Other/unknown") == "other");
    files["/Scripts/a.ds"] = "original";
    {
        AsyncWebServerRequest r;
        chunk(r, "a.ds", 0, "bad", false);
        assert(r.status == 409);
        chunk(r, "a.ds", 3, "tail", true);
        assert(r.status == 409 && files.at("/Scripts/a.ds") == "original");
    }
    {
        AsyncWebServerRequest r;
        r.params["replace"].text = "1";
        chunk(r, "a.ds", 0, "new", false);
        assert(files.at("/Scripts/a.ds") == "original");
        chunk(r, "a.ds", 3, " version", true);
        assert(r.status == 200 && files.at("/Scripts/a.ds") == "new version");
    }
    for (bool writeFailure : {false, true}) {
        AsyncWebServerRequest r;
        r.params["replace"].text = "1";
        failWrite = writeFailure;
        chunk(r, "a.ds", 0, "partial", false);
        failWrite = false;
        assert(files.at("/Scripts/a.ds") == "new version");
        // Destruction simulates a disconnected or failed upload.
    }
    {
        AsyncWebServerRequest r;
        handleUpload(&r, "a.ds", 0, nullptr, MAX_UPLOAD_BYTES + 1, true);
        assert(r.status == 413 && files.at("/Scripts/a.ds") == "new version");
    }
    {
        AsyncWebServerRequest r;
        r.params["replace"].text = "1";
        failCommit = true;
        chunk(r, "a.ds", 0, "cannot commit", true);
        failCommit = false;
        assert(r.status == 500 && files.at("/Scripts/a.ds") == "new version");
    }
    {
        AsyncWebServerRequest r;
        files["/legacy/a.ds"] = "legacy";
        r.params["replace"].text = "1";
        r.params["path"].text = "/legacy/a.ds";
        chunk(r, "a.ds", 0, "edited", true);
        assert(r.status == 200 && files.at("/legacy/a.ds") == "edited");
        assert(files.at("/Scripts/a.ds") == "new version");
    }
    {
        AsyncWebServerRequest r;
        chunk(r, "empty.ds", 0, "", true);
        assert(r.status == 200 && files.at("/Scripts/empty.ds").empty());
    }
    for (bool crossSite : {false, true}) {
        AsyncWebServerRequest r;
        r.authorized = false; r.crossSite = crossSite;
        chunk(r, "blocked.ds", 0, "blocked", true);
        assert(r.status == (crossSite ? 403 : 401));
        assert(!files.count("/Scripts/blocked.ds"));
    }
    {
        AsyncWebServerRequest r;
        chunk(r, "../escape.ds", 0, "blocked", true);
        assert(r.status == 400);
    }
    {
        AsyncWebServerRequest r;
        chunk(r, "offset.ds", 0, "first", false);
        chunk(r, "offset.ds", 2, "invalid offset", true);
        assert(r.status == 400 && !files.count("/Scripts/offset.ds"));
    }
    {
        AsyncWebServerRequest r;
        chunk(r, "race.ds", 0, "first", false);
        files["/Scripts/race.ds"] = "created during upload";
        chunk(r, "race.ds", 5, "tail", true);
        assert(r.status == 409 && files.at("/Scripts/race.ds") == "created during upload");
    }
    {
        AsyncWebServerRequest r;
        directories.insert("/Other/folder");
        r.params["replace"].text = "1";
        chunk(r, "folder", 0, "must not replace directory", true);
        assert(r.status == 500 && directories.count("/Other/folder"));
        assert(!files.count("/Other/folder"));
    }
    for (const auto& entry : files) assert(entry.first.find(".upload-") == std::string::npos);
    puts("PASS uploads: conflicts, chunks, disconnects, write/commit failures, paths and authentication");
}
''').replace("std::recursive_mutex", "CheckedRecursiveMutex")


def cdc_check():
    cdc = re.sub(r'^#include .*$', '', source("src/Devices/USB/USBCDC.cpp"), flags=re.MULTILINE)
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Devices/USB/USBCDC.h"), flags=re.MULTILINE)
    return (r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
static uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
struct CheckedRecursiveMutex {
    int depth = 0;
    void lock() { ++depth; }
    void unlock() { --depth; }
};
struct Preferences { bool raw = false; bool getBool(const char*, bool) { return raw; } };
struct RedLilyGOCapability {
    enum class SettingType { Bool };
    template<class... T> void registerUserConfigurableSetting(T...) {}
};
#define CATEGORY_USB "USB"
enum class USBDeviceType { None, Serial };
namespace Devices::USB { struct {
    USBDeviceType mode = USBDeviceType::Serial;
    USBDeviceType currentDeviceType() { return mode; }
} Core; }
namespace Debug { struct {
    void error(const char*, const std::string&) {}
    void info(const char*, const std::string&) {}
    void warning(const char*, const std::string&) {}
} Log; }
struct SerialMock {
    std::deque<uint8_t> ready;
    std::vector<uint8_t> sent;
    bool endless = false, failWrite = false, connected = true;
    size_t writeLimit = SIZE_MAX;
    size_t bytesRead = 0;
    size_t readLimit = SIZE_MAX;
    int available() { return endless ? 3000 : ready.size(); }
    int read() {
        if (endless) { ++bytesRead; return 0; }
        if (ready.empty()) return -1;
        int result = ready.front(); ready.pop_front(); ++bytesRead; return result;
    }
    size_t read(uint8_t* buffer, size_t size) {
        size_t count = 0;
        size = std::min(size, readLimit);
        while (count < size && available()) buffer[count++] = read();
        return count;
    }
    size_t readBytes(uint8_t*, size_t) {
        assert(false && "CDC must not wait for incoming bytes"); return 0;
    }
    void begin(unsigned long) {}
    void setRxBufferSize(size_t) {}
    void end() { ready.clear(); }
    explicit operator bool() { return connected; }
    void println(const char*) {}
    size_t write(uint8_t byte) { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) {
        assert(false && "Serial.write can spin forever on a stalled host"); return 0;
    }
    size_t fifoWrite(const uint8_t* data, size_t size) {
        if (failWrite) return 0;
        size = std::min(size, writeLimit);
        sent.insert(sent.end(), data, data + size); return size;
    }
    void frame(uint8_t tag, const std::vector<uint8_t>& payload) {
        ready.push_back(tag);
        for (unsigned shift = 0; shift < 32; shift += 8) ready.push_back(payload.size() >> shift);
        ready.insert(ready.end(), payload.begin(), payload.end());
    }
} Serial;
bool tud_cdc_n_connected(int) { return Serial.connected; }
size_t tud_cdc_n_write(int, const uint8_t* data, size_t size) { return Serial.fifoWrite(data, size); }
void tud_cdc_n_write_flush(int) {}
''' + header + cdc + r'''
int main() {
    Preferences prefs;
    USBCDCWrapper cdc;
    cdc.begin(prefs);
    std::vector<std::vector<uint8_t>> received;
    cdc.setCallback(HostCommand::AgentStatus, [&](uint8_t* data, size_t size) {
        assert(cdcMutex.depth == 0 && "callback holds transport lock");
        received.emplace_back(data, data + size);
    });
    const std::vector<uint8_t> payload{1, 2, 3, 4};
    Serial.frame(HostCommand::AgentStatus, payload);
    const auto fragmented = Serial.ready;
    Serial.ready.clear();
    for (uint8_t byte : fragmented) {
        Serial.ready.push_back(byte); ++clockMs; cdc.loop(prefs);
    }
    assert(received.size() == 1 && received.back() == payload);

    // Header already available; each later loop receives another payload chunk.
    Serial.frame(HostCommand::AgentStatus, payload);
    Serial.ready.resize(5); cdc.loop(prefs);
    assert(received.size() == 1);
    Serial.ready = {1}; cdc.loop(prefs);
    Serial.ready = {2, 3, 4}; cdc.loop(prefs);
    assert(received.size() == 2 && received.back() == payload);

    for (int i = 0; i < 10; ++i) Serial.frame(HostCommand::DebugMsg, {99, 98, 97});
    Serial.frame(HostCommand::AgentStatus, payload);
    for (int i = 0; i < 11; ++i) cdc.loop(prefs);
    assert(received.size() == 3 && received.back() == payload && Serial.ready.empty());
    Serial.frame(HostCommand::AgentStatus, {}); cdc.loop(prefs);
    assert(received.size() == 4 && received.back().empty());
    const std::vector<uint8_t> maximum(2048, 0x7e);
    Serial.frame(HostCommand::AgentStatus, maximum); cdc.loop(prefs);
    assert(received.size() == 5 && received.back() == maximum);

    // Actual reads may be shorter than available() without corrupting the frame.
    Serial.frame(HostCommand::AgentStatus, payload);
    Serial.readLimit = 1;
    for (int i = 0; i < 9; ++i) cdc.loop(prefs);
    Serial.readLimit = SIZE_MAX;
    assert(received.size() == 6 && received.back() == payload);

    // Deadline covers the whole frame and survives millis() wraparound.
    for (bool partialHeader : {false, true}) {
        clockMs = UINT32_MAX - 500;
        Serial.frame(HostCommand::AgentStatus, payload);
        Serial.ready.resize(partialHeader ? 1 : 6); cdc.loop(prefs);
        clockMs += 999;
        Serial.ready.push_back(2); cdc.loop(prefs);
        assert(received.size() == 6);
        ++clockMs; cdc.loop(prefs);
        Serial.frame(HostCommand::AgentStatus, payload); cdc.loop(prefs);
        assert(received.back() == payload);
        received.pop_back();
    }

    // Repeated malformed headers cannot permanently disable the transport.
    for (int i = 0; i < 12; ++i) {
        Serial.frame(i % 2 ? HostCommand::MIN : HostCommand::MAX, {}); cdc.loop(prefs);
        assert(cdc.isReady());
    }
    Serial.frame(HostCommand::AgentStatus, std::vector<uint8_t>(2049)); cdc.loop(prefs);
    for (int i = 0; i < 2; ++i) cdc.loop(prefs);
    assert(received.size() == 6 && Serial.ready.empty());
    Serial.frame(HostCommand::AgentStatus, payload); cdc.loop(prefs);
    assert(received.size() == 7 && received.back() == payload);

    // Even a peer sending garbage continuously gets a bounded amount of work.
    Serial.endless = true;
    for (int i = 0; i < 10; ++i) {
        const size_t before = Serial.bytesRead;
        cdc.loop(prefs); assert(Serial.bytesRead - before <= 2053);
    }
    Serial.endless = false; cdc.loop(prefs);
    Serial.frame(HostCommand::AgentStatus, payload); cdc.loop(prefs);
    assert(received.size() == 8);

    Serial.failWrite = true;
    for (int i = 0; i < 10; ++i) cdc.writeBinary(HostCommand::RequestAgentStatus, nullptr, 0);
    Serial.failWrite = false;
    assert(cdc.isReady());
    cdc.loop(prefs); // Flush the status requests queued during the stall.
    Serial.sent.clear(); cdc.writeBinary(HostCommand::AgentStatus, payload.data(), payload.size());
    assert(Serial.sent == std::vector<uint8_t>({8, 4, 0, 0, 0, 1, 2, 3, 4}));

    // Full queues drop whole frames; short FIFO writes preserve frame boundaries.
    Serial.sent.clear(); Serial.failWrite = true;
    cdc.writeBinary(HostCommand::AgentStatus, maximum.data(), maximum.size());
    cdc.writeBinary(HostCommand::AgentStatus, maximum.data(), maximum.size()); // Does not fit.
    cdc.writeBinary(HostCommand::RequestAgentStatus, nullptr, 0);
    Serial.failWrite = false; Serial.writeLimit = 7;
    for (int i = 0; i < 600; ++i) cdc.loop(prefs);
    assert(Serial.sent.size() == 2058);
    assert(Serial.sent[0] == 8 && Serial.sent[1] == 0 && Serial.sent[2] == 8);
    assert(std::vector<uint8_t>(Serial.sent.begin() + 5, Serial.sent.begin() + 2053) == maximum);
    assert(Serial.sent[2053] == 7 && Serial.sent[2054] == 0);
    // Repeated transfers cross the ring boundary without byte loss or reordering.
    Serial.sent.clear();
    for (int i = 0; i < 1000; ++i) {
        cdc.writeBinary(HostCommand::AgentStatus, payload.data(), payload.size());
        cdc.loop(prefs); cdc.loop(prefs);
    }
    const std::vector<uint8_t> frame{8, 4, 0, 0, 0, 1, 2, 3, 4};
    assert(Serial.sent.size() == 9000);
    for (size_t i = 0; i < Serial.sent.size(); ++i) assert(Serial.sent[i] == frame[i % frame.size()]);
    Serial.failWrite = true; cdc.writeBinary(HostCommand::RequestAgentStatus, nullptr, 0);
    Serial.connected = false; cdc.loop(prefs);
    Serial.connected = true; Serial.failWrite = false; Serial.writeLimit = SIZE_MAX;
    Serial.sent.clear(); cdc.loop(prefs); assert(Serial.sent.empty());

    // A mode switch inside a callback cannot free its still-active payload.
    cdc.setCallback(HostCommand::AgentStatus, [&](uint8_t* data, size_t size) {
        assert(cdcMutex.depth == 0); cdc.end();
        assert(!cdc.isReady() && std::vector<uint8_t>(data, data + size) == payload);
    });
    Serial.frame(HostCommand::AgentStatus, payload); cdc.loop(prefs);
    assert(!cdc.isReady());
    cdc.begin(prefs); assert(cdc.isReady());
    prefs.raw = true; cdc.begin(prefs);
    Serial.frame(HostCommand::AgentStatus, payload); cdc.loop(prefs);
    assert(!cdc.isReady() && Serial.ready.size() == 9);
    Serial.failWrite = true;
    cdc.writeDebugString(std::string(4094, 'x')); // Exactly fills the queue with CRLF.
    cdc.writeDebugString("dropped");
    Serial.failWrite = false; cdc.loop(prefs);
    assert(Serial.sent.size() == 4096 && Serial.sent[4094] == '\r' && Serial.sent[4095] == '\n');
    cdc.end();
    puts("PASS CDC nonblocking RX/TX, deadlines, ring bounds, recovery and unlocked callback lifetime");
}
''').replace("std::recursive_mutex", "CheckedRecursiveMutex")


def msc_check():
    msc = source("src/Devices/USB/USBMSC.cpp")
    return r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#define LOGICAL_BLOCK_SIZE 512
#define TAG_USB "USB"
static std::recursive_mutex mscMutex;
struct File {
    bool opened = false;
    uint32_t offset = 0;
    explicit operator bool() const { return opened; }
    bool seek(uint32_t value) { offset = value; return true; }
    size_t readBytes(char*, size_t size) { return size; }
} mscFile;
namespace Debug { struct { void error(const char*, const std::string&) {} } Log; }
namespace Devices {
struct {
    bool running = true;
    uint32_t lastLba = 0, calls = 0;
    bool isRunning() { return running; }
    bool isRawAccessSupported() { return true; }
    size_t sectorSize() { return 512; }
    int32_t readRawSectors(uint8_t*, uint32_t lba, uint32_t sectors) {
        lastLba = lba; ++calls; return sectors * 512;
    }
    int32_t writeRawSectors(uint8_t* buffer, uint32_t lba, uint32_t sectors) {
        return readRawSectors(buffer, lba, sectors);
    }
} Storage;
namespace USB { struct { void setActivityStateState(bool) {} } MSC; }
}
''' + function(msc, "static int32_t msc_read_cb") + "\n" + function(msc, "static int32_t msc_write_cb") + r'''
int main() {
    alignas(4) uint8_t buffer[1024]{};
    const uint32_t fourGiBLba = 8388608;
    for (uint32_t lba : {0U, fourGiBLba - 1, fourGiBLba, fourGiBLba + 1}) {
        assert(msc_read_cb(lba, buffer, sizeof(buffer)) == sizeof(buffer) && "raw SD read capped at 4 GiB");
        assert(Devices::Storage.lastLba == lba);
        assert(msc_write_cb(lba, buffer, sizeof(buffer)) == sizeof(buffer) && "raw SD write capped at 4 GiB");
        assert(Devices::Storage.lastLba == lba);
    }
    auto calls = Devices::Storage.calls;
    assert(msc_read_cb(0, nullptr, 512) == -1);
    assert(msc_write_cb(0, buffer, 511) == -1);
    assert(msc_read_cb(0, buffer, 0) == -1);
    Devices::Storage.running = false;
    assert(msc_read_cb(0, buffer, 512) == -1);
    assert(msc_write_cb(0, buffer, 512) == -1);
    Devices::Storage.running = true;
    mscFile.opened = true;
    assert(msc_read_cb(1, buffer, 512) == 512 && mscFile.offset == 512);
    assert(msc_read_cb(fourGiBLba, buffer, 512) == -1);
    assert(msc_read_cb(fourGiBLba - 1, buffer, sizeof(buffer)) == -1);
    assert(msc_write_cb(0, buffer, 512) == -1);
    assert(Devices::Storage.calls == calls && "image I/O fell through to raw SD");
    puts("PASS MSC raw SD access across 4 GiB, image bounds and invalid I/O");
}
'''


def ducky_check():
    display = display_source()
    button = display[display.index("    // Handle the button gestures"):display.index("    // Handle double-click gestures")]
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Attacks/Ducky/DuckyPayload.h"), flags=re.MULTILINE)
    payload = re.sub(r'^#include .*$', '', source("src/Attacks/Ducky/DuckyPayload.cpp"), flags=re.MULTILINE)
    return r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#define ARDUINO_ARCH_ESP32 1
struct RedLilyGOCapability {};
struct Preferences { bool getBool(const char*, bool value) { return value; } };
static uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
using ExtensionCommands = std::unordered_map<std::string, std::function<int(const std::string&,
    const std::unordered_map<std::string, std::string>&, const std::unordered_map<std::string, int>&)>>;
using UserDefinedConstants = std::vector<std::function<std::pair<std::string, std::string>()>>;
void addDuckyScriptExtensions(ExtensionCommands&, UserDefinedConstants&) {}
struct DuckyInterpreter {
    enum class USB_MODE { OFF };
    static constexpr int SCRIPT_ERROR = -1, END_OF_FILE = -2;
    std::function<int(const std::string&)> execute;
    bool inExecute = false;
    template<class... T> DuckyInterpreter(T...) {}
    void Restart() { assert(!inExecute && "interpreter restarted during Execute"); }
    int Execute(const std::string& file, const ExtensionCommands&, const UserDefinedConstants&) {
        inExecute = true;
        const int result = execute(file);
        inExecute = false;
        return result;
    }
};
namespace Debug { struct {
    std::vector<std::string> errors;
    void info(const char*, const std::string&) {}
    void warning(const char*, const std::string&) {}
    void error(const char*, const std::string& text) { errors.push_back(text); }
} Log; }
struct { void restart() { assert(false); } } ESP;
namespace Devices {
struct {
    bool pressed = false, longPress = false;
    bool hasButtonBeenPressed() { return pressed; }
    bool wasLastPressLong() { return longPress; }
    void resetButtonPressedState() { pressed = false; }
} Button;
struct {
    bool touched = false;
    bool hasBeenTouched() { return touched; }
    void resetTouchState() { touched = false; }
} Touch;
struct {
    size_t autorunSize = 0;
    size_t getFileSize(const std::string&) { return autorunSize; }
    std::string readLineFromFile(const std::string&, int) { return ""; }
} Storage;
struct { bool safeMode = false; bool isSafeMode() { return safeMode; } } Board;
struct { void wakeForActivity() {} } Display;
struct { template<class... T> void changeLEDState(T...) {} } LED;
namespace USB {
struct {
    bool active = false;
    bool hasActivity() { return active; }
    void resetActivityState() { active = false; }
} MSC;
struct {
    int queued = 0;
    template<class... T> void keyboard_press(T...) { ++queued; }
    void keyboard_release() { ++queued; }
    void releaseAll() { queued = 3; }
    void clearQueues() { queued = 0; }
    bool IsQueueEmpty() { return true; }
    bool keyboardQueueHasSpace(size_t = 1) { return true; }
    bool isReady() { return true; }
    template<class... T> void loop(T...) {}
} HID;
struct { template<class... T> void changeUSBMode(T...) {} void end() {} } Core;
}
}
namespace Attacks { struct {
    bool result = false, connected = true;
    bool hasAgentCmdResult() { return result; }
    void resetAgentCmdResultState() { result = false; }
    bool isAgentConnected() { return connected; }
} Agent; }
''' + header + payload + r'''
namespace DisplayInternal {
enum { MODE_DASHBOARD, MODE_SCREENSAVER, MODE_PAYLOAD_SELECTOR, MODE_LOG_VIEWER };
static int currentMode = MODE_DASHBOARD, bootAnimStep = 7, selectedPayloadIdx = 0;
static unsigned long lastActionTime = 0, lastDashboardUpdate = 0;
static bool screensaverInitialized = false, isScreenOff = false, bootScreenCleared = false;
static std::vector<std::string> payloadList;
void payloadSelectorReset() { selectedPayloadIdx = 0; }
void payloadSelectorNext() {}
bool payloadSelectorRunSelected() { return !payloadList.empty(); }
}
using DisplayInternal::isScreenOff;
using DisplayInternal::currentMode;
using DisplayInternal::bootAnimStep;
using DisplayInternal::lastActionTime;
using DisplayInternal::lastDashboardUpdate;
using DisplayInternal::screensaverInitialized;
using DisplayInternal::bootScreenCleared;
using DisplayInternal::MODE_DASHBOARD;
using DisplayInternal::MODE_SCREENSAVER;
using DisplayInternal::MODE_PAYLOAD_SELECTOR;
using DisplayInternal::MODE_LOG_VIEWER;
struct { void clear(int) {} } lcd;
#define TFT_BLACK 0
void powerOff() {}
void powerOn() {}
void displayButton() {
    const auto currentMillis = millis();
''' + button + r'''
}
int main() {
    Preferences prefs;
    auto& ducky = Attacks::Ducky;
    ducky.begin(prefs);
    Devices::Board.safeMode = true;
    Devices::Storage.autorunSize = 128;
    ducky.loop(prefs);
    assert(ducky.getPayloadRunningStatus() == "Ready" && currentlyExecutingFile.empty());
    Devices::Board.safeMode = false;
    Devices::Storage.autorunSize = 0;
    clockMs = UINT32_MAX - 50;
    requestDelay(100);
    clockMs += 99; assert(serviceWait());
    ++clockMs; assert(!serviceWait());
    requestDelay(0); assert(!serviceWait());
    waitForButtonWithTimeout(0);
    clockMs += 60000; assert(serviceWait());
    Devices::Button.pressed = true; Devices::Button.longPress = true;
    displayButton(); // Display runs before Ducky in the real Arduino loop.
    assert(!serviceWait() && lastButtonPressState == 1 && !Devices::Button.pressed);
    assert(!ducky.isWaitingForButton());
    Devices::Button.pressed = true; Devices::Button.longPress = false;
    displayButton(); assert(!Devices::Button.pressed && isScreenOff);
    waitForButtonWithTimeout(200);
    clockMs += 200; assert(!serviceWait() && lastButtonPressState == -1);
    for (int i = 0; i < 10000; ++i) {
        waitForButton(); ducky.stop(); requestDelay(5);
        clockMs += 4; assert(serviceWait());
        ++clockMs; assert(!serviceWait());
    }
    startWait(WaitKind::AgentResult, 30000);
    Attacks::Agent.result = true;
    assert(!serviceWait() && !Attacks::Agent.result && Debug::Log.errors.empty());
    startWait(WaitKind::AgentResult, 30000);
    Attacks::Agent.connected = false;
    assert(!serviceWait() && Debug::Log.errors.size() == 1);
    assert(Debug::Log.errors.back().find("disconnected") != std::string::npos);
    Attacks::Agent.connected = true;
    startWait(WaitKind::AgentResult, 30000);
    clockMs += 30000;
    assert(!serviceWait() && Debug::Log.errors.size() == 2);
    startWait(WaitKind::MscActivity, 30000);
    assert(serviceWait()); Devices::USB::MSC.active = true;
    assert(!serviceWait() && !Devices::USB::MSC.active);
    startWait(WaitKind::MscIdle, 60000);
    clockMs += 900; Devices::USB::MSC.active = true; assert(serviceWait());
    clockMs += 999; assert(serviceWait()); ++clockMs; assert(!serviceWait());
    Devices::Touch.touched = true;
    startWait(WaitKind::Touch, 30000); assert(serviceWait());
    Devices::Touch.touched = true; assert(!serviceWait());
    startWait(WaitKind::Touch, 30000); clockMs += 30000; assert(!serviceWait());

    // Replace a file from another task while one instruction is executing.
    ducky.setPayload("/Scripts/old.ds");
    duckyFileParser.execute = [&](const std::string& file) {
        assert(file == "/Scripts/old.ds");
        std::thread([&] { ducky.setPayload("/Scripts/new.ds"); }).join();
        requestDelay(5000); // Stale instruction cannot stall its replacement.
        return DuckyInterpreter::END_OF_FILE;
    };
    ducky.loop(prefs);
    assert(currentlyExecutingFile == "/Scripts/new.ds" && !serviceWait());
    duckyFileParser.execute = [&](const std::string& file) {
        assert(file == "/Scripts/new.ds"); return DuckyInterpreter::END_OF_FILE;
    };
    ducky.loop(prefs); assert(ducky.getPayloadRunningStatus() == "Ready");

    // A newly queued inline command cannot replace text already being parsed.
    ducky.setPayloadCmdLine("old inline");
    duckyFileParser.execute = [&](const std::string&) {
        std::thread([&] { ducky.setPayloadCmdLine("new inline"); }).join();
        assert(readLineFromFileOrCmdLine("", 0) == "old inline");
        return DuckyInterpreter::SCRIPT_ERROR;
    };
    ducky.loop(prefs); assert(ducky.getPayloadRunningStatus() == "Running");
    duckyFileParser.execute = [&](const std::string&) {
        assert(readLineFromFileOrCmdLine("", 0) == "new inline");
        return DuckyInterpreter::END_OF_FILE;
    };
    ducky.loop(prefs); assert(ducky.getPayloadRunningStatus() == "Ready");
    ducky.setPayload("/Scripts/stop.ds");
    duckyFileParser.execute = [&](const std::string&) {
        std::thread([&] { ducky.stop(); }).join();
        requestDelay(5000); reset();
        keyboard_press(0, 4, 0, 0, 0, 0, 0); keyboard_release();
        assert(Devices::USB::HID.queued == 3 && "Stop lost release reports or accepted stale input");
        return DuckyInterpreter::SCRIPT_ERROR;
    };
    ducky.loop(prefs);
    assert(!serviceWait() && !requiresReset && ducky.getPayloadRunningStatus() == "Ready");
    puts("PASS cooperative waits, rollover, cancellation, Agent result and concurrent payload replacement");
}
'''


def hid_check():
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Devices/USB/USBHID.h"), flags=re.MULTILINE)
    hid = re.sub(r'^#include .*$', '', source("src/Devices/USB/USBHID.cpp"), flags=re.MULTILINE)
    return r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
struct Preferences {};
struct RedLilyGOCapability {};
static uint32_t clockMs = 1;
uint32_t millis() { return clockMs; }
void delay(unsigned long) {}
enum class USBDeviceType { None, Serial };
enum class USBClassType { None, HID };
namespace Devices::USB { struct {
    USBDeviceType currentDeviceType() { return USBDeviceType::Serial; }
    USBClassType currentClassType() { return USBClassType::HID; }
} Core; }
namespace Debug { struct {
    void error(const char*, const std::string&) {}
    void info(const char*, const std::string&) {}
    void warning(const char*, const std::string&) {}
} Log; }
struct {
    bool connected = true;
    bool mounted() { return connected; }
    bool suspended() { return false; }
    void remoteWakeup() {}
} TinyUSBDevice;
#define TUD_HID_REPORT_DESC_KEYBOARD(...) 0
#define TUD_HID_REPORT_DESC_MOUSE(...) 0
#define TUD_HID_REPORT_DESC_CONSUMER(...) 0
#define HID_ITF_PROTOCOL_NONE 0
struct Adafruit_USBD_HID {
    bool writable = true;
    std::vector<std::pair<int, int>> reports;
    template<class... T> Adafruit_USBD_HID(T...) {}
    template<class... T> void setPollInterval(T...) {}
    template<class... T> void setReportDescriptor(T...) {}
    template<class... T> void setStringDescriptor(T...) {}
    bool begin() { return true; }
    bool ready() { return writable; }
    bool report(int id, int value) {
        if (!writable) return false;
        reports.emplace_back(id, value); writable = false; return true;
    }
    bool sendReport16(int id, int value) { return report(id, value); }
    bool keyboardReport(int id, int mods, const uint8_t* keys) { return report(id, mods | keys[0]); }
    bool keyboardRelease(int id) { return report(id, 0); }
    bool mouseButtonRelease(int id) { return report(id, 0); }
    bool mouseButtonPress(int id, int buttons) { return report(id, buttons); }
    bool mouseScroll(int id, int, int) { return report(id, 1); }
    bool mouseMove(int id, int, int) { return report(id, 1); }
};
''' + header + hid + r'''
int main() {
    Preferences prefs;
    USBHID hid;
    hid.begin(prefs);
    hid.keyboard_press(1, 4, 0, 0, 0, 0, 0); hid.loop(prefs);
    assert(usb_hid->reports.back().second != 0);
    hid.consumer_device_keypress(42); hid.mouseButton(0, true);
    hid.releaseAll();
    for (int i = 0; i < 3; ++i) { usb_hid->writable = true; hid.loop(prefs); }
    assert(hid.IsQueueEmpty());
    assert(usb_hid->reports.size() == 4);
    for (size_t i = 1; i < 4; ++i) assert(usb_hid->reports[i].second == 0);
    assert(usb_hid->reports.back().first == RID_KEYBOARD);

    // A stalled endpoint becomes writable once per poll, as on the actual USB bus.
    hid.keyboard_press(0, 5, 0, 0, 0, 0, 0);
    hid.loop(prefs); clockMs += 1500; hid.loop(prefs);
    assert(!hid.IsQueueEmpty() && "host stall discarded release reports");
    const size_t before = usb_hid->reports.size();
    for (int i = 0; i < 3; ++i) { usb_hid->writable = true; hid.loop(prefs); }
    assert(hid.IsQueueEmpty() && usb_hid->reports.size() == before + 3);
    for (size_t i = before; i < before + 3; ++i) assert(usb_hid->reports[i].second == 0);

    hid.keyboard_press(0, 6, 0, 0, 0, 0, 0);
    TinyUSBDevice.connected = false; hid.loop(prefs); assert(hid.IsQueueEmpty());
    hid.releaseAll(); assert(hid.IsQueueEmpty()); // No stale releases before the next mount.
    TinyUSBDevice.connected = true; usb_hid->writable = true; hid.loop(prefs);
    assert(hid.IsQueueEmpty());
    puts("PASS HID release-all, consumer cancellation, stalled endpoint recovery and disconnect");
}
'''


def display_check():
    display = display_source()
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Devices/Display/DisplayDevice.h"), flags=re.MULTILINE)
    loop = display[display.index('void DisplayDevice::loop'):display.index('    // Handle the button gestures')] + '\n}\n'
    methods = '\n'.join(function(display, sig) for sig in [
        'void DisplayDevice::displayPng', 'void DisplayDevice::requestPng',
        'void DisplayDevice::wakeForActivity', 'void DisplayDevice::setScreensaverEnabled',
        'bool DisplayDevice::isScreensaverEnabled'])
    binary_read = function(source('src/Devices/Storage/StorageDevice.cpp'), 'uint8_t* StorageDevice::readFileAsBinary')
    return r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
struct Preferences {};
struct RedLilyGOCapability {};
struct String : std::string {
    using std::string::string;
    void toLowerCase() { for (char& c : *this) c = std::tolower(static_cast<unsigned char>(c)); }
    bool endsWith(const std::string& end) { return size() >= end.size() && compare(size() - end.size(), end.size(), end) == 0; }
};
static uint32_t clockMs = 100;
uint32_t millis() { return clockMs; }
namespace Debug { struct { void info(const char*, const std::string&) {} } Log; }
#define LOG_TFT "TFT"
#define LOG_MMC "MMC"
static constexpr size_t MAX_BINARY_READ_BYTES = 1024 * 1024;
static std::recursive_mutex storageMutex;
static std::vector<uint8_t> fileBytes{1, 2, 3, 4};
static size_t declaredSize = 4;
static bool fileExists = true, shortRead = false;
static std::string openedPath;
struct { size_t heap = 100000; size_t getFreeHeap() { return heap; } } ESP;
struct File {
    std::vector<uint8_t> snapshot;
    explicit operator bool() { return fileExists; }
    size_t size() { return declaredSize; }
    size_t read(uint8_t* out, size_t size) {
        const auto count = std::min(size, snapshot.size());
        std::copy_n(snapshot.begin(), count, out);
        return shortRead && count ? count - 1 : count;
    }
    void close() { fileBytes.assign(999, 0xff); } // Replacement after this open was read.
};
struct {
    File open(const char* name, const char*) { openedPath = name; return {fileBytes}; }
} FILE_INTERFACE;
struct StorageDevice {
    bool running = true;
    bool isRunning() { return running; }
    bool isSafeUserPath(const std::string& path) { return !path.empty() && path[0] == '/' && path.find("..") == std::string::npos; }
    uint8_t* readFileAsBinary(const std::string&, size_t&);
};
namespace Devices { StorageDevice Storage; }
''' + binary_read + header + r'''
static const auto displayTask = std::this_thread::get_id();
static int drawCalls = 0, powerCalls = 0;
struct {
    void startWrite() { assert(std::this_thread::get_id() == displayTask); }
    void endWrite() {}
    void drawPng(uint8_t* data, size_t size) {
        assert(size == 4 && "decoder length differs from its actual allocation");
        assert(data[0] == 1 && data[3] == 4); ++drawCalls;
    }
} lcd;
static int xpos = 0, ypos = 0;
static std::mutex imageRequestMutex;
static std::string requestedImage;
enum { MODE_DASHBOARD, MODE_SCREENSAVER };
static int currentMode = MODE_DASHBOARD, bootAnimStep = 0;
static bool isScreenOff = true, bootScreenCleared = true, screensaverInitialized = true;
static unsigned long lastActionTime = 0, lastDashboardUpdate = 99;
DisplayDevice::DisplayDevice() {}
void DisplayDevice::begin(Preferences&) {}
void DisplayDevice::powerOn() { assert(std::this_thread::get_id() == displayTask); ++powerCalls; }
''' + methods + loop + r'''
int main() {
    DisplayDevice display;
    Preferences prefs;
    std::thread([&] {
        display.wakeForActivity();
        display.requestPng("/Images/old.png");
        display.requestPng(std::string("/Images/current.png"));
        display.setScreensaverEnabled(false);
    }).join();
    assert(powerCalls == 0 && drawCalls == 0 && isScreenOff);
    display.loop(prefs);
    assert(powerCalls == 1 && drawCalls == 1 && !isScreenOff);
    assert(openedPath == "/Images/current.png" && !display.isScreensaverEnabled());
    display.loop(prefs); assert(drawCalls == 1 && powerCalls == 1);
    isScreenOff = true; display.setScreensaverEnabled(false); display.loop(prefs);
    assert(isScreenOff && powerCalls == 1);
    isScreenOff = false; currentMode = MODE_SCREENSAVER; display.loop(prefs);
    assert(currentMode == MODE_DASHBOARD && powerCalls == 2);

    size_t size = 77;
    auto read = [&] { size = 77; return Devices::Storage.readFileAsBinary("/Images/test.png", size); };
    fileBytes = {1, 2, 3, 4}; declaredSize = 4;
    uint8_t* data = read(); assert(data && size == 4 && data[4] == 0); free(data);
    shortRead = true; assert(read() == nullptr && size == 0); shortRead = false;
    declaredSize = 0; assert(read() == nullptr && size == 0);
    declaredSize = MAX_BINARY_READ_BYTES + 1; assert(read() == nullptr && size == 0);
    declaredSize = 4; ESP.heap = 8; assert(read() == nullptr && size == 0); ESP.heap = 100000;
    fileExists = false; assert(read() == nullptr && size == 0); fileExists = true;
    Devices::Storage.running = false; assert(read() == nullptr && size == 0);
    puts("PASS display task ownership, queued image lifetime, exact PNG length and bounded binary reads");
}
'''


def pcap_check():
    pcap = re.sub(r'^#include .*$', '', source("src/Utilities/PcapBuffer.cpp"), flags=re.MULTILINE)
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Utilities/PcapBuffer.h"), flags=re.MULTILINE)
    header = re.sub(r'#ifdef NO_SD.*?#endif\s*', '', header, flags=re.DOTALL)
    return r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <string>
#include <vector>
struct FakeFileState {
    bool opened = false;
    int writes = 0, failAfter = -1, flushes = 0;
    std::vector<uint8_t> bytes;
};
namespace fs {
class File {
public:
    FakeFileState* state = nullptr;
    File() = default;
    explicit File(FakeFileState* value) : state(value) {}
    explicit operator bool() const { return state && state->opened; }
    size_t write(const uint8_t* data, size_t size) {
        assert(state);
        ++state->writes;
        if (state->failAfter >= 0 && state->writes > state->failAfter) return size == 0 ? 0 : size - 1;
        state->bytes.insert(state->bytes.end(), data, data + size);
        return size;
    }
    void flush() { ++state->flushes; }
    void close() { if (state) state->opened = false; }
};
class FS {
public:
    FakeFileState state;
    std::vector<std::string> directories;
    std::vector<std::string> paths;
    bool exists(const char*) { return false; }
    bool mkdir(const char* path) { directories.emplace_back(path); return true; }
    File open(const char* path, const char*) { paths.emplace_back(path); state.opened = true; return File(&state); }
};
}
struct FakeMutex { bool locked = false; };
using SemaphoreHandle_t = FakeMutex*;
static constexpr int pdTRUE = 1;
static constexpr int pdMS_TO_TICKS(int value) { return value; }
static constexpr int portMAX_DELAY = 1;
SemaphoreHandle_t xSemaphoreCreateMutex() { return new FakeMutex; }
int xSemaphoreTake(SemaphoreHandle_t mutex, int) { assert(mutex && !mutex->locked); mutex->locked = true; return pdTRUE; }
void xSemaphoreGive(SemaphoreHandle_t mutex) { assert(mutex && mutex->locked); mutex->locked = false; }
void vSemaphoreDelete(SemaphoreHandle_t mutex) { assert(mutex && !mutex->locked); delete mutex; }
static unsigned long nowUs = 0;
unsigned long micros() { return nowUs; }
''' + header + pcap + r'''
int main() {
    fs::FS storage;
    {
        PcapBuffer capture("/Captures/Other/usbncm", &storage, 1);
        assert(capture.isReady());
        assert(storage.directories == std::vector<std::string>({"/Captures", "/Captures/Other"}));
        assert(storage.paths == std::vector<std::string>({"/Captures/Other/usbncm_0.pcap"}));
        assert(storage.state.bytes.size() == 24 && storage.state.writes == 7);
        const uint8_t first[] = {1, 2, 3};
        const uint8_t second[] = {4, 5};
        nowUs = 1'500'123; assert(capture.append(const_cast<uint8_t*>(first), sizeof(first)));
        nowUs = 2'000'004; assert(capture.append(const_cast<uint8_t*>(second), sizeof(second)));
        capture.flushToDisk();
        assert(storage.state.writes == 17 && storage.state.bytes.size() == 24 + 19 + 18);
        assert(storage.state.bytes[0] == 0xd4 && storage.state.bytes[32] == 3);
        assert(storage.state.bytes[40] == 1 && storage.state.bytes[59] == 4);
        for (int i = 0; i < 15; ++i) assert(capture.append(const_cast<uint8_t*>(first), sizeof(first)));
        assert(!capture.append(const_cast<uint8_t*>(first), sizeof(first)) && "PCAP queue must stay bounded");
    }
    fs::FS failingStorage;
    failingStorage.state.failAfter = 7; // Global header writes, then first record timestamp fails.
    {
        PcapBuffer capture("/Captures/Other/failing", &failingStorage);
        assert(capture.isReady());
        uint8_t packet[] = {9, 8, 7};
        assert(capture.append(packet, sizeof(packet)));
        assert(capture.append(packet, sizeof(packet)));
        capture.flushToDisk();
        assert(!capture.isReady());
        assert(failingStorage.state.writes == 8 && "short write must stop later PCAP records");
        assert(!capture.append(packet, sizeof(packet)));
    }
    puts("PASS PCAP parent directory, exact records, bounded queue and short-write cleanup");
}
'''


def format_check(display=True):
    code = re.sub(r'^#include .*$', '', source("src/Utilities/Format.cpp"), flags=re.MULTILINE)
    return ("#define DISPLAY_WIDTH 160\n" if display else "") + r'''
#include <cassert>
#include <cstdio>
#include <string>
#define ARDUINO_ARCH_ESP32 1
struct Preferences {};
static unsigned delays = 0;
void delay(unsigned time) { delays += time; }
struct { unsigned restarts = 0; void restart() { ++restarts; } } ESP;
namespace Debug { struct {
    void warning(const char*, const std::string&) {}
} Log; }
namespace Devices {
struct {
    bool pressed = false, longPress = false;
    void loop(Preferences&) {}
    bool hasButtonBeenPressed() { return pressed; }
    bool wasLastPressLong() { return longPress; }
    void resetButtonPressedState() { pressed = false; }
} Button;
struct {
    int getTextSize() { return 10; }
    void clearScreen() {}
    void display(int, int, const std::string&) {}
} Display;
struct {
    bool mounted = false;
    unsigned formats = 0;
    void begin(Preferences&, bool format) { assert(format); ++formats; }
    bool isRunning() { return mounted; }
} Storage;
struct { unsigned stops = 0; void end() { ++stops; } } Wifi;
namespace USB { struct { void end() {} } Core; }
}
namespace Attacks {
struct { void stop() {} } Ducky;
struct { void stopAll() {} } Marauder;
}
''' + code + r'''
int main() {
    Preferences prefs;
    AskFormatSD(prefs);
    assert(ESP.restarts == 0 && Devices::Storage.formats == 0 && Devices::Wifi.stops == 0);
    Devices::Button.pressed = true;
    AskFormatSD(prefs); // A short press never authorizes formatting.
    assert(ESP.restarts == 0 && Devices::Storage.formats == 0);
#ifdef DISPLAY_WIDTH
    for (bool succeeds : {false, true}) {
        Devices::Button.pressed = Devices::Button.longPress = true;
        Devices::Storage.mounted = succeeds;
        AskFormatSD(prefs);
    }
    assert(Devices::Storage.formats == 2 && Devices::Wifi.stops == 2 && ESP.restarts == 2);
#endif
    puts("PASS missing SD preserves recovery access; formatting requires long press");
}
'''


def agent_check():
    header = re.sub(r'^#(?:include|pragma) .*$', '', source("src/Attacks/Agent/Agent.h"), flags=re.MULTILINE)
    code = re.sub(r'^#include .*$', '', source("src/Attacks/Agent/Agent.cpp"), flags=re.MULTILINE)
    return r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#define CATEGORY_USB 0
static uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
struct Preferences {
    bool getBool(const char*, bool value) { return value; }
    int16_t getShort(const char*, int16_t) { return 7; }
    int getInt(const char*, int value) { return value; } // NVS type mismatch.
};
struct RedLilyGOCapability {
    enum class SettingType { Bool, Int16 };
    template<class... T> void registerUserConfigurableSetting(T...) {}
};
enum HostCommand { AgentStatus, ExecuteResult, RequestAgentStatus, Execute };
namespace Debug { struct {
    void info(const char*, const std::string&) {}
    void warning(const char*, const std::string&) {}
    void error(const char*, const std::string&) {}
} Log; }
namespace Devices::USB { struct {
    bool ready = true;
    unsigned polls = 0;
    std::map<HostCommand, std::function<void(uint8_t*, size_t)>> callbacks;
    void setCallback(HostCommand tag, std::function<void(uint8_t*, size_t)> callback) { callbacks[tag] = callback; }
    bool isReady() { return ready; }
    void writeBinary(HostCommand tag, const uint8_t*, size_t) { if (tag == RequestAgentStatus) ++polls; }
    void respond() {
        std::string status = "test-host\n0.1.0\n2";
        callbacks[AgentStatus](reinterpret_cast<uint8_t*>(status.data()), status.size());
    }
} CDC; }
''' + header + code + r'''
int main() {
    Preferences prefs;
    auto& agent = Attacks::Agent;
    auto& cdc = Devices::USB::CDC;
    agent.begin(prefs);
    assert(agentPollingIntervalInSec == 7 && "saved Int16 polling interval ignored");
    for (unsigned count = 1; count <= 20; ++count) {
        clockMs += 7000;
        agent.loop(prefs);
        assert(cdc.polls == count && "connected Agent was not polled");
        cdc.respond();
        assert(agent.isAgentConnected() && agent.protocolCompatible());
    }
    clockMs += 30000;
    agent.loop(prefs); // Genuine missing replies still expire the session.
    assert(!agent.isAgentConnected() && !agent.protocolCompatible());
    cdc.respond(); assert(agent.isAgentConnected());
    cdc.ready = false; agent.loop(prefs); assert(!agent.isAgentConnected());
    puts("PASS Agent typed settings, live heartbeat polling and disconnect expiry");
}
'''


def compact_log_checks():
    marauder = source("src/Attacks/Marauder/Marauder.cpp")
    display = display_source()
    web = source("src/Comms/Web/WebServer.cpp") + "\n" + source("src/Comms/Web/WebRoutes.cpp")
    logging = source("src/Diagnostics/Logging.cpp")
    storage = source("src/Devices/Storage/StorageDevice.cpp")
    buffer = source("lib/ESP32Marauder/esp32_marauder/Buffer.cpp")
    microphone = source("src/Devices/Microphone/NoMicrophoneDevice.cpp")
    wifi = source("src/Devices/WiFi/WiFiDevice.cpp")
    ir = source("src/Devices/IR/NoIRDevice.cpp")
    ble = source("lib/ESP32Marauder/esp32_marauder/WiFiScan.cpp")
    assert 'if (count <= 0) return 0;' in ble
    run_setup = ble[ble.index('bool WiFiScan::RunSetup()'):ble.index('this->initWiFi(1);', ble.index('bool WiFiScan::RunSetup()'))]
    assert 'NimBLEDevice::init("");' not in run_setup
    assert 'this->shutdownBLE();' not in run_setup
    portal = source("lib/ESP32Marauder/esp32_marauder/EvilPortal.cpp")

    for command in (
        'verb == "attack"', 'verb.rfind("scan", 0) == 0',
        'verb.rfind("sniff", 0) == 0', 'verb == "blespam"',
        'verb == "btwardrive"', 'verb == "wardrive"',
        'verb == "sigmon"', 'verb == "channelhop"',
        'verb == "evilportal"',
    ):
        assert command in marauder
    assert 'const size_t stopHint = logMessage.find(". Stop with ");' in marauder
    command_loop = marauder[marauder.index('cli_obj.runCommand(command.c_str());'):marauder.index('/**\n * @brief Queue one Marauder command')]
    assert 'else if (!isRadioCommand(command))' in command_loop
    assert 'marauderRadioActive = false;' in command_loop
    assert 'marauderRadioActive || wifi_scan_obj.scanning() || evil_portal_obj.isActive()' in marauder
    apa = source("src/Devices/LED/APA102/LEDDevice.cpp")
    assert 'if (on && Attacks::Marauder.isActive() && hue != 120)' in apa
    assert 'hue = 45; // MRD: orange' in apa
    assert 'if (isMarauderRunning)' in display
    assert 'hue = 30; // Marauder activity: orange.' in display
    assert 'pBLEScan->setAdvertisedDeviceCallbacks(nullptr);' in ble
    assert 'static bluetoothScanAllCallback scanAllCallback;' in ble
    assert 'static bluetoothScanSkimmersCallback scanSkimmersCallback;' in ble
    assert 'new bluetoothScan' not in ble
    assert 'delete scanCallback;' not in ble
    shutdown = ble[ble.index('bool WiFiScan::shutdownBLE()'):ble.index('// Function to stop all wifi scans')]
    assert 'NimBLEDevice::deinit()' not in shutdown
    stop_scan = ble[ble.index('void WiFiScan::StopScan'):ble.index('String WiFiScan::getStaMAC')]
    assert 'currentScanMode = WIFI_SCAN_OFF;' in stop_scan
    assert 'if (message == "Starting Marauder on user request.") return "MRD start";' in logging
    assert 'Debug::Log.info(LOG_WEB, "Download requested from web UI")' not in web
    assert 'Debug::Log.info(LOG_WEB, "Sending file from web UI")' not in web
    assert 'std::string("File missing ") + filename.c_str()' in web
    assert 'std::string("404 ") + url.c_str()' in web
    assert 'url != "/chat"' in web
    format_route = web[web.index('if (url == "/system" && request->hasParam("cmd"))'):web.index('else if (url == "/settings"')]
    assert format_route.index('Attacks::Ducky.stop();') < format_route.index('Devices::Storage.begin(*preferences, true);')
    assert format_route.index('Attacks::Marauder.stopAll();') < format_route.index('Devices::Storage.begin(*preferences, true);')
    assert format_route.index('Devices::USB::Core.end();') < format_route.index('Devices::Storage.begin(*preferences, true);')
    assert 'return static_cast<int32_t>(now - stopFeedbackUntil) < 0;' in display
    assert 'DisplayInternal::stopFeedbackActive(millis())' in display
    assert 'selectedPayloadIdx >= payloadList.size()' in display
    assert 'duckyStatus == "Complete"' not in display
    assert 'if (message == "Marauder command stopped") return "Stopped";' in logging
    assert 'if (message == "Marauder command finished") return "Finished";' in logging
    assert 'std::string("Binary skip ") + filename' in storage
    assert 'if (filename.empty()) return {};' in storage[storage.index('std::string StorageDevice::resolveImagePath'):]
    assert 'if (bufA == nullptr || bufB == nullptr)' in buffer
    assert 'Serial.availableForWrite() < static_cast<int>(frameSize)' in buffer
    assert 'preserving frame boundaries' in buffer
    # The maintained T-Dongle profile has no microphone or infrared hardware.
    profile = source("platformio.ini").split('[env:LILYGO-T-Dongle-S3]', 1)[1]
    assert '-D NO_MIC' in profile and '-D NO_IR' in profile
    assert '#ifdef NO_MIC' in microphone
    assert 'return false;' in function(microphone, 'bool MicrophoneDevice::isConfigured() const')
    assert wifi.index('if (Attacks::Marauder.isEvilPortalActive())') < wifi.index('if (Attacks::Marauder.isActive())')
    assert '#ifdef NO_IR' in ir
    assert not function(ir, 'void IRDevice::begin(Preferences &prefs)').split('{', 1)[1].rsplit('}', 1)[0].strip()
    assert 'std::to_string(fileSize)' in storage
    assert 'std::to_string(freeHeap)' in storage
    start_portal = function(marauder, 'bool ESP32Marauder::startEvilPortal')
    assert start_portal.index('evilPortalActive.store(true') < start_portal.index('nextMarauderCommandToRun = "evilportal -c start"')
    assert start_portal.index('evilPortalActive.store(true') > start_portal.index('setPortalSsid')
    assert 'Devices::Storage.openFile(path.c_str(), "r")' in portal
    assert 'readFileAsBinary(path.c_str())' not in portal
    assert 'msg == "Marauder will start when requested from web UI."' in logging
    assert 'msg == "Starting Marauder on user request."' in logging
    assert 'msg == "Marauder command queued"' in logging
    assert 'msg == "Running Marauder command"' in logging
    print("PASS compact logs, continuous Marauder commands and MRD LED priority")


def main():
    compiler = os.environ.get("CXX") or shutil.which("g++")
    if not compiler and Path("C:/msys64/mingw64/bin/g++.exe").is_file():
        compiler = "C:/msys64/mingw64/bin/g++.exe"
    if not compiler:
        raise SystemExit("g++ required; install it or set CXX to its path")
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="redlilygo-regressions-") as directory:
        compact_log_checks()
        for name, code in [("logging", logging_check()), ("web_security", web_security_check()),
                           ("web_mutation_gate", web_mutation_gate_check()),
                           ("websocket", websocket_check()), ("upload", upload_check()),
                           ("cdc", cdc_check()), ("msc", msc_check()), ("ducky", ducky_check()),
                           ("format_screen", format_check()), ("format_headless", format_check(False)),
                           ("agent", agent_check()), ("hid", hid_check()), ("display", display_check()),
                           ("pcap", pcap_check())]:
            cpp = Path(directory) / (name + ".cpp")
            binary = Path(directory) / (name + ".exe")
            cpp.write_text(code, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-pthread", str(cpp), "-o", str(binary)],
                           check=True, env=env, timeout=60)
            subprocess.run([str(binary)], check=True, env=env, timeout=10)


if __name__ == "__main__":
    main()
