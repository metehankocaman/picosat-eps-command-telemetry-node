# Architecture

This project is a bench-scale command and telemetry system for a spacecraft-style
EPS node. The final validated path uses two Raspberry Pi Pico 2 W boards: one as
a USB-to-RS-485 OBC bridge and one as the EPS controller.

## Final System

```text
Python ground station
  tools/ground_station.py
  tools/protocol.py
      |
      | USB CDC serial, raw binary packets
      v
OBC bridge Pico
  firmware/obc_bridge/main.cpp
      |
      | UART + MAX485 half-duplex RS-485
      v
EPS Pico
  firmware/eps_node/main_uart.cpp
  firmware/common/eps_app.cpp
      |
      +-- GP10 / GP11 measured LED loads
      +-- INA219 on I2C1, GP6 SDA / GP7 SCL
      +-- SSD1306-compatible OLED on same I2C bus
```

The OBC bridge is transport plumbing. It forwards complete framed packets
between USB and the RS-485 UART path; it does not become a second command
authority. CRC validation and EPS safety decisions remain inside the EPS node.

## Bring-Up Path

The project was staged so the command/telemetry and safety behavior could be
proved before adding transport and sensor complexity:

1. Host-only Python protocol tests.
2. Host-only C++ EPS state-machine tests.
3. One-Pico USB EPS demo.
4. Direct Pico-to-Pico UART.
5. MAX485 RS-485 transport.
6. INA219 power telemetry.
7. Command-correlated measured loads.
8. OLED status display.

The one-Pico USB firmware remains useful as a fallback and diagnostic target. It
uses the same packet semantics and EPS state machine, but removes the OBC bridge
and MAX485 hardware from the path.

## Software Boundaries

- `tools/protocol.py`: Python packet encoder/decoder, CRC-16, telemetry
  structures, and command payload helpers.
- `tools/ground_station.py`: operator CLI for commands, telemetry, power
  telemetry, fault injection, and CRC rejection demos.
- `firmware/common`: host-testable C++ protocol helpers and EPS application
  logic.
- `firmware/eps_node`: Pico SDK EPS firmware targets.
- `firmware/obc_bridge`: USB CDC to UART/RS-485 bridge firmware.
- `firmware/tests`: host-side C++ tests for protocol parsing and EPS safety
  behavior.
- `firmware/micropython_fallback`: same core behavior in MicroPython for rapid
  fallback bring-up.

## EPS State Machine

```text
BOOT -> NOMINAL
NOMINAL -- ENTER_SAFE or INJECT_FAULT --> SAFE
SAFE -- CLEAR_FAULTS --> SAFE with no active fault flags
SAFE -- REQUEST_NOMINAL and no faults --> NOMINAL
SAFE -- REQUEST_NOMINAL and faults active --> SAFE
```

Faults are reported as flags in telemetry. They are not a separate operating
mode.

## Safety Behavior

- In `SAFE`, all load outputs are forced off.
- `SET_LOADS` is accepted only in `NOMINAL` with no active fault flags.
- `CLEAR_FAULTS` clears fault flags but does not leave `SAFE`.
- `REQUEST_NOMINAL` is the only command that can leave `SAFE`, and only when
  fault flags are clear.
- Bad CRC frames are rejected before command execution and counted in telemetry.

## Sensor And Display Boundaries

INA219 support is local to the EPS node. It adds `GET_POWER_TELEMETRY` and
`POWER_TELEMETRY` without changing the core command/telemetry safety behavior.

OLED support is also local to the EPS node. The display mirrors already-known
state and power telemetry; it is not a command source. If no SSD1306-compatible
display responds at `0x3C` or `0x3D`, the EPS firmware continues running.
