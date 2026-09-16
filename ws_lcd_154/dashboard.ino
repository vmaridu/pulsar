/* ===========================================================================
   The dashboard — four fixed bands. Call them by name.

       STATUS   0..20    device internals: battery, the reading's timestamp,
                         Wi-Fi, the poll hairline, the mute icon
       ALERT   20..56    info / warn / crit, and its one-line message
       BODY    56..216   the stats and the graph for the current metric
       FOOTER 216..240   gateway name + metric name, and the position rule

   A band never moves, never resizes, and never borrows another band's job.

   THE ALERT COVERS THE WHOLE SCREEN. Under warn, crit or a fetch
   fault every one of the four bands goes to the level colour for 500 ms
   every 5 s and every foreground on them turns to near-black ink — the
   device flashes, not a stripe of it. Tap through to whichever screen you
   want; the alert follows you, because it belongs to the response and not to
   any one metric.
   =========================================================================== */

/* ----------------------------------------------------------------- cycle
   snapshot the screen being left, so the next one's numbers count up from it */
void beginCount(){
  const Row& r = snap.rows[curRow];
  for (int i = 0; i < MAX_TILES; i++)
    countFromP[i] = i < r.ntiles ? r.tiles[i].value : 0;
  countT0 = millis();
}

/* the next metric screen, wrapping. Driven by the 5 s cycle in net.ino and
   by a tap on the glass.                                                  */
void nextScreen(){
  beginCount();
  curRow = snap.nrows ? (curRow + 1) % snap.nrows : 0;
  sweepT0 = millis();
}

/* a small speaker — 12 x 7. Always shown, so its shape (not just its
   presence) says what state sound is in; drawMuteCross() overlays the
   cross on top of it only while muted. */
static void drawSpeakerIcon(int x, int y, uint16_t c){
  cv->fillRect(x, y + 2, 2, 3, c);
  cv->fillTriangle(x + 2, y + 3, x + 5, y, x + 5, y + 6, c);
}
static void drawMuteCross(int x, int y, uint16_t c){
  cv->drawLine(x + 7, y + 1, x + 11, y + 5, c);
  cv->drawLine(x + 7, y + 5, x + 11, y + 1, c);
}

/* a small lightning bolt — 7 x 8, the explicit "charging" tell beside the
   battery shell. The fill going cyan is a hint; this is the actual answer. */
static void drawChargeIcon(int x, int y, uint16_t c){
  cv->fillTriangle(x + 5, y,     x,     y + 4, x + 4, y + 4, c);
  cv->fillTriangle(x + 4, y + 3, x + 6, y + 3, x + 1, y + 8, c);
}

/* a Wi-Fi fan: a dot and three arcs, lit by signal — ~10 x 8, anchored by
   its bottom-centre. Scaled down from ws_lcd_349's own drawWifiIcon(),
   same rules: not joined dims the whole fan and adds a slash through it
   (a different shape at a glance, not just a dimmer one); joined but
   walled in by a sign-in page turns the dot orange. Replaces the old
   three ascending bars, which read as a phone's signal icon, not Wi-Fi's. */
static void drawWifiIcon(int cx, int cy, const struct Theme& th){
  static const int   RADII[3] = { 2, 4, 6 };
  static float cs[31], sn[31];
  static bool  tab = false;
  if (!tab){                                  /* the 90-degree fan, once */
    for (int i = 0; i < 31; i++){ const float a = (-135 + i * 3) * PI / 180; cs[i] = cos(a); sn[i] = sin(a); }
    tab = true;
  }
  const int lit = net.connected ? (net.rssi > -55 ? 3 : net.rssi > -65 ? 2 : net.rssi > -75 ? 1 : 0) : 0;
  const uint16_t on  = th.ink ? th.tx : C_DIM;
  const uint16_t off = th.ink ? th.rule : C_DIM2;
  for (int k = 0; k < 3; k++){
    const uint16_t c = k < lit ? on : off;
    for (int i = 0; i < 31; i++)
      cv->drawPixel(cx + (int)lround(RADII[k] * cs[i]), cy + (int)lround(RADII[k] * sn[i]), c);
  }
  cv->fillCircle(cx, cy, 1, th.ink ? th.tx : (net.connected ? (net.portalBlocked ? C_OR : C_DIM) : C_DIM2));
  if (!net.connected){                        /* no link: a slash across the fan */
    const uint16_t sc = th.ink ? th.tx : C_OR;
    cv->drawLine(cx - 5, cy + 1, cx + 5, cy - 7, sc);
    cv->drawLine(cx - 4, cy + 1, cx + 6, cy - 7, sc);
  }
}

/* ================================================================== STATUS
   The device talking about itself, and nothing else. No gateway lives here —
   there is only one, and the FOOTER names it.                             */
static void drawStatus(const struct Theme& th){
  const int y = BAND_STATUS_Y, h = BAND_STATUS_H;
  cv->fillRect(0, y, 240, h, th.bg);

  /* battery shell, filled by charge; cyan while charging, plus an explicit
     bolt glyph so charging never depends on noticing a colour change. No
     number beside it any more — the shell's fill is the only readout, so
     it's quantised to 10 % steps (100, 90, 80 … 10, 0) instead of a smooth
     fill: a shell this small (11 px of fillable width) can't show a exact
     percentage anyway, so it reads as the rounded tier it actually is. */
  const int pc = batteryPercent();
  const int lvl = ((pc + 5) / 10) * 10;
  const bool onCharge = charging();
  const int bx = 9, by = y + 7;
  const uint16_t shell = th.ink ? th.rule : C_LINE2;
  cv->fillRect(bx, by, 13, 7, shell);
  cv->fillRect(bx + 1, by + 1, 11, 5, th.bg);
  cv->fillRect(bx + 1, by + 1, (11 * lvl) / 100, 5,
               th.ink ? th.tx : (onCharge ? C_CY : C_DIM));
  cv->fillRect(bx + 13, by + 2, 1, 3, shell);
  if (onCharge) drawChargeIcon(bx + 16, by - 1, th.ink ? th.tx : C_CY);

  /* the reading's clock — MM/DD hh:mm AM of measured_at, shown in the zone
     picked on the setup page (cfg.tzIndex — ws_lcd_154.ino's TZ_TABLE),
     daylight saving applied automatically by localtime_r() itself, never
     computed here. This build has no RTC, so it can't show "now" on its
     own — this is always the time of the data, not the time now. Nothing
     to do with the clock signed requests use: that's raw UTC seconds from
     time(nullptr), which localtime_r() never touches — net.ino.          */
  time_t t = (time_t)snap.measured_at;
  struct tm tmv; localtime_r(&t, &tmv);
  const int h12 = tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12;
  char clk[24]; snprintf(clk, sizeof clk, "%02d/%02d %02d:%02d %s", tmv.tm_mon + 1, tmv.tm_mday,
                         h12, tmv.tm_min, tmv.tm_hour < 12 ? "AM" : "PM");
  txt(clk, 120, y + 6, 1, th.dim, 'c');

  /* Wi-Fi — a dot and three arcs, lit by signal, same shape as ws_lcd_349's.
     cy sits at y+12 so the fan's visual centre (the arcs sit above the dot)
     lines up with the speaker icon and the battery/clock row beside it. */
  drawWifiIcon(229, y + 12, th);

  /* sound: a speaker always shown, left of the Wi-Fi fan, dim at rest; a
     brighter cross overlays it while muted, same dim-vs-lit convention the
     Wi-Fi fan uses for its own bars. */
  drawSpeakerIcon(200, y + 6, th.ink ? th.tx : C_DIM2);
  if (soundMuted) drawMuteCross(200, y + 6, th.ink ? th.tx : C_DIM);

  /* the poll hairline brightens for a moment after a forced refresh */
  cv->fillRect(0, y + h - 1, 240, 1,
               th.ink ? th.rule : (refreshT0 && millis() - refreshT0 < 420 ? C_CY : C_LINE));
}

/* A whole-character marquee — the one exception to "clients truncate, never
   scroll" (AGENTS.md §9), scoped to this one label on this one build: the
   ALERT band's detail is the only text on either device that can carry a
   genuinely long backend-supplied sentence (api.md §5, up to 20 chars) into
   a slot this narrow. Steps one character every SCROLL_STEP_MS rather than
   scrolling pixel-smooth — this font has no partial-glyph clip, so every
   draw is a run of whole characters, never a sliced one. Message + a small
   gap loops continuously; a message that already fits is just drawn once,
   still. */
#define SCROLL_STEP_MS 260
static void drawScrolling(const char* msg, int x, int w, int y, uint8_t size, uint16_t c){
  const int maxChars = w / fontFor(size).adv;
  if (maxChars <= 0) return;
  if ((int)strlen(msg) <= maxChars){ txt(msg, x, y, size, c); return; }

  char loopStr[28]; snprintf(loopStr, sizeof loopStr, "%s    ", msg);  /* gap before it repeats */
  const int loopLen = (int)strlen(loopStr);
  const int pos = (int)((millis() / SCROLL_STEP_MS) % loopLen);

  char win[24];
  const int n = maxChars < (int)sizeof(win) - 1 ? maxChars : (int)sizeof(win) - 1;
  for (int i = 0; i < n; i++) win[i] = loopStr[(pos + i) % loopLen];
  win[n] = 0;
  txt(win, x, y, size, c);
}

/* =================================================================== ALERT
   Plain background at rest, the same ground every other band sits on.
   Colour lives entirely in the level word: bold, and at full strength —
   that's the whole identification, no background tint needed beside it.
   The flash is still the whole screen's job, not this band's own resting
   look: here the colour changes only because `th` does, exactly like
   every other band.

   When the fetch layer has a fault standing, that is what this band says —
   OFFLINE, NO ACCESS, SETUP — over numbers that are being held from the last
   good poll. The payload's own `alert` is not shown then: "INFO" written
   above held numbers reads as good news, which is the one thing a fault must
   never look like. api.md §6.                                            */
static void drawAlert(const struct Level& L, const struct Theme& th){
  const int y = BAND_ALERT_Y, h = BAND_ALERT_H;
  cv->fillRect(0, y, 240, h, th.bg);

  /* word and detail sit side by side now, not stacked — a bare word alone
     left most of the band's width empty. The word is left-aligned, bold,
     and a size up from before to fill the vertical room stacking used to
     need; the detail starts right after it, baseline-matched, and takes
     whatever width is left — never wrapped; a detail too long for that
     width scrolls slowly instead of truncating, drawScrolling() above.
     A long fault word (SERVER ERROR, NO ACCESS) can leave the detail
     little or no room; that's fine, the word alone still says enough. */
  const char* word = alertWord(L);
  const uint16_t wordC = th.ink ? th.tx : L.c;
  txt(word, X_L, y + 7, 3, wordC, 'l', true);

  const int msgX = X_L + txtW(word, 3) + 10;
  const int avail = X_R - msgX;
  if (avail >= 12){                              /* room for at least one glyph */
    char msg[24]; strlcpy(msg, alertDetail(), sizeof msg);   /* api.md §5: <= 20 */
    drawScrolling(msg, msgX, avail, y + 12, 2, th.ink ? th.tx : th.dim);
  }
}

/* ==================================================================== BODY
   Full width, from just under the hero to the bottom of the band. No
   headroom: the peak lands in the gap under the hero, so a steady series runs
   between the rows of text instead of striking through them.             */
static void drawGraph(const struct Row& r, int top, int base, const struct Theme& th){
  if (r.nbuckets < 2) return;              /* no buckets, or a type this build does not draw */

  const int PH = base - top, YB = base;
  long long hi = 1;
  for (int i = 0; i < r.nbuckets; i++) if (r.buckets[i] > hi) hi = r.buckets[i];

  const float sweep = easeOut(clampf((millis() - sweepT0) / (float)SWEEP_MS, 0, 1));
  const int cols = (int)(240 * sweep + 0.5f);
  const float step = (float)PH / FADE;
  int prev = -1;

  for (int px = 0; px < cols; px++){
    int pos = px * (r.nbuckets - 1) * 256 / 239;
    int i = pos >> 8, fr = pos & 255;
    int a = r.buckets[i], b = r.buckets[min(i + 1, r.nbuckets - 1)];
    long long val = a + (((long long)(b - a) * fr) >> 8);
    int yTop = YB - (int)((val * (PH - 1) + hi / 2) / hi);

    /* fill in horizontal fade bands — a handful of fillRects per column */
    for (int k = 0; k < FADE; k++){
      int s0 = (int)lround(YB - PH + k * step), s1 = (int)lround(YB - PH + (k + 1) * step);
      int from = max(s0, yTop + 2), to = min(s1, YB + 1);
      if (to > from) cv->fillRect(px, from, 1, to - from, th.gFill[k]);
    }
    /* join to the previous column so a steep drop stays one unbroken line */
    int y0 = prev < 0 ? yTop : min(prev, yTop), y1 = prev < 0 ? yTop : max(prev, yTop);
    cv->fillRect(px, y0, 1, y1 - y0 + 1, th.gLine);
    prev = yTop;
  }
}

/* One tile, the same shape everywhere: the value on the left — its unit
   tucked in right after it, when it has one — and the name stacked to
   the right. The caller may also pass a second line under the name (the
   hero's span; body tiles never have one). Only the scale
   (valSize/unitSize/labSize/shareSize) and the Y offsets differ, passed
   in by the caller. A hot tile (0 = not hot) paints the value — and that
   second line, if given — in that colour instead of the theme's.        */
static void drawTile(int x, int right, int y, uint8_t valSize, uint8_t unitSize,
                     uint8_t labSize, uint8_t shareSize, int ySub, const char* val,
                     const char* unit, const char* lab, const char* share, uint16_t hot,
                     const struct Theme& th){
  const uint16_t vc = hot ? hot : th.tx;
  const int w = txt(val, x, y, valSize, vc, 'l', true, th.bg);
  /* the unit sits on the value's baseline, whatever the two sizes are */
  const int yUnit = y + fontFor(valSize).cap - fontFor(unitSize).cap;
  if (unit) txt(unit, x + w + 3, yUnit, unitSize, th.dim, 'l', false, th.bg);
  txt(lab, right, y, labSize, th.dim, 'r', false, th.bg);
  if (share) txt(share, right, ySub, shareSize, vc, 'r', false, th.bg);
}

/* A tile's `level` — "crit" red, "warn" orange, "info" (or
   omitted) the theme's own colour. Under a level flash every foreground
   is already near-black ink, so a tile's own colour never fights it.    */
static uint16_t levelColor(const char* level, const struct Theme& th){
  if (th.ink) return 0;
  if (!strcmp(level, "crit")) return C_RD;
  if (!strcmp(level, "warn")) return C_OR;
  return 0;
}

/* One hero, four smaller tiles over the graph — the stats and the graph
   combined, one metric at a time. `r.tiles[0]` is always the hero;
   `r.tiles[1..4]` fill the four body slots in order, left to right then
   top to bottom — a row with fewer than 5 tiles just leaves the remaining
   slots blank, never reflows them. Every tile follows the same layout as
   the hero (drawTile(), above) — only the heading draws bigger.          */
/* Nothing has ever parsed — a board that has not been told where to look, or
   one that cannot get there yet. The band stays blank of numbers on purpose:
   there is no placeholder data to show and inventing some would make this the
   most dangerous screen in the product. What it shows instead is the way
   out, since the ALERT band above has already named the fault.           */
static void drawEmptyBody(const struct Theme& th){
  const int y = BAND_BODY_Y;
  cv->fillRect(0, y, 240, BAND_BODY_H, th.bg);
  hFade(X_L, y, X_R - X_L, th.bg, th.rule);   /* soft rule, ALERT from BODY */
  txt("NO DATA YET", 120, y + 38, 2, th.dim, 'c', true);
  if (!configHasEndpoint()){
    txt("this board has no endpoint", 120, y + 68, 1, th.dim, 'c');
    txt("HOLD DOWN 2 S", 120, y + 88, 2, th.ink ? th.tx : C_CY, 'c', true);
    txt("join the wi-fi it raises, set the url", 120, y + 110, 1, th.dim, 'c');
  } else {
    char host[40]; urlHostOf(cfg.url, host, sizeof host);
    txt("waiting on", 120, y + 68, 1, th.dim, 'c');
    txt(host, 120, y + 82, 1, th.ink ? th.tx : C_TX, 'c', true);
    txt(faultDetail[0] ? faultDetail : "nothing has answered yet", 120, y + 104, 1, th.dim, 'c');
    txt("HOLD DOWN 2 S TO CHANGE IT", 120, y + 124, 1, th.dim, 'c');
  }
}

static void drawBody(const struct Theme& th){
  const int y = BAND_BODY_Y;
  if (!snap.nrows){ drawEmptyBody(th); return; }
  const Row& r = snap.rows[curRow];
  cv->fillRect(0, y, 240, BAND_BODY_H, th.bg);
  hFade(X_L, y, X_R - X_L, th.bg, th.rule);   /* soft rule, ALERT from BODY */
  drawGraph(r, y + Y_GRAPH_TOP, y + Y_BASE, th);

  /* numbers count up from the previous screen's values */
  const float t = easeOut(clampf((millis() - countT0) / 520.0f, 0, 1));
  double p[MAX_TILES];
  for (int i = 0; i < r.ntiles; i++)
    p[i] = countFromP[i] + (r.tiles[i].value - countFromP[i]) * t;
  char v[16], sh[16];

  /* hero — value size 4 (the big face), name/span size 2, the graph starts below it. The
     row's own span ("LAST 30M") sits in the tile's second line.         */
  if (r.ntiles > 0){
    const Tile& hero = r.tiles[0];
    const char* hu = fmtTileValue(p[0], hero.unit, v, sizeof v);
    spanCaption(r.size, r.count, r.unit, sh, sizeof sh);
    drawTile(X_L, X_R, y + Y_HERO, 4, 2, 2, 2, y + Y_HERO_SHARE,
             v, hu, hero.name, sh, levelColor(hero.level, th), th);
  }

  /* a hairline between the two tile columns */
  vFade(COL2 - 9, y + Y_TILE1 + 2, Y_TILE2 + 18 - Y_TILE1, th.bg, th.rule);

  static const int qx[2]     = { X_L,       COL2 };
  static const int qright[2] = { COL2 - 14, X_R  };
  static const int qy[4]     = { Y_TILE1, Y_TILE1, Y_TILE2, Y_TILE2 };
  for (int i = 1; i < MAX_TILES; i++){
    if (i >= r.ntiles) continue;              /* fewer than 5 tiles this row — leave it blank */
    const int q = i - 1;
    const Tile& tl = r.tiles[i];
    const char* unit = fmtTileValue(p[i], tl.unit, v, sizeof v);
    const uint16_t hot = levelColor(tl.level, th);
    /* The value grows as big as it can without ever touching the name
       stacked to the right, and falls back to the old, always-safe size
       the moment it doesn't — a short value (the common case) reads far
       bigger than a rare long one. The unit stays put; it's already as
       small as it needs to be. There is no second line down here at all —
       that's the hero's span caption alone.                             */
    const int colW = qright[q % 2] - qx[q % 2];
    const uint8_t valSize = strlen(v) <= 3 ? 3 : 2;
    const int valW = txtW(v, valSize);
    const uint8_t labSize = (valW + txtW(tl.name, 2) + 3 <= colW) ? 2 : 1;
    drawTile(qx[q % 2], qright[q % 2], y + qy[q], valSize, 1, labSize,
             1, y + qy[q] + 9, v, unit, tl.name, nullptr, hot, th);
  }
}

/* ================================================================== FOOTER
   Gateway name and metric name on one line, one size: the gateway's first
   five letters dim at the left edge, the metric name bright at the right
   edge. The gateway comes from the row, so a backend that merged platforms
   still says which one this came from. The rule above is the position —
   one segment per metric screen, the current one lit.                    */
static void drawFoot(const struct Theme& th){
  const int y = BAND_FOOT_Y;
  cv->fillRect(0, y, 240, BAND_FOOT_H, th.bg);
  if (!snap.nrows) return;
  const Row& r = snap.rows[curRow];

  const int gap = 4;
  const float seg = (float)(X_R - X_L - gap * (snap.nrows - 1)) / snap.nrows;
  for (int i = 0; i < snap.nrows; i++)
    cv->fillRect((int)lround(X_L + i * (seg + gap)), y, (int)lround(seg), i == curRow ? 2 : 1,
                 i == curRow ? th.tx : (th.ink ? th.rule : C_LINE2));

  /* chars that fit at size 2; the metric name gives way first */
  const int room = (X_R - X_L) / 12;
  char up[6]; strlcpy(up, r.gateway, sizeof up);          /* first five letters */
  for (int n = strlen(up); n > 0 && up[n - 1] == ' '; n--) up[n - 1] = 0;
  for (char* p = up; *p; p++) *p = toupper(*p);

  char nm[18]; strlcpy(nm, r.name, sizeof nm);
  const int fit = max(0, room - (int)strlen(up) - 1);
  if ((int)strlen(nm) > fit){
    nm[fit] = 0;
    char* sp = strrchr(nm, ' ');                        /* "Disburse P" → "Disburse" */
    if (sp && sp > nm && strlen(sp + 1) < 3) *sp = 0;
  }
  txt(up, X_L, y + 6, 2, th.dim);
  if (nm[0]) txt(nm, X_R, y + 6, 2, th.tx, 'r', true);
}

/* ------------------------------------------------------------------ toast */
/* a short message in a box over the middle of the screen, gone after 1.2 s */
void drawToast(){
  if (!toastT0 || millis() - toastT0 > 1200) return;
  const int w = txtW(toastText, 2) + 28, x = 120 - w / 2, y = 100;
  cv->fillRect(x, y, w, 40, C_BG);
  cv->drawRect(x, y, w, 40, C_LINE2);
  txt(toastText, 120, y + 12, 2, C_TX, 'c', true);
}

/* The four bands tile all 240 rows, so no fillScreen() per frame. One theme
   for all of them: during an alert flash every band is lit together, which
   is what makes the whole screen the alert. The boot intro paints this once
   too, to cross-fade into it.                                             */
void drawDashboard(){
  const Level& L = levelNow();
  const Theme& th = themeFor(L);
  drawStatus(th);
  drawAlert(L, th);
  /* the privacy lock (lock.ino) only ever hides BODY and FOOTER — STATUS and
     ALERT above are drawn exactly the same whether locked or not           */
  if (lockIsLocked()) drawLockedBody(th);
  else { drawBody(th); drawFoot(th); }
}
