#!/bin/bash
# Builds and runs the native test suite under AddressSanitizer + UBSan.
# Usage: scripts/run_sanitizer_tests.sh
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

echo "[nebula] compiling with -fsanitize=address,undefined..."
g++ -std=c++17 -I "$CPP_DIR" -g -O1 -fsanitize=address,undefined \
    -fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter \
    "${SOURCES[@]}" -o "$OUT_DIR/nebula_tests_asan"

echo "[nebula] running under sanitizers..."
"$OUT_DIR/nebula_tests_asan"
