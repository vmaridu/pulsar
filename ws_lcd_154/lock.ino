/* ===========================================================================
   The privacy lock — a 6-digit code that keeps BODY and FOOTER (the actual
   stats and graphs) off screen until it is typed in. STATUS and ALERT are
   never affected: the level, the message and the whole-screen flash still
   read exactly as they do unlocked. Nor is sound, or any other key — this
   is strictly about who can read the numbers, never about anything else the
   device does.

   HOLD UP 2 s locks the screen instantly, no code needed — there is nothing
   to prove to take a screen OUT of view. Held again while already locked, it
   raises a keypad instead, over the whole screen like Settings or the setup
   hotspot; six correct digits unlock it, DOWN backs out without entering
   anything, and the wrong code just clears the entry and counts as a try.

   Armed only once BOTH of these are true:
     · real data has been shown at least once (snap.nrows > 0) — there is
       nothing to hide from a SETUP or NO DATA YET screen, and locking one
       would just be a dead end
     · this is a touch SKU (touchOK) — the keypad is typed on the glass, and
       three keys with no display of their own cannot enter six digits.
       A non-touch board never locks itself: UP hold 2 s stays future use,
       same as it always has been on that SKU

   Locked is the default the moment it does arm — every restart starts
   locked, on purpose, whether or not it was unlocked before the reset.

   Five wrong codes inside a rolling 5 minutes blocks further tries until
   enough of that window has passed — a count plus the epoch time of the
   most recent failure, both kept in NVS ("lock" namespace) so a restart
   cannot be used to dodge the lockout. Epoch, not millis(): millis() means
   nothing across a reboot, and the device already has a real clock via
   SNTP (net.ino) for signed requests. Until that clock has answered, a
   pending lockout is never assumed to have expired — only ever the safe
   direction to be wrong in. A correct code clears the count outright, on
   the theory that proving you know it now is worth more than a handful of
   old mistakes.

   THE CODE ITSELF IS NEVER LOGGED. Not the stored one, not what was typed,
   right or wrong — only that an attempt happened, and whether it was
   accepted, exactly like every other secret in this codebase (config.ino's
   header explains why). Configured on the setup page, same masked
   convention as the API secret: it comes back as "set", never as itself,
   and an empty field posted back means "keep the one you have". Defaults
   to 123456 on a board that has never set its own.

   The one deliberate exception is the keypad's own dot row: the digit just
   pressed shows briefly (LOCK_REVEAL_MS) before folding into a masked dot
   like the rest, the same way a phone's PIN entry does — so a mis-tap is
   visible to whoever is looking at the *screen* right now. This never
   touches Serial, and it never shows anything but the single most recent
   digit — the code is still never reconstructable from a glance.

   Mirrors ws_lcd_154/mockup.html — same rules, same default, same timings.
   =========================================================================== */

#define LOCK_DIGITS       6
#define LOCK_MAX_FAILS    5
#define LOCK_FAIL_WINDOW_S  (5UL * 60UL)    /* the rolling 5 minutes, in epoch seconds */
#define LOCK_REVEAL_MS     550    /* how long the just-pressed digit shows before it masks */

static bool     locked        = true;    /* the default the moment the feature arms */
static uint32_t unlockedAt    = 0;       /* millis() of the last successful code — lockTick() below */
static char     lockEntry[LOCK_DIGITS + 1] = "";
static uint8_t  lockEntryLen  = 0;
static char     lockLastDigit   = 0;     /* the one digit drawLock() is allowed to reveal */
static uint32_t lockLastDigitT0 = 0;     /* millis() it was pressed — also gates the 6th
                                             digit's own submit, so it gets its reveal too */
static uint8_t  lockFailCount = 0;       /* consecutive wrong codes since the last reset — NVS-backed */
static uint32_t lockFailAt    = 0;       /* epoch seconds of the most recent one, 0 = not yet known */

/* Both conditions the header explains — nothing here locks a board with no
   data on it, or one with no way to type a code back in.                   */
bool lockArmed(){ return touchOK && snap.nrows > 0; }
bool lockIsLocked(){ return lockArmed() && locked; }

/* Loaded once at boot (ws_lcd_154.ino's setup()) — the fail count and when
   it last grew, so a restart cannot be used to dodge the lockout.         */
void lockBegin(){
  Preferences p;
  if (p.begin("lock", true)){
    lockFailCount = p.getUChar("fc", 0);
    lockFailAt    = p.getUInt("fa", 0);
    p.end();
  }
}
static void lockFailSave(){
  Preferences p;
  if (p.begin("lock", false)){
    p.putUChar("fc", lockFailCount);
    p.putUInt("fa", lockFailAt);
    p.end();
  }
}
/* net.ino's own sentinel for "SNTP has actually answered" — reused rather
   than restated, so this only ever agrees with what a signed request would
   trust. Below it, epoch seconds are a fresh boot's meaningless default,
   not a real time — a pending lockout must never look expired just because
   the clock hasn't caught up yet.                                        */
static bool lockClockOk(){ return time(nullptr) >= 1700000000L; }

static bool lockRateLimited(){
  if (lockFailCount < LOCK_MAX_FAILS) return false;
  if (!lockClockOk()) return true;                    /* can't prove the window has passed */
  if (!lockFailAt){                                    /* first trustworthy clock since these
                                                           failures — start the window now, the
                                                           safe direction to be wrong in        */
    lockFailAt = (uint32_t)time(nullptr);
    lockFailSave();
    return true;
  }
  if ((uint32_t)time(nullptr) - lockFailAt < LOCK_FAIL_WINDOW_S) return true;
  lockFailCount = 0;                                   /* the streak aged out */
  lockFailAt = 0;
  lockFailSave();
  return false;
}
/* Minutes until the streak ages out and one more try opens up — for the
   keypad's own "wait a bit" message, nothing else reads it. 0 while the
   clock itself is still the unknown, not the wait.                       */
static uint16_t lockWaitMinutes(){
  if (lockFailCount < LOCK_MAX_FAILS || !lockClockOk() || !lockFailAt) return 0;
  const uint32_t elapsed = (uint32_t)time(nullptr) - lockFailAt;
  if (elapsed >= LOCK_FAIL_WINDOW_S) return 0;
  return (uint16_t)((LOCK_FAIL_WINDOW_S - elapsed + 59) / 60);
}
static void lockRecordFail(){
  lockFailCount++;
  if (lockClockOk()) lockFailAt = (uint32_t)time(nullptr);
  lockFailSave();
  LOG("lock", "attempt rejected - wrong code");
}

static void lockClear(){ lockEntryLen = 0; lockEntry[0] = 0; }

/* UP held 2 s while unlocked — no code needed to take a screen out of view. */
void lockEngage(){
  if (!lockArmed() || locked) return;
  locked = true;
  if (view == VIEW_LOCK) view = VIEW_MAIN;
  LOG("lock", "locked");
}

/* UP held 2 s while already locked — raises the keypad. */
void lockOpenKeypad(){
  if (!lockArmed() || !locked) return;
  lockClear();
  view = VIEW_LOCK;
  LOG("lock", "keypad opened");
}

static void lockSubmit(){
  if (lockRateLimited()){                        /* shouldn't reach here — the grid is inert — belt and braces */
    lockClear();
    return;
  }
  if (!strcmp(lockEntry, cfg.lockCode)){
    locked = false;
    unlockedAt = millis();
    view = VIEW_MAIN;
    lockClear();
    if (lockFailCount){ lockFailCount = 0; lockFailAt = 0; lockFailSave(); }
    LOG("lock", "unlocked");
    showToast("UNLOCKED");
  } else {
    lockRecordFail();
    lockClear();
    showToast("WRONG CODE");
  }
}

void lockDigit(char d){
  if (lockRateLimited() || lockEntryLen >= LOCK_DIGITS) return;
  lockEntry[lockEntryLen++] = d;
  lockEntry[lockEntryLen] = 0;
  lockLastDigit = d;
  lockLastDigitT0 = millis();
  /* Never the digit itself, stored or typed — same rule as everywhere else
     in this file — but the running count is safe to show and is the one
     thing that proves taps are actually being counted at all.            */
  LOGF("lock", "entry now %u/%u digits", (unsigned)lockEntryLen, (unsigned)LOCK_DIGITS);
  /* The 6th digit does NOT submit here — it waits for its own reveal to
     have a moment on screen first, same as digits 1-5 get. lockTick()
     below is what actually submits, once LOCK_REVEAL_MS has passed.      */
}

/* Auto-relock: cfg.lockTimeoutMin minutes after a successful unlock, 0 = never.
   Checked every loop(), same shape as sound.ino's soundTick(). Also where a
   completed 6-digit entry actually submits — see lockDigit() above.       */
void lockTick(){
  if (view == VIEW_LOCK && lockEntryLen == LOCK_DIGITS
      && millis() - lockLastDigitT0 >= LOCK_REVEAL_MS){
    lockSubmit();
  }
  if (!lockArmed() || locked || !cfg.lockTimeoutMin) return;
  if (millis() - unlockedAt < (uint32_t)cfg.lockTimeoutMin * 60000UL) return;
  locked = true;
  if (view == VIEW_LOCK) view = VIEW_MAIN;
  LOGF("lock", "auto-relocked after %u minutes", (unsigned)cfg.lockTimeoutMin);
}

/* ------------------------------------------------------------------ keypad
   A 3-row, 3-column grid — digits 1-9 only, each cell a full 80 x 58, big
   enough to hit reliably with a fingertip on a 1.54" panel. The code is
   validated to 1-9 only (config.ino) specifically so this grid never needs
   a 0, a clear or a backspace key competing for the same space — a wrong
   digit just rides out to a rejected attempt; DOWN (not a grid cell) backs
   out and re-opening the keypad (lockOpenKeypad()) always starts blank, so
   that is the "undo". Auto-submits on the sixth digit.

   The touch axes needed an outright swap plus one inversion to line up
   with the canvas (input.ino's handleTouch() — confirmed on real hardware,
   not a guess: columns and the row0/row1 boundary all land correctly now).
   STILL OPEN: the row1/row2 boundary specifically reads a bit early, so
   the lower part of 4/5/6 can still register as 7/8/9 — likely the touch
   panel's real coordinate range not being exactly 0-239, an error small
   near the top of the grid and large enough to matter two rows down.
   Every tap still logs its raw point and the resolved cell, which is what
   a fix here needs: the exact (x,y) of a press that lands wrong.         */
static const char* const LOCK_KEYS[3][3] = {
  { "1", "2", "3" },
  { "4", "5", "6" },
  { "7", "8", "9" },
};
#define LOCK_GRID_Y   64
#define LOCK_CELL_W   80
#define LOCK_CELL_H   58

void lockHandleTap(int16_t x, int16_t y){
  if (lockRateLimited()){ LOG("touch", "lock: tap ignored - rate limited"); return; }
  if (y < LOCK_GRID_Y){ LOGF("touch", "lock: tap at %d,%d - above the grid, ignored", x, y); return; }
  /* Clamped on both ends, not just the top — a coordinate transform that is
     still slightly off at an edge must never index LOCK_KEYS out of bounds. */
  int row = (y - LOCK_GRID_Y) / LOCK_CELL_H; if (row < 0) row = 0; if (row > 2) row = 2;
  int col = x / LOCK_CELL_W;                 if (col < 0) col = 0; if (col > 2) col = 2;
  LOGF("touch", "lock: tap at %d,%d -> cell (%d,%d)", x, y, row, col);
  lockDigit(LOCK_KEYS[row][col][0]);
}

void drawLock(){
  cv->fillScreen(C_BG);
  txt("ENTER CODE", 120, 8, 2, C_CY, 'c', true);

  const int dotY = 34, gap = 18, w = (LOCK_DIGITS - 1) * gap;
  const bool revealing = lockLastDigit && millis() - lockLastDigitT0 < LOCK_REVEAL_MS;
  for (int i = 0; i < LOCK_DIGITS; i++){
    const int cx = 120 - w / 2 + i * gap;
    if (i < lockEntryLen){
      if (i == lockEntryLen - 1 && revealing){
        char ds[2] = { lockLastDigit, 0 };
        txt(ds, cx, dotY - 8, 2, C_TX, 'c', true);   /* replaces the dot, not squeezed into it */
      } else cv->fillCircle(cx, dotY, 5, C_TX);      /* masked, like every one before it */
    } else cv->drawCircle(cx, dotY, 5, C_LINE2);
  }
  txt("TAP DOWN - BACK", 120, 46, 1, C_DIM2, 'c');   /* the one hint, in the header —
                                                          the grid below keeps every pixel */

  if (lockRateLimited()){
    txt("TOO MANY ATTEMPTS", 120, 110, 2, C_OR, 'c', true);
    const uint16_t mins = lockWaitMinutes();
    char v[32];
    if (mins) snprintf(v, sizeof v, "try again in %u min", (unsigned)mins);
    else      strlcpy(v, "waiting on the clock", sizeof v);
    txt(v, 120, 134, 1, C_DIM, 'c');
    return;
  }

  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++){
      const int x0 = c * LOCK_CELL_W, y0 = LOCK_GRID_Y + r * LOCK_CELL_H;
      cv->drawRect(x0 + 2, y0 + 2, LOCK_CELL_W - 4, LOCK_CELL_H - 4, C_LINE);
      txt(LOCK_KEYS[r][c], x0 + LOCK_CELL_W / 2, y0 + LOCK_CELL_H / 2 - 7, 2, C_TX, 'c', true);
    }
}

/* ------------------------------------------------------------ locked BODY
   Replaces BODY and FOOTER together — STATUS and ALERT above are untouched,
   drawn by drawDashboard() exactly as always. A padlock glyph, "LOCKED", and
   the way out, in the same two-line shape drawEmptyBody() (dashboard.ino)
   already uses for "no data yet".                                          */
void drawLockedBody(const struct Theme& th){
  const int y = BAND_BODY_Y;
  cv->fillRect(0, y, 240, BAND_BODY_H + BAND_FOOT_H, th.bg);
  if (th.ink) cv->fillRect(X_L, y, X_R - X_L, 1, th.rule);

  const int cx = 120, top = y + 56;
  const uint16_t ic = th.ink ? th.tx : th.dim;
  cv->drawCircle(cx, top, 15, ic);
  cv->fillRect(cx - 15, top - 1, 30, 16, th.bg);           /* keep only the shackle's top arc */
  cv->fillRect(cx - 18, top, 36, 26, ic);                  /* the body — plain rect, no RoundRect
                                                               dependency to keep this drawable with
                                                               only the GFX primitives already used
                                                               elsewhere in this codebase           */
  cv->fillRect(cx - 3, top + 9, 6, 10, th.bg);

  txt("LOCKED", 120, y + 96, 2, th.ink ? th.tx : th.dim, 'c', true);
  txt("HOLD UP 2 S, ENTER CODE", 120, y + 120, 1, th.dim, 'c');
}
