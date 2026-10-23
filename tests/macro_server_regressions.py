"""Check macro API validation and bind policy without installing keyboard hooks."""
import contextlib
from http.client import HTTPConnection
import importlib.util
import json
from pathlib import Path
import sys
import threading
import tempfile
from types import SimpleNamespace
from unittest.mock import Mock, patch

TOOL = Path(__file__).resolve().parents[1] / "tools" / "macro-recorder"
sys.path.insert(0, str(TOOL))
spec = importlib.util.spec_from_file_location("macro_server", TOOL / "server.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


def main():
    with tempfile.TemporaryDirectory() as folder:
        recorder = server.Recorder(str(Path(folder) / "events.jsonl"))
        listener = Mock()
        keyboard = SimpleNamespace(Listener=Mock(return_value=listener))
        with patch.dict(sys.modules, {"pynput": SimpleNamespace(keyboard=keyboard)}):
            listener.start.side_effect = OSError("test listener unavailable")
            try:
                recorder.start()
            except OSError:
                pass
            else:
                raise AssertionError("listener failure must propagate")
            assert not recorder.recording and recorder._listener is None
            listener.stop.assert_called_once()
            listener.start.side_effect = None
            assert recorder.start()["ok"]
            old_press = keyboard.Listener.call_args.kwargs["on_press"]
            assert not recorder.start()["ok"]
            recorder._on_press(SimpleNamespace(char="x"))
            assert recorder.snapshot()["total"] == 1
            assert recorder.stop()["ok"]
            recorder._record(SimpleNamespace(char="y"), "down")
            assert recorder.snapshot()["total"] == 1, "late callback must not record after stop"
            assert recorder.start()["ok"]
            old_press(SimpleNamespace(char="stale"))
            assert recorder.snapshot()["total"] == 1, "old session callback must not leak into a restarted recording"
            current_press = keyboard.Listener.call_args.kwargs["on_press"]
            workers = [threading.Thread(target=lambda: [current_press(SimpleNamespace(char="x")) for _ in range(20)]) for _ in range(4)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join(timeout=3)
                assert not worker.is_alive()
            assert recorder.stop()["ok"]
            events = recorder.all_events()
            assert len(events) == 81 and [event["seq"] for event in events] == list(range(1, 82))
            disk = [json.loads(line) for line in Path(recorder.log_path).read_text(encoding="utf-8").splitlines()]
            assert len([event for event in disk if event["type"] != "meta"]) == 81
            assert recorder.start()["ok"]
            with patch("recorder.time.time", side_effect=[1700000000.0, 1699999999.0]), patch("recorder.time.monotonic", side_effect=[1.0, 1.5]):
                recorder._on_press(SimpleNamespace(char="a"))
                recorder._on_press(SimpleNamespace(char="b"))
            assert recorder.all_events()[-1]["dt"] == 0.5, "wall-clock adjustments must not reverse macro timing"
            recorder.stop()

        bounded = server.Recorder(str(Path(folder) / "bounded.jsonl"))
        bounded.recording = True
        with patch.object(bounded, "_write"):
            for _ in range(5002):
                bounded._record(SimpleNamespace(char="x"), "down")
        snapshot = bounded.snapshot()
        assert len(snapshot["events"]) == 5000 and snapshot["total"] == 5002
        assert snapshot["events"][0]["seq"] == 3 and snapshot["next"] == 5002
        assert [event["seq"] for event in bounded.snapshot(5000)["events"]] == [5001, 5002]
        assert bounded.clear()["cleared"] == 5000
        assert bounded.snapshot() == {"events": [], "next": 5002, "total": 5002}
        bounded._record(SimpleNamespace(char="y"), "down")
        assert bounded.snapshot()["events"][0]["seq"] == 5003

    # An invalid bind must fail before opening a socket.
    for host, token, allowed in [
        ("127.0.0.1", "", True), ("localhost", "", True),
        ("::1", "", True), ("0.0.0.0", "", False),
        ("192.168.1.2", "", False), ("other-host", "", False),
        ("0.0.0.0", "test-token", True),
    ]:
        with patch.object(server, "HOST", host), patch.object(server, "TOKEN", token):
            with patch.object(server, "ThreadingHTTPServer", side_effect=RuntimeError("socket boundary")) as bind:
                try:
                    server.main()
                except (SystemExit, RuntimeError):
                    # The patched bind raises RuntimeError, and a rejected bind
                    # may exit; both are the expected outcomes under test.
                    pass
                assert bind.called == allowed, (host, allowed)

    with server.ThreadingHTTPServer(("127.0.0.1", 0), server.Handler) as httpd:
        worker = threading.Thread(target=httpd.serve_forever, daemon=True)
        worker.start()
        try:
            # Localhost binding does not stop a malicious browser tab submitting POSTs.
            with patch.object(server, "TOKEN", ""), patch.object(server.recorder, "clear", return_value={"ok": True}) as clear:
                host = "%s:%s" % httpd.server_address
                for headers, expected in [
                    ({"Origin": "https://foreign.example"}, 403),
                    ({"Origin": "null"}, 403),
                    ({"Sec-Fetch-Site": "cross-site"}, 403),
                    ({"Sec-Fetch-Site": "same-site"}, 403),
                    ({"Origin": "http://" + host}, 200),
                    ({"Sec-Fetch-Site": "same-origin"}, 200),
                    ({}, 200),
                ]:
                    with contextlib.closing(HTTPConnection(*httpd.server_address, timeout=3)) as conn:
                        conn.request("POST", "/api/clear", headers=headers)
                        response = conn.getresponse()
                        response.read()
                        assert response.status == expected, (headers, response.status)
                assert clear.call_count == 3
                for method, path in (("GET", "/api/status"), ("POST", "/api/clear")):
                    with contextlib.closing(HTTPConnection(*httpd.server_address, timeout=3)) as conn:
                        conn.request(method, path, headers={"Host": "rebound.example"})
                        response = conn.getresponse()
                        response.read()
                        assert response.status == 401, "hostile DNS-rebinding Host must not bypass local-only access"
                assert clear.call_count == 3
            with patch.object(server, "TOKEN", "test-token"):
                for query, authorized, expected in [
                    ("", False, 401), ("", True, 200),
                    ("?since=0", True, 200), ("?since=2", True, 200),
                    ("?since=bad", True, 400), ("?since=-1", True, 400),
                    ("?since=1.5", True, 400),
                ]:
                    headers = {"Authorization": "Bearer test-token"} if authorized else {}
                    with contextlib.closing(HTTPConnection(*httpd.server_address, timeout=3)) as conn:
                        conn.request("GET", "/api/events" + query, headers=headers)
                        response = conn.getresponse()
                        body = json.loads(response.read())
                        assert response.status == expected, (query, response.status)
                        assert ("error" in body) == (expected != 200)
                with patch.object(server.recorder, "start", side_effect=OSError("test failure")):
                    with contextlib.closing(HTTPConnection(*httpd.server_address, timeout=3)) as conn:
                        conn.request("POST", "/api/start", headers={"Authorization": "Bearer test-token"})
                        response = conn.getresponse()
                        assert response.status == 503
                        assert json.loads(response.read()) == {"error": "recording unavailable"}
        finally:
            httpd.shutdown()
            worker.join(timeout=3)
    print("PASS macro bind policy, authentication, cursor validation and recorder failure recovery")


if __name__ == "__main__":
    main()
