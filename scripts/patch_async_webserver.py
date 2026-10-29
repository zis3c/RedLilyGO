# ruff: noqa: F821  (Import and env are injected by PlatformIO's SCons build)
"""Harden ESPAsyncWebServer's response writer against short socket writes.

The vendored AsyncAbstractResponse::_ack assumes `client()->write()` always
queues the whole buffer. On the ESP32 the socket queue can fill up first
(especially while other traffic such as websocket frames is in flight), and
the stock code then advances its sent counter while silently dropping the
unsent bytes. For a gzip-compressed asset that produces a body shorter than
the advertised Content-Length, which browsers surface as
"cannot decode raw data" (Firefox NS_ERROR_INVALID_CONTENT_ENCODING).

This keeps the unsent tail so a later ack resumes exactly where it stopped.
If the tail cannot be buffered because the heap is exhausted, the connection
is closed instead of letting std::bad_alloc abort the whole firmware.
"""

import os

Import("env")

ORIGINAL = """    if (headLen) {
      _head = emptyString;
    }

    if (outLen) {
      _writtenLength += request->client()->write((const char*)buf, outLen);
      _in_flight += outLen;
      --_in_flight_credit; // take a credit
    }

    if (_chunked) {
      _sentLength += readLen;
    } else {
      _sentLength += outLen - headLen;
    }
"""

PATCHED = """    size_t written = 0;
    if (outLen) {
      written = request->client()->write((const char*)buf, outLen);
      _writtenLength += written;
      _in_flight += written;
      --_in_flight_credit; // take a credit
      if (written < outLen) {
        // Socket queue filled before every byte was accepted. Keep the unsent
        // body bytes (the head remainder stays in _head) so the next ack
        // resumes exactly where we stopped instead of dropping data.
        try {
          _cache.insert(_cache.begin(), buf + headLen + (written > headLen ? written - headLen : 0), buf + outLen);
        } catch (const std::bad_alloc&) {
          // Heap exhaustion must not abort the device inside the TCP callback.
          // Drop this connection; the client retries the request.
          free(buf);
          request->client()->close(true);
          return 0;
        }
      }
    }

    const size_t headWritten = written < headLen ? written : headLen;
    const size_t bodyWritten = written > headLen ? written - headLen : 0;
    if (headWritten < headLen) {
      _head = _head.substring(headWritten);
    } else {
      _head = emptyString;
    }

    if (_chunked) {
      _sentLength += readLen;
    } else {
      _sentLength += bodyWritten;
    }
"""

MARKER = "catch (const std::bad_alloc&)"


def patch():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    target = os.path.join(libdeps, env_name, "ESPAsyncWebServer", "src", "WebResponses.cpp")

    if not os.path.isfile(target):
        print("[patch] ESPAsyncWebServer WebResponses.cpp not found at %s; skipping" % target)
        return

    with open(target, "r", encoding="utf-8") as handle:
        source = handle.read()

    if MARKER in source:
        print("[patch] ESPAsyncWebServer response writer already hardened")
        return

    if ORIGINAL not in source:
        print("[patch] WARNING: expected ESPAsyncWebServer code not found; skipping patch")
        return

    if "#include <new>" not in source:
        source = source.replace('#include "WebResponseImpl.h"', '#include "WebResponseImpl.h"\n#include <new>', 1)

    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(source.replace(ORIGINAL, PATCHED, 1))
    print("[patch] Hardened ESPAsyncWebServer response writer (short-write safe)")


patch()
