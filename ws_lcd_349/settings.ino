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
   place to type a URL. CONTROLS is the one deliberate exception: quick
   adjustments that are safe to make from here — sound on/off, lock/unlock,
   and a brightness nudge, BRIGHTNESS_STEP_PCT at a time between
   BRIGHTNESS_MIN and BRIGHTNESS_MAX. Locking still needs no code, the
   same as it never has; unlocking still opens the real keypad rather
   than skipping it. The brightness nudge is RAM only, the same rule
   mute already follows — gone the moment the board restarts, and never
   written back to the setup page's own stored figure → power.ino's
   brightnessNudge().

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
   Part 1 — the only interactive part of this screen, a 2x2 grid: sound
   top-left, lock top-right, brightness minus/plus across the bottom row.
   Every cell is the same size on purpose — the sound icon used to draw
   noticeably smaller than the lock icon beside it, despite a comment
   claiming otherwise. Row content sits near the top and bottom of its
   own half rather than centred on the row boundary, so a visible gap
   opens up between the two rows without shrinking either one's tap
   target. Sound moved here off a RIGHT double-tap; brightness is new —
   see input.ino's own header comment and power.ino's brightnessNudge()
   for why each exists.                                                 */
#define CTRL_ROW1_Y0   40   /* sound / lock — below the heading */
#define CTRL_ROW1_Y1   94
#define CTRL_ROW2_Y0   94   /* brightness minus / plus */
#define CTRL_ROW2_Y1  148   /* above the back-hint line */
#define CTRL_MID_X   (COL_X[0] + (COL_X[1] - COL_X[0]) / 2)

/* the speaker glyph — same shape as dashboard.ino's own mute icon. Muted
   adds the same cross that icon draws, right onto this one, instead of
   a separate ON/OFF word — the glyph alone says the state, colour says
   it twice (lime on, red off, the same info/crit palette every level
   colour already uses).                                                */
static void drawSpeakerIcon(int x, int y, uint16_t c, bool muted){
  cv->fillRect(x, y + 4, 4, 6, c);
  cv->fillTriangle(x + 4, y + 6, x + 10, y, x + 10, y + 12, c);
  if (muted){
    cv->drawLine(x + 14, y + 2, x + 22, y + 10, c);
    cv->drawLine(x + 14, y + 10, x + 22, y + 2, c);
  }
}
/* a small padlock, closed or open — two different silhouettes, not one
   glyph recoloured. Closed: the shackle sits centred over the body,
   same shape as the big one drawn behind a locked BODY/FOOTER. Open: the
   shackle swings up and right, its lower-left quarter erased, so it
   reads as unlatched at a glance, not just a different colour. Locked
   is lime, unlocked is red — same info/crit palette as sound. Not yet
   armed skips the colour rule entirely and stays dim grey, since that's
   a third, unavailable state, not a point on the locked/unlocked scale. */
static void drawLockIcon(int cx, int cy, bool locked, uint16_t c, uint16_t bg){
  const int shackleR = 6, shackleTh = 2, bodyW = 15, bodyH = 11;
  const int sx = locked ? cx : cx + 4;
  const int sy = locked ? cy : cy - 3;
  cv->fillCircle(sx, sy, shackleR, c);
  cv->fillCircle(sx, sy, shackleR - shackleTh, bg);
  if (!locked) cv->fillRect(sx - shackleR - 1, sy, shackleR + 1, shackleR + 1, bg);
  cv->fillRect(cx - bodyW / 2, cy, bodyW, bodyH, c);
  cv->fillCircle(cx, cy + bodyH * 2 / 5, 2, bg);
  cv->fillRect(cx - 1, cy + bodyH * 2 / 5, 2, bodyH * 3 / 10, bg);
}
/* a sun with the direction embedded right in the disc — a "-" or "+"
   cut into its centre, background-coloured, instead of a separate
   character floating beside it. Smaller and dimmer-looking on the
   minus side, bigger on the plus side, so size alone hints at the
   direction before anyone reads the cutout. Dims to C_LINE2 once a tap
   there would no longer move the value, at either end.                 */
static void drawSunIcon(int cx, int cy, int r, bool plus, uint16_t c, uint16_t bg){
  cv->fillCircle(cx, cy, r, c);
  const int rayIn = r + 2, rayOut = r + (plus ? 5 : 4);
  for (int i = 0; i < 8; i++){
    const float a = i * PI / 4.0f;
    cv->drawLine(cx + (int)lround(cos(a) * rayIn),  cy + (int)lround(sin(a) * rayIn),
                 cx + (int)lround(cos(a) * rayOut), cy + (int)lround(sin(a) * rayOut), c);
  }
  cv->fillRect(cx - r + 1, cy - 1, 2 * r - 2, 2, bg);
  if (plus) cv->fillRect(cx - 1, cy - r + 1, 2, 2 * r - 2, bg);
}
static void drawSettingsControls(){
  const int x = COL_X[0] + 14, w = COL_X[1] - COL_X[0];
  settingHead(x, w, "CONTROLS");
  const int cxL = COL_X[0] + (CTRL_MID_X - COL_X[0]) / 2;
  const int cxR = CTRL_MID_X + (COL_X[1] - CTRL_MID_X) / 2;

  /* sound — top-left, icon high in the row, hugging the heading */
  const int cy1 = 54;
  const uint16_t sc = soundMuted ? C_RD : C_GR;         /* sound.ino */
  drawSpeakerIcon(cxL - 11, cy1 - 6, sc, soundMuted);

  /* lock — top-right, same row */
  if (!lockArmed()){                                    /* lock.ino — no data yet, or no touch to type a code with */
    drawLockIcon(cxR, cy1, true, C_LINE2, C_BG);
    txt("FUTURE USE", cxR, cy1 + 20, 1, C_DIM2, 'c');
  } else {
    const bool locked = lockIsLocked();
    const uint16_t lc = locked ? C_GR : C_RD;
    drawLockIcon(cxR, cy1, locked, lc, C_BG);
  }

  /* brightness — bottom row, icons low, hugging the back-hint line.
     Temporary only: power.ino's own liveBrightnessPct, never the
     stored config → see brightnessNudge().                             */
  const int cy2 = 132;
  const uint16_t mc = liveBrightnessPct <= BRIGHTNESS_MIN ? C_LINE2 : C_TX;
  const uint16_t pc = liveBrightnessPct >= BRIGHTNESS_MAX ? C_LINE2 : C_TX;
  drawSunIcon(cxL, cy2, 4, false, mc, C_BG);
  drawSunIcon(cxR, cy2, 7, true, pc, C_BG);
  char v[8]; snprintf(v, sizeof v, "%d%%", liveBrightnessPct);
  txt(v, CTRL_MID_X, cy2 - 17, 1, C_DIM, 'c');
}

/* Called from input.ino's handleTouch() for every confirmed tap while
   VIEW_SETTINGS is up. A hit inside a control's own cell acts on that
   control; anywhere else is the same "tap to go back" every other
   secondary view already has. Locking needs no code, same as the RIGHT
   hold does it — only unlocking does, and that opens the same keypad
   the RIGHT hold would, never a shortcut around it. Brightness is a
   temporary nudge, same "gone on restart" rule sound already follows
   → power.ino's brightnessNudge().                                     */
void settingsHandleTap(int16_t x, int16_t y){
  if (x >= COL_X[1]){ LOG("touch", "tap -> back to main"); showMain(); return; }   /* input.ino */
  if (y >= CTRL_ROW1_Y0 && y < CTRL_ROW1_Y1){
    if (x < CTRL_MID_X){
      toggleMute();                                      /* sound.ino */
      LOG("touch", "tap on the sound icon -> mute toggled");
    } else if (!lockArmed()){
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
  if (y >= CTRL_ROW2_Y0 && y < CTRL_ROW2_Y1){
    brightnessNudge(x < CTRL_MID_X ? -BRIGHTNESS_STEP_PCT : BRIGHTNESS_STEP_PCT);   /* power.ino */
    LOGF("touch", "tap on the brightness %s icon -> %d%%", x < CTRL_MID_X ? "-" : "+", liveBrightnessPct);
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
  txt("TAP AN ICON TO USE IT  ·  TAP ELSEWHERE TO GO BACK", W / 2, 156, 1, C_DIM2, 'c');
}
