/* ===========================================================================
   The setup hotspot — the board's own Wi-Fi, the page it serves, and the
   screen that tells you how to get in.

   HOLD DOWN 2 s. The board raises an access point named after itself, serves
   one page at 192.168.4.1, and everything on that page is everything the
   firmware does not bake in: the full URL to poll, the API key, the API
   secret, and the networks to reach them over, in priority order.

   Three decisions worth knowing about:

   · THE AP IS WPA2, NOT OPEN, and its password is made fresh every time the
     hotspot is raised and shown on the device's screen. An open AP means
     anybody within range can rewrite where this device points and read what
     it posts; the screen is the out-of-band channel that fixes that, and it
     costs one glance to use
   · A DNS RESPONDER ANSWERS EVERY NAME with 192.168.4.1, and anything not
     served is redirected to the page, so joining the network pops the page
     up on its own the way a hotel's does. That is the whole reason the
     phone's captive-portal probe exists, and it is much kinder than telling
     someone to type an address
   · NOTHING IS TESTED FROM HERE, on purpose. One radio cannot hold this AP
     up and be associated to your office network at the same time without
     dropping the phone that is mid-edit, so the page scans (which needs no
     association, and tells you whether the SSID you typed is even in the
     room) and then hands off: Save and restart, and the device's own screen
     says OFFLINE, NO ACCESS or real numbers within a few seconds. The
     screen is the test, and it is the screen you will be reading anyway

   The page itself is PAGE[], in hotspot_page.h — standalone, no fonts or
   scripts fetched from anywhere, because a device serving its own setup page
   has no internet to fetch them over.
   =========================================================================== */

#define AP_IP_STR   "192.168.4.1"
#define AP_PASS_LEN 8            /* WPA2's floor is 8 */
#define AP_JSON_MAX  3072        /* every response this serves fits inside this */
#define AP_SCAN_MAX    24        /* scan rows sent to the page — keeps it inside AP_JSON_MAX */

/* --------------------------------------------------------------- the page
   Standalone: every byte of style and script is right here, because a device
   serving its own setup page has no internet to fetch a font or a framework
   over. Kept in flash and sent straight from it, never copied into RAM.

   It shows no stored password back — the firmware masks them (config.ino) and
   the page says "saved" with an empty box beside it. An empty box posted back
   means "keep the one you have", matched up by network name, so reordering
   the list or renaming one network never silently loses another's password.

   Lives in hotspot_page.h, not here — see that file for why.             */
#include "hotspot_page.h"

/* ------------------------------------------------------------------ the AP */

/* A fresh password per session, from an alphabet with no character anyone has
   to squint at — no 0/O, no 1/l/I. It is read off a 240 px screen and typed
   into a phone once.                                                       */
static void apMakePassword(){
  static const char alpha[] = "23456789abcdefghjkmnpqrstuvwxyz";
  const size_t n = sizeof alpha - 1;
  for (int i = 0; i < AP_PASS_LEN; i++) apPass[i] = alpha[esp_random() % n];
  apPass[AP_PASS_LEN] = 0;
}

static int apClients(){ return WiFi.softAPgetStationNum(); }

/* --------------------------------------------------------------- responses */

/* Serialised into a fixed buffer rather than a String: this runs on a device
   with 4 KB of config and no business growing the heap a character at a time
   while a phone is connected. Too big to fit is a 500 that says so, not a
   truncated document the page would fail to parse with no explanation.    */
static void apSendJson(int code, JsonDocument& doc){
  static char out[AP_JSON_MAX];
  const size_t need = measureJson(doc);
  if (need + 1 > sizeof out){
    LOGF("view", "hotspot: a %u byte response will not fit in %u - sending 500",
         (unsigned)need, (unsigned)sizeof out);
    apServer->send(500, "application/json", "{\"ok\":false,\"error\":\"response too large\"}");
    return;
  }
  serializeJson(doc, out, sizeof out);
  apServer->send(code, "application/json", out);
}
static void apSendError(int code, const char* msg){
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = msg;
  apSendJson(code, doc);
}

/* ---------------------------------------------------------------- handlers */

static void apHandleRoot(){
  apLastHit = millis();
  LOG("view", "hotspot: serving the setup page");
  apServer->sendHeader("Cache-Control", "no-store");
  apServer->send_P(200, "text/html", PAGE);
}

/* Everything else on every name — this is what makes a phone pop the page. */
static void apHandleElsewhere(){
  apLastHit = millis();
  LOGF("view", "hotspot: %s -> redirected to the setup page", apServer->uri().c_str());
  apServer->sendHeader("Location", "http://" AP_IP_STR "/", true);
  apServer->send(302, "text/plain", "");
}

static void apHandleConfigGet(){
  apLastHit = millis();
  JsonDocument doc;
  configJson(doc);
  doc["device"] = deviceName;
  LOGF("view", "hotspot: GET /api/config -> url %s, %u saved network%s (no secrets sent)",
       cfg.url[0] ? cfg.url : "(none)", (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s");
  apSendJson(200, doc);
}

static void apHandleConfigPost(){
  apLastHit = millis();
  const String body = apServer->arg("plain");
  LOGF("cfg", "hotspot: POST /api/config - %u byte body received", (unsigned)body.length());
  if (!body.length()){
    LOG("cfg", "hotspot: rejected - the page sent no body at all");
    apSendError(400, "empty body");
    return;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body.c_str());
  if (err){
    LOGF("cfg", "the page posted something unparseable: %s", err.c_str());
    apSendError(400, err.c_str());
    return;
  }

  /* A safe summary of what was posted — never the secret/psk/pass fields
     themselves, same rule config.ino's own logging already follows.       */
  {
    JsonObjectConst in = doc.as<JsonObjectConst>();
    JsonArrayConst nets = in["nets"].is<JsonArrayConst>() ? in["nets"].as<JsonArrayConst>() : JsonArrayConst();
    LOGF("cfg", "hotspot: posted url=\"%s\" key=%s secret=%s ca=%s lock=%s nets=%u",
         (const char*)(in["url"] | ""),
         strlen(in["key"] | "")    ? "given" : "kept/none",
         (in["secretClear"] | false) ? "cleared" :
           strlen(in["secret"] | "") ? "given" : "kept/none",
         (in["caClear"] | false)     ? "cleared" :
           strlen(in["ca"] | "")     ? "given" : "kept/none",
         strlen(in["lockCode"] | "") ? "given" : "kept",
         (unsigned)nets.size());
  }

  char problem[96];
  if (!configApplyJson(doc.as<JsonObjectConst>(), problem, sizeof problem)){
    LOGF("cfg", "rejected what the page posted: %s", problem);
    apSendError(422, problem);
    return;
  }
  const bool saved = configSave();
  if (saved) apSaves++;
  LOGF("cfg", "hotspot: %s - url now \"%s\", %u network%s (save #%u this session)",
       saved ? "saved to NVS" : "NVS WRITE FAILED", cfg.url, (unsigned)cfg.nnets,
       cfg.nnets == 1 ? "" : "s", (unsigned)apSaves);

  JsonDocument out;
  out["ok"] = saved;
  if (!saved) out["error"] = "could not write to storage";
  out["nets"] = cfg.nnets;
  out["url"]  = cfg.url;
  apSendJson(saved ? 200 : 500, out);
  LOGF("cfg", "hotspot: replied to the page - {\"ok\":%s,\"nets\":%u}",
       saved ? "true" : "false", (unsigned)cfg.nnets);
}

/* What is actually in the air. Asked for repeatedly by the page while a scan
   runs, so it answers "still scanning" rather than blocking the server.    */
static void apHandleScan(){
  apLastHit = millis();
  JsonDocument doc;
  const int found = WiFi.scanComplete();

  if (found == -1){ doc["scanning"] = true; apSendJson(200, doc); return; }

  if (found < 0){
    const int16_t started = WiFi.scanNetworks(true /* async */);
    doc["scanning"] = started != WIFI_SCAN_FAILED;
    if (started == WIFI_SCAN_FAILED){
      doc["error"] = "the radio would not start a scan";
      LOG("view", "hotspot: scan would not start");
    } else {
      LOG("view", "hotspot: scanning for the page");
    }
    apSendJson(200, doc);
    return;
  }

  doc["scanning"] = false;
  JsonArray arr = doc["nets"].to<JsonArray>();
  for (int i = 0; i < found && i < AP_SCAN_MAX; i++){
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    /* What the beacon claims, as a hint for the security picker. An
       enterprise network advertises WPA2_ENTERPRISE; everything else with a
       key looks like a PSK from out here.                                  */
    const wifi_auth_mode_t a = WiFi.encryptionType(i);
    o["security"] = a == WIFI_AUTH_OPEN ? "open"
                  : a == WIFI_AUTH_WPA2_ENTERPRISE ? "enterprise" : "psk";
  }
  LOGF("view", "hotspot: sent %d network%s to the page", found, found == 1 ? "" : "s");
  WiFi.scanDelete();
  apSendJson(200, doc);
}

static void apHandleReboot(){
  apLastHit = millis();
  JsonDocument doc;
  doc["ok"] = true;
  apSendJson(200, doc);
  LOG("view", "hotspot: the page asked for a restart - going now");
  apServer->client().flush();
  delay(300);
  Serial.flush();
  ESP.restart();
}

/* -------------------------------------------------------------- lifecycle */

void hotspotStart(){
  apMakePassword();
  apSaves   = 0;
  apLastHit = 0;

  /* The station side is stood down but kept enabled: a scan needs it, and a
     scan is how the page tells you whether the SSID you just typed is
     actually in the room. It never associates while the AP is up — one
     radio cannot follow your office network's channel and hold this AP
     still at the same time.                                               */
  wifiSuspend();                                   /* wifi.ino */
  WiFi.mode(WIFI_AP_STA);
  const bool up = WiFi.softAP(deviceName, apPass);
  if (!up){
    LOG("view", "hotspot: the access point would not come up");
    return;
  }
  LOGF("view", "hotspot: \"%s\" up at %s, password %s", deviceName,
       WiFi.softAPIP().toString().c_str(), apPass);

  apDns = new DNSServer();
  if (apDns){
    apDns->setErrorReplyCode(DNSReplyCode::NoError);
    apDns->start(53, "*", WiFi.softAPIP());         /* every name points here */
    LOG("view", "hotspot: dns is answering every name - joining should pop the page");
  } else LOG("view", "hotspot: no memory for dns - the page needs typing in by hand");

  apServer = new WebServer(80);
  if (!apServer){
    LOG("view", "hotspot: no memory for the web server");
    return;
  }
  apServer->on("/",            HTTP_GET,  apHandleRoot);
  apServer->on("/api/config",  HTTP_GET,  apHandleConfigGet);
  apServer->on("/api/config",  HTTP_POST, apHandleConfigPost);
  apServer->on("/api/scan",    HTTP_GET,  apHandleScan);
  apServer->on("/api/reboot",  HTTP_POST, apHandleReboot);
  apServer->onNotFound(apHandleElsewhere);
  apServer->begin();
  LOG("view", "hotspot: serving http://" AP_IP_STR "/");
}

void hotspotStop(){
  /* Leaving by a DOWN tap calls this twice — once from onTap, once from inside
     showMain() — so it has to be safe to call when nothing is up. apPass is
     the session marker: hotspotStart() sets it first and this clears it last. */
  if (!apPass[0] && !apServer && !apDns) return;
  if (apServer){ apServer->stop(); delete apServer; apServer = nullptr; }
  if (apDns){ apDns->stop(); delete apDns; apDns = nullptr; }
  WiFi.softAPdisconnect(true);
  apPass[0] = 0;                                   /* the session is over — see the guard above */
  LOGF("view", "hotspot: closed%s", apSaves ? " - config was saved" : "");
  if (apSaves) netBegin();                         /* the banner says what we are waiting for now */
  wifiResume();                                    /* wifi.ino — start over with the saved networks */
}

/* Called every frame from loop(); does nothing at all unless the AP is up. */
void hotspotTick(){
  if (!apServer) return;
  if (apDns) apDns->processNextRequest();
  apServer->handleClient();
}

/* ------------------------------------------------------------------ screen
   Everything needed to get in, in the order it is needed: the network, its
   password, then the address. Nothing here is a placeholder — if the AP did
   not come up, this screen says that instead of lying about a password.   */
void drawHotspot(){
  cv->fillScreen(C_BG);
  txt("SETUP", X_L, 8, 2, C_CY, 'l', true);
  cv->fillRect(X_L, 28, X_R - X_L, 1, C_LINE);

  if (!apServer || !apPass[0]){
    txt("HOTSPOT FAILED", 120, 100, 2, C_OR, 'c', true);
    txt("the radio would not raise it", 120, 126, 1, C_DIM2, 'c');
    txt("TAP DOWN - BACK", 120, 196, 1, C_DIM, 'c');
    return;
  }

  txt("JOIN THIS WI-FI", X_L, 34, 1, C_DIM2);
  txt(deviceName, X_L, 45, 2, C_TX, 'l', true);
  txt("PASSWORD", X_L, 68, 1, C_DIM2);
  txt(apPass, X_L, 79, 2, C_CY, 'l', true);
  txt("THEN OPEN", X_L, 102, 1, C_DIM2);
  txt(AP_IP_STR, X_L, 113, 2, C_TX);
  cv->fillRect(X_L, 136, X_R - X_L, 1, C_LINE);

  /* live state, so you can see the phone arrive and the save land */
  const int n = apClients();
  char v[40];
  if (!n) txt("WAITING FOR A PHONE", X_L, 144, 1, C_DIM, 'l');
  else {
    snprintf(v, sizeof v, "%d CONNECTED", n);
    txt(v, X_L, 144, 1, C_GR, 'l', true);
  }
  if (apLastHit) txt("PAGE OPENED", X_R, 144, 1, C_DIM2, 'r');

  if (apSaves){
    snprintf(v, sizeof v, "SAVED %u TIME%s", (unsigned)apSaves, apSaves == 1 ? "" : "S");
    txt(v, X_L, 158, 1, C_GR, 'l', true);
    txt("RESTART FROM THE PAGE TO USE IT", X_L, 170, 1, C_DIM2);
  } else {
    snprintf(v, sizeof v, "%u network%s saved, url %s", (unsigned)cfg.nnets,
             cfg.nnets == 1 ? "" : "s", cfg.url[0] ? "set" : "NOT SET");
    txt(v, X_L, 158, 1, C_DIM2);
  }

  cv->fillRect(X_L, 184, X_R - X_L, 1, C_LINE);
  txt("TAP DOWN - BACK", 120, 192, 1, C_DIM, 'c');
  txt("HOLD DOWN 2 S - BACK", 120, 204, 1, C_DIM2, 'c');
  txt("THE AP CLOSES WHEN YOU LEAVE", 120, 220, 1, C_DIM2, 'c');
}
