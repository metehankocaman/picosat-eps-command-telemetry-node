# PicoSat EPS Command/Telemetry Node

Small embedded systems project that models a spacecraft electrical power
subsystem (EPS) node. A Python ground station sends CRC-framed commands through
an OBC bridge Pico, over a MAX485 RS-485 link, to an EPS Pico that controls LED
loads, reports telemetry, measures load current with an INA219, updates a small
OLED status display, and enters SAFE mode on injected faults.

The project is intentionally bench-sized, but the software surfaces are the
same ones I would expect in a flight-adjacent embedded workflow: packet framing,
state-machine safety behavior, diagnostics, hardware bring-up notes, and tests
that run without hardware.

## Final Bench Configuration

```text
Python ground station
  |
  | USB CDC serial
  v
Raspberry Pi Pico 2 W  (OBC bridge)
  |
  | UART + MAX485 half-duplex RS-485
  v
Raspberry Pi Pico 2 W  (EPS node)
  |
  +-- GP10/GP11 measured LED loads
  +-- INA219 current/power sensor on I2C
  +-- 0.91 inch SSD1306-compatible OLED on I2C
```

The one-Pico USB EPS firmware is still included as a simpler bring-up and
fallback path. The final demo uses the two-Pico RS-485 path.

## Project Demonstration

[![PicoSat EPS Node Demo](https://img.youtube.com/vi/h9m17SZwZaI/maxresdefault.jpg)](https://www.youtube.com/watch?v=h9m17SZwZaI)

## What It Demonstrates

- Binary command/telemetry protocol with CRC-16/CCITT-FALSE protection.
- Host-side Python encoder/decoder and operator CLI.
- Pico SDK C++ firmware for EPS and OBC bridge roles.
- EPS modes: `BOOT`, `NOMINAL`, and `SAFE`.
- Faults represented as telemetry flags, not as a separate operating state.
- SAFE mode forces loads off and rejects unsafe load commands.
- `REQUEST_NOMINAL` succeeds only after fault flags are cleared.
- Bad CRC frames are dropped before command execution and counted in telemetry.
- INA219 power telemetry correlated with commanded load state.
- OLED mirrors EPS mode, load mask, fault flags, voltage/current, and uptime.
- Python protocol tests and host-side C++ EPS logic tests.

## Validation Status

| Area | Status | Evidence |
| --- | --- | --- |
| Python protocol tests | Tested on host | `pytest` |
| C++ EPS state-machine tests | Tested on host | `tools/check_host.sh` |
| One-Pico USB EPS demo | Bench-tested | `docs/bench_validation.md` |
| Direct Pico-to-Pico UART | Bench-tested | `docs/bench_validation.md` |
| MAX485 RS-485 transport | Bench-tested | DMM checks plus `bench_artifacts/stage2_rs485_demo_20260602_142519.txt` |
| INA219 measured-load telemetry | Bench-tested | `bench_artifacts/commanded_power_demo_20260606_003430.txt` |
| OLED status display | Bench-tested | Observed during final bench run; summarized in `docs/bench_validation.md` |

Bench validation is limited to the hardware setup described in this repository.
No flight qualification, radiation tolerance, environmental testing, or
spacecraft power switching capability is claimed.

## Repository Layout

```text
docs/                         Architecture, protocol, wiring, and validation notes
firmware/common/              Shared C++ protocol and EPS application logic
firmware/eps_node/            Pico SDK EPS firmware targets
firmware/obc_bridge/          USB-to-RS-485 OBC bridge firmware
firmware/tests/               Host-side C++ EPS tests
tests/                        Python protocol tests
tools/                        Ground-station CLI and build/demo helpers
bench_artifacts/              Curated bench logs used as evidence
```

## Host Setup

From the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
pytest
tools/check_host.sh
```

You can inspect packets without hardware:

```bash
python3 tools/ground_station.py --dry-run ping
python3 tools/ground_station.py --dry-run set-loads 0x03
python3 tools/ground_station.py --dry-run bad-crc
```

## Firmware Build

Primary firmware is C++ with the Pico SDK.

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
tools/build_pico_sdk.sh
```

By default the build targets `pico2_w`. Override with `PICO_BOARD` if needed:

```bash
PICO_BOARD=pico tools/build_pico_sdk.sh
```

The build script produces:

```text
firmware/eps_node/build/eps_node.uf2
firmware/eps_node/build/eps_node_measured_load.uf2
firmware/eps_node/build/eps_node_uart.uf2
firmware/eps_node/build/eps_node_uart_measured_load.uf2
firmware/eps_node/build/obc_bridge.uf2
```

For the final bench configuration:

- Flash `obc_bridge.uf2` to the OBC Pico.
- Flash `eps_node_uart_measured_load.uf2` to the EPS Pico.

## Running The Demos

Final RS-485 measured-load/power demo:

```bash
PORT=/dev/ttyACM0 tools/run_power_demo.sh
```

Core command/telemetry safety demo:

```bash
FIRMWARE=rs485 PORT=/dev/ttyACM0 tools/run_demo.sh
```

One-Pico USB fallback demo with Pico SDK C++ firmware:

```bash
FIRMWARE=cpp PORT=/dev/ttyACM0 tools/run_demo.sh
```

One-Pico USB fallback demo with MicroPython:

```bash
FIRMWARE=micropython PORT=/dev/ttyACM0 tools/run_demo.sh
```

The MicroPython fallback uses an ASCII-hex wrapper around the same binary packet
format because MicroPython's USB REPL treats some raw bytes as control
characters. The final C++/RS-485 demo uses raw binary frames.

## Key Documents

- `docs/architecture.md`: system boundaries and state machine.
- `docs/protocol.md`: packet format, node IDs, message IDs, telemetry payloads.
- `docs/wiring_pinout.md`: one-Pico, RS-485, INA219, and OLED wiring.
- `docs/stage2_rs485_plan.md`: RS-485 bring-up and MAX485 divider notes.
- `docs/commanded_power_load.md`: measured-load rail wiring and expected power behavior.
- `docs/bench_validation.md`: tested claims and evidence.
- `docs/demo_script.md`: concise 5-minute walkthrough structure.

## Notes On Scope

The LED loads are visible bench loads, not spacecraft power outputs. The INA219
is used for low-current telemetry correlation, not precision calibration. The
MAX485 modules are used as an accessible half-duplex RS-485 transport; this is
not an RS-422 design.
