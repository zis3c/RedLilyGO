# Web UI development

`ui/src/` is the maintained source for the dashboard markup, styles, and
JavaScript. Firmware serves the generated files from `ui/content/` and embeds
them in the build. Start at the source tree when changing browser behavior.

## Source layout

- `core/`: navigation, shortcuts, dropdowns, modals, toasts, and shared styles.
- `features/`: dashboard, files, logs, settings, tools, and project information;
  keep feature-specific CSS beside its markup and behavior.
- `layout/`: page shell and shared navigation markup.
- `modals/`: confirmation dialog markup.
- `../vnc/index.html`: project VNC page and input-only controls.
- `../vnc/core/` and `../vnc/vendor/`: noVNC client code and bundled libraries;
  check their notices before changing or redistributing them.

## Build chain

1. Edit files under `ui/src/`.
2. Run `python build_ui.py` from the repository root. It expands HTML includes
   and writes generated HTML, CSS, and JavaScript under `ui/content/`.
3. Build the selected PlatformIO environment. `platformio_assets.py` compresses
   the firmware-facing assets into generated headers under `src/html/`.

Do not edit generated files under `ui/content/` or `src/html/` by hand. Check
both generated diffs after a build because they are committed build outputs.

## Local preview and checks

`preview_server.py` serves the generated page with local fixture responses.
To proxy requests to a connected device, set the device URL before launching:

```powershell
$env:REDLILYGO_DEVICE_URL = 'http://4.3.2.1'
python .\preview_server.py
```

The preview is not a substitute for checking the page on the firmware. Run
source contracts and syntax checks with `.\tools\verify.ps1`; run the browser
suite with the instructions in [tests/README.md](../../tests/README.md). For a
physical UI check, connect to the device AP and record the firmware version and
endpoint responses.

## Runtime boundaries

- Keep route names, WebSocket paths, and firmware-consumed element IDs aligned
  with the web service under `src/Comms/Web/` and the API descriptions.
- Use device responses as data. Show empty or unavailable states when storage,
  sensors, radio scans, or the Agent do not provide a value.
- Preserve the existing origin checks and optional Basic Authentication flow.
  The default device web service uses HTTP without TLS.
- Do not treat a successful page render, contract check, or mock response as
  evidence that a physical peripheral works.
- Avoid large per-frame allocations because the firmware has limited RAM.
