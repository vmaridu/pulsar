/* ===========================================================================
   Input — three keys on the case and the touch glass.

   THE STANDARD MAP. This is the whole contract; anything not in it is FUTURE
   USE, and future use is a thing we say out loud on Serial, never a key that
   silently does nothing.

     GLASS  anywhere   tap: next metric screen
                       double-tap: future use
                       hold 2 s: force a refresh
     LEFT   GPIO0      tap: settings screen on / off
                       double-tap: future use
                       hold 2 s: hotspot mode (settings UI to come)
     POWER  PWR  GPIO5 tap: future use
                       double-tap: future use
                       hold 2 s: power off · from off, hold 2 s: on
     RIGHT  GPIO4      tap: display on / off
                       double-tap: sound mute / unmute (until restart)
                       hold 2 s: future use

   LEFT and RIGHT are physical positions, not silkscreen names — this board's
   PLUS/BOOT keys are wired the opposite of Waveshare's own labeling, so
   KEY_LEFT/KEY_RIGHT are swapped in ws_lcd_154.ino to match. Nothing here cares
   which GPIO is which; it only ever talks about LEFT and RIGHT.

   Off the main screen, a tap on the glass goes back to it.

   Tap timing: a key or finger with a double-tap action waits DOUBLE_TAP_MS
   before firing its single tap, so one gesture never fires both. A key with
   no double-tap action fires its tap the moment it is released — LEFT opens
   settings with no lag.

   While anything is held past a tap, a ring fills toward the 2 s mark with
   the action written inside, so you can see it coming and let go.

   Every press and every touch prints on Serial with the action it caused —
   this board has no other way to tell you what it thought you did.
   =========================================================================== */

#define HOLD_MS         2000     /* every hold — keys and touch */
#define TAP_MS           400     /* released sooner than this, it was a tap */
#define DEBOUNCE_MS       30
#define DOUBLE_TAP_MS    450     /* a second tap inside this is a double-tap */
#define TOUCH_LIFT_MS     60     /* the controller drops the odd sample mid-touch — lifted only after this */
#define TOUCH_MOVE_PX     24     /* a finger that travels further is not a tap */

enum { K_LEFT, K_POWER, K_RIGHT, K_COUNT };
struct Key {
  uint8_t     pin;
  const char* name;
  bool        hasDouble;         /* defer the single tap to wait for a second? */
  bool        down, fired;
  uint32_t    t0, pending;       /* pending: a tap waiting to see if a second follows */
};
static Key keys[K_COUNT] = {
  { KEY_LEFT,  "LEFT",  false, false, false, 0, 0 },
  { KEY_POWER, "POWER", false, false, false, 0, 0 },
  { KEY_RIGHT, "RIGHT", true,  false, false, 0, 0 },   /* double-tap mutes */
};

/* the ring: 60 ticks round the centre filling clockwise, the action inside */
static void holdRing(float p, const char* label){
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
  if (view == VIEW_HOTSPOT) stopHotspot();          /* settings.ino */
  if (view != VIEW_MAIN){
    view = VIEW_MAIN;
    cycleResetTimer();                              /* net.ino — a full 5 s on this screen */
    LOG("view", "main");
  }
}

/* ------------------------------------------------------------------- keys */

/* a key already down at boot (the power key, just held to switch on) is
   ignored until it is let go                                              */
static void keysBegin(){
  for (int i = 0; i < K_COUNT; i++){
    pinMode(keys[i].pin, INPUT_PULLUP);
    keys[i].down = keys[i].fired = digitalRead(keys[i].pin) == LOW;
    keys[i].pending = 0;
  }
  LOG("boot", "keys ready - LEFT settings | POWER future use | RIGHT display, double-tap mute");
}

static void onTap(int k){
  switch (k){
    case K_LEFT:
      if (view == VIEW_HOTSPOT) stopHotspot();
      if (view == VIEW_MAIN){ view = VIEW_SETTINGS; LOG("key", "LEFT tap -> settings"); }
      else { showMain(); LOG("key", "LEFT tap -> back to main"); }
      break;
    case K_POWER:
      LOG("key", "POWER tap -> future use (hold 2 s to switch off)");
      break;
    case K_RIGHT:
      displayToggle();                              /* power.ino — logs what it did */
      break;
  }
}

static void onDoubleTap(int k){
  switch (k){
    case K_LEFT:  LOG("key", "LEFT double-tap -> future use");  break;
    case K_POWER: LOG("key", "POWER double-tap -> future use"); break;
    case K_RIGHT: LOG("key", "RIGHT double-tap -> mute"); toggleMute(); break;   /* sound.ino */
  }
}

static void onHold(int k){
  switch (k){
    case K_LEFT:
      if (view == VIEW_HOTSPOT){ LOG("key", "LEFT hold -> leaving hotspot"); showMain(); }
      else { LOG("key", "LEFT hold -> hotspot"); view = VIEW_HOTSPOT; startHotspot(); }
      break;
    case K_POWER:
      LOG("key", "POWER hold -> power off");
      powerOff();                                   /* power.ino */
      break;
    case K_RIGHT:
      LOG("key", "RIGHT hold -> future use");
      break;
  }
}

static void handleKeys(){
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
   sure no second one follows, then moves to the next metric screen.       */
struct TouchState { bool down, fired; uint32_t t0, seen; int16_t x0, y0, x, y; };
static TouchState tp;
static uint32_t touchPending = 0;

static void handleTouch(){
  if (!touchOK) return;
  const uint32_t now = millis();
  int16_t x[2], y[2];
  if (touch.getPoint(x, y, 1)){ tp.seen = now; tp.x = x[0]; tp.y = y[0]; }
  const bool down = tp.seen && now - tp.seen < TOUCH_LIFT_MS;

  if (down && !tp.down){
    tp.down = true; tp.fired = false; tp.t0 = now; tp.x0 = tp.x; tp.y0 = tp.y;
    LOGF("touch", "down at %d,%d", tp.x, tp.y);
  } else if (!down && tp.down){
    tp.down = false;
    const bool still = abs(tp.x - tp.x0) < TOUCH_MOVE_PX && abs(tp.y - tp.y0) < TOUCH_MOVE_PX;
    if (!still){ LOG("touch", "slide - ignored (a tap must not travel)"); touchPending = 0; }
    else if (!tp.fired && tp.seen - tp.t0 < TAP_MS){
      if (view != VIEW_MAIN){
        LOG("touch", "tap -> back to main");
        showMain();
        touchPending = 0;
      } else if (touchPending && now - touchPending < DOUBLE_TAP_MS){
        touchPending = 0;
        LOG("touch", "double-tap -> future use");
      } else touchPending = now;
    }
  }

  if (tp.down && !tp.fired && view == VIEW_MAIN && now - tp.t0 >= HOLD_MS){
    tp.fired = true; touchPending = 0;
    LOG("touch", "hold 2 s -> force refresh");
    refreshNow();                                   /* net.ino */
  }
  if (touchPending && !tp.down && now - touchPending >= DOUBLE_TAP_MS){
    touchPending = 0;
    nextScreen();                                   /* dashboard.ino */
    cycleResetTimer();                              /* net.ino — a full 5 s on the chosen screen */
    LOGF("touch", "tap -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
         snap.rows[curRow].gateway, snap.rows[curRow].name);
  }
}

/* drawn over whatever is on screen while something is on its way to 2 s.
   A key whose hold is future use shows no ring — there is nothing coming. */
static void drawHoldOverlay(){
  const uint32_t now = millis();
  if (tp.down && !tp.fired && view == VIEW_MAIN && now - tp.t0 >= TAP_MS){
    holdRing((now - tp.t0) / (float)HOLD_MS, "REFRESH");
    return;
  }
  for (int i = 0; i < K_COUNT; i++){
    const Key& k = keys[i];
    if (!k.down || k.fired || now - k.t0 < TAP_MS) continue;
    if (i == K_RIGHT) continue;                     /* RIGHT hold is future use */
    const char* label = i == K_POWER ? "OFF"
                      : view == VIEW_HOTSPOT ? "EXIT" : "HOTSPOT";
    holdRing((now - k.t0) / (float)HOLD_MS, label);
    return;
  }
}
