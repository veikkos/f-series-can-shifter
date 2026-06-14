#!/usr/bin/env bash
# Live-reloading dev server for the WASM simulator.
#
# Rebuilds when the firmware C/C++ sources change and reloads the browser when
# anything under web/ changes (including the regenerated simulator.js/.wasm).
# Requires Emscripten (emcc) and Node (uses npx browser-sync + nodemon, both
# fetched on demand). Plain serving without auto-reload is just build.sh + a
# static server; see README.
set -euo pipefail
cd "$(dirname "$0")"

command -v emcc >/dev/null || { echo "emcc not on PATH (see README)"; exit 1; }
command -v npx  >/dev/null || { echo "npx not on PATH (install Node)"; exit 1; }

PORT="${PORT:-8000}"

# Serve simulator/ (so web/ can reach ../assets) and reload on web/ changes.
npx --yes browser-sync start \
  --server --startPath web \
  --files "web/main.js,web/index.html,web/style.css,web/simulator.js,web/simulator.wasm" \
  --port "$PORT" --no-notify &
BS_PID=$!
trap 'kill "$BS_PID" 2>/dev/null || true' EXIT INT TERM

# Rebuild on firmware-source changes. nodemon runs build.sh once on startup and
# again on each change; the .js/.wasm it emits are not watched here, so there is
# no rebuild loop. Sources live in the repo root and simulator/src + include.
npx --yes nodemon \
  --watch .. \
  --ext ino,cpp,h \
  --ignore web/ \
  --exec './build.sh || true'
