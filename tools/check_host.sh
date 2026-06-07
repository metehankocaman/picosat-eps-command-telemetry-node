#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

echo "== Python protocol tests =="
python3 -m pytest tests

echo
echo "== Python syntax check =="
python3 -m py_compile tools/protocol.py tools/ground_station.py firmware/micropython_fallback/main.py

echo
echo "== C++ EPS app host tests =="
g++ -std=c++17 -Wall -Wextra -Werror \
    -I firmware/common \
    firmware/common/protocol.cpp \
    firmware/common/eps_app.cpp \
    firmware/tests/test_eps_app.cpp \
    -o /tmp/picosat_eps_cpp_tests
/tmp/picosat_eps_cpp_tests

echo
echo "Host checks passed"
