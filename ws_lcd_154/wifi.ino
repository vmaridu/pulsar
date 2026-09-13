/* ===========================================================================
   Joining the saved networks — priority, enterprise, and the portals that
   stand between a join and the internet.

   NOTHING HERE BLOCKS FOR LONG. The render loop runs at ~25 fps and a board
   frozen for ten seconds on a network that is not in the room looks broken,
   so joining is a state machine stepped from loop():

       IDLE ──▶ SCAN ──▶ JOIN ──▶ PORTAL ──▶ ONLINE
                  │        │                    │
                  └────────┴──▶ WAIT ──▶ (30 s) ┘   and round again
                                              drop

   · SCAN lists what is actually in the air, so the order the saved networks
     are tried is "highest priority of the ones that are really here" rather
     than "highest priority, then twelve seconds of nothing". Saved networks
     the scan did NOT see go on the end of the list anyway — a hidden SSID
     never appears in a scan and is still perfectly joinable
   · PRIORITY IS THE ORDER OF THE LIST. cfg.nets[0] wins, and the setup page
     is where it gets reordered. There is no score and no signal preference:
     a person who puts the office network first meant it
   · Failing every network is a normal outcome, not an error to escalate.
     The banner says OFFLINE, the last good numbers stay, and it tries again
     in 30 s — a board carried out of range is not a board with a problem

   Two kinds of network need more than an SSID and a password, and they fail
   in completely different places:

   · WPA2-ENTERPRISE authenticates during the association itself — 802.1X
     wants an outer identity and an inner username and password, and without
     them you never get an IP at all. EAP is configured on the driver before
     the join, and explicitly disabled before joining anything else, or the
     last enterprise network's credentials leak into the next join
   · A CAPTIVE PORTAL joins perfectly, hands out an IP, and then holds every
     request until a sign-in form is posted. The join succeeds and the
     backend is still unreachable, which is why every join ends with a probe
     rather than a celebration. Where a network is marked as having one,
     wifiPortalLogin() fetches the page, finds the form, fills the field
     that looks like a username and the field that looks like a password,
     and posts it. That works on the ordinary "accept and sign in" portal
     and it is honestly all it can do: a portal that builds its form in
     JavaScript, or bounces through an identity provider, cannot be driven
     by a device with no browser. When it cannot get through it says so,
     leaves `net.portalBlocked` set, and the settings screen shows PORTAL —
     a device that quietly showed OFFLINE would send you looking at the
     backend for a fault that is three metres away in the ceiling
   =========================================================================== */

#define JOIN_MS      12000      /* an association gets this long before we move on */
#define SCAN_MS       8000      /* a scan that never reports back */
#define RETRY_MS     30000      /* every network failed — wait, then scan again */
#define PROBE_MS      4000      /* the portal probe's own timeout */
#define PORTAL_MS     6000      /* fetching and posting the sign-in form */
#define PROBE_BODY    2048      /* a 204 has no body; a portal's redirect page is small */
#define PORTAL_MAX   10240      /* how much sign-in page we are willing to read */

/* The probe asks a host whose only job is to answer 204 with nothing in it.
   Anything else — a 200, a redirect, a timeout — means something is sitting
   between this device and the internet.                                   */
#define PROBE_URL "http://connectivitycheck.gstatic.com/generate_204"

/* Only the disable is needed by hand: the core's own enterprise begin() sets
   the identity and credentials and enables EAP itself, but nothing in the
   core ever turns it back off, and EAP left enabled offers the last
   enterprise network's identity to the next join — which fails looking
   exactly like a wrong password. The call names moved between IDF 4 and 5;
   the main sketch picked which header is here.                            */
#if PULSAR_EAP_IDF5
#define EAP_DISABLE() esp_wifi_sta_enterprise_disable()
#else
#define EAP_DISABLE() esp_wifi_sta_wpa2_ent_disable()
#endif

/* Case-insensitive substring. strcasestr() is a GNU extension and not worth
   depending on here; the portal parser below leans on this heavily.       */
static const char* strFindNoCase(const char* hay, const char* needle){
  if (!hay || !needle || !needle[0]) return nullptr;
  const size_t n = strlen(needle);
  for (; *hay; hay++) if (!strncasecmp(hay, needle, n)) return hay;
  return nullptr;
}

/* ------------------------------------------------------------------ state */

const char* wifiStateName(uint8_t s){
  switch (s){
    case WS_IDLE:      return "idle";
    case WS_SCAN:      return "scanning";
    case WS_JOIN:      return "joining";
    case WS_PORTAL:    return "checking the way out";
    case WS_ONLINE:    return "online";
    case WS_WAIT:      return "waiting to retry";
    case WS_SUSPENDED: return "suspended";
    default:           return "?";
  }
}
static void wifiGo(uint8_t s){
  if (wifiState == s) return;
  wifiState   = s;
  wifiStateT0 = millis();
}

/* What the settings screen and the STATUS band read. Kept in one place so
   "connected" can never disagree with the IP beside it.                   */
static void wifiNoteUp(){
  net.connected = true;
  net.rssi      = WiFi.RSSI();
  strlcpy(net.ssid, WiFi.SSID().c_str(), sizeof net.ssid);
  strlcpy(net.ip,   WiFi.localIP().toString().c_str(), sizeof net.ip);
}
static void wifiNoteDown(){
  net.connected     = false;
  net.portalBlocked = false;
  net.rssi          = 0;
  net.ip[0]         = 0;
  net.ssid[0]       = 0;
}

/* ------------------------------------------------------------------- time
   The signed request mode needs a clock the backend will accept — api.md's
   HMAC appendix rejects a timestamp more than 60 s out, and this board has
   no RTC, so it starts in 1970 every time. SNTP is asked once per join and
   polled for an answer; nothing waits on it. The STATUS band's clock is
   unaffected either way: that shows the payload's own measured_at, which is
   the time of the data and not the time now.                              */
static void wifiTimeBegin(){
  net.timeSynced = false;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}
static void wifiTimePoll(){
  if (net.timeSynced) return;
  if (time(nullptr) < 1700000000L) return;            /* still 1970 — no answer yet */
  net.timeSynced = true;
  LOG("wifi", "clock set from SNTP - signed requests can be timestamped");
}

/* -------------------------------------------------------------- url helpers
   Small and deliberately dumb: enough to follow a portal's own form action,
   which is either absolute, rooted, or relative to the page it came on.    */
static void urlOrigin(const char* url, char* out, size_t cap){
  out[0] = 0;
  const char* p = strstr(url, "://");
  if (!p) return;
  const char* slash = strchr(p + 3, '/');
  const size_t n = slash ? (size_t)(slash - url) : strlen(url);
  if (n + 1 > cap) return;
  memcpy(out, url, n);
  out[n] = 0;
}
static void urlResolve(const char* base, const char* rel, char* out, size_t cap){
  if (!rel || !rel[0]){ strlcpy(out, base, cap); return; }
  if (!strncasecmp(rel, "http://", 7) || !strncasecmp(rel, "https://", 8)){
    strlcpy(out, rel, cap);
    return;
  }
  char origin[96];
  urlOrigin(base, origin, sizeof origin);
  if (rel[0] == '/'){ snprintf(out, cap, "%s%s", origin, rel); return; }
  /* relative to the page's directory */
  char dir[160];
  strlcpy(dir, base, sizeof dir);
  char* lastSlash = strrchr(dir + (strstr(dir, "://") ? 8 : 0), '/');
  if (lastSlash) *lastSlash = 0;
  snprintf(out, cap, "%s/%s", dir[0] ? dir : origin, rel);
}
static void urlEncodeInto(String& out, const char* s){
  for (const char* p = s; *p; p++){
    const unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else if (c == ' ') out += '+';
    else { char hex[4]; snprintf(hex, sizeof hex, "%%%02X", c); out += hex; }
  }
}

/* ------------------------------------------------------------- html helpers
   One attribute out of one tag. Handles "…", '…' and bare, and never reads
   past the tag it was given.                                              */
static bool tagAttr(const char* tag, const char* tagEnd, const char* attr,
                    char* out, size_t cap){
  out[0] = 0;
  char pat[24];
  snprintf(pat, sizeof pat, " %s", attr);
  for (const char* p = tag; p && p < tagEnd; ){
    p = strFindNoCase(p, pat);
    if (!p || p >= tagEnd) return false;
    const char* q = p + strlen(pat);
    while (q < tagEnd && (*q == ' ' || *q == '\t')) q++;
    if (q >= tagEnd || *q != '='){ p = q; continue; }          /* a different attribute, e.g. "named" */
    q++;
    while (q < tagEnd && (*q == ' ' || *q == '\t')) q++;
    char quote = 0;
    if (q < tagEnd && (*q == '"' || *q == '\'')){ quote = *q; q++; }
    const char* start = q;
    while (q < tagEnd && ((quote && *q != quote) || (!quote && *q != ' ' && *q != '>' && *q != '\t'))) q++;
    const size_t n = (size_t)(q - start);
    if (n + 1 > cap) return false;
    memcpy(out, start, n);
    out[n] = 0;
    return true;
  }
  return false;
}
/* Does this input's name read like the thing we are trying to fill? Portals
   name their fields every way a developer ever has — user, username, uname,
   login, email, id — so match loosely rather than guess one spelling.     */
static bool nameLooksLikeUser(const char* n){
  return strFindNoCase(n, "user") || strFindNoCase(n, "login") || strFindNoCase(n, "email")
      || strFindNoCase(n, "uname") || strFindNoCase(n, "account") || !strcasecmp(n, "id");
}
static bool nameLooksLikePass(const char* n){
  return strFindNoCase(n, "pass") || strFindNoCase(n, "pwd") || strFindNoCase(n, "secret");
}

/* ------------------------------------------------------------------- probe
   Is there a way out of this network? 204-and-nothing means yes. Anything
   else hands back where the portal wants us to go, when it said.          */
static bool wifiProbeOnline(char* portalUrl, size_t cap){
  if (portalUrl) portalUrl[0] = 0;
  WiFiClient c;
  HTTPClient http;
  http.setConnectTimeout(PROBE_MS);
  http.setTimeout(PROBE_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(c, PROBE_URL)){
    LOG("wifi", "probe: could not start the request");
    return false;
  }
  const int code = http.GET();
  const String loc = http.getLocation();
  http.end();

  if (code == 204){ LOG("wifi", "probe: 204 - the way out is clear"); return true; }
  if (code <= 0){
    LOGF("wifi", "probe: failed (%s) - no route out yet", HTTPClient::errorToString(code).c_str());
    return false;
  }
  if (portalUrl){
    if (loc.length()) strlcpy(portalUrl, loc.c_str(), cap);
    else              strlcpy(portalUrl, PROBE_URL, cap);     /* it answered 200 itself — ask it again, fully */
  }
  LOGF("wifi", "probe: %d%s%s - something is intercepting, a sign-in page most likely",
       code, loc.length() ? " -> " : "", loc.length() ? loc.c_str() : "");
  return false;
}

/* ---------------------------------------------------------- the sign-in form
   Where to post, and what to post, out of a portal's HTML. Split out from the
   requests around it on purpose: this is heuristic pointer work over somebody
   else's markup, and it is the one piece here that can be tested properly
   without a radio in the room.

   Hidden inputs are carried through exactly as they came — they are how a
   portal remembers which client is asking, and dropping them is why a
   hand-rolled sign-in usually fails. The username and password go into
   whichever fields look like they are for them; everything else keeps its
   own value.

   Returns false when there is nothing worth posting, having said why.     */
static bool portalForm(const char* html, const char* pageUrl, const struct WifiNet& w,
                       char* post, size_t pcap, String& body,
                       int* users, int* passes, int* carried){
  *users = *passes = *carried = 0;
  body = String("");

  const char* form = strFindNoCase(html, "<form");
  if (!form){
    LOG("wifi", "portal: no <form> on the page - it builds one in JavaScript, "
                "which this device cannot run. Sign in once from a phone on this network");
    return false;
  }
  const char* formTagEnd = strchr(form, '>');
  if (!formTagEnd){ LOG("wifi", "portal: the <form> tag never closes - giving up"); return false; }
  const char* formEnd = strFindNoCase(form, "</form>");
  if (!formEnd) formEnd = html + strlen(html);

  char action[192] = "", method[8] = "";
  tagAttr(form, formTagEnd, "action", action, sizeof action);
  tagAttr(form, formTagEnd, "method", method, sizeof method);
  urlResolve(pageUrl, action, post, pcap);

  for (const char* in = form; in && in < formEnd; ){
    in = strFindNoCase(in, "<input");
    if (!in || in >= formEnd) break;
    const char* end = strchr(in, '>');
    if (!end || end > formEnd) break;

    char name[64] = "", type[24] = "", value[192] = "";
    if (tagAttr(in, end, "name", name, sizeof name) && name[0]){
      tagAttr(in, end, "type", type, sizeof type);
      tagAttr(in, end, "value", value, sizeof value);
      const bool isPass = !strcasecmp(type, "password") || nameLooksLikePass(name);
      const bool isUser = !isPass && (!strcasecmp(type, "text") || !strcasecmp(type, "email")
                                      || !type[0]) && nameLooksLikeUser(name);
      const char* send = value;
      if (isPass){ send = w.portalPass; (*passes)++; }
      else if (isUser){ send = w.portalUser; (*users)++; }
      else if (!strcasecmp(type, "checkbox") || !strcasecmp(type, "radio")){
        send = value[0] ? value : "on";                  /* the terms box a portal insists on */
        (*carried)++;
      } else (*carried)++;

      if (body.length()) body += "&";
      urlEncodeInto(body, name);
      body += "=";
      urlEncodeInto(body, send);
    }
    in = end + 1;
  }

  LOGF("wifi", "portal: form -> %s %s (%d username, %d password, %d other field%s)",
       method[0] ? method : "POST", post, *users, *passes, *carried, *carried == 1 ? "" : "s");
  if (!*passes && !*users){
    LOG("wifi", "portal: the form has no field that looks like a username or a password - "
                "not posting a guess at it");
    return false;
  }
  return true;
}

/* ------------------------------------------------------------ portal login
   Fetch the page, read its form, post it. Best effort, and it says which
   step it got to every time. See the header for what it can and cannot do. */
static bool wifiPortalLogin(const struct WifiNet& w, const char* portalUrl){
  if (!w.portalUser[0] && !w.portalPass[0]){
    LOG("wifi", "portal: nothing saved to sign in with - leaving it");
    return false;
  }
  LOGF("wifi", "portal: fetching the sign-in page at %s", portalUrl);

  WiFiClient c;
  HTTPClient http;
  http.setConnectTimeout(PORTAL_MS);
  http.setTimeout(PORTAL_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(c, portalUrl)){ LOG("wifi", "portal: could not start the request"); return false; }
  const int code = http.GET();
  if (code <= 0){
    LOGF("wifi", "portal: fetch failed (%s)", HTTPClient::errorToString(code).c_str());
    http.end();
    return false;
  }
  String page = http.getString();
  http.end();
  if (page.length() > PORTAL_MAX) page.remove(PORTAL_MAX);
  LOGF("wifi", "portal: page is %u bytes, status %d", (unsigned)page.length(), code);

  char post[224];
  String body;
  int users = 0, passes = 0, carried = 0;
  if (!portalForm(page.c_str(), portalUrl, w, post, sizeof post, body, &users, &passes, &carried))
    return false;

  WiFiClient c2;
  HTTPClient up;
  up.setConnectTimeout(PORTAL_MS);
  up.setTimeout(PORTAL_MS);
  up.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!up.begin(c2, post)){ LOG("wifi", "portal: could not start the sign-in post"); return false; }
  up.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const int sent = up.POST(body);
  up.end();
  if (sent <= 0){
    LOGF("wifi", "portal: sign-in post failed (%s)", HTTPClient::errorToString(sent).c_str());
    return false;
  }
  LOGF("wifi", "portal: signed in, status %d - checking the way out again", sent);
  return true;
}

/* -------------------------------------------------------------- candidates
   The saved networks worth trying, best first: the ones the scan actually
   saw in saved order, then the ones it did not. A hidden SSID is never in a
   scan and joins perfectly well, so it belongs on the list — just after
   everything we have proof of.                                            */
static void wifiBuildCandidates(int found){
  wifiNcand = 0;
  wifiTry   = 0;
  bool seen[MAX_NETWORKS];
  for (int i = 0; i < MAX_NETWORKS; i++) seen[i] = false;

  for (int i = 0; i < cfg.nnets; i++){
    for (int j = 0; j < found; j++){
      if (!strcmp(cfg.nets[i].ssid, WiFi.SSID(j).c_str())){
        seen[i] = true;
        wifiCand[wifiNcand++] = (uint8_t)i;
        LOGF("wifi", "  candidate %u: \"%s\" %s, %d dBm", (unsigned)wifiNcand,
             cfg.nets[i].ssid, secName(cfg.nets[i].security), WiFi.RSSI(j));
        break;
      }
    }
  }
  for (int i = 0; i < cfg.nnets; i++){
    if (seen[i] || wifiNcand >= MAX_NETWORKS) continue;
    wifiCand[wifiNcand++] = (uint8_t)i;
    LOGF("wifi", "  candidate %u: \"%s\" %s, not in the scan - trying it anyway (hidden?)",
         (unsigned)wifiNcand, cfg.nets[i].ssid, secName(cfg.nets[i].security));
  }
}

/* ------------------------------------------------------------------- join */
static void wifiJoinOne(const struct WifiNet& w){
  LOGF("wifi", "joining \"%s\" (%s%s)", w.ssid, secName(w.security),
       w.portal ? ", expect a sign-in page" : "");
  WiFi.disconnect(false, false);

  /* Always clear enterprise first: left enabled, the previous network's
     identity is offered to this one, which fails in a way that looks like a
     wrong password.                                                       */
  EAP_DISABLE();

  if (w.security == SEC_ENT){
#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
    /* PEAP, which is what all but a handful of 802.1X networks ask for. The
       core's overload sets the identity and both credentials and turns EAP
       on, then associates.                                               */
    LOGF("wifi", "enterprise: PEAP, identity \"%s\", user \"%s\"", w.identity, w.user);
    WiFi.begin(w.ssid, WPA2_AUTH_PEAP, w.identity, w.user, w.pass);
#else
    LOGF("wifi", "\"%s\" is enterprise, but this core was built without 802.1X support - skipping it", w.ssid);
#endif
  } else if (w.security == SEC_OPEN){
    WiFi.begin(w.ssid);
  } else {
    WiFi.begin(w.ssid, w.psk);
  }
}

/* Why the last attempt did not work, in the words the radio used. */
static const char* wifiStatusName(wl_status_t s){
  switch (s){
    case WL_NO_SSID_AVAIL:  return "not found in range";
    case WL_CONNECT_FAILED: return "rejected - wrong password, or 802.1X refused it";
    case WL_CONNECTION_LOST:return "connection lost";
    case WL_DISCONNECTED:   return "disconnected";
    case WL_IDLE_STATUS:    return "idle";
    case WL_SCAN_COMPLETED: return "scan completed";
    case WL_CONNECTED:      return "connected";
    default:                return "no answer";
  }
}

/* --------------------------------------------------------------- lifecycle */
void wifiBegin(){
  WiFi.persistent(false);            /* the config here is the only config — NVS is ours, not the driver's */
  WiFi.setAutoReconnect(false);      /* this state machine decides what to join and when */
  WiFi.mode(WIFI_STA);
  wifiNoteDown();
  if (!cfg.nnets){
    LOG("wifi", "no saved networks - the radio stays idle. Hold LEFT 2 s to add one");
    wifiGo(WS_SUSPENDED);
    return;
  }
  LOGF("wifi", "%u saved network%s, in priority order", (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s");
  wifiGo(WS_IDLE);
}

/* The hotspot needs the radio to itself, and coming back from it the saved
   networks may be completely different ones — so resuming always starts over
   from a fresh scan rather than trying to pick up where it left off.      */
void wifiSuspend(){
  if (wifiState == WS_SUSPENDED) return;
  LOG("wifi", "suspended - the setup hotspot has the radio");
  if (WiFi.scanComplete() == -1) WiFi.scanDelete();
  WiFi.disconnect(false, false);
  wifiNoteDown();
  wifiGo(WS_SUSPENDED);
}
void wifiResume(){
  LOG("wifi", "resuming - starting over with the saved networks");
  WiFi.mode(WIFI_STA);
  wifiNoteDown();
  wifiGo(cfg.nnets ? WS_IDLE : WS_SUSPENDED);
  if (!cfg.nnets) LOG("wifi", "still no saved networks - the radio stays idle");
}

/* ------------------------------------------------------------------- tick */
void wifiTick(){
  const uint32_t now = millis();

  switch (wifiState){

    case WS_SUSPENDED:
      return;

    case WS_IDLE: {
      if (!cfg.nnets){ wifiGo(WS_SUSPENDED); return; }
      const int16_t r = WiFi.scanNetworks(true /* async */);
      if (r == WIFI_SCAN_FAILED){
        LOG("wifi", "scan would not start - retrying in 30 s");
        wifiGo(WS_WAIT);
        return;
      }
      LOG("wifi", "scanning for the saved networks");
      wifiGo(WS_SCAN);
      return;
    }

    case WS_SCAN: {
      const int found = WiFi.scanComplete();
      if (found == -1){                                  /* still running */
        if (now - wifiStateT0 > SCAN_MS){
          LOG("wifi", "scan did not finish - retrying in 30 s");
          WiFi.scanDelete();
          wifiGo(WS_WAIT);
        }
        return;
      }
      if (found < 0){
        LOG("wifi", "scan failed - retrying in 30 s");
        wifiGo(WS_WAIT);
        return;
      }
      LOGF("wifi", "scan found %d network%s in range", found, found == 1 ? "" : "s");
      wifiBuildCandidates(found);
      WiFi.scanDelete();
      if (!wifiNcand){
        LOG("wifi", "none of the saved networks are reachable - retrying in 30 s");
        wifiGo(WS_WAIT);
        return;
      }
      wifiJoinOne(cfg.nets[wifiCand[0]]);
      wifiGo(WS_JOIN);
      return;
    }

    case WS_JOIN: {
      const wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED){
        wifiNoteUp();
        wifiJoined = now;
        LOGF("wifi", "joined \"%s\" - %s, %d dBm, took %lums",
             net.ssid, net.ip, net.rssi, (unsigned long)(now - wifiStateT0));
        wifiTimeBegin();
        wifiGo(WS_PORTAL);
        return;
      }
      const bool hard = (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL);
      if (!hard && now - wifiStateT0 < JOIN_MS) return;  /* still trying */

      const uint8_t idx = wifiTry < wifiNcand ? wifiCand[wifiTry] : 0;
      LOGF("wifi", "\"%s\" did not join: %s",
           idx < cfg.nnets ? cfg.nets[idx].ssid : "?", wifiStatusName(st));
      wifiTry++;
      if (wifiTry < wifiNcand){
        wifiJoinOne(cfg.nets[wifiCand[wifiTry]]);
        wifiStateT0 = now;                               /* same state, next candidate */
        return;
      }
      WiFi.disconnect(false, false);
      wifiNoteDown();
      LOG("wifi", "every saved network failed - retrying in 30 s");
      setFault("OFFLINE", "no wi-fi");
      wifiGo(WS_WAIT);
      return;
    }

    case WS_PORTAL: {
      /* One blocking probe, a few seconds at worst, and only right after a
         join — not on the poll path.                                      */
      char portalUrl[224];
      if (wifiProbeOnline(portalUrl, sizeof portalUrl)){
        net.portalBlocked = false;
        LOG("wifi", "online");
        wifiGo(WS_ONLINE);
        netFetch();                                      /* the dashboard has been empty long enough */
        return;
      }
      if (wifiTry >= wifiNcand || wifiCand[wifiTry] >= cfg.nnets){
        LOG("wifi", "the candidate list moved under us - rescanning");
        wifiGo(WS_IDLE);
        return;
      }
      const struct WifiNet& w = cfg.nets[wifiCand[wifiTry]];
      if (portalUrl[0] && w.portal && wifiPortalLogin(w, portalUrl)
          && wifiProbeOnline(nullptr, 0)){
        net.portalBlocked = false;
        LOG("wifi", "online - the sign-in page accepted the saved credentials");
        wifiGo(WS_ONLINE);
        netFetch();
        return;
      }
      /* Associated, with an IP, and still walled in. Stay here rather than
         churn the radio: the network is fine, the way out is not, and the
         poll will say so honestly every cycle.                            */
      net.portalBlocked = portalUrl[0] != 0;
      if (net.portalBlocked)
        LOGF("wifi", "joined \"%s\" but a sign-in page is still in the way%s",
             net.ssid, w.portal ? "" : " - mark it as needing sign-in in the setup page");
      else
        LOGF("wifi", "joined \"%s\" but nothing answers out there", net.ssid);
      setFault("PORTAL", net.portalBlocked ? "wi-fi sign-in" : "no route out");
      wifiGo(WS_ONLINE);
      return;
    }

    case WS_ONLINE: {
      if (WiFi.status() != WL_CONNECTED){
        LOGF("wifi", "dropped \"%s\" after %lus - rescanning",
             net.ssid, (unsigned long)((now - wifiJoined) / 1000));
        wifiNoteDown();
        setFault("OFFLINE", "wi-fi dropped");
        wifiGo(WS_IDLE);
        return;
      }
      net.rssi = WiFi.RSSI();
      wifiTimePoll();
      /* A portal that walled us in may have been signed into from a phone
         since; re-probe now and then so the device lets itself out.       */
      if (net.portalBlocked && now - wifiStateT0 > RETRY_MS){
        wifiStateT0 = now;
        if (wifiProbeOnline(nullptr, 0)){
          net.portalBlocked = false;
          LOG("wifi", "the sign-in page has let us out - online");
          clearFault();
          netFetch();
        }
      }
      return;
    }

    case WS_WAIT:
      if (now - wifiStateT0 >= RETRY_MS) wifiGo(WS_IDLE);
      return;
  }
}
