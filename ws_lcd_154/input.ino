/* ===========================================================================
   Input — three keys on the case and the touch glass.

   THE STANDARD MAP. This is the whole contract; anything not in it is FUTURE
   USE, and future use is a thing we say out loud on Serial, never a key that
   silently does nothing.

     GLASS  anywhere   tap: next metric screen — or, while the dashboard is
                             showing locked, opens the unlock keypad instead
                       double-tap: settings screen on / off (works locked or not —
                                   settings is never gated by the lock)
                       hold 2 s: force a refresh
     DOWN   GPIO0      tap: future use — off the main screen, back to it
                       double-tap: settings screen on / off
                       hold 2 s: the setup hotspot — hotspot.ino
     POWER  PWR  GPIO5 tap: future use
                       double-tap: future use
                       hold 2 s: power off · from off, hold 2 s: on
     UP     GPIO4      tap: display on / off
                       double-tap: sound mute / unmute (until restart)
                       hold 2 s: lock the screen, or — already locked — open
                                 the unlock keypad. Only once armed: real
                                 data has shown at least once, and this is a
                                 touch SKU (the keypad needs the glass) —
                                 lock.ino

   DOWN and UP are physical positions, not silkscreen names — this board's
   PLUS/BOOT keys are wired the opposite of Waveshare's own labeling, so
   KEY_LEFT/KEY_RIGHT are swapped in ws_lcd_154.ino to match. Nothing here cares
   which GPIO is which; it only ever talks about DOWN and UP.

   Off the main screen, a tap on the glass goes back to it — except the lock
   keypad, which reads a tap as a digit instead; DOWN backs out of that one.
   On the main screen itself, while it's showing locked, a tap opens that
   same keypad instead of advancing the (hidden) metric cycle underneath —
   the same destination UP's own hold already reaches.

   Tap timing: a key or finger with a double-tap action waits DOUBLE_TAP_MS
   before firing its single tap, so one gesture never fires both. A key with
   no double-tap action fires its tap the moment it is released.

   While anything is held past a tap, a ring fills toward the 2 s mark with
   the action written inside, so you can see it coming and let go.

   Every press and every touch prints on Serial with the action it caused —
   this board has no other way to tell you what it thought you did.
   =========================================================================== */

#define HOLD_MS         2000     /* every hold — keys and touch */
#define TAP_MS           400     /* released sooner than this, it was a tap */
#define DEBOUNCE_MS       30
#define DOUBLE_TAP_MS    450     /* a second tap inside this is a double-tap */
#define TOUCH_LIFT_MS     60     /* a read that FAILS mid-touch is not a lift — only this long without one is */
#define TOUCH_LIFT_ZEROS   2     /* the controller answering "no finger" this many reads running IS a lift */
#define TOUCH_MOVE_PX     24     /* a finger that travels further is not a tap */

enum { K_DOWN, K_POWER, K_UP, K_COUNT };
struct Key {
  uint8_t     pin;
  const char* name;
  bool        hasDouble;         /* defer the single tap to wait for a second? */
  bool        down, fired;
  uint32_t    t0, pending;       /* pending: a tap waiting to see if a second follows */
};
static Key keys[K_COUNT] = {
  { KEY_LEFT,  "DOWN",  true,  false, false, 0, 0 },   /* double-tap opens settings */
  { KEY_POWER, "POWER", false, false, false, 0, 0 },
  { KEY_RIGHT, "UP",    true,  false, false, 0, 0 },   /* double-tap mutes */
};

/* the ring: 60 ticks round the centre filling clockwise, the action inside */
void holdRing(float p, const char* label){
  cv->fillCircle(120, 120, 60, C_BG);
  cv->drawCircle(120, 120, 60, C_LINE);
  const int lit = (int)(clampf(p, 0, 1) * 60);
  for (int i = 0; i < 60; i++){
    const float a = -PI / 2 + i * 2 * PI / 60;
    cv->fillRect(120 + (int)lround(cos(a) * 50) - 1, 120 + (int)lround(sin(a) * 50) - 1, 3, 3,
                 i < lit ? C_TX : C_LINE2);
  }
  txt(label, 120, 110, 2, C_TX, 'c', true);
  txt("HOLD", 120, 130, 1, C_DIM, 'c');
}

/* ------------------------------------------------------------------ views */
static void showMain(){
  if (view == VIEW_HOTSPOT) hotspotStop();          /* hotspot.ino — drops the AP */
  if (view != VIEW_MAIN){
    view = VIEW_MAIN;
    cycleResetTimer();                              /* net.ino — a full 5 s on this screen */
    LOG("view", "main");
  }
}

/* ------------------------------------------------------------------- keys */

/* a key already down at boot (the power key, just held to switch on) is
   ignored until it is let go                                              */
void keysBegin(){
  for (int i = 0; i < K_COUNT; i++){
    pinMode(keys[i].pin, INPUT_PULLUP);
    keys[i].down = keys[i].fired = digitalRead(keys[i].pin) == LOW;
    keys[i].pending = 0;
  }
  LOG("boot", "keys ready - DOWN double-tap settings, hold hotspot | POWER hold off | UP display, double-tap mute, hold lock/unlock | glass double-tap settings, hold refresh");
}

/* settings on, or — from any other screen — back to the main one */
static void settingsToggle(const char* who){
  if (view == VIEW_MAIN){ view = VIEW_SETTINGS; LOGF("key", "%s -> settings", who); }
  else { LOGF("key", "%s -> back to main", who); showMain(); }
}

static void onTap(int k){
  switch (k){
    case K_DOWN:
      if (view != VIEW_MAIN){ LOG("key", "DOWN tap -> back to main"); showMain(); }
      else LOG("key", "DOWN tap -> future use (double-tap for settings, hold 2 s for the setup hotspot)");
      break;
    case K_POWER:
      LOG("key", "POWER tap -> future use (hold 2 s to switch off)");
      break;
    case K_UP:
      displayToggle();                              /* power.ino — logs what it did */
      break;
  }
}

static void onDoubleTap(int k){
  switch (k){
    case K_DOWN:  settingsToggle("DOWN double-tap"); break;
    case K_POWER: LOG("key", "POWER double-tap -> future use"); break;
    case K_UP:    LOG("key", "UP double-tap -> mute"); toggleMute(); break;   /* sound.ino */
  }
}

static void onHold(int k){
  switch (k){
    case K_DOWN:
      if (view == VIEW_HOTSPOT){ LOG("key", "DOWN hold -> leaving the setup hotspot"); showMain(); }
      else { LOG("key", "DOWN hold -> setup hotspot"); view = VIEW_HOTSPOT; hotspotStart(); }
      break;
    case K_POWER:
      LOG("key", "POWER hold -> power off");
      powerOff();                                   /* power.ino */
      break;
    case K_UP:
      if (!lockArmed()){                             /* lock.ino — no data yet, or no touch to type a code with */
        LOG("key", "UP hold -> future use");
      } else if (lockIsLocked()){
        LOG("key", "UP hold -> opening the unlock keypad");
        lockOpenKeypad();                            /* lock.ino */
      } else {
        LOG("key", "UP hold -> locked");
        lockEngage();                                /* lock.ino */
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
   Anywhere on the glass, no zones. A single tap waits DOUBLE_TAP_MS to be
   sure no second one follows, then moves to the next metric screen; a
   double-tap opens settings.

   The controller is asked every few ms (idleWait in the main sketch). A
   finger is down while it keeps answering with one; it is up the moment
   it answers "none" twice running, or — reads failing outright — once
   TOUCH_LIFT_MS pass without a finger. Waiting the full 60 ms on every
   lift merged the two taps of a quick double-tap into one long touch.  */
struct TouchState { bool down, fired; uint32_t t0, seen; int16_t x0, y0, x, y, rawX, rawY; uint8_t zeros; };
static TouchState tp;
static uint32_t touchPending = 0;

void handleTouch(){
  if (!touchOK) return;
  const uint32_t now = millis();
  int16_t x[2], y[2];
  if (touch.getPoint(x, y, 1)){
    tp.seen = now; tp.zeros = 0;
    /* CST816 reports raw panel coordinates, not screen coordinates — this
       board's canvas is rotated 90 degrees right (PANEL_ROTATION 1), and
       nothing needed to know that until a tap started meaning a screen
       *position* instead of just a gesture (lock.ino's keypad). Two rounds
       of on-device testing on the keypad, empirically, not guessed:
         round 1: the column came back exactly inverted (1 -> the 3rd
                  column's digit, 2 -> the 2nd, 3 -> the 1st) — fixed by
                  swapping the axes outright instead of rotating them
         round 2: with the column now right, the row came back inverted
                  too — the header strip above the grid (dots/title, never
                  meant to be tappable) registered as the bottom row, the
                  top button row registered as the middle row, and the
                  middle row registered as the top — fixed by inverting
                  the remaining axis
       Both axes are right now; a smaller boundary issue remains between
       the middle and bottom rows specifically (lock.ino has the detail).
       "lock: tap at" (lock.ino) still logs the resolved cell on every tap,
       so if anything is still off, that log is the whole diagnosis.      */
    tp.rawX = x[0]; tp.rawY = y[0];   /* the chip's own output, before any of the transform below —
                                          logged alongside the resolved point on every touch-down,
                                          since that raw pair is what a real fix here needs, not the
                                          already-transformed one "down at" used to log alone.       */
    tp.x = y[0];
    tp.y = 239 - x[0];
  } else if (tp.zeros < TOUCH_LIFT_ZEROS) tp.zeros++;
  const bool down = tp.seen && tp.zeros < TOUCH_LIFT_ZEROS && now - tp.seen < TOUCH_LIFT_MS;

  if (down && !tp.down){
    tp.down = true; tp.fired = false; tp.t0 = now; tp.x0 = tp.x; tp.y0 = tp.y;
    LOGF("touch", "down at %d,%d (raw %d,%d)", tp.x, tp.y, tp.rawX, tp.rawY);
  } else if (!down && tp.down){
    tp.down = false;
    const bool still = abs(tp.x - tp.x0) < TOUCH_MOVE_PX && abs(tp.y - tp.y0) < TOUCH_MOVE_PX;
    if (!still){ LOG("touch", "slide - ignored (a tap must not travel)"); touchPending = 0; }
    /* The keypad reads every non-sliding release as a digit — no TAP_MS
       deadline here, unlike the rest of the glass: a person aiming at one
       button on a small grid routinely holds it past 400 ms, and there is
       no competing double-tap or hold gesture on this view to protect
       against. Dropping the deadline is what actually fixes "the keypad
       doesn't register" — it was never a missing tap, it was a tap that
       arrived a bit later than TAP_MS and got silently discarded.        */
    else if (view == VIEW_LOCK){
      LOGF("touch", "lock: tap at %d,%d", tp.x, tp.y);
      lockHandleTap(tp.x, tp.y);                  /* lock.ino — a keypad digit, not navigation */
      touchPending = 0;
    }
    else if (!tp.fired && tp.seen - tp.t0 < TAP_MS){
      if (view != VIEW_MAIN){
        LOG("touch", "tap -> back to main");
        showMain();
        touchPending = 0;
      } else if (touchPending && now - touchPending < DOUBLE_TAP_MS){
        touchPending = 0;
        view = VIEW_SETTINGS;
        LOG("touch", "double-tap -> settings");
      } else touchPending = now;
    }
  }

  if (tp.down && !tp.fired && view == VIEW_MAIN && now - tp.t0 >= HOLD_MS){
    tp.fired = true; touchPending = 0;
    refreshNow("glass held 2 s");                    /* net.ino */
  }
  if (touchPending && !tp.down && now - touchPending >= DOUBLE_TAP_MS){
    touchPending = 0;
    if (lockIsLocked()){                             /* lock.ino — showing locked, not navigating */
      LOG("touch", "tap -> opening the unlock keypad");
      lockOpenKeypad();                              /* lock.ino */
    } else {
      nextScreen();                                   /* dashboard.ino */
      cycleResetTimer();                              /* net.ino — a full 5 s on the chosen screen */
      LOGF("touch", "tap -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
           snap.rows[curRow].gateway, snap.rows[curRow].name);
    }
  }
}

/* drawn over whatever is on screen while something is on its way to 2 s.
   A key whose hold is future use shows no ring — there is nothing coming. */
void drawHoldOverlay(){
  const uint32_t now = millis();
  if (tp.down && !tp.fired && view == VIEW_MAIN && now - tp.t0 >= TAP_MS){
    holdRing((now - tp.t0) / (float)HOLD_MS, "REFRESH");
    return;
  }
  for (int i = 0; i < K_COUNT; i++){
    const Key& k = keys[i];
    if (!k.down || k.fired || now - k.t0 < TAP_MS) continue;
    const char* label;
    if (i == K_UP){
      if (!lockArmed()) continue;                   /* not armed yet — genuinely future use */
      label = lockIsLocked() ? "UNLOCK" : "LOCK";
    } else {
      label = i == K_POWER ? "OFF" : (view == VIEW_HOTSPOT ? "EXIT" : "SETUP");
    }
    holdRing((now - k.t0) / (float)HOLD_MS, label);
    return;
  }
}
