/* ===========================================================================
   The settings screen — a double-tap on the glass. Everything the board knows about itself,
   read-only, in the same three parts the dashboard uses: what am I
   holding, what did it join and where is it pointing, and what came back.
   Tap again to go back. It holds the 5 s cycle still.

   Read-only is the point: this is the screen you look at when the device
   is not showing what you expected, and every line answers a question
   you would actually ask. Editing happens over the setup hotspot — hold
   the left part 2 s — because a strip of glass is a bad place to type a URL.

   Mirrors ws_lcd_349/mockup.html.
   =========================================================================== */

/* one label over one value, the value at size 2 while it fits the part.
   Rows sit ROW_PITCH apart: label 8 px, a 2 px gap, value 11 px, 5 px of
   air — four rows end at 147, clear of the footer at 156.               */
#define ROW_PITCH 26
#define ROW_Y0    45
static void settingRow(int x, int w, int row, const char* label, const char* value, uint16_t c){
  const int y = ROW_Y0 + row * ROW_PITCH;
  txt(label, x, y, 1, C_DIM);
  const uint8_t size = (int)strlen(value) * 9 <= w - 24 ? 2 : 1;   /* size 2 is 9 px a character */
  txt(value, x, y + 10, size, c, 'l', true);
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

void drawSettings(){
  cv->fillScreen(C_BG);
  txt("SETTINGS", W / 2, 4, 2, C_CY, 'c', true);
  hFade(W / 2 - 80, 21, 160, C_BG, C_LINE);
  char v[48];

  /* DEVICE — what am I holding. Sound is not listed: the crossed speaker
     in STATUS already says it; the padlock says the lock.               */
  int x = COL_X[0] + 14, w = COL_X[1] - COL_X[0];
  settingHead(x, w, "DEVICE");
  snprintf(v, sizeof v, "%d%% %s", batteryPercent(), charging() ? "CHARGING" : "ON BATTERY");
  settingRow(x, w, 0, "BATTERY", v, C_TX);
  settingRow(x, w, 1, "NAME", deviceName, C_TX);
  snprintf(v, sizeof v, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  settingRow(x, w, 2, "MAC", v, C_TX);              /* the address a network's allowlist wants */

  /* WI-FI and BACKEND — what did it join, where is it pointing. A network
     joined but walled in by a sign-in page is its own state and says so. */
  x = COL_X[1] + 14; w = COL_X[2] - COL_X[1];
  vFade(COL_X[1], 22, H - 44, C_BG, C_LINE);
  settingHead(x, w, "WI-FI / BACKEND");
  if (net.connected && net.portalBlocked){
    snprintf(v, sizeof v, "%s SIGN-IN", net.ssid);
    settingRow(x, w, 0, "NETWORK", v, C_OR);
  } else if (net.connected){
    settingRow(x, w, 0, "NETWORK", net.ssid, C_TX);
  } else {
    settingRow(x, w, 0, "NETWORK", cfg.nnets ? wifiStateName(wifiState) : "none saved", cfg.nnets ? C_DIM : C_OR);
  }
  if (net.connected) snprintf(v, sizeof v, "%d dBm  %s", net.rssi, net.ip); else strlcpy(v, "-", sizeof v);
  settingRow(x, w, 1, "SIGNAL / IP", v, C_TX);
  char host[40]; urlHostOf(cfg.url, host, sizeof host);
  settingRow(x, w, 2, "HOST", host, cfg.url[0] ? C_TX : C_OR);
  /* TLS with no pasted root is encrypted but unauthenticated — said, not implied */
  const bool https = !strncasecmp(cfg.url, "https://", 8);
  snprintf(v, sizeof v, "%s / %s",
           !cfg.key[0] ? "no key" : configSigned() ? "signed" : "bearer",
           !https ? "NO TLS" : configTlsVerified() ? "tls ok" : "TLS UNVERIFIED");
  settingRow(x, w, 3, "AUTH", v, (https && configTlsVerified() && cfg.key[0]) ? C_TX : C_OR);

  /* DATA — what came back. The dot is the current level: the response's
     verdict, or grey while a fault is standing.                         */
  x = COL_X[2] + 14; w = COL_X[3] - COL_X[2];
  vFade(COL_X[2], 22, H - 44, C_BG, C_LINE);
  settingHead(x, w, "DATA");
  cv->fillRect(x + 36, 27, 5, 5, levelNow().c);
  char gwList[48]; joinGateways(gwList, sizeof gwList);
  settingRow(x, w, 0, "GATEWAYS", gwList, C_TX);
  if (snap.nrows){
    time_t t = (time_t)snap.measured_at;
    struct tm tmv; localtime_r(&t, &tmv);
    const int h12 = tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12;
    snprintf(v, sizeof v, "%02d/%02d %02d:%02d %s", tmv.tm_mon + 1, tmv.tm_mday, h12, tmv.tm_min,
             tmv.tm_hour < 12 ? "AM" : "PM");
    settingRow(x, w, 1, "MEASURED", v, C_TX);
    char age[16]; ageText(millis() - snap.polledAt, age, sizeof age);
    snprintf(v, sizeof v, "%u screens, %s", (unsigned)snap.nrows, age);
    settingRow(x, w, 2, "LAST POLL", v, C_TX);
  } else {
    settingRow(x, w, 1, "MEASURED", "-", C_DIM);
    settingRow(x, w, 2, "LAST POLL", faultWord[0] ? faultWord : "nothing yet", C_DIM);
  }
  snprintf(v, sizeof v, "every %lu s", (unsigned long)(pollIntervalMs() / 1000));
  settingRow(x, w, 3, "POLLS", v, C_TX);

  /* the way out, on a line of its own under everything */
  hFade(W / 2 - 200, 150, 400, C_BG, C_LINE);
  txt("TAP - BACK     SETUP HOTSPOT: HOLD THE LEFT PART OF THE DASHBOARD 2 S", W / 2, 156, 1, C_DIM2, 'c');
}
