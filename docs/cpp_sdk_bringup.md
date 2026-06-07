# Pico SDK C++ Bring-Up

Validation status: C++ EPS application logic is host-tested. Pico SDK firmware
flashing and the one-Pico USB demo are `TESTED` on the bench.

## Why This Matters

The primary firmware path is Pico SDK C++. The MicroPython implementation is
kept as a fallback/recovery path, but the final validated system uses the C++
EPS and OBC bridge images.

## Host Checks First

Run:

```bash
tools/check_host.sh
```

Expected result:

```text
Python protocol tests pass
Python syntax check passes
C++ EPS app tests passed
```

These tests do not require Pico SDK or hardware.

## Build The Pico SDK Firmware

After installing or locating your Pico SDK checkout:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
tools/build_pico_sdk.sh
```

Expected artifact:

```text
firmware/eps_node/build/eps_node.uf2
```

## Flash

1. Unplug the Pico.
2. Hold `BOOTSEL`.
3. Plug the Pico into USB.
4. Copy `firmware/eps_node/build/eps_node.uf2` to the `RPI-RP2` drive.
5. Wait for the board to reboot and reappear as `/dev/ttyACM*`.

## Run The C++ Demo

The C++ firmware uses raw binary USB serial transport, not the MicroPython
ASCII-hex wrapper:

```bash
FIRMWARE=cpp tools/run_demo.sh
```

If using a fresh terminal that does not yet have `dialout` as the active group:

```bash
sg dialout -c 'FIRMWARE=cpp tools/run_demo.sh'
```

## Acceptance Criteria

- Demo completes without timeout.
- GP10 turns on for load 0.
- GP11 joins GP10 for load mask `0x03`.
- Injected fault enters `SAFE` and turns loads off.
- `SET_LOADS` while `SAFE` returns `NACK BAD_STATE`.
- `REQUEST_NOMINAL` fails while fault flags are active.
- `CLEAR_FAULTS` plus `REQUEST_NOMINAL` returns to `NOMINAL`.
- Corrupted CRC produces no response.
- Final telemetry reports `crc_error_count=1`.

The criteria above passed on 2026-06-01 using the saved log
`bench_artifacts/one_pico_eps_cpp_demo_20260601_235703.txt`.
