// GUI for the GWS shifter simulator. Loads the firmware-as-WASM module, wires
// the controls to the bridge's exported functions, and renders the captured
// outputs (gear-indicator display, joystick buttons, status) each frame.

// GWS display byte values (see types.h GwsDisplay).
const DISPLAY_FLASH = 0x08;
const DISPLAY = { 0x20: 'park', 0x40: 'reverse', 0x60: 'neutral', 0x80: 'drive', 0x81: 'drive_ms' };
const BACKLIGHT_FULL = 0xff;

const BUTTON_NAMES = ['Reverse', 'Drive', 'Sport', 'Paddle Up', 'Paddle Down', 'Park'];

// Raw firmware gear state (GwsGear in types.h) — surfaces TRANSITIONAL, which
// is otherwise invisible because the indicator shows it as a plain flashing D.
const GEAR_STATE = { '-1': 'Reverse', 0: 'Neutral', 1: 'Drive', 2: 'Transitional', 3: 'Park' };

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
    gear: c('sim_gear', 'number', []),
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
  const enterMsBtn = $('enter-ms');
  const exitMsBtn = $('exit-ms');

  // --- Lever controls -----------------------------------------------------
  $('rev1').addEventListener('click', () => sim.tipReverse(1));
  $('rev2').addEventListener('click', () => sim.tipReverse(2));
  $('drv1').addEventListener('click', () => sim.tipDrive(1));
  $('drv2').addEventListener('click', () => sim.tipDrive(2));
  enterMsBtn.addEventListener('click', () => sim.enterMs());
  exitMsBtn.addEventListener('click', () => sim.leaveMs());
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
  const responding = $('responding');

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

  // --- Simulated game -----------------------------------------------------
  // With "Game responding" on, the host game reads the firmware's bound
  // gamepad buttons and updates its gear just like a real game would, closing
  // the loop back to the indicator. The firmware holds one gear/mode button at
  // a time (see shifter.cpp applyButtons), so the held mask maps straight to a
  // gear; paddles are momentary, so a rising edge is one manual shift.
  const BTN = { REVERSE: 0, DRIVE: 1, MANUAL: 2, PADDLE_UP: 3, PADDLE_DOWN: 4, PARK: 5 };
  let prevMask = 0;
  const held = (mask, b) => (mask & (1 << b)) !== 0;

  function gameRespond(mask) {
    const rising = (b) => held(mask, b) && !held(prevMask, b);
    const falling = (b) => !held(mask, b) && held(prevMask, b);

    // The M/S side gate drives the Sport flag: on entering D->M/S, off again on
    // leaving M/S->D or as soon as the first manual shift demotes it to manual
    if (rising(BTN.MANUAL)) sport.checked = true;
    if (falling(BTN.MANUAL)) sport.checked = false;

    let manual = +manualGear.value;
    if (held(mask, BTN.MANUAL)) {
      if (rising(BTN.PADDLE_UP)) { manual = Math.min(manual + 1, +manualGear.max); sport.checked = false; }
      if (rising(BTN.PADDLE_DOWN)) { manual = Math.max(manual - 1, +manualGear.min); sport.checked = false; }
    }

    // Engaged gear from the single held gear/mode button (none held -> Neutral)
    let sel;
    if (held(mask, BTN.PARK)) sel = 0;
    else if (held(mask, BTN.REVERSE)) sel = 1;
    else if (held(mask, BTN.MANUAL)) sel = 4;
    else if (held(mask, BTN.DRIVE)) sel = 3;
    else sel = 2;

    // Drive the existing controls + bridge setters, only on change
    if (+gearSel.value !== sel) { gearSel.value = String(sel); sim.setGear(sel); syncManualVisible(); }
    if (+manualGear.value !== manual) { manualGear.value = String(manual); sim.setManualGear(manual); }
    sim.setSport(sport.checked ? 1 : 0);
  }

  // While responding, the game owns these controls; show them as read-only
  const drivenControls = [gearSel, manualGear, sport];
  const applyResponding = () => drivenControls.forEach((el) => (el.disabled = responding.checked));
  responding.addEventListener('change', applyResponding);
  applyResponding();

  // --- Render loop --------------------------------------------------------
  function setOverlay(name, on) {
    if (overlays[name]) overlays[name].classList.toggle('on', on);
  }

  function render(now) {
    sim.tick(now);

    const display = sim.displayByte();
    const inMs = sim.leverGate() === 1; // lever in the M/S side gate
    const flashing = (display & DISPLAY_FLASH) !== 0;
    const code = display & ~DISPLAY_FLASH;
    let active = DISPLAY[code] || null;
    if (active === 'drive_ms') active = inMs ? 'ms' : 'drive';

    // Blink the lit segment at ~2.5 Hz while the firmware sets the flash bit
    const visible = !flashing || Math.floor(now / 200) % 2 === 0;
    for (const name of Object.keys(overlays)) {
      setOverlay(name, active === name && visible);
    }

    // Backlight swaps in the illuminated shifter image; the gear LED stays lit
    const backlit = sim.backlight() === BACKLIGHT_FULL;
    shifter.classList.toggle('backlit', backlit);
    backlightPill.textContent = backlit ? 'Backlight on' : 'Backlight off';
    backlightPill.className = 'pill ' + (backlit ? 'ok' : 'off');

    // Joystick buttons
    const mask = sim.buttons();
    leds.forEach((led, i) => led.classList.toggle('on', (mask & (1 << i)) !== 0));

    // Let the simulated game react to the buttons, then remember the edge state
    if (responding.checked) gameRespond(mask);
    prevMask = mask;

    // Status pills
    const conn = sim.connected() === 1;
    connPill.textContent = conn ? 'Game connected' : 'Config mode (button box)';
    connPill.className = 'pill ' + (conn ? 'ok' : 'off');

    const mismatch = sim.mismatch() === 1;
    mismatchPill.className = 'pill warn' + (mismatch ? '' : ' hidden');

    // Gate-dependent control availability, plus the raw firmware gear state
    const gearState = GEAR_STATE[sim.gear()] || '?';
    gateLabel.textContent = `— ${inMs ? 'in M/S gate' : 'in auto gate'} · ${gearState}`;
    tipButtons.forEach((b) => (b.disabled = inMs));
    paddleButtons.forEach((b) => (b.disabled = !inMs));
    enterMsBtn.disabled = inMs;
    exitMsBtn.disabled = !inMs;

    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
});
