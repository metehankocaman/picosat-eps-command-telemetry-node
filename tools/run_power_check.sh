#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

PORT="${PORT:-}"
if [[ -z "$PORT" ]]; then
    PORT="$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$PORT" || ! -e "$PORT" ]]; then
    echo "No Pico serial device found. Plug in the OBC bridge Pico and check: ls /dev/ttyACM* /dev/ttyUSB*"
    exit 1
fi

if [[ "$(id -gn)" != "dialout" ]] && ! id -nG | tr ' ' '\n' | grep -qx "dialout"; then
    echo "This shell does not have dialout permission."
    echo "Run once: sudo usermod -aG dialout \$USER"
    echo "Then log out/in, or run: sg dialout -c 'tools/run_power_check.sh'"
    exit 1
fi

if command -v lsof >/dev/null 2>&1 && lsof "$PORT" >/tmp/picosat_port_users.txt 2>/dev/null; then
    echo "$PORT is already in use:"
    cat /tmp/picosat_port_users.txt
    echo "Close the listed program, then rerun this script."
    exit 1
fi

mkdir -p bench_artifacts
LOG="bench_artifacts/ina219_power_$(date +%Y%m%d_%H%M%S).txt"

echo "Project: $PROJECT_DIR"
echo "Port:    $PORT"
echo "Log:     $LOG"
echo

python3 tools/ground_station.py --port "$PORT" --transport raw --timeout 3 --settle 1 power | tee "$LOG"

echo
echo "Power telemetry log saved to $LOG"
