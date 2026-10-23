"""Local, transparent, self-hosted keyboard/macro recorder.

Records keystrokes from THIS machine only, and only while recording is
explicitly enabled (Start/Stop). Events are appended to a local JSONL file
and kept in a bounded in-memory buffer for the local dashboard.

Design boundaries (intentional):
  * No stealth: recording state is always visible in the console and UI.
  * No persistence or autostart: it only runs when you run it.
  * No remote exfiltration: logs stay on this machine.
  * No global hook while idle: the OS keyboard listener is only active
    while recording is turned on.
"""

import json
import os
import threading
import time
from datetime import datetime
from collections import deque

TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_LOG = os.path.join(TOOL_DIR, "logs", "keystrokes.jsonl")
MAX_BUFFER = 5000


class Recorder:
    def __init__(self, log_path=DEFAULT_LOG):
        self.log_path = log_path
        self.events = deque(maxlen=MAX_BUFFER)
        self.recording = False
        self.started_at = None
        self._lock = threading.Lock()
        self._file_lock = threading.Lock()
        self._listener = None
        self._session = 0
        self._prev_ts = None
        self._seq = 0
        self._total = 0
        os.makedirs(os.path.dirname(log_path) or ".", exist_ok=True)

    def status(self):
        with self._lock:
            return {
                "recording": self.recording,
                "started_at": self.started_at,
                "event_count": len(self.events),
                "total_recorded": self._total,
                "log_path": os.path.abspath(self.log_path),
            }

    def start(self):
        from pynput import keyboard  # lazy import; keep server importable without it

        with self._lock:
            if self.recording:
                return {"ok": False, "reason": "already_recording"}
            self._session += 1
            session = self._session
            listener = keyboard.Listener(
                on_press=lambda key: self._on_press(key, session),
                on_release=lambda key: self._on_release(key, session),
            )
            listener.daemon = True
            try:
                listener.start()
            except Exception:
                listener.stop()
                raise
            self._listener = listener
            self.recording = True
            self.started_at = self._now()
            self._prev_ts = None

        self._write_meta("start")
        print("[recorder] RECORDING STARTED (this device only)")
        return {"ok": True, "recording": True, "started_at": self.started_at}

    def stop(self):
        with self._lock:
            if not self.recording:
                return {"ok": False, "reason": "not_recording"}
            self.recording = False
            listener = self._listener
            self._listener = None

        if listener is not None:
            listener.stop()
        self._write_meta("stop")
        print("[recorder] recording stopped")
        return {"ok": True, "recording": False}

    def clear(self):
        with self._lock:
            n = len(self.events)
            self.events.clear()
        print(f"[recorder] cleared {n} in-memory events (disk log untouched)")
        return {"ok": True, "cleared": n}

    def snapshot(self, since=0):
        """Return events with seq > `since`, plus the latest seq."""
        with self._lock:
            new = [e for e in self.events if e["seq"] > since]
            return {
                "events": new,
                "next": self._seq,
                "total": self._total,
            }

    def all_events(self):
        with self._lock:
            return list(self.events)

    def _on_press(self, key, session=None):
        if self.recording:
            self._record(key, "down", session)

    def _on_release(self, key, session=None):
        if self.recording and self._describe(key)["kind"] == "special":
            self._record(key, "up", session)

    def _describe(self, key):
        if hasattr(key, "char") and key.char is not None:
            return {"kind": "char", "name": key.char}
        name = getattr(key, "name", None) or str(key)
        label = str(name).replace("Key.", "", 1)
        return {"kind": "special", "name": label}

    def _record(self, key, kind, session=None):
        d = self._describe(key)
        with self._lock:
            if not self.recording or (session is not None and session != self._session):
                return
            now = time.time()
            monotonic_now = time.monotonic()
            dt = round(monotonic_now - self._prev_ts, 4) if self._prev_ts is not None else None
            self._prev_ts = monotonic_now
            self._seq += 1
            self._total += 1
            ev = {
                "seq": self._seq,
                "ts": now,
                "time": datetime.fromtimestamp(now).astimezone().isoformat(
                    timespec="milliseconds"
                ),
                "type": kind,
                "key": d["name"],
                "kind": d["kind"],
                "dt": dt,
            }
            self.events.append(ev)
        self._write(ev)

    def _write_meta(self, action):
        self._write({"type": "meta", "action": action, "time": self._now()})

    def _write(self, ev):
        line = json.dumps(ev, ensure_ascii=False)
        with self._file_lock:
            try:
                with open(self.log_path, "a", encoding="utf-8") as f:
                    f.write(line + "\n")
            except OSError as exc:
                print(f"[recorder] WARN could not write log: {exc}")

    @staticmethod
    def _now():
        return datetime.now().astimezone().isoformat(timespec="seconds")
