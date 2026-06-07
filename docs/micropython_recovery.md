# MicroPython Recovery Notes

If `main.py` takes over USB serial and `mpremote` cannot enter raw REPL, recover
the Pico with BOOTSEL:

1. Unplug the Pico.
2. Hold `BOOTSEL`.
3. Plug the Pico back in.
4. Release `BOOTSEL` after the `RP2350` mass-storage drive appears.
5. Copy Raspberry Pi's `flash_nuke.uf2` to the drive to erase flash.
6. Re-enter BOOTSEL mode and copy the Pico 2 W MicroPython UF2.
7. Copy `firmware/micropython_fallback/main.py` again with `mpremote`.

The fallback firmware now leaves Ctrl-C enabled and accepts a plain `STOP` line
over the serial connection to return to the REPL.
