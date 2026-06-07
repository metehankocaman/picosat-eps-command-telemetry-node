# RS-485 / Two-Pico Bring-Up

Hardware validation status:

- Direct two-Pico UART transport: `TESTED`.
- MAX485 physical RS-485 transport: `TESTED` on the documented bench wiring.

This design uses MAX485 modules as a half-duplex RS-485 physical layer. It is
not an RS-422 design.

## Goal

Add a second Pico as an OBC bridge:

```text
Laptop ground station
  |
  | USB serial
  v
Pico OBC bridge
  |
  | UART + MAX485 half-duplex bus
  v
Pico EPS node
```

The packet protocol and EPS safety behavior remain unchanged. RS-485 is only
the transport layer.

## Implemented Firmware Images

The Stage 2 firmware builds alongside the Stage 1 USB EPS image:

| UF2 | Pico role | Notes |
| --- | --- | --- |
| `firmware/eps_node/build/eps_node.uf2` | Stage 1 USB EPS | Bench-tested one-Pico demo |
| `firmware/eps_node/build/eps_node_uart.uf2` | Stage 2 EPS node | UART/RS-485 command receiver; no USB serial |
| `firmware/eps_node/build/obc_bridge.uf2` | Stage 2 OBC bridge | USB serial to UART/RS-485 packet bridge |

Build command:

```bash
export PICO_SDK_PATH="$HOME/pico/pico-sdk"
tools/build_pico_sdk.sh
```

The OBC bridge forwards complete framed packets without interpreting the CRC.
That means the deliberately corrupted CRC packet still reaches the EPS, and the
EPS should increment `crc_error_count` just like the Stage 1 demo.

## Intended Roles

- OBC bridge Pico: USB CDC to UART/RS-485 forwarding, direction control, basic
  packet-boundary handling.
- EPS Pico: receive the same command frames over UART instead of USB, run the
  same `BOOT` / `NOMINAL` / `SAFE` state machine, and return telemetry frames.
- Python ground station: unchanged except port points to the OBC bridge Pico.

## Bring-Up Philosophy

Do not start with MAX485. First prove the two firmware images with direct
3.3 V Pico-to-Pico UART wiring. After that works, insert the MAX485 modules and
validate the 5 V receive path with a DMM before connecting `RO` to Pico `GP1`.

## Stage 2A: Direct UART Sanity Test

This step avoids the 5 V MAX485 modules and proves the OBC bridge plus EPS UART
firmware first. This step passed on 2026-06-02 and is summarized in
`docs/bench_validation.md`.

Flash:

1. Label one Pico `OBC`.
2. Label the other Pico `EPS`.
3. Flash `obc_bridge.uf2` onto the `OBC` Pico.
4. Flash `eps_node_uart.uf2` onto the `EPS` Pico.
5. Keep the LED loads on the `EPS` Pico at `GP10` and `GP11`.

Direct UART wiring:

| Connection | Purpose |
| --- | --- |
| OBC `GP0` -> EPS `GP1` | OBC UART TX to EPS UART RX |
| EPS `GP0` -> OBC `GP1` | EPS UART TX to OBC UART RX |
| OBC `GND` -> EPS `GND` | Shared reference |

Power both Picos over USB. Only the `OBC` Pico should expose a USB serial port
for the ground station; the `EPS` UART firmware intentionally has USB serial
disabled.

Run:

```bash
sg dialout -c 'FIRMWARE=rs485 tools/run_demo.sh'
```

Expected result:

- The terminal demo sequence matches the Stage 1 C++ demo.
- `PING` returns `ACK`.
- `SET_LOADS 0x03` turns on EPS load LEDs 0 and 1.
- `INJECT_FAULT` enters `SAFE` and turns the LEDs off.
- `SET_LOADS` while `SAFE` returns `NACK status=BAD_STATE`.
- `REQUEST_NOMINAL` fails while the fault is active.
- `CLEAR_FAULTS` plus `REQUEST_NOMINAL` recovers to `NOMINAL`.
- The bad CRC packet receives no response.
- Final telemetry reports `mode=NOMINAL`, `load_mask=0x01`,
  `fault_flags=0x0000`, and `crc_error_count=1`.

If this direct UART test fails, do not wire MAX485 yet. Recheck flashed images,
TX/RX crossover, common ground, and the selected serial port.

## MAX485 Safety Warning

The available MAX485 modules are listed as 5 V parts. Pico GPIO is not 5 V
tolerant. Measure `RO` with the DMM before connecting it to Pico RX.

Do not connect MAX485 `RO` directly to Pico RX unless measured output is safely
within Pico input limits.

## RX Divider Options From Available Resistors

Option A:

```text
MAX485 RO -> 5.6k ohm -> Pico RX
Pico RX   -> 10k ohm  -> GND
```

Approximate divider output from 5 V: 3.2 V.

Option B:

```text
MAX485 RO -> 10k ohm + 5.6k ohm series -> Pico RX
Pico RX   -> 22k ohm                   -> GND
```

Approximate divider output from 5 V: 2.93 V.

## Stage 2B: MAX485 Wiring

This step passed on 2026-06-02 with log
`bench_artifacts/stage2_rs485_demo_20260602_142519.txt`. Before connecting the
divider nodes to Pico `GP1`, the measured divider outputs were `3.22 V` on the
OBC side and `3.22-3.23 V` on the EPS side.

| Pico signal | GPIO | MAX485 signal |
| --- | ---: | --- |
| UART TX | `GP0` | `DI` |
| UART RX | `GP1` | `RO` through divider |
| Direction | `GP2` | `DE` and `/RE` tied together |
| 5 V | `VBUS` or external 5 V | `VCC` |
| GND | `GND` | `GND` |

Connect module A to module A and B to B. If no traffic decodes, swap A/B on one
module and retest.

Recommended order:

1. Unplug USB power before wiring.
2. Build the `RO` divider for each MAX485 module, but leave the divider output
   disconnected from Pico `GP1`.
3. Power one Pico/module at a time and measure the divider output with the DMM.
4. Confirm the divider output is below 3.3 V.
5. Power down again.
6. Connect the divider output to Pico `GP1`.
7. Connect each Pico `GP0` to its module `DI`.
8. Connect each Pico `GP2` to its module `DE` and `/RE`.
9. Connect module `A` to module `A`, module `B` to module `B`, and grounds
   together.
10. Power both Picos over USB.

Run the same demo command as Stage 2A:

```bash
sg dialout -c 'FIRMWARE=rs485 tools/run_demo.sh'
```

## Validation Checklist

1. DMM: verify Pico 3V3 and GND rails.
2. DMM: power MAX485 at 5 V, leave Pico RX disconnected, measure `RO` high level.
3. Add the selected RX divider.
4. DMM: verify divider output is below Pico input limit.
5. Flash a UART loopback or bridge test.
6. Logic analyzer: capture Pico TX, MAX485 DI, DE, and RO.
7. Confirm direction timing: DE asserted before transmit and released after the
   final stop bit.
8. Run `ping` through the OBC bridge.
9. Run `set-loads`, `inject-fault`, `telemetry`, and `bad-crc`.
10. Record results in `bench_validation.md`.

## What To Record

Before marking Stage 2 as `TESTED`, capture:

- Which UF2 was flashed onto each Pico.
- Direct UART demo log, if run.
- MAX485 VCC measurement.
- MAX485 `RO` divider measurement before connecting Pico `GP1`.
- Ground-station log saved under `bench_artifacts/stage2_rs485_demo_*.txt`.
- A photo of the RS-485 wiring.
- Any logic-analyzer capture, if available.

## Fallbacks

- If MAX485 levels are unsafe, do not use the modules; fall back to the one-Pico
  USB demo or direct UART test.
- If A/B wiring is ambiguous, swap A/B on one side after voltage checks.
- If half-duplex timing is unreliable, first prove Pico-to-Pico UART without
  MAX485, then reintroduce RS-485.
- If Stage 2 is incomplete, present it as documented future work rather than a
  validated feature.
