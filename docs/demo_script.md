# Five-Minute Demo Outline

This is the intended walkthrough structure for a short application video or
live bench demo. It is not required for running the project; the commands below
are normal ground-station commands that exercise the real hardware.

## 0:00 - 0:35: System Overview

Show the hardware path:

```text
Laptop -> USB -> OBC Pico -> MAX485 RS-485 -> EPS Pico
```

Identify the EPS-side hardware:

- GP10 and GP11 measured LED loads.
- INA219 current/power sensor on the measured load rail.
- OLED status display on the EPS I2C bus.

Main point: this is a small bench system, but it exercises command/telemetry,
transport, hardware interfaces, diagnostics, and fault handling.

## 0:35 - 1:25: Packet Protocol

Show `docs/protocol.md` or a packet diagram.

Point out:

- Sync bytes: `A5 5A`.
- Header: version, source, destination, sequence, type, length.
- Payload: command-specific data.
- CRC-16/CCITT-FALSE, little-endian, over header and payload.

Main point: corrupted frames are rejected before command execution.

## 1:25 - 2:20: Bring-Up Strategy

Explain the staged path:

1. Host protocol tests.
2. Host C++ EPS state-machine tests.
3. One-Pico USB EPS demo.
4. Direct Pico-to-Pico UART.
5. MAX485 RS-485.
6. INA219 and OLED polish.

Main point: the project was built in layers so RS-485 or sensor wiring could not
block the core command/telemetry demo.

## 2:20 - 4:20: Live Demo

Run the final power demo:

```bash
PORT=/dev/ttyACM0 tools/run_power_demo.sh
```

Expected sequence:

1. Link check returns `ACK`.
2. Loads are forced off and baseline power telemetry is read.
3. `SET_LOADS 0x01` turns on GP10 and current rises.
4. `SET_LOADS 0x03` turns on GP10 and GP11 and current rises again.
5. `INJECT_FAULT` enters `SAFE`; both loads turn off.
6. Recovery is gated: clear faults, request nominal, then command load 0 again.

If also showing the core safety/CRC demo:

```bash
FIRMWARE=rs485 PORT=/dev/ttyACM0 tools/run_demo.sh
```

Point out:

- `SET_LOADS` is rejected in `SAFE`.
- `REQUEST_NOMINAL` fails while fault flags are active.
- Bad CRC receives no response.
- Final telemetry shows `crc_error_count=1`.

## 4:20 - 5:00: Close

Summarize the engineering value:

- The OBC bridge is transport only.
- The EPS owns command validation and safety decisions.
- SAFE mode is enforced in firmware and visible on hardware.
- Power telemetry independently confirms load state.
- Tests and bench logs document what has actually been validated.
