/* ===========================================================================
   The settings screen — a tap on the left part of the dashboard. Three
   parts, the same split the dashboard itself uses: part 1 is CONTROLS
   (interactive — the only part of this screen that is), parts 2 and 3
   are read-only device info, same job the old single "DEVICE" part did,
   now split across two columns since part 1 no longer carries any of it.
   Tap anywhere outside the controls to go back. It holds the 5 s cycle
   still while it's up.

   Read-only is the point for parts 2/3: this is the screen you look at
   when the device isn't showing what you expected, and every line
   answers a question you'd actually ask. Editing happens over the setup
   hotspot — hold the left part 2 s — because a strip of glass is a bad
   place to type a URL. CONTROLS is the one deliberate exception: toggles
   that are safe to flip from here — sound on/off, and lock/unlock, so
   far. Locking still needs no code, the same as it never has; unlocking
   still opens the real keypad rather than skipping it.

   Mirrors ws_lcd_349/mockup.html.
   =========================================================================== */

/* one label + value on a single compact line, value at a fixed offset
   after the label — dense enough to fit close to twice as many rows as
   the old two-line layout, now that part 1 is controls, not info.      */
#define ROW_PITCH_C 17
#define ROW_Y0_C    44
#define ROW_VALUE_X 62
static void settingRow(int x, int w, int row, const char* label, const char* value, uint16_t c){
  const int y = ROW_Y0_C + row * ROW_PITCH_C;
  txt(label, x, y, 1, C_DIM);
  txt(value, x + ROW_VALUE_X, y, 1, c, 'l', true);
}

/* A response can mix rows from more than one platform (api.md §4) — join
   the distinct `gateway` names actually present, in first-seen order.   */
static void joinGateways(char* out, size_t cap){
  out[0] = 0;
  for (int i = 0; i < snap.nrows; i++){
    const char* g = snap.rows[i].gateway;
    if (strstr(out, g)) continue;
    char tmp[48];
    snprintf(tmp, sizeof tmp, "%s%s", out[0] ? ", " : "", g);
    strlcat(out, tmp, cap);
  }
  if (!out[0]) strlcpy(out, "-", cap);
}

/* a part's heading: the word, and a rule under it */
static void settingHead(int x, int w, const char* title){
  txt(title, x, 26, 1, C_DIM2, 'l', true);
  hFade(x, 38, w - 24, C_BG, C_LINE);
}

/* ================================================================ CONTROLS
   Part 1 — the only interactive part of this screen. Two so far: sound
   on/off, and lock/unlock, each its own icon and its own tappable
   region, stacked top and bottom. Sound moved here off a RIGHT
   double-tap — see input.ino's own header comment for why. Room for
   more to join below; settingsHandleTap() is where a new one's hit
   test would go.                                                       */
#define CTRL_SOUND_Y0  40   /* the sound icon's tappable region — */
#define CTRL_SOUND_Y1  92   /* below the heading */
#define CTRL_LOCK_Y0   92   /* the lock icon's, stacked right under it — */
#define CTRL_LOCK_Y1  144   /* above the back-hint line */

/* the speaker glyph — same shape as STATUS's small mute icon (dashboard.ino),
   drawn bigger since this one is meant to be tapped, with the cross only
   when muted (STATUS's own copy is only ever drawn crossed, since it only
   shows up at all while muted).                                         */
static void drawSpeakerIcon(int x, int y, int s, bool muted, uint16_t c){
  cv->fillRect(x, y + 2 * s, 2 * s, 3 * s, c);
  cv->fillTriangle(x + 2 * s, y + 3 * s, x + 5 * s, y, x + 5 * s, y + 6 * s, c);
  if (muted){
    cv->drawLine(x + 7 * s, y + s, x + 12 * s, y + 6 * s, c);
    cv->drawLine(x + 7 * s, y + 6 * s, x + 12 * s, y + s, c);
  }
}
/* a small padlock — same shape as dashboard.ino's drawLockedBody(), the
   one drawn full-size behind BODY/FOOTER while locked, scaled down to
   fit a tappable control icon instead. One glyph either way; only the
   colour and the word beside it say which state it's in.                */
static void drawLockIcon(int cx, int cy, uint16_t c, uint16_t bg){
  const int shackleR = 9, shackleTh = 4, bodyW = 26, bodyH = 19;
  cv->fillCircle(cx, cy, shackleR, c);
  cv->fillCircle(cx, cy, shackleR - shackleTh, bg);
  cv->fillRect(cx - bodyW / 2, cy, bodyW, bodyH, c);
  cv->fillCircle(cx, cy + bodyH * 2 / 5, 2, bg);
  cv->fillRect(cx - 1, cy + bodyH * 2 / 5, 2, bodyH * 3 / 10, bg);
}
static void drawSettingsControls(){
  const int x = COL_X[0] + 14, w = COL_X[1] - COL_X[0];
  settingHead(x, w, "CONTROLS");
  const int cx = COL_X[0] + (COL_X[1] - COL_X[0]) / 2;

  const uint16_t sc = soundMuted ? C_OR : C_TX;         /* sound.ino */
  drawSpeakerIcon(cx - 15, 48, 2, soundMuted, sc);
  txt(soundMuted ? "SOUND OFF" : "SOUND ON", cx, 74, 2, sc, 'c', true);

  if (!lockArmed()){                                    /* lock.ino — no data yet, or no touch to type a code with */
    drawLockIcon(cx, 104, C_LINE2, C_BG);
    txt("LOCK: FUTURE USE", cx, 128, 1, C_DIM2, 'c');
  } else {
    const bool locked = lockIsLocked();
    const uint16_t lc = locked ? C_OR : C_TX;
    drawLockIcon(cx, 104, lc, C_BG);
    txt(locked ? "LOCKED" : "UNLOCKED", cx, 128, 2, lc, 'c', true);
  }
}

/* Called from input.ino's handleTouch() for every confirmed tap while
   VIEW_SETTINGS is up. A hit inside a control's own region acts on
   that control; anywhere else is the same "tap to go back" every other
   secondary view already has. Locking needs no code, same as the RIGHT
   hold does it — only unlocking does, and that opens the same keypad
   the RIGHT hold would, never a shortcut around it.                    */
void settingsHandleTap(int16_t x, int16_t y){
  if (x < COL_X[1] && y >= CTRL_SOUND_Y0 && y < CTRL_SOUND_Y1){
    toggleMute();                                      /* sound.ino */
    LOG("touch", "tap on the sound icon -> mute toggled");
    return;
  }
  if (x < COL_X[1] && y >= CTRL_LOCK_Y0 && y < CTRL_LOCK_Y1){
    if (!lockArmed()){
      LOG("touch", "tap on the lock icon -> future use (the lock arms once real data has shown)");
    } else if (lockIsLocked()){
      LOG("touch", "tap on the lock icon -> opening the unlock keypad");
      lockOpenKeypad();                                 /* lock.ino */
    } else {
      LOG("touch", "tap on the lock icon -> locked");
      lockEngage();                                     /* lock.ino */
    }
    return;
  }
  LOG("touch", "tap -> back to main");
  showMain();                                          /* input.ino */
}

void drawSettings(){
  cv->fillScreen(C_BG);
  txt("SETTINGS", W / 2, 4, 2, C_CY, 'c', true);
  hFade(W / 2 - 80, 21, 160, C_BG, C_LINE);
  char v[48];

  drawSettingsControls();
  vFade(COL_X[1], 22, H - 44, C_BG, C_LINE);

  /* part 2 — DEVICE / WI-FI: what am I holding, what did it join. A
     network joined but walled in by a sign-in page is its own state
     and says so.                                                       */
  int x = COL_X[1] + 14, w = COL_X[2] - COL_X[1];
  settingHead(x, w, "DEVICE / WI-FI");
  snprintf(v, sizeof v, "%d%% %s", batteryPercent(), charging() ? "CHARGING" : "ON BATTERY");
  settingRow(x, w, 0, "BATTERY", v, C_TX);
  settingRow(x, w, 1, "NAME", deviceName, C_TX);
  snprintf(v, sizeof v, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  settingRow(x, w, 2, "MAC", v, C_TX);               /* the address a network's allowlist wants */
  if (net.connected && net.portalBlocked){
    snprintf(v, sizeof v, "%s SIGN-IN", net.ssid);
    settingRow(x, w, 3, "NETWORK", v, C_OR);
  } else if (net.connected){
    settingRow(x, w, 3, "NETWORK", net.ssid, C_TX);
  } else {
    settingRow(x, w, 3, "NETWORK", cfg.nnets ? wifiStateName(wifiState) : "none saved", cfg.nnets ? C_DIM : C_OR);
  }
  if (net.connected) snprintf(v, sizeof v, "%d dBm  %s", net.rssi, net.ip); else strlcpy(v, "-", sizeof v);
  settingRow(x, w, 4, "SIGNAL/IP", v, C_TX);

  /* part 3 — BACKEND / DATA: where is it pointing, what came back. The
     dot by the heading is the current level: the response's verdict,
     or grey while a fault is standing.                                 */
  x = COL_X[2] + 14; w = COL_X[3] - COL_X[2];
  vFade(COL_X[2], 22, H - 44, C_BG, C_LINE);
  settingHead(x, w, "BACKEND / DATA");
  cv->fillRect(x + 108, 27, 5, 5, levelNow().c);
  char host[40]; urlHostOf(cfg.url, host, sizeof host);
  settingRow(x, w, 0, "HOST", host, cfg.url[0] ? C_TX : C_OR);
  /* TLS with no pasted root is encrypted but unauthenticated — said, not implied */
  const bool https = !strncasecmp(cfg.url, "https://", 8);
  snprintf(v, sizeof v, "%s / %s",
           !cfg.key[0] ? "no key" : configSigned() ? "signed" : "bearer",
           !https ? "NO TLS" : configTlsVerified() ? "tls ok" : "TLS UNVERIFIED");
  settingRow(x, w, 1, "AUTH", v, (https && configTlsVerified() && cfg.key[0]) ? C_TX : C_OR);
  char gwList[48]; joinGateways(gwList, sizeof gwList);
  settingRow(x, w, 2, "GATEWAYS", gwList, C_TX);
  if (snap.nrows){
    time_t t = (time_t)snap.measured_at;
    struct tm tmv; localtime_r(&t, &tmv);
    const int h12 = tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12;
    snprintf(v, sizeof v, "%02d/%02d %02d:%02d %s", tmv.tm_mon + 1, tmv.tm_mday, h12, tmv.tm_min,
             tmv.tm_hour < 12 ? "AM" : "PM");
    settingRow(x, w, 3, "MEASURED", v, C_TX);
    char age[16]; ageText(millis() - snap.polledAt, age, sizeof age);
    snprintf(v, sizeof v, "%u screens, %s", (unsigned)snap.nrows, age);
    settingRow(x, w, 4, "LAST POLL", v, C_TX);
  } else {
    settingRow(x, w, 3, "MEASURED", "-", C_DIM);
    settingRow(x, w, 4, "LAST POLL", faultWord[0] ? faultWord : "nothing yet", C_DIM);
  }
  snprintf(v, sizeof v, "every %lu s", (unsigned long)(pollIntervalMs() / 1000));
  settingRow(x, w, 5, "POLLS", v, C_TX);

  /* the way out, on a line of its own under everything */
  hFade(W / 2 - 200, 150, 400, C_BG, C_LINE);
  txt("TAP AN ICON TO TOGGLE IT  ·  TAP ELSEWHERE TO GO BACK", W / 2, 156, 1, C_DIM2, 'c');
}
