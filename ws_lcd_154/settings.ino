/* ===========================================================================
   Settings and hotspot — the two screens behind the LEFT key.

     LEFT tap      the settings screen: everything the board knows about
                   itself, read-only. Tap again to go back.
     LEFT hold 2 s hotspot mode: the board raises its own Wi-Fi and serves a
                   page where the settings become editable. THE PAGE AND ITS
                   SPEC ARE STILL TO COME — for now the screen says so.

   Both hold the 5 s screen cycle still. Coming back to the main screen gives
   the metric on it a fresh 5 s.
   =========================================================================== */

/* one label / value line on the settings screen */
static void infoRow(int y, const char* label, const char* value, uint16_t c){
  txt(label, X_L, y, 1, C_DIM);
  txt(value, X_R, y, 1, c, 'r');
}

/* A response can mix rows from more than one platform (api.md §4) — join the
   distinct `gateway` names actually present, in first-seen order, so this
   line still means something whether it's one name or several.           */
static void joinGateways(char* out, size_t cap){
  out[0] = 0;
  for (int i = 0; i < snap.nrows; i++){
    const char* g = snap.rows[i].gateway;
    if (strstr(out, g)) continue;                    /* already listed */
    char tmp[48];
    snprintf(tmp, sizeof tmp, "%s%s", out[0] ? ", " : "", g);
    strlcat(out, tmp, cap);
  }
  if (!out[0]) strlcpy(out, "-", cap);
}

/* ---------------------------------------------------------------- settings */
static void drawSettings(){
  cv->fillScreen(C_BG);
  txt("SETTINGS", X_L, 8, 2, C_CY, 'l', true);
  cv->fillRect(X_L, 28, X_R - X_L, 1, C_LINE);
  char v[32];

  txt("DEVICE", X_L, 34, 1, C_DIM2);
  snprintf(v, sizeof v, "%d%% %s", batteryPercent(), charging() ? "CHARGING" : "ON BATTERY");
  infoRow(45, "Battery", v, C_TX);
  infoRow(56, "Name", deviceName, C_TX);
  infoRow(67, "Sound", soundMuted ? "muted" : "on", soundMuted ? C_OR : C_TX);

  txt("WI-FI", X_L, 81, 1, C_DIM2);
  infoRow(92, "Connected", net.connected ? "yes" : "no - offline build",
          net.connected ? C_TX : C_DIM2);
  if (net.connected) snprintf(v, sizeof v, "%d dBm", net.rssi); else strlcpy(v, "-", sizeof v);
  infoRow(103, "Signal", v, C_TX);
  infoRow(114, "IP", net.connected ? net.ip : "-", C_TX);
  snprintf(v, sizeof v, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  infoRow(125, "MAC", v, C_TX);

  /* The dot is the current alert.level — global to the response, not any
     one platform. The name(s) beside it are every distinct `gateway` a row
     actually named, joined; one response, however many it merged.        */
  txt("GATEWAYS", X_L, 139, 1, C_DIM2);
  cv->fillRect(X_L, 151, 5, 5, levelNow().c);
  char gwList[48]; joinGateways(gwList, sizeof gwList);
  txt(gwList, X_L + 10, 150, 1, C_TX, 'l', true);
  snprintf(v, sizeof v, "%u screens", (unsigned)snap.nrows);
  txt(v, 168, 150, 1, C_DIM, 'r');
  ageText(millis() - snap.polledAt, v, sizeof v);
  txt(v, X_R, 150, 1, C_TX, 'r');
  snprintf(v, sizeof v, "every %lus", (unsigned long)(pollIntervalMs() / 1000));
  infoRow(162, "Poll", v, C_DIM);

  cv->fillRect(X_L, 178, X_R - X_L, 1, C_LINE);
  txt("TAP LEFT - BACK", 120, 186, 1, C_DIM, 'c');
  txt("HOLD LEFT 2 S - HOTSPOT", 120, 198, 1, C_DIM2, 'c');
  txt("TAP RIGHT - DISPLAY OFF", 120, 210, 1, C_DIM2, 'c');
  txt("2 TAPS RIGHT - SOUND", 120, 222, 1, C_DIM2, 'c');
}

/* ---------------------------------------------------------------- hotspot
   Placeholder. The real thing raises a Wi-Fi access point named after the
   device and serves a page to change the settings; its spec is still to come. */
static void startHotspot(){
  LOGF("view", "hotspot: placeholder - would raise \"%s\" at 192.168.4.1 (spec to come)", deviceName);
  /* TODO(hotspot): WiFi.softAP(deviceName); serve the settings page; apply and
     save what it posts (Preferences, namespace "gw" — docs/device.md §5).  */
}
static void stopHotspot(){
  LOG("view", "hotspot: closed");
  /* TODO(hotspot): stop the web server and WiFi.softAPdisconnect(true).    */
}

static void drawHotspot(){
  cv->fillScreen(C_BG);
  txt("HOTSPOT", X_L, 8, 2, C_CY, 'l', true);
  cv->fillRect(X_L, 28, X_R - X_L, 1, C_LINE);
  txt("JOIN THIS WI-FI", X_L, 40, 1, C_DIM);
  txt(deviceName, X_L, 52, 2, C_TX, 'l', true);
  txt("THEN OPEN", X_L, 76, 1, C_DIM);
  txt("192.168.4.1", X_L, 88, 2, C_TX);
  cv->fillRect(X_L, 114, X_R - X_L, 1, C_LINE);
  txt("NOT STARTED", 120, 128, 2, C_OR, 'c', true);
  txt("the settings page", 120, 152, 1, C_DIM2, 'c');
  txt("is still to be specified", 120, 164, 1, C_DIM2, 'c');
  cv->fillRect(X_L, 184, X_R - X_L, 1, C_LINE);
  txt("TAP LEFT - BACK", 120, 196, 1, C_DIM, 'c');
  txt("HOLD LEFT 2 S - BACK", 120, 208, 1, C_DIM2, 'c');
}
