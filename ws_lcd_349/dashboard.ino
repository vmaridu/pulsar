/* ===========================================================================
   The dashboard — the four bands folded into three parts across 640 px.

       part 1   x   0..212   STATUS on top: battery, Wi-Fi, mute icon.
                             ALERT beneath: the level word, its message,
                             and the reading's clock
       part 2   x 213..426   BODY: the heading tile over the graph
       part 3   x 427..639   BODY's other four tiles, name left / value
                             right, one per row — and the FOOTER's job:
                             gateway + metric name across the top, the
                             position rule along the bottom

   Three columns of equal width, the same soft fade for every divider, and
   nothing boxed off: each part reads as one calm block. Colour lives in
   the level word alone at rest.

   THE ALERT COVERS THE WHOLE SCREEN. Under warn, crit or a fetch
   fault every part goes to the level colour for 500 ms every 5 s and every
   foreground on it turns to near-black ink — the device flashes, not a
   stripe of it. That happens through `th`, never a fill any one part
   hardcodes.

   Mirrors ws_lcd_349/mockup.html — the same three parts, the same numbers.
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

/* ------------------------------------------------------------------ icons */
/* a speaker with a cross — 12 x 7 */
static void drawMuteIcon(int x, int y, uint16_t c){
  cv->fillRect(x, y + 2, 2, 3, c);
  cv->fillTriangle(x + 2, y + 3, x + 5, y, x + 5, y + 6, c);
  cv->drawLine(x + 7, y + 1, x + 11, y + 5, c);
  cv->drawLine(x + 7, y + 5, x + 11, y + 1, c);
}
/* a small lightning bolt — 7 x 8, beside the battery while charging */
static void drawChargeIcon(int x, int y, uint16_t c){
  cv->fillTriangle(x + 5, y,     x,     y + 4, x + 4, y + 4, c);
  cv->fillTriangle(x + 4, y + 3, x + 6, y + 3, x + 1, y + 8, c);
}
/* battery: a 26 x 14 shell, filled by charge. No number beside it — the
   shell's fill is the only readout, quantised to 10 % steps (100, 90,
   80 … 10, 0) instead of a smooth fill, same convention as ws_lcd_154's
   own battery icon. */
static void drawBattery(int x, int y, const struct Theme& th){
  const int bw = 26, bh = 14, pc = batteryPercent();
  const int lvl = ((pc + 5) / 10) * 10;
  const bool onCharge = charging();
  const uint16_t shell = th.ink ? th.rule : C_LINE2;
  cv->fillRect(x, y, bw, bh, shell);
  cv->fillRect(x + 2, y + 2, bw - 4, bh - 4, th.bg);
  int fill = (bw - 4) * lvl / 100; if (fill < 2) fill = 2;
  cv->fillRect(x + 2, y + 2, fill, bh - 4, th.ink ? th.tx : (onCharge ? C_CY : C_DIM));
  cv->fillRect(x + bw, y + bh / 2 - 2, 2, 4, shell);
  if (onCharge) drawChargeIcon(x + bw + 3, y + 3, th.ink ? th.tx : C_CY);
}
/* a Wi-Fi fan: a dot and three arcs, lit by signal — 24 x 15, anchored by
   its bottom-centre. Not joined: the whole fan dim with a slash through
   it, a different shape at a glance, not just a dimmer one. Joined but
   walled in by a sign-in page: the dot goes orange.                     */
static void drawWifiIcon(int cx, int cy, const struct Theme& th){
  static const int   RADII[3] = { 5, 9, 13 };
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
    for (int i = 0; i < 31; i++){
      cv->drawPixel(cx + (int)lround(RADII[k] * cs[i]), cy + (int)lround(RADII[k] * sn[i]), c);
      cv->drawPixel(cx + (int)lround((RADII[k] + 1) * cs[i]), cy + (int)lround((RADII[k] + 1) * sn[i]), c);
    }
  }
  cv->fillCircle(cx, cy, 1, th.ink ? th.tx : (net.connected ? (net.portalBlocked ? C_OR : C_DIM) : C_DIM2));
  if (!net.connected){                        /* no link: a slash across the fan */
    const uint16_t sc = th.ink ? th.tx : C_OR;
    cv->drawLine(cx - 9, cy + 1, cx + 9, cy - 15, sc);
    cv->drawLine(cx - 8, cy + 1, cx + 10, cy - 15, sc);
  }
}

/* A small glyph beside the level word — a vector shape, not a literal
   Unicode emoji: ProFont is a monochrome bitmap font with no colour-emoji
   glyphs in it, so cv->print() has nothing to draw for one and would just
   show a blank box. A distinct silhouette per level does the same job at
   a glance, same drawing style as the icons above. Escalating shape reads
   the same way the words themselves escalate: a plain dot, a caution
   triangle, a diamond for the one that sounds. Radius is fixed — the
   glyph never resizes between levels, only INFO_LEVEL/WARN_LEVEL/etc pick
   which shape draws inside that same radius.                            */
#define LI_INFO  0
#define LI_WARN  1
#define LI_CRIT  2
#define LI_FAULT 3
/* a plain int, not an enum: the Arduino prototype scanner hoists a
   generated declaration for this function above this very definition,
   and an enum type it hasn't seen yet there fails to compile — see
   AGENTS.md's note on this project not relying on that scanner.       */
static void drawLevelIcon(int cx, int cy, int r, int shape, uint16_t c){
  switch (shape){
    case LI_INFO:                              /* a plain dot */
      cv->fillCircle(cx, cy, r, c);
      break;
    case LI_WARN:                              /* a caution triangle */
      cv->fillTriangle(cx, cy - r, cx - r, cy + r - 1, cx + r, cy + r - 1, c);
      break;
    case LI_CRIT:                              /* a diamond — the loudest silhouette */
      cv->fillTriangle(cx - r, cy, cx, cy - r, cx + r, cy, c);
      cv->fillTriangle(cx - r, cy, cx, cy + r, cx + r, cy, c);
      break;
    case LI_FAULT:                             /* a slashed ring — no signal, same
                                                   language as drawWifiIcon()'s own slash */
      cv->drawCircle(cx, cy, r, c);
      cv->drawCircle(cx, cy, r - 1, c);
      cv->drawLine(cx - r, cy + r, cx + r, cy - r, c);
      cv->drawLine(cx - r, cy + r - 1, cx + r, cy - r - 1, c);
      break;
  }
}

/* A tile's `level` — "crit" red, "warn" orange, "info" (or
   omitted) the theme's own colour. Under a flash every foreground is
   already ink, so a tile's own colour never fights it.                  */
static uint16_t levelColor(const char* level, const struct Theme& th){
  if (th.ink) return 0;
  if (!strcmp(level, "crit")) return C_RD;
  if (!strcmp(level, "warn")) return C_OR;
  return 0;
}

/* The message's two lines: everything up to MSG_CHARS on the first, split
   at the last space that keeps it inside; the rest on the second, cut at
   MSG_CHARS. A message that fits stays on one line and l2 comes back
   empty. api.md caps the message at 20, so the second line is short.  */
static void splitMessage(const char* s, char* l1, char* l2){
  const int n = (int)strlen(s);
  if (n <= MSG_CHARS){ strlcpy(l1, s, MSG_CHARS + 1); l2[0] = 0; return; }
  int cut = -1;
  for (int i = MSG_CHARS; i > 0; i--) if (s[i] == ' '){ cut = i; break; }
  if (cut < 1) cut = MSG_CHARS;                    /* one long word — split it */
  memcpy(l1, s, cut); l1[cut] = 0;
  const char* rest = s + cut;
  while (*rest == ' ') rest++;
  strlcpy(l2, rest, MSG_CHARS + 1);
}

/* ================================================================= PART 1
   STATUS across the top — the device talking about itself — and ALERT
   filling the rest: the level word loud and coloured, the message bold
   under it, the reading's clock at the foot. All centred: one calm block
   in a column with room to spare, not a stack pinned to the left edge. */
static void drawPart1(const struct Level& L, const struct Theme& th){
  const int x = COL_X[0], w = COL_X[1] - COL_X[0], cx = x + w / 2;
  cv->fillRect(x, 0, w, H, th.bg);

  drawBattery(x + 12, 6, th);
  drawWifiIcon(x + w - 22, 20, th);
  if (soundMuted) drawMuteIcon(x + w - 52, 6, th.ink ? th.tx : C_DIM);
  /* the hairline under STATUS brightens for a moment after a forced refresh */
  if (!th.ink && refreshT0 && millis() - refreshT0 < 420) cv->fillRect(x + 10, TOP_BAR_H, w - 20, 1, C_CY);
  else hFade(x + 10, TOP_BAR_H, w - 20, th.bg, th.ink ? th.rule : C_LINE);

  const uint16_t wordC = th.ink ? th.tx : L.c;
  const char* word = alertWord(L);
  const int shape = &L == &LV_CRIT ? LI_CRIT : &L == &LV_WARN ? LI_WARN :
                     &L == &LV_FAULT ? LI_FAULT : LI_INFO;
  const int iconR = 9, gap = 10;
  const int wordW = (int)strlen(word) * fontFor(3).adv;
  const int startX = cx - (iconR * 2 + gap + wordW) / 2;
  drawLevelIcon(startX + iconR, TOP_BAR_H + 24 + fontFor(3).cap / 2, iconR, shape, wordC);
  txt(word, startX + iconR * 2 + gap, TOP_BAR_H + 24, 3, wordC, 'l', true);

  /* the message in its own size, bright and bold under the word — one
     line when it fits, else two, broken at a space                      */
  char msg[24]; strlcpy(msg, alertDetail(), 21);          /* api.md §5: <= 20 */
  char l1[MSG_CHARS + 1], l2[MSG_CHARS + 1];
  splitMessage(msg, l1, l2);
  if (l2[0]){ txtMsg(l1, cx, 76, th.tx, 'c', true); txtMsg(l2, cx, 95, th.tx, 'c', true); }
  else txtMsg(l1, cx, 85, th.tx, 'c', true);
  /* a fault standing over numbers from an earlier poll says so */
  if (faultWord[0] && snap.nrows) txt("HELD", cx, 118, 1, wordC, 'c', true);

  /* the reading's clock — MM/DD hh:mm AM of measured_at, in the display
     zone. The time of the data, not the time now.                       */
  time_t t = (time_t)snap.measured_at;
  struct tm tmv; localtime_r(&t, &tmv);
  const int h12 = tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12;
  char clk[40]; snprintf(clk, sizeof clk, "%02d/%02d %02d:%02d %s", tmv.tm_mon + 1, tmv.tm_mday,
                         h12, tmv.tm_min, tmv.tm_hour < 12 ? "AM" : "PM");
  txt(clk, cx, H - 26, 2, th.dim, 'c');

  vFade(x + w - 1, 0, H, th.bg, th.ink ? th.rule : C_LINE);
}

/* ================================================================== graph
   The sparkline, drawn into any x0/width so it sits inside part 2. A fill
   fading to the baseline under one thin line; sweeps in left → right.   */
static void drawGraph(const struct Row& r, int x0, int w, int top, int base, const struct Theme& th){
  if (r.nbuckets < 2 || w < 2) return;

  const int PH = base - top, YB = base;
  long long hi = 1;
  for (int i = 0; i < r.nbuckets; i++) if (r.buckets[i] > hi) hi = r.buckets[i];

  const float sweep = easeOut(clampf((millis() - sweepT0) / (float)SWEEP_MS, 0, 1));
  const int cols = (int)(w * sweep + 0.5f);
  const float step = (float)PH / FADE;
  int prev = -1;

  for (int px = 0; px < cols; px++){
    int pos = px * (r.nbuckets - 1) * 256 / (w - 1);
    int i = pos >> 8, fr = pos & 255;
    int a = r.buckets[i], b = r.buckets[min(i + 1, r.nbuckets - 1)];
    long long val = a + (((long long)(b - a) * fr) >> 8);
    int yTop = YB - (int)((val * (PH - 1) + hi / 2) / hi);

    for (int k = 0; k < FADE; k++){
      int s0 = (int)lround(YB - PH + k * step), s1 = (int)lround(YB - PH + (k + 1) * step);
      int from = max(s0, yTop + 2), to = min(s1, YB + 1);
      if (to > from) cv->fillRect(x0 + px, from, 1, to - from, th.gFill[k]);
    }
    int y0 = prev < 0 ? yTop : min(prev, yTop), y1 = prev < 0 ? yTop : max(prev, yTop);
    cv->fillRect(x0 + px, y0, 1, y1 - y0 + 1, th.gLine);
    prev = yTop;
  }
}

/* the tile values, counting up from the previous screen's over ~520 ms */
static void countedValues(const struct Row& r, double* p){
  const float t = easeOut(clampf((millis() - countT0) / 520.0f, 0, 1));
  for (int i = 0; i < MAX_TILES; i++)
    p[i] = i < r.ntiles ? countFromP[i] + (r.tiles[i].value - countFromP[i]) * t : 0;
}

/* ================================================================= PART 2
   The graph alone, with the heading tile — value big on the left, its
   name and the row's span on the right — laid over the top of it.       */
static void drawPart2(const struct Theme& th){
  const int x = COL_X[1], w = COL_X[2] - COL_X[1];
  cv->fillRect(x, 0, w, H, th.bg);
  const Row& r = snap.rows[curRow];
  drawGraph(r, x + 4, w - 8, 54, H - 12, th);

  double p[MAX_TILES]; countedValues(r, p);
  if (r.ntiles > 0){
    const Tile& hero = r.tiles[0];
    char v[16], sh[16];
    const char* hu = fmtTileValue(p[0], hero.unit, v, sizeof v);
    const uint16_t hot = levelColor(hero.level, th);
    const int wv = txt(v, x + 8, 8, 3, hot ? hot : th.tx, 'l', true, th.bg);
    if (hu) txt(hu, x + 8 + wv + 4, 19, 1, th.dim, 'l', false, th.bg);      /* bottom-aligned with the value */
    txt(hero.name, x + w - 8, 8, 2, th.dim, 'r', false, th.bg);
    spanCaption(r.size, r.count, r.unit, sh, sizeof sh);
    txt(sh, x + w - 8, 27, 1, hot ? hot : th.dim, 'r', false, th.bg);
  }
  vFade(x + w - 1, 0, H, th.bg, th.ink ? th.rule : C_LINE);
}

/* ================================================================= PART 3
   Gateway flush left and metric name flush right across the top, then
   the row's other tiles — up to four, one full-width row each, name left
   and value right — and the position rule along the bottom: one segment
   per metric screen, the current one lit. A row with fewer than 5 tiles
   leaves its remaining rows blank; nothing reflows.                     */
static void drawPart3(const struct Theme& th){
  const int x = COL_X[2], w = COL_X[3] - COL_X[2];
  cv->fillRect(x, 0, w, H, th.bg);
  const Row& r = snap.rows[curRow];

  char gw[6]; strlcpy(gw, r.gateway, sizeof gw);          /* first five letters */
  for (int n = strlen(gw); n > 0 && gw[n - 1] == ' '; n--) gw[n - 1] = 0;
  for (char* c = gw; *c; c++) *c = toupper(*c);
  txt(gw, x + 10, 9, 1, th.dim);
  /* the name at size 2 while it clears the gateway beside it, else size 1 */
  const bool big = (int)strlen(r.name) * 9 + 12 <= w - 20 - (int)strlen(gw) * 6;
  txt(r.name, x + w - 10, big ? 7 : 9, big ? 2 : 1, th.tx, 'r', true);
  hFade(x + 10, TOP_BAR_H, w - 20, th.bg, th.ink ? th.rule : C_LINE2);

  double p[MAX_TILES]; countedValues(r, p);
  const int top = TOP_BAR_H + 8, rowH = (H - top - 10) / 4;
  for (int i = 1; i < MAX_TILES; i++){
    if (i >= r.ntiles) continue;
    const Tile& t = r.tiles[i];
    const int rowY = top + (i - 1) * rowH;
    char v[16];
    const char* unit = fmtTileValue(p[i], t.unit, v, sizeof v);
    const uint16_t hot = levelColor(t.level, th);
    const int uw = unit ? (int)strlen(unit) * 6 + 4 : 0;
    txt(t.name, x + 10, rowY + 6, 1, th.dim);
    if (unit) txt(unit, x + w - 10, rowY + 11, 1, th.dim, 'r');             /* bottom-aligned with the value */
    txt(v, x + w - 10 - uw, rowY, 3, hot ? hot : th.tx, 'r', true);
  }

  const int gap = 4;
  const float seg = (float)(w - 20 - gap * (snap.nrows - 1)) / snap.nrows;
  for (int i = 0; i < snap.nrows; i++)
    cv->fillRect((int)lround(x + 10 + i * (seg + gap)), H - 6, (int)lround(seg), i == curRow ? 2 : 1,
                 i == curRow ? th.tx : (th.ink ? th.rule : C_LINE2));
}

/* Nothing has ever parsed — a board that has not been told where to look,
   or one that cannot get there yet. Parts 2 and 3 stay blank of numbers
   on purpose: there is no placeholder data to show, and inventing some
   would make this the most dangerous screen in the product. What they
   show instead is the way out; part 1 has already named the fault.      */
static void drawEmptyBody(const struct Theme& th){
  const int x = COL_X[1], w = COL_X[3] - COL_X[1], cx = x + w / 2;
  cv->fillRect(x, 0, w, H, th.bg);
  txt("NO DATA YET", cx, 30, 3, th.dim, 'c', true);
  if (!configHasEndpoint()){
    txt("this board has no endpoint", cx, 68, 1, th.dim, 'c');
    txt("HOLD THE LEFT PART 2 S", cx, 86, 2, th.ink ? th.tx : C_CY, 'c', true);
    txt("join the wi-fi it raises, set the url", cx, 112, 1, th.dim, 'c');
  } else {
    char host[40]; urlHostOf(cfg.url, host, sizeof host);
    txt("waiting on", cx, 68, 1, th.dim, 'c');
    txt(host, cx, 84, 2, th.ink ? th.tx : C_TX, 'c', true);
    txt(faultDetail[0] ? faultDetail : "nothing has answered yet", cx, 110, 1, th.dim, 'c');
    txt("HOLD THE LEFT PART 2 S TO CHANGE IT", cx, 132, 1, th.dim, 'c');
  }
}

/* ------------------------------------------------------------------ toast */
/* a short message in a box over the middle of the screen, gone after 1.2 s */
void drawToast(){
  if (!toastT0 || millis() - toastT0 > 1200) return;
  const int w = (int)strlen(toastText) * 12 + 28, x = W / 2 - w / 2, y = H / 2 - 20;
  cv->fillRect(x, y, w, 40, C_BG);
  cv->drawRect(x, y, w, 40, C_LINE2);
  txt(toastText, W / 2, y + 12, 2, C_TX, 'c', true);
}

/* The three parts tile all 640 columns, so no fillScreen() per frame. One
   theme for all of them: during an alert flash every part is lit together,
   which is what makes the whole screen the alert.                        */
void drawDashboard(){
  const Level& L = levelNow();
  const Theme& th = themeFor(L);
  drawPart1(L, th);
  /* the privacy lock (lock.ino) only ever hides parts 2 and 3 — part 1 is
     drawn exactly the same whether locked or not                          */
  if (lockIsLocked()){ drawLockedBody(th); return; }
  if (!snap.nrows){ drawEmptyBody(th); return; }
  drawPart2(th);
  drawPart3(th);
}
