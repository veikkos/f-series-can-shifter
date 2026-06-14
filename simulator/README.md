# GWS shifter simulator

A browser simulator for the BMW GWS gear-lever firmware. The actual firmware
(`e90-can-shifter.ino` `loop()`, `shifter.cpp`, `gws_lever.cpp`) is compiled to
WebAssembly and driven by host backends, so the simulator exercises the real
gear logic rather than a reimplementation.

```
  lever controls ──► GWS_ID_POSITION frames ─┐
  game telemetry ──► s_input fields ─────────┤
                                             ▼
                       firmware (WASM)  ──►  gear-indicator display  (rendered)
                                        ──►  gamepad buttons          (LEDs)
```

- `include/Arduino.h` — minimal Arduino-core shim
- `src/can_adapter_wasm.cpp` — captures DISPLAY/BACKLIGHT, feeds lever frames
- `src/serial_wasm.cpp` — writes the game's gear/mode/low-beam into `s_input`
- `src/gamepad_wasm.cpp` — records the button bitmask
- `src/bridge.cpp` — clock, physical-lever model, telemetry encoder, JS exports
- `web/` — the GUI (loads `simulator.js`/`.wasm`)

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

## Using it

- **Lever** buttons tip the lever toward Drive/Reverse, move into/out of the
  M/S gate, hold the sequential paddles, and press Park.
- **Game telemetry** dropdown/toggles mock the serial feed from the game. Turn
  off **Game connected** and after 5 s the lever drops to config (button-box)
  mode, exactly like the firmware.
- The gear indicator blinks while the firmware is waiting for the game to engage
  a requested gear or while the physical gate disagrees with the game's mode.
