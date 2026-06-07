#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
    echo "PICO_SDK_PATH is not set."
    echo "Set it to your pico-sdk checkout, then rerun this script."
    exit 1
fi

BUILD_DIR="${BUILD_DIR:-firmware/eps_node/build}"
PICO_BOARD="${PICO_BOARD:-pico2_w}"

missing_tools=()
for tool in cmake arm-none-eabi-gcc arm-none-eabi-g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing_tools+=("$tool")
    fi
done

if ! command -v make >/dev/null 2>&1 && ! command -v ninja >/dev/null 2>&1; then
    missing_tools+=("make or ninja")
fi

if (( ${#missing_tools[@]} > 0 )); then
    echo "Missing required build tool(s): ${missing_tools[*]}"
    echo
    echo "On Fedora, install the usual Pico SDK build tools with:"
    echo "  sudo dnf install cmake make ninja-build arm-none-eabi-gcc-cs arm-none-eabi-gcc-cs-c++ arm-none-eabi-newlib"
    echo
    echo "Then rerun:"
    echo "  export PICO_SDK_PATH=\"$PICO_SDK_PATH\""
    echo "  tools/build_pico_sdk.sh"
    exit 1
fi

echo "PICO_SDK_PATH=$PICO_SDK_PATH"
echo "PICO_BOARD=$PICO_BOARD"
echo "BUILD_DIR=$BUILD_DIR"

cmake -S firmware/eps_node -B "$BUILD_DIR" -DPICO_BOARD="$PICO_BOARD"
cmake --build "$BUILD_DIR"

UF2="$BUILD_DIR/eps_node.uf2"
if [[ -f "$UF2" ]]; then
    echo
    echo "Built UF2 images:"
    for image in "$BUILD_DIR"/*.uf2; do
        echo "  $image"
    done
    echo
    echo "Stage 1 USB EPS image: $BUILD_DIR/eps_node.uf2"
    echo "Stage 1 USB measured-load EPS image: $BUILD_DIR/eps_node_measured_load.uf2"
    echo "Stage 2 EPS UART/RS-485 image: $BUILD_DIR/eps_node_uart.uf2"
    echo "Stage 2 EPS UART/RS-485 measured-load image: $BUILD_DIR/eps_node_uart_measured_load.uf2"
    echo "Stage 2 OBC USB-to-RS-485 bridge image: $BUILD_DIR/obc_bridge.uf2"
    echo
    echo "Flash by holding BOOTSEL while plugging in the Pico, then copy the desired UF2 to the RPI-RP2 drive."
else
    echo
    echo "Build completed, but $UF2 was not found. Check the CMake output above."
fi
