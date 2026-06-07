# Commanded Power Load Bring-Up

Validation status: `TESTED` on the documented RS-485 bench wiring.

This stage makes INA219 telemetry change when the EPS receives `SET_LOADS`.
It uses the Pico GPIO pins as low-side switches, so the commanded LED load
current flows through the INA219 shunt before returning through the GPIO pin.

This is the final measured-load wiring because the terminal shows a command,
the LED visibly changes state, and power telemetry changes with it.

## Firmware Target

Use the measured-load EPS image:

```text
firmware/eps_node/build/eps_node_uart_measured_load.uf2
```

The OBC bridge does not change:

```text
firmware/eps_node/build/obc_bridge.uf2
```

The measured-load EPS image drives load GPIOs as active-low outputs:

- `SET_LOADS 0x00`: GPIOs are driven high, measured LEDs off.
- `SET_LOADS 0x01`: `GP10` is driven low, measured load 0 on.
- `SET_LOADS 0x03`: `GP10` and `GP11` are driven low, measured loads 0 and 1
  on.
- `SAFE`: all measured loads are forced off.

The normal `eps_node_uart.uf2` image keeps the original active-high LED wiring.

## Keep This Wiring

Leave the existing RS-485 wiring in place.

Leave the INA219 I2C wiring in place:

```text
EPS Pico 3V3(OUT) -> INA219 VCC
EPS Pico GND      -> INA219 GND
EPS Pico GP6      -> INA219 SDA
EPS Pico GP7      -> INA219 SCL
```

## Remove This Temporary Load

Remove the previous always-on INA219 test LED/resistor:

```text
INA219 VIN- -> resistor -> LED -> GND
```

It was useful for proving the sensor, but it creates baseline current that is
not command-controlled.

## New Measured Load Wiring

Create a measured 3.3 V load rail after the INA219 shunt:

```text
EPS Pico 3V3(OUT) ---- INA219 VIN+
INA219 VIN- ---------- MEASURED_LOAD_3V3 rail
```

Wire load 0:

```text
MEASURED_LOAD_3V3 ---- resistor ---- LED anode
LED cathode ------------------------ EPS Pico GP10
```

Wire load 1:

```text
MEASURED_LOAD_3V3 ---- resistor ---- LED anode
LED cathode ------------------------ EPS Pico GP11
```

Use `330 ohm` or `470 ohm` series resistors.

ASCII view:

```text
                 INA219
EPS 3V3 ---- VIN+ [shunt] VIN- ---- measured 3V3 rail
                                      |
                                      +---- [330R/470R] ---->| ---- GP10
                                      |                    LED0
                                      |
                                      +---- [330R/470R] ---->| ---- GP11
                                                           LED1

EPS GND ------------------------------------------------ common ground
```

Important: the measured LED cathodes go to `GP10` and `GP11`, not directly to
ground. The GPIO pins act as the commanded return path.

## Pre-Power Checks

With both Picos unplugged:

1. Confirm the old always-on INA219 test LED/resistor is removed.
2. Confirm `INA219 VIN+` goes to EPS `3V3(OUT)`.
3. Confirm `INA219 VIN-` feeds the measured load rail.
4. Confirm each measured LED has a resistor in series.
5. Confirm measured LED cathodes go to `GP10` and `GP11`.
6. Confirm measured LED cathodes are not tied directly to the breadboard ground
   rail.
7. Check EPS `3V3(OUT)` to `GND` with the DMM. It should not read as a near-zero
   short.

Continuity between `VIN+` and `VIN-` is normal because the INA219 module has a
low-value shunt resistor.

## Run The Demo

After flashing the measured-load EPS firmware, plug in both Picos and run:

```bash
sg dialout -c 'PORT=/dev/ttyACM0 tools/run_power_demo.sh'
```

Expected physical behavior:

1. Measured LEDs start off.
2. `SET_LOADS 0x01` turns on the `GP10` measured LED.
3. `SET_LOADS 0x03` turns on both `GP10` and `GP11` measured LEDs.
4. Injected fault enters `SAFE`; both measured LEDs turn off.
5. Recovery returns to `NOMINAL`; `GP10` measured LED turns on again.

Expected telemetry behavior:

- `POWER_TELEMETRY` with loads off reports low baseline current.
- `POWER_TELEMETRY` with load 0 on reports higher current and power.
- `POWER_TELEMETRY` with loads 0 and 1 on reports higher current again.
- `POWER_TELEMETRY` in `SAFE` returns to low baseline current.

If current is negative, reverse the INA219 sense direction by swapping what is
connected to `VIN+` and `VIN-`.

## Evidence To Save

- `bench_artifacts/commanded_power_demo_*.txt`
- Photo of the measured-load wiring.
- Short clip or photo showing the LEDs and terminal together.

Bench-tested result:

```text
Log: bench_artifacts/commanded_power_demo_20260606_003430.txt
loads off:      load_mask=0x00 current_mA=-0.3 power_mW=0
load 0 on:      load_mask=0x01 current_mA=3.6 power_mW=14
loads 0+1 on:   load_mask=0x03 current_mA=7.2 power_mW=22
SAFE:           load_mask=0x00 current_mA=0.1 power_mW=0
recovered load: load_mask=0x01 current_mA=3.7 power_mW=14
```
