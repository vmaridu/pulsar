/* ===========================================================================
   Configuration — the endpoint, its credentials, and the networks to reach it
   over. Nothing in here is compiled in: every field arrives from the setup
   page (hotspot.ino) and lives in NVS, so changing the backend a board points
   at never needs a reflash.

   Two namespaces, because they are two different concerns and a person may
   well replace one without touching the other:

       "gw"    u   the full URL to poll, exactly as given — api.md §1
               k   API key — the bearer token, or the public X-Api-Key
               s   API secret — set it and requests are signed instead
               ca  one pasted PEM root, optional, empty = TLS unverified
               mt  minutes until a mute auto-clears; 0 = never — sound.ino
               tz  index into TZ_TABLE (ws_lcd_349.ino) — the STATUS band's
                   display-only clock zone, daylight saving and all.
                   Signed requests never read this; they sign raw UTC — net.ino
               lk  the privacy lock's 6-digit code — lock.ino. Defaults to
                   "123456" the first time this key is ever read, same as
                   every other secret here: never sent back, never logged
               lt  minutes until an unlocked screen re-locks itself; 0 =
                   never (only a manual lock or a restart) — lock.ino

       "wifi"  v   layout version; a blob written by an older struct is
                   dropped rather than read back as nonsense
               n   how many networks are stored
               b   the networks themselves, one blob, in priority order

   The networks are one blob rather than a key per field because a network has
   nine fields and six may be stored: fifty-four keys is a thing that drifts.
   One blob is written and read atomically and has one version to check.

   SECRETS ARE NEVER SENT BACK OUT. configJson() masks every password to an
   empty string and sets a `…Set` flag beside it, so the page can say "saved"
   without the stored value crossing the air to whoever joined the hotspot.
   An empty password coming back IN therefore means "keep what you have",
   matched up by SSID so that reordering the list does not lose them —
   configApplyJson() below is where that happens.
   =========================================================================== */

#define NETS_VERSION 1          /* bump when WifiNet changes shape */

/* ------------------------------------------------------------------ helpers */

/* What a security mode is called on the wire between the page and here. */
const char* secName(uint8_t s){
  return s == SEC_ENT ? "enterprise" : s == SEC_OPEN ? "open" : "psk";
}
static uint8_t secFromName(const char* s){
  if (!s) return SEC_PSK;
  if (!strcmp(s, "enterprise")) return SEC_ENT;
  if (!strcmp(s, "open"))       return SEC_OPEN;
  return SEC_PSK;
}

/* Copy a JSON string into a fixed field, trimming the spaces a phone keyboard
   puts on the end of everything. Truncates rather than refusing — the page
   enforces the real limits, and a cut SSID is visible on the settings screen
   where a silently dropped one would not be.                              */
static void copyTrimmed(char* dst, size_t cap, const char* src){
  if (!src){ dst[0] = 0; return; }
  while (*src == ' ' || *src == '\t') src++;
  strlcpy(dst, src, cap);
  for (int n = (int)strlen(dst); n > 0; n--){
    if (dst[n - 1] == ' ' || dst[n - 1] == '\t' || dst[n - 1] == '\r' || dst[n - 1] == '\n') dst[n - 1] = 0;
    else break;
  }
}

/* Is there enough to try a poll at all? The key is not required — a backend
   may not want one — but the URL is the whole address.                    */
bool configHasEndpoint(){ return cfg.url[0] != 0; }

/* Set if an API secret is stored: the key is then the public X-Api-Key and
   each request is signed, rather than the key being a bearer token.
   api.md's HMAC appendix. One field decides it; there is no mode to set
   wrongly beside it.                                                      */
bool configSigned(){ return cfg.secret[0] != 0; }

/* TLS with no root to check against is TLS that a machine in the middle can
   read. It is allowed — a lot of backends sit behind a private CA nobody
   wants to paste — but net.ino says so on every single poll.             */
bool configTlsVerified(){ return cfg.ca[0] != 0; }

/* Tidies a pasted URL — trims whitespace, checks it is http(s). Nothing else:
   this is the exact URL the device polls, so nothing is stripped or assumed
   about its shape. Returns false for anything that is not http(s).        */
static bool configNormalizeUrl(char* url, size_t cap){
  copyTrimmed(url, cap, url);
  if (!url[0]) return true;                           /* empty is "not configured", not invalid */
  const bool https = !strncasecmp(url, "https://", 8);
  if (!https && strncasecmp(url, "http://", 7)) return false;
  return true;
}

/* Just the host out of the configured URL — the scheme and any path are
   noise on a line this narrow, and the host is the part you check.        */
void urlHostOf(const char* url, char* out, size_t cap){
  out[0] = 0;
  if (!url || !url[0]){ strlcpy(out, "NOT SET", cap); return; }
  const char* p = strstr(url, "://");
  const char* h = p ? p + 3 : url;
  const char* end = strchr(h, '/');
  size_t n = end ? (size_t)(end - h) : strlen(h);
  if (n >= cap) n = cap - 1;
  memcpy(out, h, n);
  out[n] = 0;
}

/* ---------------------------------------------------------------- load/save */

void configLoad(){
  memset(&cfg, 0, sizeof cfg);
  /* The defaults first, unconditionally. A board with nothing stored yet
     — first boot, or straight after a factory reset — has no "gw"
     namespace at all, so the block below never runs for it; without
     these it would come up with every figure at zero, and a backlight
     at 0 % is a dark glass.                                             */
  cfg.muteTimeoutMin     = 30;
  cfg.tzIndex            = TZ_DEFAULT_INDEX;
  strlcpy(cfg.lockCode, "123456", sizeof cfg.lockCode);
  cfg.lockTimeoutMin     = 30;
  cfg.brightnessBattery  = BRIGHTNESS_DEFAULT_BATTERY;
  cfg.brightnessCharging = BRIGHTNESS_DEFAULT_CHARGING;

  Preferences p;
  if (p.begin("gw", true)){                            /* read-only */
    p.getString("u",  cfg.url,    sizeof cfg.url);
    p.getString("k",  cfg.key,    sizeof cfg.key);
    p.getString("s",  cfg.secret, sizeof cfg.secret);
    p.getString("ca", cfg.ca,     sizeof cfg.ca);
    /* the default (30) is what getUShort returns when "mt" was never written
       at all — a fresh board, or one upgraded from before this existed —
       never confused with a user explicitly choosing "never" (0), which
       only ever gets stored by an actual save.                            */
    cfg.muteTimeoutMin = p.getUShort("mt", 30);
    cfg.tzIndex        = (uint8_t)p.getUChar("tz", TZ_DEFAULT_INDEX);
    if (cfg.tzIndex >= TZ_COUNT) cfg.tzIndex = TZ_DEFAULT_INDEX;   /* a stale index from a smaller table */
    /* "123456" only the very first time — after that, whatever was saved,
       same disambiguation the mute timeout above already relies on.      */
    if (p.isKey("lk")) p.getString("lk", cfg.lockCode, sizeof cfg.lockCode);
    else                strlcpy(cfg.lockCode, "123456", sizeof cfg.lockCode);
    cfg.lockTimeoutMin = p.getUShort("lt", 30);
    /* a stale/foreign value here (someone hand-editing NVS, or a namespace
       reused from something else) gets clamped rather than trusted — this
       drives a live PWM duty, not just a display number.                 */
    cfg.brightnessBattery  = (uint8_t)p.getUChar("bb", BRIGHTNESS_DEFAULT_BATTERY);
    cfg.brightnessCharging = (uint8_t)p.getUChar("bc", BRIGHTNESS_DEFAULT_CHARGING);
    if (cfg.brightnessBattery  < BRIGHTNESS_MIN || cfg.brightnessBattery  > BRIGHTNESS_MAX)
      cfg.brightnessBattery  = BRIGHTNESS_DEFAULT_BATTERY;
    if (cfg.brightnessCharging < BRIGHTNESS_MIN || cfg.brightnessCharging > BRIGHTNESS_MAX)
      cfg.brightnessCharging = BRIGHTNESS_DEFAULT_CHARGING;
    p.end();
  } else {
    LOG("cfg", "no \"gw\" namespace yet - first boot, nothing configured, defaults in force");
  }

  if (p.begin("wifi", true)){
    const uint8_t ver = p.getUChar("v", 0);
    const uint8_t n   = p.getUChar("n", 0);
    const size_t  len = p.getBytesLength("b");
    if (ver && ver != NETS_VERSION){
      LOGF("cfg", "saved networks are layout v%u, this build reads v%u - ignored",
           (unsigned)ver, (unsigned)NETS_VERSION);
    } else if (n && n <= MAX_NETWORKS && len == (size_t)n * sizeof(WifiNet)){
      p.getBytes("b", cfg.nets, len);
      cfg.nnets = n;
    } else if (n || len){
      LOGF("cfg", "saved networks look wrong (%u entries, %u bytes) - ignored",
           (unsigned)n, (unsigned)len);
    }
    p.end();
  }

  /* Say exactly what was found, before anything depends on it. A board that
     will not poll should make the reason obvious on the first screen of
     serial, not on the fifth.                                             */
  LOGF("cfg", "url %s", cfg.url[0] ? cfg.url : "NOT SET");
  LOGF("cfg", "auth %s%s",
       cfg.key[0] ? (configSigned() ? "signed (key + secret)" : "bearer token") : "NONE",
       configTlsVerified() ? ", TLS root pinned" : ", TLS NOT VERIFIED (no CA pasted)");
  LOGF("cfg", "%u saved network%s", (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s");
  if (cfg.muteTimeoutMin) LOGF("cfg", "mute auto-clears after %u minutes", (unsigned)cfg.muteTimeoutMin);
  else                    LOG("cfg", "mute auto-clear: never (only a double-tap or a restart clears it)");
  LOGF("cfg", "display clock: %s (signed requests still sign raw UTC seconds, always)",
       TZ_TABLE[cfg.tzIndex].label);
  if (cfg.lockTimeoutMin) LOGF("cfg", "lock auto-relocks after %u minutes", (unsigned)cfg.lockTimeoutMin);
  else                    LOG("cfg", "lock auto-relock: never (only a manual lock or a restart re-locks it)");
  LOGF("cfg", "backlight: %u%% on battery, %u%% charging",
       (unsigned)cfg.brightnessBattery, (unsigned)cfg.brightnessCharging);
  for (int i = 0; i < cfg.nnets; i++){
    const WifiNet& w = cfg.nets[i];
    LOGF("cfg", "  %d. \"%s\" %s%s", i + 1, w.ssid, secName(w.security),
         w.portal ? " + sign-in page" : "");
  }
}

bool configSave(){
  bool ok = true;
  Preferences p;
  if (p.begin("gw", false)){
    ok &= p.putString("u",  cfg.url)    > 0 || cfg.url[0]    == 0;
    ok &= p.putString("k",  cfg.key)    > 0 || cfg.key[0]    == 0;
    ok &= p.putString("s",  cfg.secret) > 0 || cfg.secret[0] == 0;
    ok &= p.putString("ca", cfg.ca)     > 0 || cfg.ca[0]     == 0;
    ok &= p.putUShort("mt", cfg.muteTimeoutMin) > 0;
    ok &= p.putUChar("tz", cfg.tzIndex) > 0;
    ok &= p.putString("lk", cfg.lockCode) > 0;
    ok &= p.putUShort("lt", cfg.lockTimeoutMin) > 0;
    ok &= p.putUChar("bb", cfg.brightnessBattery) > 0;
    ok &= p.putUChar("bc", cfg.brightnessCharging) > 0;
    p.end();
  } else { ok = false; }

  if (p.begin("wifi", false)){
    p.putUChar("v", NETS_VERSION);
    p.putUChar("n", cfg.nnets);
    if (cfg.nnets) ok &= p.putBytes("b", cfg.nets, (size_t)cfg.nnets * sizeof(WifiNet)) > 0;
    else           p.remove("b");
    p.end();
  } else { ok = false; }

  LOGF("cfg", "saved - url %s, %u network%s%s",
       cfg.url[0] ? cfg.url : "NOT SET", (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s",
       ok ? "" : " (SOME WRITES FAILED - is the nvs partition full?)");
  return ok;
}

/* Wipes every stored setting and every saved network — both NVS namespaces
   entirely, not just cfg's own fields in RAM, which the restart right
   after this (hotspot.ino) makes moot anyway. Only ever reached from the
   setup page's own factory-reset button, itself gated behind a checkbox
   and a confirm() dialog on that page — there is no undo once this
   returns, and nothing here asks a second time.                         */
bool configFactoryReset(){
  bool ok = true;
  Preferences p;
  if (p.begin("gw", false))   { ok &= p.clear(); p.end(); } else ok = false;
  if (p.begin("wifi", false)) { ok &= p.clear(); p.end(); } else ok = false;
  LOGF("cfg", "FACTORY RESET - every setting and saved network wiped%s",
       ok ? "" : " (SOME WIPES FAILED)");
  return ok;
}

/* --------------------------------------------------------------------- JSON */

/* The config as the setup page reads it. Every password comes out empty with
   a flag beside it — see the header. Everything else comes out whole,
   because you cannot edit a list you cannot see.                         */
void configJson(JsonDocument& doc){
  doc["url"]       = cfg.url;
  doc["key"]       = "";
  doc["keySet"]    = cfg.key[0]    != 0;
  doc["secretSet"] = cfg.secret[0] != 0;
  doc["caSet"]     = cfg.ca[0]     != 0;
  doc["signed"]    = configSigned();
  doc["maxNets"]   = MAX_NETWORKS;
  doc["muteTimeoutMin"] = cfg.muteTimeoutMin;
  doc["tzIndex"]        = cfg.tzIndex;
  doc["lockCodeSet"]    = cfg.lockCode[0] != 0;   /* never the code itself — same rule as every secret here */
  doc["lockTimeoutMin"] = cfg.lockTimeoutMin;
  doc["brightnessBattery"]  = cfg.brightnessBattery;
  doc["brightnessCharging"] = cfg.brightnessCharging;

  /* TZ_TABLE (ws_lcd_349.ino) is the one copy of this list — sent here so
     the setup page never hardcodes its own second copy to drift out of
     sync with it.                                                        */
  JsonArray tz = doc["tzones"].to<JsonArray>();
  for (uint8_t i = 0; i < TZ_COUNT; i++) tz.add(TZ_TABLE[i].label);

  JsonArray arr = doc["nets"].to<JsonArray>();
  for (int i = 0; i < cfg.nnets; i++){
    const WifiNet& w = cfg.nets[i];
    JsonObject o = arr.add<JsonObject>();
    o["ssid"]           = w.ssid;
    o["security"]       = secName(w.security);
    o["pskSet"]         = w.psk[0]  != 0;
    o["identity"]       = w.identity;
    o["user"]           = w.user;
    o["passSet"]        = w.pass[0] != 0;
    o["portal"]         = w.portal != 0;
    o["portalUser"]     = w.portalUser;
    o["portalPassSet"]  = w.portalPass[0] != 0;
  }
}

/* One password field coming back from the page. Empty means "keep the one you
   already have" — the page never saw it, so it cannot send it back. `old` is
   the stored network with the same SSID, or NULL when this one is new.     */
static void keepOrSet(char* dst, size_t cap, JsonVariantConst v, const char* old){
  const char* in = v.is<const char*>() ? v.as<const char*>() : nullptr;
  if (in && in[0]) copyTrimmed(dst, cap, in);
  else if (old)    strlcpy(dst, old, cap);
  else             dst[0] = 0;
}

/* Apply what the page posted. Validates first and writes `cfg` only once
   everything has passed, the same rule parseSnapshot() follows for payloads:
   a half-applied config is how a board ends up unreachable with no way back
   in. On a problem it fills `err` and changes nothing at all.

   Returns true when `cfg` was replaced (the caller then saves it).        */
bool configApplyJson(JsonObjectConst in, char* err, size_t errcap){
  err[0] = 0;
  if (in.isNull()){ strlcpy(err, "body is not a JSON object", errcap); return false; }

  static Config next;          /* static: sizeof(Config) is a few KB and this runs
                                  on the loop task's stack, same reason
                                  parseSnapshot() keeps its scratch off it */
  memset(&next, 0, sizeof next);

  /* ---- endpoint. A blank key or secret keeps the stored one; clearing is
     explicit, so that a page loaded before a secret was set cannot wipe it
     by simply being saved.                                               */
  copyTrimmed(next.url, sizeof next.url, in["url"] | "");
  if (!configNormalizeUrl(next.url, sizeof next.url)){
    strlcpy(err, "the URL must start with https:// or http://", errcap);
    return false;
  }
  keepOrSet(next.key, sizeof next.key, in["key"], cfg.key);
  if (in["secretClear"] | false) next.secret[0] = 0;
  else keepOrSet(next.secret, sizeof next.secret, in["secret"], cfg.secret);
  if (in["caClear"] | false) next.ca[0] = 0;
  else keepOrSet(next.ca, sizeof next.ca, in["ca"], cfg.ca);

  /* ---- how long a mute lasts on its own. The setup page only ever offers
     this fixed set (sound.ino's mute, cleared by a double-tap, a restart,
     or this many minutes — 0 meaning never); reject anything else outright
     rather than store a number the page could never have actually sent.  */
  {
    const long mt = in["muteTimeoutMin"] | 30;
    static const long ALLOWED[] = { 0, 5, 10, 30, 60, 360, 720, 1440 };
    bool validMt = false;
    for (size_t i = 0; i < sizeof(ALLOWED) / sizeof(ALLOWED[0]); i++)
      if (mt == ALLOWED[i]){ validMt = true; break; }
    if (!validMt){
      snprintf(err, errcap, "%ld is not a valid mute timeout", mt);
      return false;
    }
    next.muteTimeoutMin = (uint16_t)mt;
  }

  /* ---- the display clock's zone — an index into TZ_TABLE, never a raw
     offset: a fixed number can't be right through a daylight-saving change,
     and this field never touches signed requests either way (those always
     sign time(nullptr) directly, in net.ino, never this).                */
  {
    const long tzi = in["tzIndex"] | TZ_DEFAULT_INDEX;
    if (tzi < 0 || tzi >= TZ_COUNT){
      snprintf(err, errcap, "%ld is not a valid time zone", tzi);
      return false;
    }
    next.tzIndex = (uint8_t)tzi;
  }

  /* ---- the privacy lock's own code. Blank means "keep the one you have" —
     the same convention as the API secret and every Wi-Fi password: this
     page never shows what is already stored, only that something is.
     Any digit 0-9: this build's keypad (the mockup's ten cells) can type
     every one of them back in. Stored now; lock.ino reads it once the
     keypad arrives.                                                     */
  {
    const char* lk = in["lockCode"] | "";
    if (lk[0]){
      if (strlen(lk) != 6 || strspn(lk, "0123456789") != 6){
        strlcpy(err, "the lock code must be exactly 6 digits", errcap);
        return false;
      }
      strlcpy(next.lockCode, lk, sizeof next.lockCode);
    } else {
      strlcpy(next.lockCode, cfg.lockCode, sizeof next.lockCode);
    }
  }

  /* ---- how long an unlocked screen stays that way before it re-locks
     itself — the same fixed set as the mute timeout above, and rejected
     outright if it is anything else the page could not actually have sent. */
  {
    const long lt = in["lockTimeoutMin"] | 30;
    static const long ALLOWED[] = { 0, 5, 10, 30, 60, 360, 720, 1440 };
    bool validLt = false;
    for (size_t i = 0; i < sizeof(ALLOWED) / sizeof(ALLOWED[0]); i++)
      if (lt == ALLOWED[i]){ validLt = true; break; }
    if (!validLt){
      snprintf(err, errcap, "%ld is not a valid lock timeout", lt);
      return false;
    }
    next.lockTimeoutMin = (uint16_t)lt;
  }

  /* ---- backlight brightness, one figure for on-battery and one for
     charging — both rejected outright outside the slider's own 30-100
     range, the same "the page could not actually have sent this" rule
     as the timeouts above. */
  {
    const long bb = in["brightnessBattery"] | BRIGHTNESS_DEFAULT_BATTERY;
    if (bb < BRIGHTNESS_MIN || bb > BRIGHTNESS_MAX){
      snprintf(err, errcap, "battery brightness must be %d-%d%%", BRIGHTNESS_MIN, BRIGHTNESS_MAX);
      return false;
    }
    next.brightnessBattery = (uint8_t)bb;

    const long bc = in["brightnessCharging"] | BRIGHTNESS_DEFAULT_CHARGING;
    if (bc < BRIGHTNESS_MIN || bc > BRIGHTNESS_MAX){
      snprintf(err, errcap, "charging brightness must be %d-%d%%", BRIGHTNESS_MIN, BRIGHTNESS_MAX);
      return false;
    }
    next.brightnessCharging = (uint8_t)bc;
  }

  /* ---- networks, in the order the page listed them: that order IS the
     priority, so the array is the preference list and nothing else needs
     to carry a rank.                                                     */
  JsonVariantConst netsV = in["nets"];
  if (!netsV.isNull() && !netsV.is<JsonArrayConst>()){
    strlcpy(err, "\"nets\" must be an array", errcap);
    return false;
  }
  if (netsV.is<JsonArrayConst>()){
    for (JsonVariantConst nv : netsV.as<JsonArrayConst>()){
      if (next.nnets >= MAX_NETWORKS){
        snprintf(err, errcap, "more than %d networks", MAX_NETWORKS);
        return false;
      }
      if (!nv.is<JsonObjectConst>()) continue;                /* a blank row the page left behind */
      JsonObjectConst o = nv.as<JsonObjectConst>();
      WifiNet& w = next.nets[next.nnets];
      copyTrimmed(w.ssid, sizeof w.ssid, o["ssid"] | "");
      if (!w.ssid[0]) continue;                               /* an unnamed network is not a network */
      w.security = secFromName(o["security"] | "psk");
      w.portal   = (o["portal"] | false) ? 1 : 0;

      /* the stored network of the same name, so its unseen passwords survive
         being reordered, renamed around, or saved from a page that only ever
         showed dots where they are                                        */
      const WifiNet* old = nullptr;
      for (int i = 0; i < cfg.nnets; i++)
        if (!strcmp(cfg.nets[i].ssid, w.ssid)){ old = &cfg.nets[i]; break; }

      keepOrSet(w.psk,  sizeof w.psk,  o["psk"],  old ? old->psk  : nullptr);
      keepOrSet(w.pass, sizeof w.pass, o["pass"], old ? old->pass : nullptr);
      keepOrSet(w.portalPass, sizeof w.portalPass, o["portalPass"],
                old ? old->portalPass : nullptr);
      copyTrimmed(w.identity,   sizeof w.identity,   o["identity"]   | "");
      copyTrimmed(w.user,       sizeof w.user,       o["user"]       | "");
      copyTrimmed(w.portalUser, sizeof w.portalUser, o["portalUser"] | "");

      /* Enough to actually join? Say so now, on the page, rather than let a
         board leave the bench quietly unable to associate.                */
      if (w.security == SEC_PSK && !w.psk[0]){
        snprintf(err, errcap, "\"%s\" needs a password", w.ssid);
        return false;
      }
      if (w.security == SEC_ENT && (!w.user[0] || !w.pass[0])){
        snprintf(err, errcap, "\"%s\" needs a username and password", w.ssid);
        return false;
      }
      if (w.security == SEC_ENT && !w.identity[0])
        strlcpy(w.identity, w.user, sizeof w.identity);       /* the usual outer identity */
      next.nnets++;
    }
  }

  cfg = next;
  return true;
}
