/* ===========================================================================
   Input — two keys on the case and the touch glass.

   The keys are named by where they sit on the case, LEFT and RIGHT. LEFT is
   the key on the board's power circuit (GPIO16): it is what switches the
   board on from the cell, and on this hardware a press of it pulses the
   power rail — the chip resets before any firmware sees the key. So LEFT
   is power, and nothing else is ever put on it. RIGHT is the key on GPIO0.
   The third key on the case is RESET, wired to the chip's reset line.

   This board has no separate key for settings or the setup hotspot —
   those live on the glass instead, on the left part of the dashboard
   (the STATUS / ALERT part) which is on screen whenever the dashboard is,
   locked or not. The other two parts keep the glass's own roles.

   No gesture here waits to see if a second tap follows any more. A
   double-tap was tried for settings and for mute, and on both the keys
   and the glass it read as unreliable more than deliberate: a real tap
   sometimes sat waiting half a second for a second one that was never
   coming, and this hardware's own touch chatter (see handleTouch()) made
   a stray "second tap" too easy to manufacture by accident. Every tap
   fires the moment the finger or the key lifts. One narrow exception: a
   second tap on the left part landing right after the first one opened
   settings is recognized and ignored — double-tap on the left part is
   future use, reserved rather than misread as a fresh tap on whatever
   settings happens to be showing there (the sound icon, today) — see
   handleTouch()'s own comment on SETTINGS_REOPEN_GUARD_MS.

     GLASS  left part     tap: settings screen on / off
                          double-tap: future use
                          hold 2 s: the setup hotspot on / off — hotspot.ino
            right parts   tap: next metric screen
                          hold 2 s: force a refresh — net.ino
            any screen    off the main screen, a tap goes back; in the hotspot,
                          a 2 s hold anywhere leaves it
     RIGHT  GPIO0         tap: display on / off
                          hold 2 s: lock the screen, or — already locked — open the
                                    unlock keypad. Only once armed: real data has shown
                                    and the glass can type — lock.ino
     KEYPAD (lock view)   every tap is a digit, or a tap on the readout to back out;
                          no hold does anything — the setup and refresh holds are
                          the dashboard's, not the keypad's
     SETTINGS (glass)     a tap on the sound icon (part 1) mutes / unmutes — sound
                          moved here from a RIGHT double-tap, for the reason above;
                          a tap anywhere else goes back — settings.ino
     LEFT   GPIO16        hold 2 s: power off · from off, hold 2 s: on
                          tap: future use — and on this board a press resets the chip anyway

   Every press and every touch prints on Serial with the action it caused.
   =========================================================================== */

#define HOLD_MS         2000     /* every hold — keys and glass */
#define TAP_MS           400
#define DEBOUNCE_MS       30     /* a raw pin change must hold steady this long before it's trusted */
#define TOUCH_LIFT_MS     60     /* a read that FAILS mid-touch is not a lift — only this long without one is */
#define TOUCH_LIFT_ZEROS   2     /* the controller answering "no finger" this many reads running IS a lift */
#define TOUCH_MOVE_PX     24     /* a finger that travels further is not a tap */
#define TOUCH_GLITCH_MS        35   /* a held touch won't be treated as lifted until sensed has stayed false this long */
#define TOUCH_DOWN_DEBOUNCE_MS 20   /* a fresh touch-down won't be trusted until sensed has stayed true this long */
#define SETTINGS_REOPEN_GUARD_MS 400   /* a second tap on the left part landing this soon after the
                                           first opened settings is the second half of a double-tap,
                                           not a fresh tap on whatever settings shows there now */

enum { K_LEFT, K_RIGHT, K_COUNT };
struct Key {
  uint8_t     pin;
  const char* name;
  bool        rawDown;           /* last raw pin reading — not trusted until it settles */
  bool        down, fired;
  uint32_t    t0, edgeT0;
};
static Key keys[K_COUNT] = {
  { KEY_POWER, "LEFT",  false, false, false, 0, 0 },
  { KEY_BOOT,  "RIGHT", false, false, false, 0, 0 },
};

/* the ring: 48 ticks round the centre filling clockwise, the action inside */
void holdRing(float p, const char* label){
  const int cx = W / 2, cy = H / 2, R = 44;
  cv->fillCircle(cx, cy, R, C_BG);
  cv->drawCircle(cx, cy, R, C_LINE);
  const int lit = (int)(clampf(p, 0, 1) * 48);
  for (int i = 0; i < 48; i++){
    const float a = -PI / 2 + i * 2 * PI / 48;
    cv->fillRect(cx + (int)lround(cos(a) * 36) - 1, cy + (int)lround(sin(a) * 36) - 1, 2, 2,
                 i < lit ? C_TX : C_LINE2);
  }
  txt(label, cx, cy - 10, 2, C_TX, 'c', true);
  txt("HOLD", cx, cy + 8, 1, C_DIM, 'c');
}

/* ------------------------------------------------------------------ views */
static void showMain(){
  if (view == VIEW_HOTSPOT) hotspotStop();          /* hotspot.ino — drops the AP */
  if (view == VIEW_MAIN) return;
  view = VIEW_MAIN;
  cycleResetTimer();                                /* net.ino — a full 5 s on this screen */
  LOG("view", "main");
}

/* ------------------------------------------------------------------- keys */

/* a key already down at boot (LEFT, just held to switch on) is ignored
   until it is let go                                                      */
void keysBegin(){
  for (int i = 0; i < K_COUNT; i++){
    pinMode(keys[i].pin, INPUT_PULLUP);
    keys[i].rawDown = keys[i].down = keys[i].fired = digitalRead(keys[i].pin) == LOW;
    keys[i].edgeT0 = 0;
  }
  LOG("boot", "keys ready - RIGHT: tap display, hold lock/unlock | LEFT: hold off | glass left part: tap settings, hold hotspot | right parts: tap next screen, hold refresh");
}

static void onTap(int k){
  switch (k){
    case K_LEFT:  LOG("key", "LEFT tap -> future use (hold 2 s to switch off)"); break;
    case K_RIGHT: displayToggle(); break;           /* power.ino — logs what it did */
  }
}

static void onHold(int k){
  switch (k){
    case K_LEFT:
      LOG("key", "LEFT hold -> power off");
      powerOff();                                   /* power.ino */
      break;
    case K_RIGHT:
      if (!lockArmed()){                             /* lock.ino — no data yet, or no touch to type a code with */
        LOG("key", "RIGHT hold -> future use (the lock arms once real data has shown)");
      } else if (lockIsLocked()){
        LOG("key", "RIGHT hold -> opening the unlock keypad");
        lockOpenKeypad();
      } else {
        LOG("key", "RIGHT hold -> locked");
        lockEngage();
      }
      break;
  }
}

void handleKeys(){
  const uint32_t now = millis();
  for (int i = 0; i < K_COUNT; i++){
    Key& k = keys[i];
    const bool raw = digitalRead(k.pin) == LOW;

    /* Real debounce: a raw change is only trusted once it has held steady
       for DEBOUNCE_MS. Without this, contact chatter — worst right at the
       release of a 2 s hold — re-opens a fresh down/up pair for every
       bounce, and one physical press reads back as a hold plus a stray
       tap or two. Filtering by completed-tap duration (as before) doesn't
       catch this: a bounce mid-press can easily last longer than that. */
    if (raw != k.rawDown){ k.rawDown = raw; k.edgeT0 = now; }
    const bool down = (now - k.edgeT0 >= DEBOUNCE_MS) ? k.rawDown : k.down;

    if (down && !k.down){
      k.down = true; k.fired = false; k.t0 = now;
      LOGF("key", "%s down (GPIO%d)", k.name, k.pin);
    } else if (!down && k.down){
      k.down = false;
      const uint32_t held = now - k.t0;
      if (!k.fired && held < TAP_MS) onTap(i);
      k.fired = false;
    }

    if (k.down && !k.fired && now - k.t0 >= HOLD_MS){
      k.fired = true;
      onHold(i);
    }
  }
}

/* ------------------------------------------------------------------ touch
   Two zones on the dashboard: the left part (STATUS / ALERT, x < COL_X[1])
   carries settings and the hotspot; the other two parts carry next-screen
   and the refresh. A tap on the left part opens settings directly — not
   next-screen, and not the hotspot, which stays on that part's hold. Off
   the main screen a tap goes back, from either part. Every touch-down
   logs both the screen point and the controller's raw pair — the raw
   pair is what a calibration fix needs.                                 */
static TouchPoint tp;
static bool     tpDown = false, tpFired = false, tpLeftPart = false;
static bool     tpLifting = false, tpCand = false;      /* edge-debounce state, see handleTouch() */
static uint32_t tpT0 = 0, tpSeen = 0;
static uint32_t tpLiftT0 = 0, tpCandT0 = 0;
static int16_t  tpX0 = 0, tpY0 = 0;
static uint8_t  tpZeros = 0;                            /* "no finger" answers in a row */
static uint32_t tpSettingsOpenT0 = 0;   /* down-time of the tap that last opened settings from the
                                            left part — see SETTINGS_REOPEN_GUARD_MS above */
static int16_t  tpSettingsOpenX = 0, tpSettingsOpenY = 0;

static void hotspotToggle(){
  if (view == VIEW_HOTSPOT){ LOG("touch", "hold 2 s -> leaving the setup hotspot"); showMain(); }
  else { LOG("touch", "hold 2 s on the left part -> setup hotspot"); view = VIEW_HOTSPOT; hotspotStart(); }   /* hotspot.ino */
}

/* a glass tap on the main screen — the only view this dispatches for;
   every other view is handled directly in handleTouch()'s up-transition */
static void onGlassTap(bool leftPart){
  if (leftPart){
    view = VIEW_SETTINGS;
    LOG("touch", "tap on the left part -> settings");
    return;
  }
  nextScreen();                                     /* dashboard.ino */
  cycleResetTimer();
  LOGF("touch", "tap -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
       snap.nrows ? snap.rows[curRow].gateway : "-", snap.nrows ? snap.rows[curRow].name : "-");
}

void handleTouch(){
  if (!touchOK) return;
  const uint32_t now = millis();
  /* The controller is asked every few ms (idleWait in the main sketch). It
     is "sensed" down while it keeps answering with one; sensed lets go the
     moment it answers "none" twice running, or — a read failing outright
     — once TOUCH_LIFT_MS pass without a finger.                          */
  TouchPoint p;
  if (touchRead(p)){
    if (p.down){ tp = p; tpSeen = now; tpZeros = 0; }
    else if (tpZeros < TOUCH_LIFT_ZEROS) tpZeros++;
  }
  const bool sensed = tpSeen && tpZeros < TOUCH_LIFT_ZEROS && now - tpSeen < TOUCH_LIFT_MS;

  /* Neither edge is trusted the instant sensed changes. This controller
     blinks "no finger" for a poll or two even under a genuinely still,
     continuous press — untreated, a single hold scattered into a run of
     tiny taps (its own timer kept restarting), and a stray trailing
     blink right after a real tap read as a brand new one and fired
     again immediately (settings closing itself the instant it opened,
     for instance). So: tpDown itself won't let go until sensed has
     stayed false for TOUCH_GLITCH_MS (a blink mid-hold no longer resets
     anything), and a fresh touch-down won't be trusted until sensed has
     stayed true for TOUCH_DOWN_DEBOUNCE_MS (a one-poll phantom never
     opens a gesture on its own).                                        */
  bool down;
  if (tpDown){
    if (sensed){ tpLifting = false; down = true; }
    else {
      if (!tpLifting){ tpLifting = true; tpLiftT0 = now; }
      down = now - tpLiftT0 < TOUCH_GLITCH_MS;
    }
  } else if (sensed){
    if (!tpCand){ tpCand = true; tpCandT0 = now; }
    down = now - tpCandT0 >= TOUCH_DOWN_DEBOUNCE_MS;
  } else {
    tpCand = false;
    down = false;
  }

  if (down && !tpDown){
    tpDown = true; tpFired = false; tpT0 = now; tpX0 = tp.x; tpY0 = tp.y;
    tpLeftPart = tp.x < COL_X[1];
    tpCand = false; tpLifting = false;
    LOGF("touch", "down at %d,%d (raw %d,%d)", tp.x, tp.y, tp.rawX, tp.rawY);
  } else if (!down && tpDown){
    tpDown = false;
    tpLifting = false;
    const bool still = abs(tp.x - tpX0) < TOUCH_MOVE_PX && abs(tp.y - tpY0) < TOUCH_MOVE_PX;
    if (!still) LOG("touch", "slide - ignored (a tap must not travel)");
    else if (tpFired){ /* the hold already fired and handled this touch */ }
    /* The keypad reads every non-sliding release as a digit — no TAP_MS
       deadline: a finger aiming at one cell often rests past 400 ms, and
       nothing else competes on this view.                               */
    else if (view == VIEW_LOCK) lockHandleTap(tp.x, tp.y);
    else if (!displayAwake) LOG("touch", "tap -> ignored, display is off (tap RIGHT to wake it)");
    else if (tpSeen - tpT0 >= TAP_MS){ /* too long for a tap, too short for the hold that never came */ }
    /* A double-tap on the left part is future use — reserved, not a
       fresh tap on whatever settings happens to show there now. Caught
       here, not by waiting to see if a second tap follows (that wait is
       exactly what made every tap feel slow): the down edge above
       already stamped tpT0/tpX0/tpY0 for THIS touch, so it costs
       nothing to compare them against the tap that just opened
       settings, after the fact, once this one is confirmed a tap too. */
    else if (view == VIEW_SETTINGS && tpSettingsOpenT0 &&
             tpT0 - tpSettingsOpenT0 < SETTINGS_REOPEN_GUARD_MS &&
             abs(tpX0 - tpSettingsOpenX) < TOUCH_MOVE_PX && abs(tpY0 - tpSettingsOpenY) < TOUCH_MOVE_PX){
      LOG("touch", "double-tap on the left part -> future use");
      tpSettingsOpenT0 = 0;
    }
    else if (view == VIEW_SETTINGS) settingsHandleTap(tp.x, tp.y);    /* settings.ino */
    else if (view != VIEW_MAIN){ LOG("touch", "tap -> back to main"); showMain(); }
    else {
      const bool openingSettings = tpLeftPart;
      onGlassTap(tpLeftPart);
      if (openingSettings){ tpSettingsOpenT0 = tpT0; tpSettingsOpenX = tpX0; tpSettingsOpenY = tpY0; }
    }
  }

  if (tpDown && !tpFired && displayAwake && now - tpT0 >= HOLD_MS){
    tpFired = true;
    if (view == VIEW_HOTSPOT || (view == VIEW_MAIN && tpLeftPart)) hotspotToggle();
    else if (view == VIEW_MAIN){ LOG("touch", "hold 2 s -> force refresh"); refreshNow(); }   /* net.ino */
    else if (view == VIEW_LOCK) LOG("touch", "hold on the keypad -> nothing, a keypad only takes taps");
    else LOG("touch", "hold 2 s -> nothing here");
  }
}

/* what a hold on the glass is heading for, or NULL when nothing is */
static const char* touchHoldLabel(){
  if (view == VIEW_HOTSPOT) return "EXIT";
  if (view != VIEW_MAIN) return NULL;
  return tpLeftPart ? "SETUP" : "REFRESH";
}

/* drawn over whatever is on screen while something is on its way to 2 s */
void drawHoldOverlay(){
  const uint32_t now = millis();
  if (tpDown && !tpFired && displayAwake && now - tpT0 >= TAP_MS){
    const char* label = touchHoldLabel();
    if (label){ holdRing((now - tpT0) / (float)HOLD_MS, label); return; }
  }
  for (int i = 0; i < K_COUNT; i++){
    const Key& k = keys[i];
    if (!k.down || k.fired || now - k.t0 < TAP_MS) continue;
    if (i == K_RIGHT && !lockArmed()) continue;        /* not armed yet — genuinely future use, no ring */
    holdRing((now - k.t0) / (float)HOLD_MS, i == K_LEFT ? "OFF" : (lockIsLocked() ? "UNLOCK" : "LOCK"));
    return;
  }
}
