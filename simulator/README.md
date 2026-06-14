# GWS shifter simulator

A browser simulator for the BMW GWS gear-lever firmware. The actual firmware is compiled to
WebAssembly and driven by host backends, so the simulator exercises the real
gear logic.

## Prerequisites

Emscripten (`emcc`) on PATH.

- macOS: `brew install emscripten`
- Windows / Linux: install the [emsdk](https://emscripten.org/docs/getting_started/downloads.html),
  then `emsdk install latest && emsdk activate latest` and source `emsdk_env`.

## Build

```sh
./build.sh        # macOS / Linux
./build.ps1       # Windows (PowerShell)
```

This writes `web/simulator.js` and `web/simulator.wasm`.

## Run

WASM must be served over HTTP (not opened as a `file://` URL):

```sh
python3 -m http.server 8000     # run from the simulator/ directory
```

Then open <http://localhost:8000/web/>.

## Develop with live reload

`./watch.sh` (needs Node) serves the page and reloads the browser on changes:
firmware C/C++ edits trigger a rebuild, `web/` edits reload directly. It runs
the initial build itself, so use it instead of the build/serve steps above.

## Using it

- **Lever** buttons tip the lever toward Drive/Reverse, move into/out of the
  M/S gate, hold the sequential paddles, and press Park
- **Game telemetry** dropdown/toggles mock the serial feed from the game. Turn
  off **Game connected** and after 5 s the lever drops to config (button-box)
  mode
- The gear indicator blinks while the firmware is waiting for the game to engage
  a requested gear or while the physical gate disagrees with the game's mode
