/* ===========================================================================
   The settings screen — LEFT tap. Everything the board knows about itself,
   read-only. Tap again to go back.

   Read-only is the point: this is the screen you look at when the device is
   not showing what you expected, and every line on it answers one of the
   questions you would actually be asking. Which network did it join? What
   address did it get? Where is it pointing? Is the key being signed, and is
   the TLS actually checked? How old are the numbers?

   EDITING happens over the setup hotspot — hold LEFT 2 s — because a
   touchscreen the size of a stamp is a bad place to type a URL.
   hotspot.ino owns that.

   It holds the 5 s screen cycle still. Coming back to the main screen gives
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
  char v[48];

  txt("DEVICE", X_L, 33, 1, C_DIM2);
  snprintf(v, sizeof v, "%d%% %s", batteryPercent(), charging() ? "CHARGING" : "ON BATTERY");
  infoRow(44, "Battery", v, C_TX);
  infoRow(55, "Name", deviceName, C_TX);
  snprintf(v, sizeof v, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  infoRow(66, "MAC", v, C_TX);            /* the address a network's allowlist wants */

  /* Which of the saved networks actually answered, or what the radio is busy
     doing instead. A network joined but walled in by a sign-in page is its
     own state and says so — OFFLINE would send you looking at the backend
     for a fault that is in the ceiling.                                   */
  txt("WI-FI", X_L, 79, 1, C_DIM2);
  if (net.connected && net.portalBlocked){
    snprintf(v, sizeof v, "%s - SIGN-IN", net.ssid);
    infoRow(90, "Network", v, C_OR);
  } else if (net.connected){
    infoRow(90, "Network", net.ssid, C_TX);
  } else {
    snprintf(v, sizeof v, "%s", cfg.nnets ? wifiStateName(wifiState) : "none saved");
    infoRow(90, "Network", v, cfg.nnets ? C_DIM : C_OR);
  }
  if (net.connected) snprintf(v, sizeof v, "%d dBm", net.rssi); else strlcpy(v, "-", sizeof v);
  infoRow(101, "Signal", v, C_TX);
  infoRow(112, "IP", net.connected ? net.ip : "-", C_TX);

  /* Where it points and how it proves who it is. TLS with no pasted root is
     encrypted but unauthenticated, so it is called out rather than implied. */
  txt("BACKEND", X_L, 125, 1, C_DIM2);
  char host[40]; urlHostOf(cfg.url, host, sizeof host);
  infoRow(136, "Host", host, cfg.url[0] ? C_TX : C_OR);
  const bool https = !strncasecmp(cfg.url, "https://", 8);
  snprintf(v, sizeof v, "%s - %s",
           !cfg.key[0] ? "no key" : configSigned() ? "signed" : "bearer",
           !https ? "NO TLS" : configTlsVerified() ? "tls ok" : "TLS UNVERIFIED");
  infoRow(147, "Auth", v, (https && configTlsVerified() && cfg.key[0]) ? C_TX : C_OR);

  /* The dot is the current level — the response's verdict, or grey while a
     fault is standing. The name(s) beside it are every distinct `gateway` a
     row actually named, joined; one response, however many it merged.     */
  txt("GATEWAYS", X_L, 160, 1, C_DIM2);
  cv->fillRect(X_L, 172, 5, 5, levelNow().c);
  char gwList[48]; joinGateways(gwList, sizeof gwList);
  txt(gwList, X_L + 10, 171, 1, C_TX, 'l', true);
  if (snap.nrows){
    snprintf(v, sizeof v, "%u scr", (unsigned)snap.nrows);
    txt(v, 176, 171, 1, C_DIM, 'r');
    ageText(millis() - snap.polledAt, v, sizeof v);
    txt(v, X_R, 171, 1, C_TX, 'r');
  } else {
    snprintf(v, sizeof v, "every %lus", (unsigned long)(pollIntervalMs() / 1000));
    txt(v, X_R, 171, 1, C_DIM2, 'r');
  }

  cv->fillRect(X_L, 184, X_R - X_L, 1, C_LINE);
  txt("TAP LEFT - BACK", 120, 188, 1, C_DIM, 'c');
  txt("HOLD LEFT 2 S - SETUP HOTSPOT", 120, 199, 1, C_CY, 'c');
  txt("TAP RIGHT - DISPLAY OFF", 120, 210, 1, C_DIM2, 'c');
  txt("2 TAPS RIGHT - SOUND", 120, 221, 1, C_DIM2, 'c');
}
