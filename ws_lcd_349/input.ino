/* ===========================================================================
   Input — two keys on the case and the touch glass.

   The keys are named by where they sit on the case, LEFT and RIGHT. LEFT is
   the key on the board's power circuit (GPIO16): it is what switches the
   board on from the cell, and on this hardware a press of it pulses the
   power rail — the chip resets before any firmware sees the key. So LEFT
   is power, and nothing else is ever put on it. RIGHT is the key on GPIO0.
   The third key on the case is RESET, wired to the chip's reset line.

   The standard map's DOWN roles (AGENTS.md §3) — settings, and the setup
   hotspot — live on the glass instead, on the left part of the dashboard
   (the STATUS / ALERT part) which is on screen whenever the dashboard is,
   locked or not. The other two parts keep the glass's own roles.

     GLASS  left part     tap: next metric screen
                          double-tap: settings screen on / off
                          hold 2 s: the setup hotspot on / off — hotspot.ino
            right parts   tap: next metric screen
                          double-tap: future use
                          hold 2 s: force a refresh — net.ino
            any screen    off the main screen, a tap goes back; in the hotspot,
                          a 2 s hold anywhere leaves it
     RIGHT  GPIO0         tap: display on / off
                          double-tap: sound mute / unmute
                          hold 2 s: lock the screen, or — already locked — open the
                                    unlock keypad. Only once armed: real data has shown
                                    and the glass can type — lock.ino
     KEYPAD (lock view)   every tap is a digit, or a tap on the readout to back out;
                          no hold does anything — the setup and refresh holds are
                          the dashboard's, not the keypad's
     LEFT   GPIO16        hold 2 s: power off · from off, hold 2 s: on
                          tap, double-tap: future use — and on this board a press resets the chip anyway

   Tap timing: an input with a double-tap action waits DOUBLE_TAP_MS before
   firing its single tap, so one gesture never fires both.

   Every press and every touch prints on Serial with the action it caused.
   =========================================================================== */

#define HOLD_MS         2000     /* every hold — keys and glass */
#define TAP_MS           400
#define DEBOUNCE_MS       30
#define DOUBLE_TAP_MS    450
#define TOUCH_LIFT_MS     60     /* a read that FAILS mid-touch is not a lift — only this long without one is */
#define TOUCH_LIFT_ZEROS   2     /* the controller answering "no finger" this many reads running IS a lift */
#define TOUCH_MOVE_PX     24     /* a finger that travels further is not a tap */

enum { K_LEFT, K_RIGHT, K_COUNT };
struct Key {
  uint8_t     pin;
  const char* name;
  bool        hasDouble;         /* defer the single tap to wait for a second? */
  bool        down, fired;
  uint32_t    t0, pending;
};
static Key keys[K_COUNT] = {
  { KEY_POWER, "LEFT",  false, false, false, 0, 0 },
  { KEY_BOOT,  "RIGHT", true,  false, false, 0, 0 },
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
    keys[i].down = keys[i].fired = digitalRead(keys[i].pin) == LOW;
    keys[i].pending = 0;
  }
  LOG("boot", "keys ready - RIGHT: display, double-tap mute, hold lock/unlock | LEFT: hold off | glass left part: double-tap settings, hold hotspot | right parts: hold refresh");
}

static void onTap(int k){
  switch (k){
    case K_LEFT:  LOG("key", "LEFT tap -> future use (hold 2 s to switch off)"); break;
    case K_RIGHT: displayToggle(); break;           /* power.ino — logs what it did */
  }
}

static void onDoubleTap(int k){
  switch (k){
    case K_LEFT:  LOG("key", "LEFT double-tap -> future use"); break;
    case K_RIGHT: LOG("key", "RIGHT double-tap -> mute"); toggleMute(); break;  /* sound.ino */
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
    const bool down = digitalRead(k.pin) == LOW;

    if (down && !k.down){
      k.down = true; k.fired = false; k.t0 = now;
      LOGF("key", "%s down (GPIO%d)", k.name, k.pin);
    } else if (!down && k.down){
      k.down = false;
      const uint32_t held = now - k.t0;
      if (!k.fired && held >= DEBOUNCE_MS && held < TAP_MS){
        if (!k.hasDouble) onTap(i);                 /* no double-tap action — fire now */
        else if (k.pending && now - k.pending < DOUBLE_TAP_MS){ k.pending = 0; onDoubleTap(i); }
        else k.pending = now;                       /* wait, a second tap may follow */
      }
      k.fired = false;
    }

    if (k.down && !k.fired && now - k.t0 >= HOLD_MS){
      k.fired = true;
      k.pending = 0;
      onHold(i);
    }
    /* no second tap came — it was a single after all */
    if (k.pending && !k.down && now - k.pending >= DOUBLE_TAP_MS){
      k.pending = 0;
      onTap(i);
    }
  }
}

/* ------------------------------------------------------------------ touch
   Two zones on the dashboard: the left part (STATUS / ALERT, x < COL_X[1])
   carries settings and the hotspot; the other two parts carry the refresh.
   A tap anywhere steps to the next metric screen; off the main screen a
   tap goes back. Every touch-down logs both the screen point and the
   controller's raw pair — the raw pair is what a calibration fix needs.  */
static TouchPoint tp;
static bool     tpDown = false, tpFired = false, tpLeftPart = false;
static uint32_t tpT0 = 0, tpSeen = 0, tpPending = 0;   /* pending: a tap waiting to see if a second follows */
static int16_t  tpX0 = 0, tpY0 = 0;
static uint8_t  tpZeros = 0;                            /* "no finger" answers in a row */

static void hotspotToggle(){
  if (view == VIEW_HOTSPOT){ LOG("touch", "hold 2 s -> leaving the setup hotspot"); showMain(); }
  else { LOG("touch", "hold 2 s on the left part -> setup hotspot"); view = VIEW_HOTSPOT; hotspotStart(); }   /* hotspot.ino */
}

/* a single glass tap, once it is certain no second one follows */
static void onGlassTap(){
  if (view != VIEW_MAIN){ LOG("touch", "tap -> back to main"); showMain(); return; }
  nextScreen();                                     /* dashboard.ino */
  cycleResetTimer();
  LOGF("touch", "tap -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
       snap.nrows ? snap.rows[curRow].gateway : "-", snap.nrows ? snap.rows[curRow].name : "-");
}
static void onGlassDoubleTap(bool leftPart){
  if (view != VIEW_MAIN){ LOG("touch", "double-tap -> back to main"); showMain(); return; }
  if (!leftPart){ LOG("touch", "double-tap -> future use"); return; }
  view = VIEW_SETTINGS;
  LOG("touch", "double-tap on the left part -> settings");
}

void handleTouch(){
  if (!touchOK) return;
  const uint32_t now = millis();
  /* The controller is asked every few ms (idleWait in the main sketch). A
     finger is down while it keeps answering with one; it is up the moment
     it answers "none" twice running, or — a read failing outright — once
     TOUCH_LIFT_MS pass without a finger. Waiting the full 60 ms on every
     lift merged the two taps of a quick double-tap into one long touch. */
  TouchPoint p;
  if (touchRead(p)){
    if (p.down){ tp = p; tpSeen = now; tpZeros = 0; }
    else if (tpZeros < TOUCH_LIFT_ZEROS) tpZeros++;
  }
  const bool down = tpSeen && tpZeros < TOUCH_LIFT_ZEROS && now - tpSeen < TOUCH_LIFT_MS;

  if (down && !tpDown){
    tpDown = true; tpFired = false; tpT0 = now; tpX0 = tp.x; tpY0 = tp.y;
    tpLeftPart = tp.x < COL_X[1];
    LOGF("touch", "down at %d,%d (raw %d,%d)", tp.x, tp.y, tp.rawX, tp.rawY);
  } else if (!down && tpDown){
    tpDown = false;
    const bool still = abs(tp.x - tpX0) < TOUCH_MOVE_PX && abs(tp.y - tpY0) < TOUCH_MOVE_PX;
    if (!still){ LOG("touch", "slide - ignored (a tap must not travel)"); tpPending = 0; }
    /* The keypad reads every non-sliding release as a digit — no TAP_MS
       deadline and no double-tap wait: a finger aiming at one cell often
       rests past 400 ms, and nothing else competes on this view.        */
    else if (view == VIEW_LOCK){ tpPending = 0; if (!tpFired) lockHandleTap(tp.x, tp.y); }
    else if (!tpFired && tpSeen - tpT0 < TAP_MS){
      if (!displayAwake) LOG("touch", "tap -> ignored, display is off (tap RIGHT to wake it)");
      else if (tpPending && now - tpPending < DOUBLE_TAP_MS){ tpPending = 0; onGlassDoubleTap(tpLeftPart); }
      else tpPending = now;                         /* wait, a second tap may follow */
    }
  }
  /* no second tap came — it was a single after all */
  if (tpPending && !tpDown && now - tpPending >= DOUBLE_TAP_MS){ tpPending = 0; onGlassTap(); }

  if (tpDown && !tpFired && displayAwake && now - tpT0 >= HOLD_MS){
    tpFired = true; tpPending = 0;
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
