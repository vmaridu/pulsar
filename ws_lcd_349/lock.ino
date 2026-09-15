/* ===========================================================================
   The privacy lock — a 6-digit code that keeps parts 2 and 3 (the actual
   stats and graph) off screen until it is typed in. Part 1 — STATUS and
   ALERT — is never affected: the level, the message and the whole-screen
   flash read exactly as they do unlocked. Nor is sound, or any other key —
   this is strictly about who can read the numbers.

   HOLD RIGHT 2 s locks the screen instantly, no code needed — there is
   nothing to prove to take a screen OUT of view. Held again while already
   locked, it raises a keypad over the whole screen; six correct digits
   unlock it, a tap on the entry readout backs out without entering
   anything, and the wrong code just clears the entry and counts as a try.
   While the keypad is up the glass is a keypad and nothing else — no
   settings, no hotspot hold, no refresh.

   Armed only once BOTH of these are true:
     · real data has been shown at least once (snap.nrows > 0) — there is
       nothing to hide from a SETUP or NO DATA YET screen
     · the touch controller answered at boot (touchOK) — the keypad is typed
       on the glass, and a board that cannot type never locks itself

   Locked is the default the moment it does arm — every restart starts
   locked, on purpose, whether or not it was unlocked before the reset.

   Five wrong codes inside a rolling 30 minutes blocks further tries until
   enough of that window has passed — a count plus the epoch time of the
   most recent failure, both kept in NVS ("lock" namespace) so a restart
   cannot be used to dodge the lockout. Epoch, not millis(): the device has
   a real clock via SNTP (net.ino) once it is online. Until that clock has
   answered, a pending lockout is never assumed to have expired — only ever
   the safe direction to be wrong in. A correct code clears the count.

   THE CODE ITSELF IS NEVER LOGGED. Not the stored one, not what was typed,
   right or wrong — only that an attempt happened, and whether it was
   accepted. Configured on the setup page, same masked convention as the
   API secret. Defaults to 123456 on a board that has never set its own.

   The one deliberate exception is the keypad's own readout: the digit just
   pressed shows briefly (LOCK_REVEAL_MS) before folding into a masked dot,
   the same way a phone's PIN entry does. Never more than the one most
   recent digit, and never on Serial.

   THE KEYPAD — seven equal columns across the 640 px, the same split as
   ws_lcd_349/mockup.html: the first two hold the entry readout (six dots),
   the other five each split top / bottom into a digit — 1-5 across the
   top, 6-0 across the bottom. Ten big cells, every digit typeable.
   =========================================================================== */

#define LOCK_DIGITS       6
#define LOCK_MAX_FAILS    5
#define LOCK_FAIL_WINDOW_S  (30UL * 60UL)   /* the rolling 30 minutes, in epoch seconds */
#define LOCK_REVEAL_MS     550    /* how long the just-pressed digit shows before it masks */

static bool     locked        = true;    /* the default the moment the feature arms */
static uint32_t unlockedAt    = 0;       /* millis() of the last successful code — lockTick() */
static char     lockEntry[LOCK_DIGITS + 1] = "";
static uint8_t  lockEntryLen  = 0;
static char     lockLastDigit   = 0;     /* the one digit drawLock() is allowed to reveal */
static uint32_t lockLastDigitT0 = 0;
static uint8_t  lockFailCount = 0;       /* wrong codes since the last reset — NVS-backed */
static uint32_t lockFailAt    = 0;       /* epoch seconds of the most recent one, 0 = not yet known */

/* the seven column edges — 0, 91, 183, 274, 366, 457, 549, 640 */
static int lockColX(int i){ return (int)lround(i * (double)W / 7.0); }
static const char LOCK_KEYS[2][5] = { { '1', '2', '3', '4', '5' }, { '6', '7', '8', '9', '0' } };

bool lockArmed(){ return touchOK && snap.nrows > 0; }
bool lockIsLocked(){ return lockArmed() && locked; }

/* Loaded once at boot — the fail count and when it last grew. */
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
/* net.ino's own sentinel for "SNTP has actually answered" */
static bool lockClockOk(){ return time(nullptr) >= 1700000000L; }

static bool lockRateLimited(){
  if (lockFailCount < LOCK_MAX_FAILS) return false;
  if (!lockClockOk()) return true;                    /* can't prove the window has passed */
  if (!lockFailAt){                                    /* first trustworthy clock since these
                                                          failures — start the window now */
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

/* RIGHT held 2 s while unlocked — no code needed to take a screen out of view. */
void lockEngage(){
  if (!lockArmed() || locked) return;
  locked = true;
  if (view == VIEW_LOCK) view = VIEW_MAIN;
  LOG("lock", "locked");
}

/* RIGHT held 2 s while already locked — raises the keypad. */
void lockOpenKeypad(){
  if (!lockArmed() || !locked) return;
  lockClear();
  view = VIEW_LOCK;
  LOG("lock", "keypad opened");
}

/* a tap on the readout — leaves the keypad without entering anything */
void lockCloseKeypad(){
  if (view != VIEW_LOCK) return;
  lockClear();
  view = VIEW_MAIN;
  LOG("lock", "keypad closed - still locked");
}

static void lockSubmit(){
  if (lockRateLimited()){ lockClear(); return; }
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

static void lockDigit(char d){
  if (lockRateLimited() || lockEntryLen >= LOCK_DIGITS) return;
  lockEntry[lockEntryLen++] = d;
  lockEntry[lockEntryLen] = 0;
  lockLastDigit = d;
  lockLastDigitT0 = millis();
  /* never the digit itself — the running count is the one safe thing to say */
  LOGF("lock", "entry now %u/%u digits", (unsigned)lockEntryLen, (unsigned)LOCK_DIGITS);
  /* the 6th digit does NOT submit here — it gets its reveal first; lockTick() submits */
}

/* Auto-relock: cfg.lockTimeoutMin minutes after a successful unlock, 0 = never.
   Also where a completed 6-digit entry actually submits.                   */
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
   Landscape screen coordinates in — the same ones the dashboard draws in
   (input.ino already turned the controller's raw pair through
   PANEL_ROTATION). The readout owns columns 0-1, the digits columns 2-6,
   the top row 1-5, the bottom row 6-0. Every tap logs its point and the
   cell it resolved to.                                                   */
void lockHandleTap(int16_t x, int16_t y){
  if (lockRateLimited()){ LOG("touch", "lock: tap ignored - rate limited"); return; }
  if (x < lockColX(2)){
    LOGF("touch", "lock: tap at %d,%d - on the readout, closing the keypad", x, y);
    lockCloseKeypad();
    return;
  }
  const float cellW = (W - lockColX(2)) / 5.0f;
  int col = (int)((x - lockColX(2)) / cellW);
  if (col < 0) col = 0;
  if (col > 4) col = 4;
  const int row = y < H / 2 ? 0 : 1;
  LOGF("touch", "lock: tap at %d,%d -> row %d col %d", x, y, row, col);
  lockDigit(LOCK_KEYS[row][col]);
}

void drawLock(){
  cv->fillScreen(C_BG);
  const int ex = lockColX(0), ew = lockColX(2) - ex, ecx = ex + ew / 2;

  /* the readout — six dots centred in the first two columns: hollow for a
     digit not yet entered, filled once it is, the one just typed shown
     as itself in its own spot until its reveal window passes           */
  const int r = 7, gap = 13, dotY = H / 2;
  const int startX = ecx - (LOCK_DIGITS * 2 * r + (LOCK_DIGITS - 1) * gap) / 2 + r;
  txt("ENTER CODE", ecx, dotY - 40, 1, C_DIM, 'c', true);
  const bool revealing = lockLastDigit && millis() - lockLastDigitT0 < LOCK_REVEAL_MS;
  for (int i = 0; i < LOCK_DIGITS; i++){
    const int dx = startX + i * (2 * r + gap);
    if (i < lockEntryLen){
      if (i == lockEntryLen - 1 && revealing){
        char ds[2] = { lockLastDigit, 0 };
        txt(ds, dx, dotY - 10, 3, C_TX, 'c', true);      /* in the dot's own spot */
      } else cv->fillCircle(dx, dotY, r, C_TX);
    } else {
      cv->drawCircle(dx, dotY, r, C_LINE2);
    }
  }
  if (lockRateLimited()){
    const uint16_t mins = lockWaitMinutes();
    char v[32];
    if (mins) snprintf(v, sizeof v, "TOO MANY TRIES - %u MIN", (unsigned)mins);
    else      strlcpy(v, "TOO MANY TRIES - WAIT", sizeof v);
    txt(v, ecx, dotY + 22, 1, C_OR, 'c', true);
  } else {
    txt("TAP HERE - BACK", ecx, dotY + 22, 1, C_DIM2, 'c');
  }
  vFade(lockColX(2), 0, H, C_BG, C_LINE);

  /* the ten digit cells */
  for (int c = 0; c < 5; c++){
    const int cx = (lockColX(c + 2) + lockColX(c + 3)) / 2;
    if (c > 0) vFade(lockColX(c + 2), 0, H, C_BG, C_LINE2);
    for (int rw = 0; rw < 2; rw++){
      const int cy = rw == 0 ? H / 4 : H * 3 / 4;
      char ds[2] = { LOCK_KEYS[rw][c], 0 };
      txt(ds, cx, cy - 10, 3, lockRateLimited() ? C_DIM2 : C_TX, 'c', true);
    }
  }
  hFade(lockColX(2), H / 2, W - lockColX(2), C_BG, C_LINE2);
}

/* ------------------------------------------------------------ locked BODY
   Replaces parts 2 and 3 together — part 1 is untouched, drawn by
   drawDashboard() exactly as always. A padlock, LOCKED, and the way out.
   The shackle is a ring with its lower half tucked behind the body.    */
void drawLockedBody(const struct Theme& th){
  const int x = COL_X[1], w = COL_X[3] - COL_X[1], cx = x + w / 2;
  cv->fillRect(x, 0, w, H, th.bg);
  const uint16_t lc = th.tx;
  const int shackleR = 17, shackleTh = 7, bodyW = 50, bodyH = 36;
  const int ringCY = H / 2 - 26;

  cv->fillCircle(cx, ringCY, shackleR, lc);
  cv->fillCircle(cx, ringCY, shackleR - shackleTh, th.bg);
  cv->fillRect(cx - bodyW / 2, ringCY, bodyW, bodyH, lc);
  cv->fillCircle(cx, ringCY + bodyH * 2 / 5, 4, th.bg);
  cv->fillRect(cx - 2, ringCY + bodyH * 2 / 5, 4, bodyH * 3 / 10, th.bg);

  txt("LOCKED", cx, ringCY + bodyH + 16, 2, lc, 'c', true);
  txt("HOLD RIGHT 2 S, ENTER CODE", cx, ringCY + bodyH + 38, 1, th.dim, 'c');
}
