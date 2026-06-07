#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

PORT="${PORT:-}"
if [[ -z "$PORT" ]]; then
    PORT="$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$PORT" || ! -e "$PORT" ]]; then
    echo "No Pico serial device found. Plug in the Pico and check: ls /dev/ttyACM* /dev/ttyUSB*"
    exit 1
fi

if [[ "$(id -gn)" != "dialout" ]] && ! id -nG | tr ' ' '\n' | grep -qx "dialout"; then
    echo "This shell does not have dialout permission."
    echo "Run once: sudo usermod -aG dialout \$USER"
    echo "Then log out/in, or run: sg dialout -c 'tools/run_demo.sh'"
    exit 1
fi

if command -v lsof >/dev/null 2>&1 && lsof "$PORT" >/tmp/picosat_port_users.txt 2>/dev/null; then
    echo "$PORT is already in use:"
    cat /tmp/picosat_port_users.txt
    echo "Close the listed program, then rerun this script."
    exit 1
fi

mkdir -p bench_artifacts
FIRMWARE="${FIRMWARE:-micropython}"
TRANSPORT="hex"
LOG_PREFIX="one_pico_eps_demo"

if [[ "$FIRMWARE" == "cpp" ]]; then
    TRANSPORT="raw"
    LOG_PREFIX="one_pico_eps_cpp_demo"
elif [[ "$FIRMWARE" == "rs485" ]]; then
    TRANSPORT="raw"
    LOG_PREFIX="stage2_rs485_demo"
elif [[ "$FIRMWARE" != "micropython" ]]; then
    echo "Unknown FIRMWARE=$FIRMWARE. Use FIRMWARE=micropython, FIRMWARE=cpp, or FIRMWARE=rs485."
    exit 1
fi

LOG="bench_artifacts/${LOG_PREFIX}_$(date +%Y%m%d_%H%M%S).txt"

echo "Project: $PROJECT_DIR"
echo "Port:    $PORT"
echo "Firmware mode: $FIRMWARE"
echo "Log:     $LOG"
echo

if [[ "$FIRMWARE" == "micropython" ]]; then
    mpremote connect "$PORT" fs cp firmware/micropython_fallback/main.py :main.py
    mpremote connect "$PORT" reset
    sleep 3
else
    echo "Assuming Pico SDK C++ firmware is already flashed."
    echo "If this is a fresh USB connection, wait for the CDC serial port to settle."
    sleep 2
fi

python3 tools/ground_station.py --port "$PORT" --transport "$TRANSPORT" --timeout 3 --settle 1 demo | tee "$LOG"

echo
echo "Demo log saved to $LOG"
