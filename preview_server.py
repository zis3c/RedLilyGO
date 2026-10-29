import http.server
import json
import socketserver
import os
import urllib.parse
import urllib.error
import urllib.request
import threading

PORT = int(os.environ.get("REDLILYGO_PREVIEW_PORT", "8080"))
DIRECTORY_CONTENT = "ui/content"
DIRECTORY_VNC = "ui/vnc"
DEVICE_BACKEND = os.environ.get("REDLILYGO_DEVICE_URL", "").rstrip("/")
REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_LOCK = threading.Lock()
MAX_REQUEST_BYTES = 4 * 1024 * 1024 + 64 * 1024
DEVICE_ROUTES = (
    '/data.json', '/files', '/health', '/setup', '/setup/status', '/csrf-token', '/wifi/', '/marauder',
    '/deauth', '/channelhop', '/evilportal', '/capture', '/download',
    '/uploadFile', '/delete', '/showimage', '/runfile', '/rawinput',
    '/runagentcmd', '/systemlog', '/clearlogs', '/set', '/system',
    '/telemetry', '/runtime-logs', '/moveFile', '/settings',
)

# State-changing routes the firmware requires as POST. The preview server serves
# them locally with a canned response when no device backend is configured.
MUTATION_ROUTES = (
    '/rawinput', '/runagentcmd', '/clearlogs', '/set', '/system', '/channelhop',
    '/runfile', '/delete', '/moveFile', '/settings', '/evilportal', '/capture',
    '/mic', '/showimage',
)

PREVIEW_RESPONSES = {
    '/setup/status': {'required': False},
    '/data.json': {
        'sdCardPercentFull': 12,
        'uptime': 'preview mode',
        'temperatureC': 42.5,
        'status': 'Ready',
        'errorCount': 0,
        'USBmode': 'Serial',
        'wifiSsid': 'RedLilyGO',
        'wifiPasswordConfigured': True,
        'usbDeviceType': 1,
        'usbNcmCapture': False,
        'freeHeap': 210000,
        'heapSize': 327680,
        'heapUsagePc': 36,
        'agentConnected': False,
        'machineName': 'Preview Host',
        'agentVersion': 'preview',
        'agentProtocolCompatible': False,
        'stealth': False,
        'autorun': True,
        'powersave': False,
        'version': 'preview',
        'channelHopDelay': 1,
        'deauthCount': 0,
        'numCores': 2,
        'chipModel': 'ESP32-S3 preview',
        'storage': {
            'available': True,
            'usedBytes': 123456,
            'totalBytes': 1048576,
            'freeBytes': 925120,
            'usedPercentage': 12,
        },
        'fileListing': ['/Portal/index.html', '/Scripts/demo.ds', '/Captures/system.log', '/Captures/demo.pcap'],
        'fileDetails': [
            {'path': '/Portal/index.html', 'name': 'index.html', 'size': 4820, 'type': 'html', 'directory': '/Portal'},
            {'path': '/Scripts/demo.ds', 'name': 'demo.ds', 'size': 128, 'type': 'ducky', 'directory': '/Scripts'},
            {'path': '/Captures/system.log', 'name': 'system.log', 'size': 2048, 'type': 'capture', 'directory': '/Captures'},
            {'path': '/Captures/demo.pcap', 'name': 'demo.pcap', 'size': 4096, 'type': 'capture', 'directory': '/Captures'},
        ],
        'systemLog': {'path': '/Captures/system.log', 'available': True, 'size': 2048, 'status': 'active'},
        'logMessages': ['Preview server active'],
        'capabilities': ['SD', 'WIFI', 'TFT', 'BUTTON', 'LED', 'MARAUDER'],
        'settingCategories': [],
    },
    '/health': {
        'overall': 'degraded',
        'version': 'preview',
        'uptime': 'preview mode',
        'freeHeap': 210000,
        'storage': 'ready',
        'wifi': 'ready',
        'usb': 'Serial',
        'agentConnected': False,
        'agentVersion': 'preview',
        'agentProtocolCompatible': False,
        'resetReason': 'preview',
    },
    '/wifi/scan': [],
    '/csrf-token': {'token': 'preview-csrf-token'},
    '/evilportal': {
        'active': False,
        'selectedPage': '/Portal/index.html',
        'submissions': 0,
        'last': None,
        'history': [],
    },
    '/evilportal/files': [
        {'path': '/Portal/index.html', 'name': 'index.html', 'size': 4820, 'form': True},
        {'path': '/Portal/company-test.html', 'name': 'company-test.html', 'size': 5310, 'form': True},
    ],
    '/systemlog': '[0 ms][INFO][PREVIEW] Local preview server active\n[1200 ms][INFO][WEB] Dashboard ready\n',
    '/deauth': {'count': 0, 'events': []},
}

class CustomHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        parsed_path = parsed.path
        if parsed_path in ('/', '/index.html', '/preview/version', '/assets/css/styles.min.css', '/assets/css/redlilygo.css', '/assets/js/ui.js'):
            try:
                import build_ui
                with BUILD_LOCK:
                    build_ui.build()
            except Exception as e:
                print(f"[preview_server] build_ui error: {e}")
                self.send_error(503, 'UI build failed')
                return
        if parsed_path == '/preview/version':
            assets = ('index.html', 'assets/css/redlilygo.css', 'assets/js/ui.js')
            mtime = max((os.path.getmtime(os.path.join(REPO_ROOT, DIRECTORY_CONTENT, asset))
                         for asset in assets if os.path.exists(os.path.join(REPO_ROOT, DIRECTORY_CONTENT, asset))), default=0)
            body = json.dumps({'version': mtime}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed_path == '/assets/css/styles.min.css':
            styles_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), DIRECTORY_CONTENT, "assets", "css", "styles.min.css")
            redlily_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), DIRECTORY_CONTENT, "assets", "css", "redlilygo.css")
            body = b""
            if os.path.exists(styles_path):
                with open(styles_path, 'rb') as f:
                    body += f.read()
            body += b"\n"
            if os.path.exists(redlily_path):
                with open(redlily_path, 'rb') as f:
                    body += f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'text/css; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed_path == '/assets/css/redlilygo.css':
            redlily_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), DIRECTORY_CONTENT, "assets", "css", "redlilygo.css")
            body = b""
            if os.path.exists(redlily_path):
                with open(redlily_path, 'rb') as f:
                    body += f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'text/css; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        query = urllib.parse.parse_qs(parsed.query)
        if DEVICE_BACKEND and self.should_proxy(parsed_path):
            self.proxy_to_device('GET')
            return
        if parsed_path == '/download':
            filename = query.get('filename', [''])[0]
            if filename.endswith('.log'):
                body = b"[0 ms][INFO][PREVIEW] Local preview server active\n[1200 ms][INFO][WEB] Dashboard ready\n"
                content_type = 'text/plain'
            elif filename.endswith('.ds'):
                body = b"REM RedLilyGO Preview Script\nDELAY 500\nSTRING Hello from preview mode!\nENTER\n"
                content_type = 'text/plain'
            else:
                body = b"Sample preview file content"
                content_type = 'text/plain'
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed_path in ('/rawinput', '/runagentcmd'):
            body = json.dumps({'ok': True, 'output': 'Preview mode: simulated output'}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed_path == '/evilportal':
            action = query.get('action', [''])[0]
            payload = dict(PREVIEW_RESPONSES['/evilportal'])
            if action == 'start':
                payload['active'] = True
                payload['selectedPage'] = query.get('file', ['/Portal/index.html'])[0]
            elif action == 'stop':
                payload['active'] = False
            elif action == 'clear':
                payload['submissions'] = 0
                payload['last'] = None
                payload['history'] = []
            body = json.dumps(payload).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed_path == '/capture':
            action = query.get('action', [''])[0]
            source = query.get('source', ['wifi'])[0]
            if action not in ('start', 'stop') or source not in ('wifi', 'usb'):
                payload = {'error': 'invalid_capture_request'}
                status = 400
            else:
                payload = {'ok': True, 'status': source + '_capture_' + ('started' if action == 'start' else 'stopped')}
                status = 200
            body = json.dumps(payload).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed_path in PREVIEW_RESPONSES:
            value = PREVIEW_RESPONSES[parsed_path]
            is_text = isinstance(value, str)
            payload = value.encode('utf-8') if is_text else json.dumps(value).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain; charset=utf-8' if is_text else 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        super().do_GET()

    def do_POST(self):
        parsed_path = urllib.parse.urlparse(self.path).path
        if DEVICE_BACKEND and self.should_proxy(parsed_path):
            self.proxy_to_device('POST')
            return
        if parsed_path == '/uploadFile':
            if self.read_request_body() is None:
                return
            body = json.dumps({'ok': True, 'path': '/Scripts/demo.ds'}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed_path in MUTATION_ROUTES:
            if self.read_request_body() is None:
                return
            body = json.dumps({'ok': True, 'status': 'preview'}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_POST()

    @staticmethod
    def should_proxy(path):
        return path in DEVICE_ROUTES or any(path.startswith(route) for route in DEVICE_ROUTES if route.endswith('/'))

    def read_request_body(self):
        if self.headers.get('Transfer-Encoding'):
            self.send_error(400, 'Transfer-Encoding unsupported')
            return None
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length < 0:
                raise ValueError
        except ValueError:
            self.send_error(400, 'Invalid Content-Length')
            return None
        if length > MAX_REQUEST_BYTES:
            self.send_error(413, 'Request too large')
            return None
        return self.rfile.read(length)

    def proxy_to_device(self, method):
        origin = self.headers.get('Origin')
        host = self.headers.get('Host', '')
        if (origin and origin not in ('http://' + host, 'https://' + host)) or (
                not origin and self.headers.get('Sec-Fetch-Site', 'none') not in ('none', 'same-origin')):
            self.send_error(403, 'Cross-site proxy request forbidden')
            return
        # Only forward an absolute-path request to the configured backend. Reject
        # anything with a scheme or authority so the target host cannot be
        # redirected by a crafted request line (partial-SSRF guard).
        request_path = self.path
        parsed_target = urllib.parse.urlsplit(request_path)
        if not request_path.startswith('/') or parsed_target.scheme or parsed_target.netloc:
            self.send_error(400, 'Invalid proxy target path')
            return
        target = DEVICE_BACKEND + request_path
        body = None
        if method == 'POST':
            body = self.read_request_body()
            if body is None:
                return
        # The target host is the fixed DEVICE_BACKEND; only the path is taken
        # from the request, and it is validated above to be an absolute path
        # with no scheme or authority. This is a local developer proxy, not a
        # request forwarder for untrusted input.
        # codeql[py/partial-ssrf]
        request = urllib.request.Request(target, data=body, method=method)
        for header in ('Cookie', 'Authorization', 'Content-Type', 'X-CSRF-Token'):
            if self.headers.get(header):
                request.add_header(header, self.headers[header])
        if origin:
            backend = urllib.parse.urlsplit(DEVICE_BACKEND)
            request.add_header('Origin', backend.scheme + '://' + backend.netloc)
        if self.headers.get('Sec-Fetch-Site'):
            request.add_header('Sec-Fetch-Site', self.headers['Sec-Fetch-Site'])
        try:
            with urllib.request.urlopen(request, timeout=15) as response:
                payload = response.read()
                status = response.status
                content_type = response.headers.get('Content-Type', 'application/octet-stream')
                response_headers = response.headers
        except urllib.error.HTTPError as error:
            payload = error.read()
            status = error.code
            content_type = error.headers.get('Content-Type', 'text/plain')
            response_headers = error.headers
        except (urllib.error.URLError, OSError) as error:
            payload = json.dumps({'error': 'device_backend_unavailable', 'detail': str(error)}).encode('utf-8')
            status = 502
            content_type = 'application/json'
            response_headers = {}
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        for header in ('WWW-Authenticate', 'Content-Disposition'):
            if response_headers.get(header):
                self.send_header(header, response_headers[header])
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    @staticmethod
    def safe_path(root, relative_path):
        root = os.path.realpath(root)
        candidate = os.path.realpath(os.path.join(root, urllib.parse.unquote(relative_path).lstrip('/')))
        normalized_root = os.path.normcase(root)
        normalized_candidate = os.path.normcase(candidate)
        try:
            within_root = os.path.commonpath((normalized_root, normalized_candidate)) == normalized_root
        except ValueError:
            within_root = False
        if not within_root:
            return os.path.join(root, '__missing__')
        return candidate

    def translate_path(self, path):
        # Parse the URL path
        parsed_path = urllib.parse.urlparse(path).path

        # If the request is for /vnc/..., serve from ui/vnc
        if parsed_path.startswith('/vnc/'):
            # Remove the /vnc/ prefix and map to ui/vnc directory
            rel_path = parsed_path[5:]
            return self.safe_path(os.path.join(REPO_ROOT, DIRECTORY_VNC), rel_path)

        # Mirror firmware aliases so local preview matches deployed routes.
        # Keep request pathname as /remote/input: the VNC page uses it to
        # activate direct USB HID mode without a hash-only redirect.
        if parsed_path in ('/remote', '/remote/input'):
            return os.path.join(REPO_ROOT, DIRECTORY_VNC, 'index.html')

        if parsed_path == '/remote/':
            return self.safe_path(os.path.join(REPO_ROOT, DIRECTORY_VNC), '__missing__')

        if parsed_path.startswith('/remote/'):
            return self.safe_path(os.path.join(REPO_ROOT, DIRECTORY_VNC), parsed_path[8:])

        # Otherwise serve from ui/content
        rel_path = parsed_path.lstrip('/')
        return self.safe_path(os.path.join(REPO_ROOT, DIRECTORY_CONTENT), rel_path)

    def send_error(self, code, message=None, explain=None):
        if code == 404:
            self.send_response(404)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            # Serve our custom 404 page
            error_page_path = os.path.join(REPO_ROOT, DIRECTORY_CONTENT, "404.html")
            if os.path.exists(error_page_path):
                with open(error_page_path, 'rb') as file:
                    self.wfile.write(file.read())
            else:
                self.wfile.write(b"404 Not Found (Custom 404.html is missing)")
        else:
            super().send_error(code, message, explain)

Handler = CustomHandler

class ThreadingTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True

def main():
    with ThreadingTCPServer(("", PORT), Handler) as httpd:
        print(f"RedLilyGO Advanced Preview Server running at http://localhost:{PORT}")
        print(f" - Serving frontend from: {DIRECTORY_CONTENT}")
        print(f" - Serving VNC from: {DIRECTORY_VNC}")
        print(f" - Custom 404 handler: Active")
        print(f"Press Ctrl+C to stop.")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

if __name__ == '__main__':
    main()
