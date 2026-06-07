# INA219 Power Telemetry Bring-Up

Validation status: `TESTED` on the documented bench wiring.

This stage adds I2C power telemetry on the EPS Pico. It does not replace the
existing command/telemetry demo; it adds a new `GET_POWER_TELEMETRY` command.

## Implemented Behavior

- EPS firmware initializes INA219 at I2C address `0x40`.
- EPS uses I2C1 on `GP6`/`GP7`.
- Ground station command: `power`.
- Response: `POWER_TELEMETRY`.
- Existing `demo` command is unchanged.

## Required Firmware

Flash the updated EPS UART image:

```text
firmware/eps_node/build/eps_node_uart.uf2
```

The OBC bridge image does not need to change for this stage because it forwards
framed packets without interpreting command IDs.

## EPS I2C Wiring

Wire the INA219 module to the EPS Pico only:

```text
EPS Pico 3V3(OUT) -> INA219 VCC
EPS Pico GND      -> INA219 GND
EPS Pico GP6      -> INA219 SDA
EPS Pico GP7      -> INA219 SCL
```

Use 3.3 V for `VCC` so the I2C pullups are Pico-safe.

## Sensor Test Load Wiring

For the first bench test, use a small always-on LED load through the INA219
shunt. This proves voltage/current/power telemetry without disturbing the
existing command-controlled `GP10` and `GP11` LED loads.

```text
EPS Pico 3V3(OUT) ---- INA219 VIN+
INA219 VIN- ---------- resistor ---------- LED anode
LED cathode ---------- EPS Pico GND
```

ASCII view:

```text
EPS 3V3 ---- INA219 VIN+ [ shunt ] INA219 VIN- ---- [330R or 470R] ---->| ---- GND
                                                                      LED
```

Expected current is only a few milliamps. If current telemetry is negative,
swap `VIN+` and `VIN-`.

## Keep Existing Wiring

Leave the tested RS-485 wiring in place:

```text
Laptop -> OBC Pico USB -> MAX485 bus -> EPS Pico UART
```

Leave the EPS command-controlled LEDs in place:

```text
EPS GP10 -> resistor -> LED -> GND
EPS GP11 -> resistor -> LED -> GND
```

The INA219 test LED is a separate power-sense load.

## Bench Sequence

1. Unplug both Picos.
2. Wire INA219 `VCC`, `GND`, `SDA`, and `SCL` to the EPS Pico.
3. Wire the INA219 test load from `VIN+` to `VIN-` to resistor/LED/GND.
4. Check that `INA219 VCC` is not shorted to `GND`.
5. Plug in the EPS Pico and OBC Pico.
6. Run the power check:

```bash
sg dialout -c 'PORT=/dev/ttyACM0 tools/run_power_check.sh'
```

Expected output:

```text
rx ... type=POWER_TELEMETRY ...
  POWER ... sensor_status=0x01(PRESENT) bus_mV=... current_mA=... power_mW=...
```

Healthy first-run ranges:

| Field | Expected |
| --- | --- |
| `sensor_status` | `0x01(PRESENT)` |
| `bus_mV` | roughly `3000` to `3400` |
| `current_mA` | a small positive value, usually a few mA |
| `power_mW` | positive nonzero value |

If `sensor_status` reports `I2C_ERROR`, check `VCC`, `GND`, `SDA`, `SCL`, and
that the INA219 address jumpers are still at the default `0x40`.

## What To Record

- Power telemetry log from `bench_artifacts/ina219_power_*.txt`.
- DMM measurement of INA219 `VCC` to `GND`.
- Photo of INA219 wiring.
- Whether the test LED visibly turns on.

Initial bench-tested standalone sensor result:

```text
POWER uptime_ms=53497 mode=NOMINAL load_mask=0x00 sensor_status=0x01(PRESENT) bus_mV=3280 shunt_uV=210 current_mA=1.5 power_mW=4
```

The INA219 test LED turned on immediately when EPS power was applied, which is
expected for the always-on test load wiring.

The retained public evidence log for INA219 behavior is the final
command-correlated measured-load run:
`bench_artifacts/commanded_power_demo_20260606_003430.txt`.
