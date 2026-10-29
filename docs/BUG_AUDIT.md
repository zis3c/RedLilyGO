# Firmware bug audit, 2026-09-28

Status: in progress. This audit covers bugs, dead code, FreeRTOS stack use,
heap fragmentation, ISR/callback concurrency, watchdog starvation, shared
flags, races, leaks, GPIO state, radio disconnects, and panic/reboot loops.
Passing host tests or a build does not close the hardware portions.

## Fixed and checked

| Area | Defect and change | Evidence |
| --- | --- | --- |
| USB CDC framing | Replaced blocking frame reads with an incremental parser, bounded payload drain, one-second frame deadline, and malformed-stream recovery. TX now drops a complete frame when the host stalls. | Host regressions pass. Hardware disconnect and stalled-host timing still need a board check. |
| USB mass storage | File byte-offset overflow checks incorrectly rejected raw SD sectors at and above the 4 GiB boundary. Apply those checks only to file-backed reads. | Host callbacks checked on both sides of the boundary, with invalid input and image-overflow cases. Physical SD I/O remains untested. |
| Wait task lifetime and heap churn | Delay/button/Agent/MSC/touch waits created six kinds of temporary task. Button waits survived Stop, stale completions could clear a replacement wait, and four failed task-creation paths leaked their heap token. Replace them with cooperative polling in the existing Arduino loop. | Host tests cover all wait kinds, unsigned timer rollover, 10,000 cancel/restart cycles, result consumption, disconnects and timeouts. No wait task stacks or heap tokens remain. |
| Parser race | Web requests and RUN_PAYLOAD could call Restart while Execute was using the interpreter's stacks. Defer restart to the Arduino loop, snapshot inline commands, and reject stale completion/wait/reset updates using the payload generation. | A host test replaces/stops a payload from another thread inside Execute. The original setter triggers the test's concurrent-restart assertion. |
| Agent result reporting | The worker cleared a received result, then interpreted the cleared flag as a timeout. The polling state completes successfully without emitting a timeout. | Success, disconnection and real timeout paths checked independently. |
| Agent heartbeat | Firmware stopped requesting status once connected, but the host only replies to requests. Keep polling connected sessions. | Twenty request/reply intervals remain connected; missing replies and unavailable CDC still expire the session. |
| Agent saved settings | agentPollSec is stored as Int16 but was read with getInt, so the NVS type mismatch restored the default. Read with getShort. | A saved seven-second interval is used by the host firmware test. |
| Missing SD reboot loop | The recovery prompt disabled Wi-Fi and restarted even without a formatting request; headless recovery also restarted unconditionally. Keep recovery access and continue without storage after the prompt. Only a physical long press formats and requests a restart. | Display and headless paths tested; no press/short press cannot format, stop Wi-Fi or reboot. |
| Formatting task | A separate animation task and completion semaphore added heap/stack lifetime and display concurrency to a synchronous formatting operation. Show a static formatting message instead. | Formatting success/failure still follows the physical-button path, with no task or semaphore allocation. |
| Dead state | Removed unused readCmdLine declaration, wait task wrappers, timeout pointer, volatile wait flag and generation-token allocations. | Firmware compilation and host execution checks. |
| BLE callback lifetime | NimBLE scan callbacks are firmware-lifetime objects. Stop no longer deletes an object that a late callback may still use. | Source regression and firmware build pass. |
| PCAP and USB NCM lifecycle | PCAP cleanup frees queued buffers after short writes. NCM start and stop serialize callback installation and avoid duplicate SD directory work. | Source regression and firmware build pass. |
| Safe mode and payload state | Safe mode skips `/autorun.ds`; payload selection clamps after SD contents change; empty image names no longer reach `front()`. | Source regression and firmware build pass. |
| Radio and portal input | Negative SSID counts are rejected and capped before narrowing. Evil Portal now takes the Wi-Fi branch before generic Marauder state. | Source regression and firmware build pass. |
| Mic, IR, Agent and buffer safety | Mic callbacks are locked and I2S timeout is bounded; IR sends one frame per loop; Agent heartbeat state is locked; failed double-buffer allocation frees both halves. | Source regression and firmware build pass. |
| Marauder serial buffer | A connected USB host that stopped reading could make `Serial.write()` spin forever. Drop a whole framed buffer unless the transport has room, so the Arduino loop stays cooperative and markers are never split. | Source regression and firmware build pass. Hardware capture delivery still needs a board check. |
| SD format lifecycle | Web and button formatting now stop Ducky, Marauder, USB NCM, and MSC owners before storage unmount and format. | Regression and firmware build pass. Live format remains untested. |

## Current verification

- `python tests/firmware_regressions.py`: pass, including USB, Ducky,
  format recovery, firmware Agent, radio, display, PCAP and storage cases.
- All ten `tests/*_contract.mjs` scripts: pass.
- `python -m platformio run -e LILYGO-T-Dongle-S3`: pass.
  RAM 109,784 / 327,680 bytes; flash 2,778,233 / 3,342,336 bytes.
- Browser 16/16 and Windows Agent 9/9 passed earlier in this audit;
  neither UI nor C# Agent source changed in this continuation.
- No flash performed. Current `platformio device list` showed no serial board;
  the computer was connected to `chem_zzz`, not the device AP.

## Open work

These are investigation targets, not claims of confirmed runtime failures.

1. Storage/heap: bound text and log-line reads against actual available heap;
   audit direct File users and concurrent USB MSC/FAT ownership.
2. Panic recovery: check crash streak reset timing, persisted backtraces, and
   watchdog reset reporting on hardware.
3. Stack use: static ELF inspection found a 4,384-byte constructor frame in
   DuckScriptInterpreter's global initialization. This alone does not prove
   overflow. ESP-IDF calls global constructors before starting the application
   scheduler, so the 4,096-byte main-task setting is not a valid direct bound
   for that constructor. Trace the actual startup stack and call depth.
   [ESP-IDF 5.1 startup source](https://github.com/espressif/esp-idf/blob/v5.1/components/esp_system/startup.c#L388-L443).
4. GPIO/ISR: GPIO BOOT uses OneButton's enabled pull-up in the installed
   source. No application attachInterrupt registration was found in the
   scanned source. Driver callbacks, optional peripherals and pin transition
   states still need a complete review; this is not proof of no ISR race.
5. Hardware evidence: task stack high-water marks, minimum/largest free heap
   during repeated operations, reconnect stress, thermal recovery, serial
   crash evidence and live HTTP checks are outstanding.

Do not increase every stack or add volatile to every flag as a substitute
for identifying ownership, lifetime, actual call depth and blocking paths.
