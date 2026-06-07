# Bench Validation

This file records claims that were tested on the bench. It is deliberately
conservative: host tests, firmware builds, DMM measurements, terminal logs, and
observed hardware behavior are called out separately.

## Current Status

| Item | Status | Evidence |
| --- | --- | --- |
| Python protocol tests | `TESTED` | `pytest` |
| C++ EPS app host tests | `TESTED` | `tools/check_host.sh` |
| C++ Pico SDK firmware build | `TESTED` | `tools/build_pico_sdk.sh` produced EPS and OBC UF2 images |
| One-Pico USB EPS demo | `TESTED` | `bench_artifacts/one_pico_eps_cpp_demo_20260601_235703.txt` |
| Direct Pico-to-Pico UART | `TESTED` | Same command sequence passed before MAX485 insertion |
| MAX485 RS-485 transport | `TESTED` | `bench_artifacts/stage2_rs485_demo_20260602_142519.txt` |
| MAX485 RO divider voltage | `TESTED` | OBC side measured `3.22 V`; EPS side measured `3.22-3.23 V` before connecting Pico RX |
| INA219 measured-load telemetry | `TESTED` | `bench_artifacts/commanded_power_demo_20260606_003430.txt` |
| OLED status display | `TESTED` | OLED mode/load/power display observed during final bench demo |

## Host Tests

Command:

```bash
tools/check_host.sh
```

Coverage:

- Python packet encode/decode and CRC behavior.
- C++ protocol round trip.
- Bad CRC parser rejection.
- Load control.
- Fault injection.
- SAFE mode load disable.
- Rejected `REQUEST_NOMINAL` while fault flags are active.
- Fault clearing and safety-gated recovery.
- CRC error count in telemetry.

## One-Pico USB C++ Demo

Log: `bench_artifacts/one_pico_eps_cpp_demo_20260601_235703.txt`

Command:

```bash
FIRMWARE=cpp tools/run_demo.sh
```

Observed:

- `PING` returned `ACK`.
- Initial telemetry reported `mode=NOMINAL`, `load_mask=0x00`.
- `SET_LOADS 0x03` returned `ACK`; telemetry reported `load_mask=0x03`.
- Injected fault entered `SAFE`; telemetry reported `fault_flags=0x0001` and
  `load_mask=0x00`.
- `SET_LOADS` while SAFE returned `NACK status=BAD_STATE`.
- `REQUEST_NOMINAL` while fault active returned `NACK status=FAULT_ACTIVE`.
- `CLEAR_FAULTS` plus `REQUEST_NOMINAL` recovered to `NOMINAL`.
- Deliberately corrupted CRC packet received no response.
- Final telemetry reported `crc_error_count=1`.

## MAX485 RS-485 Demo

Log: `bench_artifacts/stage2_rs485_demo_20260602_142519.txt`

Command:

```bash
FIRMWARE=rs485 tools/run_demo.sh
```

Pre-power checks:

- MAX485 RO divider output measured before connecting to Pico `GP1`.
- OBC divider node: `3.22 V`.
- EPS divider node: `3.22-3.23 V`.

Observed:

- The same command sequence passed through the OBC bridge and MAX485 link.
- EPS LED loads followed the expected sequence.
- SAFE mode disabled loads and rejected load commands.
- Bad CRC received no response.
- Final telemetry reported `mode=NOMINAL`, `load_mask=0x01`,
  `fault_flags=0x0000`, and `crc_error_count=1`.

This validates the documented MAX485 half-duplex RS-485 bench transport. It does
not validate RS-422, long cable runs, noisy environments, or flight hardware.

## Final Power/OLED Demo

Log: `bench_artifacts/commanded_power_demo_20260606_003430.txt`

Command:

```bash
PORT=/dev/ttyACM0 tools/run_power_demo.sh
```

Observed:

| Demo moment | Telemetry evidence |
| --- | --- |
| Loads off | `load_mask=0x00`, `current_mA=-0.3`, `power_mW=0` |
| Load 0 on | `load_mask=0x01`, `current_mA=3.6`, `power_mW=14` |
| Loads 0 and 1 on | `load_mask=0x03`, `current_mA=7.2`, `power_mW=22` |
| SAFE | `mode=SAFE`, `load_mask=0x00`, `current_mA=0.1`, `power_mW=0` |
| Recovery | `mode=NOMINAL`, `load_mask=0x01`, `current_mA=3.7`, `power_mW=14` |

OLED behavior was observed during the final run: the display tracked EPS mode,
load mask, fault state, voltage/current, and uptime while the terminal demo ran.

## Scope Limits

- LED loads are low-current bench indicators.
- INA219 values are used for correlation, not calibrated precision metrology.
- MAX485 validation was performed on the documented short bench wiring.
- No environmental, vibration, thermal, radiation, or flight qualification
  testing is claimed.
- Logic analyzer capture would be a useful future artifact, but the current
  validation record is based on host tests, DMM checks, terminal logs, and
  observed hardware behavior.
