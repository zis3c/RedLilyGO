"""Dashboard server for the macro/keyboard recorder.

Binds to 127.0.0.1 by default (this machine only). To view from your own
other devices on the LAN, set MACRO_LOGGER_HOST=0.0.0.0 and protect it with
MACRO_LOGGER_TOKEN. Run with:

    python server.py
"""

import csv
import io
import ipaddress
import json
import os
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from recorder import DEFAULT_LOG, Recorder

HOST = os.environ.get("MACRO_LOGGER_HOST", "127.0.0.1")
PORT = int(os.environ.get("MACRO_LOGGER_PORT", "8787"))
TOKEN = os.environ.get("MACRO_LOGGER_TOKEN", "")
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
STATIC_DIR = os.path.join(BASE_DIR, "static")

recorder = Recorder(DEFAULT_LOG)

MIME = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
}


def is_loopback_host(host):
    if host == "localhost":
        return True
    try:
        return ipaddress.ip_address(host).is_loopback
    except ValueError:
        return False


class Handler(BaseHTTPRequestHandler):
    server_version = "MacroRecorder/1.0"

    def _send(self, body, ctype, code=200, attachment=None):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        if attachment:
            self.send_header("Content-Disposition", f'attachment; filename="{attachment}"')
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj, code=200):
        self._send(json.dumps(obj), "application/json; charset=utf-8", code)

    def _authorized(self):
        if not TOKEN:
            try:
                return is_loopback_host(urlparse("//" + self.headers.get("Host", "")).hostname)
            except ValueError:
                return False
        if self.headers.get("Authorization") == "Bearer " + TOKEN:
            return True
        return False

    def _same_origin(self):
        origin = self.headers.get("Origin")
        if origin:
            return origin in ("http://" + self.headers.get("Host", ""), "https://" + self.headers.get("Host", ""))
        return self.headers.get("Sec-Fetch-Site", "none") in ("none", "same-origin")

    def _serve_static(self, name):
        path = os.path.join(STATIC_DIR, name)
        if not os.path.isfile(path):
            self._json({"error": "not found"}, 404)
            return
        ext = os.path.splitext(name)[1]
        with open(path, "rb") as f:
            self._send(f.read(), MIME.get(ext, "application/octet-stream"))

    def _export(self, fmt):
        events = recorder.all_events()
        if fmt == "csv":
            buf = io.StringIO()
            w = csv.writer(buf)
            w.writerow(["seq", "time", "type", "key", "kind", "dt"])
            for e in events:
                w.writerow(
                    [
                        e.get("seq", ""),
                        e.get("time", ""),
                        e.get("type", ""),
                        e.get("key", ""),
                        e.get("kind", ""),
                        e.get("dt", ""),
                    ]
                )
            self._send(buf.getvalue(), "text/csv; charset=utf-8", attachment="keystrokes.csv")
            return
        self._send(
            json.dumps(events, ensure_ascii=False, indent=2),
            "application/json; charset=utf-8",
            attachment="keystrokes.json",
        )

    def do_GET(self):
        parsed = urlparse(self.path)
        path, qs = parsed.path, parse_qs(parsed.query)

        if path.startswith("/api/") and not self._authorized():
            return self._json({"error": "unauthorized"}, 401)
        if path in ("/", "/index.html"):
            return self._serve_static("index.html")
        if path == "/app.js":
            return self._serve_static("app.js")
        if path == "/style.css":
            return self._serve_static("style.css")
        if path == "/api/status":
            return self._json(recorder.status())
        if path == "/api/events":
            try:
                since = int(qs.get("since", ["0"])[0])
                if since < 0:
                    raise ValueError
            except ValueError:
                return self._json({"error": "since must be a non-negative integer"}, 400)
            return self._json(recorder.snapshot(since))
        if path == "/api/export":
            return self._export(qs.get("format", ["json"])[0])
        return self._json({"error": "not found"}, 404)

    def do_POST(self):
        path = urlparse(self.path).path
        if path.startswith("/api/") and not self._same_origin():
            return self._json({"error": "cross-site request forbidden"}, 403)
        if path.startswith("/api/") and not self._authorized():
            return self._json({"error": "unauthorized"}, 401)
        if path == "/api/start":
            try:
                return self._json(recorder.start())
            except (ImportError, OSError, RuntimeError) as exc:
                print(f"[recorder] start failed: {exc}")
                return self._json({"error": "recording unavailable"}, 503)
        if path == "/api/stop":
            return self._json(recorder.stop())
        if path == "/api/clear":
            return self._json(recorder.clear())
        return self._json({"error": "not found"}, 404)

    def log_message(self, *_):
        pass


def lan_ip():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except OSError:
        return HOST


def main():
    if not TOKEN and not is_loopback_host(HOST):
        raise SystemExit("Set MACRO_LOGGER_TOKEN before binding outside localhost.")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print("=" * 62)
    print("  RedLilyGO - Macro / Keyboard Recorder")
    print(f"  Dashboard (this PC): http://127.0.0.1:{PORT}")
    if HOST in ("0.0.0.0", "::"):
        print(f"  Dashboard (LAN)   : http://{lan_ip()}:{PORT}")
    print(f"  Log file          : {recorder.log_path}")
    if TOKEN:
        print("  Access token      : set (required to use the API)")
    else:
        print("  Access token      : none (localhost only is safe)")
    print("-" * 62)
    print("  Transparency: records keystrokes on THIS machine only,")
    print("  while 'Record' is ON. Logs stay local. Stop with Ctrl+C.")
    print("=" * 62)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[server] shutting down")
    finally:
        try:
            recorder.stop()
        finally:
            server.server_close()


if __name__ == "__main__":
    main()
