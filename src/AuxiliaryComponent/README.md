# Auxiliary components

Optional hardware that is not present on every RedLilyGO board lives here.

Examples include external microphones, motion sensors, IR modules, and touch helpers. Keep hardware-specific code isolated so a build without the hardware still compiles and boots.

## Development rules

- Keep each hardware family in its own folder.
- Make `begin()`, `loop()`, and `end()` safe when hardware is absent.
- Avoid blocking callbacks and unbounded allocations.
- Document wiring, voltage, GPIO pins, build flags, and board limits.
- Report unavailable hardware as a capability error.

The stock T-Dongle profile does not enable every optional microphone, IR, or touch feature. Confirm the selected board and wiring before using an example.
