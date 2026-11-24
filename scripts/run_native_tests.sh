#!/bin/bash
# Builds and runs the Nebula Engine native unit test suite on the host.
# Usage: scripts/run_native_tests.sh
set -euo pipefail

cd "$(dirname "$0")/.."
CPP_DIR="app/src/main/cpp"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

SOURCES=(
  "$CPP_DIR/engine/json/json.cpp"
  "$CPP_DIR/engine/physics/physics_world.cpp"
  "$CPP_DIR/engine/scene/scene.cpp"
  "$CPP_DIR/engine/nebula_engine.cpp"
  "$CPP_DIR/engine/game/nebula_run.cpp"
  "$CPP_DIR/engine/render/gl_renderer.cpp"
  "tests/engine_tests.cpp"
)

echo "[nebula] compiling native tests..."
g++ -std=c++17 -I "$CPP_DIR" -Wall -Wextra -Wno-unused-parameter -O2 \
    "${SOURCES[@]}" -o "$OUT_DIR/nebula_tests"

echo "[nebula] running..."
"$OUT_DIR/nebula_tests"
