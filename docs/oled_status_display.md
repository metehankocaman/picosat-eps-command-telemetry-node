# OLED Status Display

Validation status: `TESTED` on the documented bench wiring. Evidence:
`bench_artifacts/commanded_power_demo_20260606_003430.txt` plus observed OLED
and LED behavior during the final bench run.

The OLED is optional polish. The EPS command path, RS-485 transport, SAFE mode,
and INA219 telemetry must continue to work even if the OLED is missing or wired
incorrectly. The firmware tries the common SSD1306 I2C addresses `0x3C` and
`0x3D`; if neither responds, it silently keeps running without display output.

## What It Shows

The EPS firmware renders four short status lines:

```text
EPS NOMINAL
LOAD 03 F0000
I 7.2MA P 22MW
VBUS 3332MV T18s
```

In SAFE mode after an injected fault, expect something like:

```text
EPS SAFE
LOAD 00 F0001
I 0.3MA P 0MW
VBUS 3336MV T21s
```

If the INA219 is unavailable but the OLED is working, the bottom lines show a
power-sensor error instead of current and voltage.

## Wiring

Use the OLED on the same EPS I2C bus as the INA219:

```text
EPS Pico 3V3(OUT)  -> OLED VCC
EPS Pico GND       -> OLED GND
EPS Pico GP6       -> OLED SDA
EPS Pico GP7       -> OLED SCL
```

Keep the existing INA219 wiring in place:

```text
EPS Pico 3V3(OUT)  -> INA219 VCC
EPS Pico GND       -> INA219 GND
EPS Pico GP6       -> INA219 SDA
EPS Pico GP7       -> INA219 SCL
```

That means `GP6` becomes a shared `SDA` row and `GP7` becomes a shared `SCL`
row on the breadboard. The INA219 and OLED each connect to those same two rows.

Use `3V3(OUT)`, not `VBUS`, for OLED logic power unless the exact display board
has been verified as 5 V tolerant. The JMDO.91A-marked 0.91 inch modules are
commonly SSD1306-compatible I2C displays, but the exact address and pin order
must still be treated as bench-verification items.

## Firmware

OLED support is compiled into the EPS firmware images:

```text
firmware/eps_node/build/eps_node.uf2
firmware/eps_node/build/eps_node_measured_load.uf2
firmware/eps_node/build/eps_node_uart.uf2
firmware/eps_node/build/eps_node_uart_measured_load.uf2
```

For the full project demo, use:

```text
EPS Pico: firmware/eps_node/build/eps_node_uart_measured_load.uf2
OBC Pico: firmware/eps_node/build/obc_bridge.uf2
```

The OBC bridge does not use the OLED.

## Bring-Up Checklist

1. Power everything off.
2. Confirm the OLED silkscreen pin names before wiring.
3. Wire `VCC`, `GND`, `SDA`, and `SCL` as listed above.
4. Leave the existing RS-485, INA219, and measured LED load wiring unchanged.
5. Flash the EPS with `eps_node_uart_measured_load.uf2`.
6. Flash or keep the OBC on `obc_bridge.uf2`.
7. Plug in the EPS, then the OBC.
8. The OLED should briefly show:

```text
EPS NODE
OLED ONLINE
WAITING CMD
```

9. Run the power demo:

```bash
PORT=/dev/ttyACM0 tools/run_power_demo.sh
```

10. Confirm the display changes with the terminal sequence:

| Demo moment | Expected OLED cue |
| --- | --- |
| Initial state | `EPS NOMINAL`, `LOAD 00 F0000` |
| Load 0 on | `LOAD 01 F0000`; current rises |
| Loads 0 and 1 on | `LOAD 03 F0000`; current rises again |
| Injected fault | `EPS SAFE`, `LOAD 00 F0001` |
| Recovery | `EPS NOMINAL`, `LOAD 01 F0000` |

## If The OLED Is Blank

Do not disturb the working RS-485/INA219 wiring first. Check the OLED in this
order:

1. Confirm OLED `VCC` is on EPS `3V3(OUT)`.
2. Confirm OLED `GND` is common with EPS ground.
3. Confirm OLED `SDA` is on EPS `GP6`.
4. Confirm OLED `SCL` is on EPS `GP7`.
5. Confirm the OLED pins are not reversed by the module's physical pin order.
6. Reflash `eps_node_uart_measured_load.uf2`.
7. Run the normal `power-demo`; if terminal telemetry still works, the display
   is the only failing polish item.

Fallback: leave the OLED disconnected and keep the already-tested
RS-485/INA219 measured-load demo as the application demo.
