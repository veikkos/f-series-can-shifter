# Build the WASM simulator on Windows. Requires Emscripten (emcc) on PATH.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$root = ".."
$exports = "['_sim_init','_sim_tick','_sim_tip_drive','_sim_tip_reverse','_sim_enter_ms','_sim_leave_ms','_sim_paddle_up','_sim_paddle_down','_sim_park','_sim_set_gear','_sim_set_sport','_sim_set_lights','_sim_set_manual_gear','_sim_set_connected','_sim_display_byte','_sim_backlight','_sim_buttons','_sim_connected','_sim_mismatch','_sim_lever_gate']"

emcc `
  -I $root -I include `
  -O2 -std=c++17 `
  -x c++ "$root/e90-can-shifter.ino" `
  "$root/shifter.cpp" "$root/gws_lever.cpp" `
  src/can_adapter_wasm.cpp src/serial_wasm.cpp src/gamepad_wasm.cpp src/bridge.cpp `
  -s MODULARIZE=1 -s EXPORT_NAME=createSim `
  -s EXPORTED_FUNCTIONS="$exports" `
  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" `
  -s ALLOW_MEMORY_GROWTH=1 `
  --no-entry `
  -o web/simulator.js

Write-Host "Built web/simulator.js + web/simulator.wasm"
