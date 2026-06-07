# Wiring And Pinout

Validation status: Stage 1 GP10/GP11 LED load behavior has been observed on the
bench. Additional loads on GP12/GP13 should be checked if wired.

## Stage 1 One-Pico USB EPS Demo

Use only one Raspberry Pi Pico 2W, USB, LEDs, and resistors.

| Pico pin | Signal | Connection |
| --- | --- | --- |
| `GP10` | Load 0 | Resistor -> LED anode, LED cathode -> GND |
| `GP11` | Load 1 | Resistor -> LED anode, LED cathode -> GND |
| `GP12` | Load 2 | Resistor -> LED anode, LED cathode -> GND |
| `GP13` | Load 3 | Resistor -> LED anode, LED cathode -> GND |
| `GND` | Ground | LED cathode common ground |
| USB | Command/telemetry | Laptop USB serial |

Recommended LED resistors from the available kit:

- Start with `330 ohm` or `470 ohm`.
- Do not drive bare LEDs directly from GPIO.
- Treat each GPIO as a visible indicator load, not a high-current power output.

Expected behavior:

- GPIO high turns the corresponding LED on.
- `SET_LOADS 0x03` turns on load 0 and load 1.
- SAFE mode turns all load GPIOs off.

## Bench Bring-Up Order

1. Wire one LED on `GP10` with a resistor.
2. Flash firmware.
3. Run `ping`.
4. Run `set-loads 0x01` and confirm the LED turns on.
5. Add remaining LEDs one at a time.
6. Run `inject-fault` and confirm all LEDs turn off.
7. Run `telemetry` and confirm mode is `SAFE` and fault flags are nonzero.

## Stage 2 Hardware

RS-485, INA219, OLED, and the second Pico are not required for the one-Pico
fallback demo. See `stage2_rs485_plan.md` before wiring MAX485 modules.

The Stage 2 direct-UART sanity test uses:

| Connection | Purpose |
| --- | --- |
| OBC `GP0` -> EPS `GP1` | OBC TX to EPS RX |
| EPS `GP0` -> OBC `GP1` | EPS TX to OBC RX |
| OBC `GND` -> EPS `GND` | Shared reference |

The Stage 2 MAX485 path uses the same UART pins plus `GP2` for half-duplex
direction control:

| Pico signal | GPIO | MAX485 signal |
| --- | ---: | --- |
| UART TX | `GP0` | `DI` |
| UART RX | `GP1` | `RO` through measured divider |
| Direction | `GP2` | `DE` and `/RE` tied together |
| 5 V | `VBUS` | `VCC` |
| Ground | `GND` | `GND` |

MAX485 hardware is `TESTED` for the documented bench wiring. The divider output
must still be measured before connecting it to Pico `GP1` if the wiring is
rebuilt or moved.

## INA219 Power Telemetry

INA219 telemetry is added on the EPS Pico. Use 3.3 V logic:

| EPS Pico pin | INA219 pin | Notes |
| --- | --- | --- |
| `3V3(OUT)` | `VCC` | I2C logic supply |
| `GND` | `GND` | Common ground |
| `GP6` | `SDA` | I2C1 data |
| `GP7` | `SCL` | I2C1 clock |

First test load:

```text
EPS 3V3(OUT) -> INA219 VIN+
INA219 VIN-  -> resistor -> LED anode
LED cathode  -> EPS GND
```

See `ina219_telemetry.md` before wiring. For the stronger
command-correlated power demo, use the active-low measured-load wiring in
`commanded_power_load.md` instead of the always-on INA219 test LED.

## OLED Status Display

OLED hardware is `TESTED` on the documented bench wiring. The display remains
optional; command/telemetry, RS-485, SAFE mode, and INA219 telemetry do not
depend on it.

Use the same EPS I2C bus as the INA219:

| EPS Pico pin | OLED pin | Notes |
| --- | --- | --- |
| `3V3(OUT)` | `VCC` | Use 3.3 V logic power |
| `GND` | `GND` | Common ground |
| `GP6` | `SDA` | Shared with INA219 `SDA` |
| `GP7` | `SCL` | Shared with INA219 `SCL` |

If both INA219 and OLED are installed, `GP6` is the shared `SDA` net and `GP7`
is the shared `SCL` net. See `oled_status_display.md` for the step-by-step
bring-up checklist.
