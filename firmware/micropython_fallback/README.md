# MicroPython Fallback Firmware

Use this only if Pico SDK setup blocks the C++ firmware path. It implements the
same one-Pico USB command/telemetry protocol and state-machine behavior:

- states: `BOOT`, `NOMINAL`, `SAFE`
- LED load control on `GP10` through `GP13`
- injected fault flags
- SAFE mode disables loads
- `REQUEST_NOMINAL` only succeeds after `CLEAR_FAULTS`
- bad CRC frames are rejected and counted in telemetry

Copy `main.py` to the Pico running MicroPython, then use the same
`tools/ground_station.py` commands from the host with `--transport hex`.

The payload is still the same CRC-protected binary protocol. The hex transport
only wraps each binary frame as an ASCII line so MicroPython's USB REPL stream
does not interpret packet bytes as control characters.

Maintenance note: send a plain `STOP` line or press Ctrl-C to stop the fallback
firmware and return to the REPL. If an older copy blocks `mpremote`, use
`docs/micropython_recovery.md`.

Hardware validation status: `TESTED` for the one-Pico USB demo on 2026-06-01.
Use `docs/bench_validation.md` as the evidence record.
