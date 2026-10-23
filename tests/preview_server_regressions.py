"""Exercise the real preview handler and a local fake device over HTTP."""
import contextlib
from http.client import HTTPConnection
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
from pathlib import Path
import threading
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location("preview_server", ROOT / "preview_server.py")
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


@contextlib.contextmanager
def running(httpd):
    with httpd:
        worker = threading.Thread(target=httpd.serve_forever, daemon=True)
        worker.start()
        try:
            yield httpd
        finally:
            httpd.shutdown()
            worker.join(timeout=3)


def request(httpd, method, path, headers=None, body=None, return_headers=False):
    with contextlib.closing(HTTPConnection(*httpd.server_address, timeout=3)) as conn:
        conn.request(method, path, body=body, headers=headers or {})
        response = conn.getresponse()
        result = response.status, response.read()
        return (*result, dict(response.getheaders())) if return_headers else result


class Device(BaseHTTPRequestHandler):
    calls = []

    def do_GET(self):
        self.calls.append((self.path, dict(self.headers)))
        self.send_response(401 if self.path == "/health" else 200)
        self.send_header("Content-Type", "application/json")
        if self.path == "/health":
            self.send_header("WWW-Authenticate", 'Basic realm="fixture"')
        self.end_headers()
        self.wfile.write(b'{"device":true}')

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        self.calls.append((self.path, dict(self.headers), body))
        self.send_response(200)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def main():
    with patch.object(preview.CustomHandler, "log_message", lambda *_: None):
        with running(preview.ThreadingTCPServer(("127.0.0.1", 0), preview.Handler)) as httpd:
            with patch.object(preview, "DEVICE_BACKEND", ""):
                for path in ("/remote", "/remote/input", "/vnc/index.html"):
                    status, body = request(httpd, "GET", path)
                    assert status == 200 and b"<html" in body.lower(), path
                for path in ("/vnc/%2e%2e/%2e%2e/platformio.ini", "/remote/%2e%2e/%2e%2e/platformio.ini",
                             "/%2e%2e/%2e%2e/platformio.ini", "/vnc/C:%5cWindows%5cwin.ini"):
                    assert request(httpd, "GET", path)[0] == 404, path
                for length, expected in (("-1", 400), ("bad", 400), (str(preview.MAX_REQUEST_BYTES + 1), 413)):
                    assert request(httpd, "POST", "/uploadFile", {"Content-Length": length})[0] == expected
                assert request(httpd, "POST", "/uploadFile", {"Transfer-Encoding": "chunked"})[0] == 400
                assert request(httpd, "POST", "/uploadFile", body=b"fixture")[0] == 200
                import build_ui
                with patch.object(build_ui, "build", side_effect=FileNotFoundError("fixture missing")):
                    assert request(httpd, "GET", "/")[0] == 503
                with patch.object(build_ui, "build") as build:
                    timestamps = {"index.html": 1, "redlilygo.css": 2, "ui.js": 3}
                    with patch.object(preview.os.path, "getmtime", side_effect=lambda p: timestamps[Path(p).name]):
                        assert json.loads(request(httpd, "GET", "/preview/version")[1])["version"] == 3
                        timestamps["ui.js"] = 4
                        assert json.loads(request(httpd, "GET", "/preview/version")[1])["version"] == 4
                        assert json.loads(request(httpd, "GET", "/preview/version")[1])["version"] == 4
                    assert build.call_count == 3, "polling must build changed sources, without rewriting unchanged bundles"

            with running(ThreadingHTTPServer(("127.0.0.1", 0), Device)) as device:
                backend = "http://%s:%s" % device.server_address
                host = "%s:%s" % httpd.server_address
                with patch.object(preview, "DEVICE_BACKEND", backend):
                    for route in ("/telemetry", "/runtime-logs", "/files?directory=%2FScripts"):
                        status, body = request(httpd, "GET", route, {"Authorization": "Basic fixture"})
                        assert status == 200 and json.loads(body) == {"device": True}
                        assert Device.calls[-1][0] == route
                        assert Device.calls[-1][1]["Authorization"] == "Basic fixture"
                    status, _, headers = request(httpd, "GET", "/health", return_headers=True)
                    assert status == 401 and headers["WWW-Authenticate"] == 'Basic realm="fixture"'
                    count = len(Device.calls)
                    for headers in ({"Origin": "http://foreign.example"}, {"Sec-Fetch-Site": "cross-site"}):
                        assert request(httpd, "POST", "/delete?filename=x", headers, b"x")[0] == 403
                        assert request(httpd, "POST", "/settings", headers, b"x")[0] == 403
                    assert len(Device.calls) == count, "cross-site requests must never reach the device"
                    status, body = request(httpd, "POST", "/settings", {
                        "Origin": "http://" + host, "Sec-Fetch-Site": "same-origin",
                        "Content-Type": "application/json",
                    }, b'{"fixture":true}')
                    assert status == 200 and body == b'{"fixture":true}'
                    assert Device.calls[-1][1]["Origin"] == backend
                    assert Device.calls[-1][1]["Sec-Fetch-Site"] == "same-origin"
                # Socket now closed: backend failure must produce an explicit 502.
            with patch.object(preview, "DEVICE_BACKEND", backend):
                status, body = request(httpd, "GET", "/telemetry")
                assert status == 502 and json.loads(body)["error"] == "device_backend_unavailable"
    print("PASS preview aliases, traversal, body limits, build failures, proxy forwarding and cross-site rejection")


if __name__ == "__main__":
    main()
