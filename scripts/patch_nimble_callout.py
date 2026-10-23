# ruff: noqa: F821  (Import and env are injected by PlatformIO's SCons build)
"""Keep a stale NimBLE callout timer from jumping to address zero.

NimBLEDevice::deinit() zeroes the callout wrapper (evq and ev.fn) even when a
FreeRTOS timer for that callout is still queued. The timer task then calls
os_callout_timer_cb(), which dereferences the zeroed function pointer and
reboots the device with InstFetchProhibitedCause. Ignore such late callbacks.
"""

import os

Import("env")

ORIGINAL = """    co = pvTimerGetTimerID(timer);
    assert(co);

    if (co->evq) {"""

PATCHED = """    co = pvTimerGetTimerID(timer);
    if (!co || !co->ev.fn) {
        // NimBLEDevice::deinit() zeroes this callout; ignore the late timer.
        return;
    }

    if (co->evq) {"""

MARKER = "ignore the late timer"


def patch():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    target = os.path.join(libdeps, env_name, "NimBLE-Arduino", "src", "nimble",
                          "porting", "npl", "freertos", "src", "npl_os_freertos.c")

    if not os.path.isfile(target):
        print("[patch] NimBLE npl_os_freertos.c not found at %s; skipping" % target)
        return

    with open(target, "r", encoding="utf-8") as handle:
        source = handle.read()

    if MARKER in source:
        print("[patch] NimBLE callout timer already guarded")
        return

    if ORIGINAL not in source:
        print("[patch] WARNING: expected NimBLE callout code not found; skipping patch")
        return

    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(source.replace(ORIGINAL, PATCHED, 1))
    print("[patch] Guarded NimBLE callout timer against zeroed callouts")


patch()
