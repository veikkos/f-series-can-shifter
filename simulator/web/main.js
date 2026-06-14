// GUI for the GWS shifter simulator. Loads the firmware-as-WASM module, wires
// the controls to the bridge's exported functions, and renders the captured
// outputs (gear-indicator display, joystick buttons, status) each frame.

// GWS display byte values (see types.h GwsDisplay).
const DISPLAY_FLASH = 0x08;
const DISPLAY = { 0x20: 'park', 0x40: 'reverse', 0x60: 'neutral', 0x80: 'drive', 0x81: 'drive_ms' };
const BACKLIGHT_FULL = 0xff;

const BUTTON_NAMES = ['Reverse', 'Drive', 'Manual', 'Paddle Up', 'Paddle Down', 'Park'];

createSim().then((Module) => {
  // --- Bind exported C functions -----------------------------------------
  const c = (name, ret, args) => Module.cwrap(name, ret, args);
  const sim = {
    init: c('sim_init', null, []),
    tick: c('sim_tick', null, ['number']),
    tipDrive: c('sim_tip_drive', null, ['number']),
    tipReverse: c('sim_tip_reverse', null, ['number']),
    enterMs: c('sim_enter_ms', null, []),
    leaveMs: c('sim_leave_ms', null, []),
    paddleUp: c('sim_paddle_up', null, ['number']),
    paddleDown: c('sim_paddle_down', null, ['number']),
    park: c('sim_park', null, []),
    setGear: c('sim_set_gear', null, ['number']),
    setSport: c('sim_set_sport', null, ['number']),
    setLights: c('sim_set_lights', null, ['number']),
    setManualGear: c('sim_set_manual_gear', null, ['number']),
    setConnected: c('sim_set_connected', null, ['number']),
    displayByte: c('sim_display_byte', 'number', []),
    backlight: c('sim_backlight', 'number', []),
    buttons: c('sim_buttons', 'number', []),
    connected: c('sim_connected', 'number', []),
    mismatch: c('sim_mismatch', 'number', []),
    leverGate: c('sim_lever_gate', 'number', []),
  };

  sim.init();

  // --- DOM handles --------------------------------------------------------
  const $ = (id) => document.getElementById(id);
  const shifter = $('shifter');
  const overlays = {};
  document.querySelectorAll('.overlay').forEach((el) => (overlays[el.dataset.gear] = el));
  const leds = Array.from(document.querySelectorAll('.led'));
  const connPill = $('conn-pill');
  const mismatchPill = $('mismatch-pill');
  const backlightPill = $('backlight-pill');
  const gateLabel = $('gate-label');

  const tipButtons = ['rev1', 'rev2', 'drv1', 'drv2'].map($);
  const paddleButtons = [$('paddle-up'), $('paddle-down')];

  // --- Lever controls -----------------------------------------------------
  $('rev1').addEventListener('click', () => sim.tipReverse(1));
  $('rev2').addEventListener('click', () => sim.tipReverse(2));
  $('drv1').addEventListener('click', () => sim.tipDrive(1));
  $('drv2').addEventListener('click', () => sim.tipDrive(2));
  $('enter-ms').addEventListener('click', () => sim.enterMs());
  $('exit-ms').addEventListener('click', () => sim.leaveMs());
  $('park-btn').addEventListener('click', () => sim.park());

  // Paddles are momentary: held while the pointer is down on the button
  const wireHold = (btn, fn) => {
    const press = (e) => { e.preventDefault(); fn(1); btn.classList.add('active'); };
    const release = () => { fn(0); btn.classList.remove('active'); };
    btn.addEventListener('pointerdown', press);
    btn.addEventListener('pointerup', release);
    btn.addEventListener('pointerleave', release);
    btn.addEventListener('pointercancel', release);
  };
  wireHold($('paddle-up'), sim.paddleUp);
  wireHold($('paddle-down'), sim.paddleDown);

  // --- Game telemetry controls -------------------------------------------
  const gearSel = $('gear-sel');
  const manualField = $('manual-field');
  const manualGear = $('manual-gear');
  const sport = $('sport');
  const lights = $('lights');
  const connected = $('connected');

  const syncManualVisible = () => {
    manualField.style.display = gearSel.value === '4' ? '' : 'none';
  };
  gearSel.addEventListener('change', () => { sim.setGear(+gearSel.value); syncManualVisible(); });
  manualGear.addEventListener('change', () => sim.setManualGear(+manualGear.value));
  sport.addEventListener('change', () => sim.setSport(sport.checked ? 1 : 0));
  lights.addEventListener('change', () => sim.setLights(lights.checked ? 1 : 0));
  connected.addEventListener('change', () => sim.setConnected(connected.checked ? 1 : 0));

  // Push initial control state into the bridge so DOM and sim agree
  sim.setGear(+gearSel.value);
  sim.setManualGear(+manualGear.value);
  sim.setSport(sport.checked ? 1 : 0);
  sim.setLights(lights.checked ? 1 : 0);
  sim.setConnected(connected.checked ? 1 : 0);
  syncManualVisible();

  // --- Render loop --------------------------------------------------------
  function setOverlay(name, on) {
    if (overlays[name]) overlays[name].classList.toggle('on', on);
  }

  function render(now) {
    sim.tick(now);

    const display = sim.displayByte();
    const gate = sim.leverGate(); // 1 = M/S side gate
    const flashing = (display & DISPLAY_FLASH) !== 0;
    const code = display & ~DISPLAY_FLASH;
    let active = DISPLAY[code] || null;
    if (active === 'drive_ms') active = gate === 1 ? 'ms' : 'drive';

    // Blink the lit segment at ~2.5 Hz while the firmware sets the flash bit
    const visible = !flashing || Math.floor(now / 200) % 2 === 0;
    for (const name of ['park', 'reverse', 'neutral', 'drive', 'ms']) {
      setOverlay(name, active === name && visible);
    }

    // Backlight dims the panel; the gear LED stays lit
    const backlit = sim.backlight() === BACKLIGHT_FULL;
    shifter.classList.toggle('backlight-off', !backlit);
    backlightPill.textContent = backlit ? 'Backlight on' : 'Backlight off';
    backlightPill.className = 'pill ' + (backlit ? 'ok' : 'off');

    // Joystick buttons
    const mask = sim.buttons();
    leds.forEach((led, i) => led.classList.toggle('on', (mask & (1 << i)) !== 0));

    // Status pills
    const conn = sim.connected() === 1;
    connPill.textContent = conn ? 'Game connected' : 'Config mode (button box)';
    connPill.className = 'pill ' + (conn ? 'ok' : 'off');

    const mismatch = sim.mismatch() === 1;
    mismatchPill.classList.toggle('hidden', !mismatch);
    mismatchPill.className = 'pill warn' + (mismatch ? '' : ' hidden');

    // Gate-dependent control availability
    gateLabel.textContent = gate === 1 ? '— in M/S gate' : '— in auto gate';
    tipButtons.forEach((b) => (b.disabled = gate === 1));
    paddleButtons.forEach((b) => (b.disabled = gate !== 1));
    $('enter-ms').disabled = gate === 1;
    $('exit-ms').disabled = gate !== 1;

    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
});
