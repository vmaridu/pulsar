/* ===========================================================================
   Network — the poll, the screen cycle, and what happens when a poll fails.

   ONE endpoint. The backend behind it may merge several platforms — every
   metric row carries its own `gateway`, and that is the ONLY place the word
   appears on the wire. There is no root-level `gateway` field: a device
   doesn't have "a" gateway any more than it has "a" metric, it shows
   whatever rows the backend sends, each one labelled.

   NOTHING IS BAKED IN. The base URL, the API key and the API secret come out
   of NVS (config.ino), set over the setup hotspot (hotspot.ino). A board with
   no URL shows SETUP and no numbers: a monitor that invents data is worse
   than one that admits it has none. The simulator (../simulator) serves this
   exact shape over real HTTP, with controls to fake every fault below —
   point a board at it and the whole error path is testable on a bench.

   Auth is one decision taken from one field, so there is no mode to set
   wrongly — api.md §1 and its HMAC appendix:

       secret empty   Authorization: Bearer <key>
       secret set     X-Api-Key: <key>
                      X-Timestamp: <unix seconds>
                      X-Signature: hex HMAC-SHA256(secret, "GET\n<path>\n<ts>\n")

   The signed mode needs a clock the backend will accept, and this board has
   no RTC — wifi.ino asks SNTP for one on every join, and a poll that would
   have to sign with a 1970 timestamp says so rather than send a request that
   is certain to be rejected.

   A FAILED POLL NEVER CLEARS THE SCREEN. The last good payload stays exactly
   where it was, and the fault is said out loud over it — api.md §6. Stale
   numbers shown calmly read as good news, so every fault flashes on the same
   5 s pattern as a warning, and none of them sound: a Wi-Fi roam must not
   sound like an outage.

   The cycle, and why the poll is tied to it:

     · each metric screen is shown for SCREEN_MS (5 s)
     · when the cycle wraps back to the first screen, the data is refetched
     · but never faster than POLL_MIN_S — so the interval is
           max(30, metric_count x 5) seconds
       Five metrics is a 25 s lap, so it polls on every second lap; ten
       metrics would be a 50 s lap and poll on every lap.
     · a 2 s hold on the glass forces a poll whatever the cycle is doing
     · a 429 holds the next poll off for its Retry-After

   Every poll prints its outcome on Serial — success and failure alike, with
   the real error text. That is the only way to debug this board.
   =========================================================================== */

#define SCREEN_MS     5000      /* each metric screen is up this long */
#define POLL_MIN_S      30      /* never ask the backend faster than this */
#define FETCH_MS      5000      /* api.md §1: clients time out at 5 s */
#define NET_MAX_BODY  8192      /* api.md §5 caps a body at 4 KB — this is the hard stop */
#define RETRY_AFTER_MAX_S 900   /* however long a 429 asks for, we come back inside this */

/* The one path every build asks for, appended to the configured base URL. */
#define HEALTH_PATH "/v1/gateway_health"

static uint32_t lastPoll      = 0;  /* millis() of the last completed poll */
static uint32_t pollHoldUntil = 0;  /* a 429's Retry-After — nothing polls before this */

/* ------------------------------------------------------------------- auth
   hex HMAC-SHA256 over the string api.md's appendix defines. mbedtls ships
   with the core, so there is nothing to install for this.                 */
static void hmacSha256Hex(const char* secret, const char* msg, char* out, size_t cap){
  out[0] = 0;
  uint8_t mac[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info || cap < 65) return;
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, info, 1 /* HMAC */) == 0
      && mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret)) == 0
      && mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg, strlen(msg)) == 0
      && mbedtls_md_hmac_finish(&ctx, mac) == 0){
    for (int i = 0; i < 32; i++) snprintf(out + i * 2, cap - i * 2, "%02x", mac[i]);
  }
  mbedtls_md_free(&ctx);
}

/* The request path, for the signature: everything the origin is not. */
static void urlPathOf(const char* url, char* out, size_t cap){
  const char* p = strstr(url, "://");
  const char* slash = p ? strchr(p + 3, '/') : nullptr;
  strlcpy(out, slash ? slash : "/", cap);
}
/* ------------------------------------------------------------------ parse
   Fills `snap` from a JSON document — but never directly. Every field is
   read into a scratch copy first; `snap` (what render() actually draws) is
   only overwritten once parsing has fully succeeded, so a bad or partial
   poll can never leave the screen showing a torn mix of old and new data —
   the last good payload stays exactly as api.md §6 promises.

   Nothing here can crash on a null, missing or wrong-typed field: every
   read has a safe default, and every time reality doesn't match the
   contract it is logged to Serial BEFORE anything is drawn from it — a
   silent client is a client you cannot debug, and a client that renders
   first and discovers a problem after is a client that already crashed.  */
static bool parseSnapshot(const char* body){
  if (!body){ LOG("data", "poll body is NULL - keeping last good data"); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err){ LOGF("data", "parse FAILED: %s - keeping last good data", err.c_str()); return false; }

  JsonObjectConst o = doc.as<JsonObjectConst>();
  if (o.isNull()){ LOG("data", "parse FAILED: body is not a JSON object - keeping last good data"); return false; }

  static Snapshot next;                 /* static: off the stack, zeroed every attempt */
  memset(&next, 0, sizeof next);

  if (o["measured_at"].isNull()) LOG("data", "measured_at missing - defaulting to 0");
  next.measured_at = o["measured_at"] | 0L;
  next.polledAt    = millis();

  JsonVariantConst alert = o["alert"];
  if (alert.isNull()) LOG("data", "alert missing entirely - defaulting to info/empty");
  if (alert["level"].isNull())   LOG("data", "alert.level missing - defaulting to \"info\"");
  if (alert["message"].isNull()) LOG("data", "alert.message missing - defaulting to empty");
  strlcpy(next.level,   alert["level"]   | "info", sizeof next.level);
  strlcpy(next.message, alert["message"] | "",     sizeof next.message);

  JsonVariantConst metricsV = o["metrics"];
  if (!metricsV.is<JsonArrayConst>()){
    LOG("data", "metrics missing or not an array - nothing to show - keeping last good data");
    return false;
  }

  next.nrows = 0;
  for (JsonVariantConst mv : metricsV.as<JsonArrayConst>()){
    if (next.nrows >= MAX_ROWS){ LOGF("data", "more than %d metrics - the rest ignored", MAX_ROWS); break; }
    if (mv.isNull() || !mv.is<JsonObjectConst>()){ LOG("data", "a metrics[] entry is null - skipped"); continue; }
    JsonObjectConst m = mv.as<JsonObjectConst>();
    Row& r = next.rows[next.nrows];

    if (m["gateway"].isNull())
      LOGF("data", "metric %u missing required \"gateway\" - defaulting to \"-\"", (unsigned)next.nrows);
    strlcpy(r.gateway, m["gateway"] | "-", sizeof r.gateway);
    if (m["name"].isNull())
      LOGF("data", "metric %u (%s) missing \"name\" - defaulting to \"-\"", (unsigned)next.nrows, r.gateway);
    strlcpy(r.name, m["name"] | "-", sizeof r.name);    /* the FOOTER text too - api.md §5 */
    strlcpy(r.unit, m["bucket_unit"] | "m", sizeof r.unit);
    r.size   = m["bucket_size"]  | 1;
    r.count  = m["bucket_count"] | 30;

    JsonVariantConst aggs = m["aggregates"];
    if (!aggs.is<JsonArrayConst>())
      LOGF("data", "%s/%s: aggregates missing or not an array - no tiles shown", r.gateway, r.name);
    r.ntiles = 0;
    if (aggs.is<JsonArrayConst>()){
      for (JsonVariantConst tv : aggs.as<JsonArrayConst>()){
        if (r.ntiles >= MAX_TILES){
          LOGF("data", "%s/%s: more than %d aggregate tiles - the rest ignored", r.gateway, r.name, MAX_TILES);
          break;
        }
        if (tv.isNull() || !tv.is<JsonObjectConst>()){ LOG("data", "an aggregates[] entry is null - skipped"); continue; }
        JsonObjectConst t = tv.as<JsonObjectConst>();
        Tile& tl = r.tiles[r.ntiles];

        if (t["name"].isNull())
          LOGF("data", "%s/%s: tile %u missing \"name\" - defaulting to \"-\"", r.gateway, r.name, (unsigned)r.ntiles);
        strlcpy(tl.name, t["name"] | "-", sizeof tl.name);     /* sizeof cuts it to 5 chars - IS the render text, api.md §5 */
        tl.value = t["value"] | 0.0;
        strlcpy(tl.unit, t["unit"] | "", sizeof tl.unit);
        strlcpy(tl.level, t["level"] | "info", sizeof tl.level);  /* the backend's call, never computed here */
        r.ntiles++;
      }
    }

    /* a series type this build does not know draws no graph rather than guess */
    const char* vtype = m["buckets_value_type"] | "";
    bool known = !strcmp(vtype, "total_count");
    if (!known && m["buckets"].is<JsonArrayConst>())
      LOGF("data", "%s/%s: buckets_value_type '%s' not supported - no graph",
           r.gateway, r.name, vtype);

    r.nbuckets = 0;
    if (known && m["buckets"].is<JsonArrayConst>()){
      for (JsonVariantConst v : m["buckets"].as<JsonArrayConst>()){
        if (r.nbuckets >= MAX_BUCKETS) break;
        int b = v.isNull() ? 0 : v.as<int>();          // a hole in the array is a zero
        r.buckets[r.nbuckets++] = b < 0 ? 0 : b;
      }
    } else if (known){
      LOGF("data", "%s/%s: buckets_value_type sent but \"buckets\" is missing - no graph", r.gateway, r.name);
    }
    /* short array → left-pad with zeros, the missing history is the old end */
    if (r.nbuckets && r.nbuckets < r.count && r.count <= MAX_BUCKETS){
      int pad = r.count - r.nbuckets;
      for (int i = r.nbuckets - 1; i >= 0; i--) r.buckets[i + pad] = r.buckets[i];
      for (int i = 0; i < pad; i++) r.buckets[i] = 0;
      r.nbuckets = r.count;
    }
    next.nrows++;
  }

  if (!next.nrows){ LOG("data", "no usable metrics parsed - keeping last good data"); return false; }

  /* Every check above has already run and logged. Only now — never before —
     does the good data replace what is on screen.                        */
  snap = next;

  LOGF("data", "level=%s \"%s\" - %u metric screens", snap.level, snap.message, (unsigned)snap.nrows);
  for (int i = 0; i < snap.nrows; i++){
    const Row& r = snap.rows[i];
    char tiles[96] = "";
    for (int k = 0; k < r.ntiles; k++){
      char one[24];
      snprintf(one, sizeof one, "%s%s=%g", k ? " " : "", r.tiles[k].name, r.tiles[k].value);
      strlcat(tiles, one, sizeof tiles);
    }
    LOGF("data", "  %u/%u %s/%s  %s  %u buckets",
         (unsigned)(i + 1), (unsigned)snap.nrows, r.gateway, r.name, tiles, (unsigned)r.nbuckets);
  }
  return true;
}

/* ------------------------------------------------------------------ fetch */

/* Read the body with a hard ceiling, so a backend that answers with a
   gigabyte cannot take the board down. api.md §5 caps a real body at 4 KB;
   anything past NET_MAX_BODY is refused rather than truncated, because half
   a JSON document is not a smaller JSON document.                         */
static bool fetchBody(HTTPClient& http, char** body, size_t* len){
  static char buf[NET_MAX_BODY + 1];          /* one poll at a time — no need for two */
  *body = buf; *len = 0;

  const int declared = http.getSize();
  if (declared > NET_MAX_BODY){
    LOGF("net", "body says it is %d bytes - over the %d limit, refusing to read it",
         declared, NET_MAX_BODY);
    return false;
  }
  WiFiClient* s = http.getStreamPtr();
  if (!s){ LOG("net", "no stream to read the body from"); return false; }

  size_t n = 0;
  const uint32_t t0 = millis();
  while (n < NET_MAX_BODY && millis() - t0 < FETCH_MS){
    if (declared >= 0 && n >= (size_t)declared) break;
    const int avail = s->available();
    if (avail <= 0){
      if (!s->connected()) break;
      delay(4);
      continue;
    }
    size_t want = NET_MAX_BODY - n;
    if ((size_t)avail < want) want = (size_t)avail;
    const int got = s->readBytes(buf + n, want);
    if (got <= 0) break;
    n += (size_t)got;
  }
  buf[n] = 0;
  *len = n;
  if (n >= NET_MAX_BODY){
    LOGF("net", "body hit the %d byte ceiling - refusing it", NET_MAX_BODY);
    return false;
  }
  if (n > 4096) LOGF("net", "body is %u bytes - api.md caps it at 4 KB", (unsigned)n);
  return n > 0;
}

/* Turn an HTTP status into the banner api.md §6 asks for. Returns the word;
   fills `detail` with the line under it.                                   */
static const char* faultForStatus(int code, char* detail, size_t cap){
  if (code >= 300 && code < 400){ snprintf(detail, cap, "fix the url (%d)", code); return "REDIRECT"; }
  if (code == 401 || code == 403){ snprintf(detail, cap, "key rejected (%d)", code); return "NO ACCESS"; }
  if (code == 404){ strlcpy(detail, "wrong url", cap); return "NOT FOUND"; }
  if (code == 429){ strlcpy(detail, "backing off", cap); return "THROTTLED"; }
  if (code >= 500){ snprintf(detail, cap, "backend said %d", code); return "BACKEND"; }
  snprintf(detail, cap, "http %d", code);
  return "BACKEND";
}

/* One poll. Everything it decides, it logs — the URL, the auth it used, the
   status, the parse result, and the real error text when there is one.    */
static bool netFetch(){
  const uint32_t t0 = millis();

  /* ---- the reasons not to even try, each of them said out loud */
  if (!configHasEndpoint()){
    LOG("net", "no endpoint configured - hold LEFT 2 s, join the hotspot, set the URL");
    setFault("SETUP", "hold LEFT 2 s");
    lastPoll = millis();
    return false;
  }
  if (!net.connected){
    LOGF("net", "no network (%s) - cannot poll %s", wifiStateName(wifiState), cfg.url);
    setFault("OFFLINE", cfg.nnets ? "no wi-fi" : "no wi-fi saved");
    lastPoll = millis();
    return false;
  }
  if (net.portalBlocked){
    LOG("net", "a wi-fi sign-in page is still in the way - the backend is unreachable");
    setFault("PORTAL", "wi-fi sign-in");
    lastPoll = millis();
    return false;
  }

  char url[LEN_URL + 32];
  snprintf(url, sizeof url, "%s%s", cfg.url, HEALTH_PATH);
  const bool https = !strncasecmp(url, "https://", 8);

  /* ---- the client. A pasted PEM root is checked against; without one the
     connection is encrypted but unauthenticated, and that is said on every
     single poll rather than once at boot where it scrolls away.           */
  WiFiClient*       plain = nullptr;
  WiFiClientSecure* tls   = nullptr;
  if (https){
    tls = new WiFiClientSecure();
    if (!tls){ LOG("net", "out of memory for the TLS client"); setFault("OFFLINE", "out of memory"); return false; }
    if (configTlsVerified()) tls->setCACert(cfg.ca);
    else                     tls->setInsecure();
    /* No setTimeout() here on purpose: HTTPClient::setTimeout() below pushes
       its own value into the client, and this client's units have meant both
       seconds and milliseconds across core versions.                      */
  } else {
    plain = new WiFiClient();
    if (!plain){ LOG("net", "out of memory for the client"); setFault("OFFLINE", "out of memory"); return false; }
  }

  HTTPClient http;
  http.setConnectTimeout(FETCH_MS);
  http.setTimeout(FETCH_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);   /* api.md §6: a 3xx is a misconfigured URL */
  http.setReuse(false);
  /* header() only answers for headers asked for in advance, and Retry-After
     is the one a 429 carries — api.md §7 says to honour it.               */
  static const char* wanted[] = { "Retry-After" };
  http.collectHeaders(wanted, 1);
  const char* warn = https ? (configTlsVerified() ? "" : " [TLS NOT VERIFIED - no CA pasted]")
                           : " [PLAIN HTTP - the key is readable on the wire]";
  LOGF("net", "poll -> %s%s", url, warn);

  const bool began = https ? http.begin(*tls, url) : http.begin(*plain, url);
  if (!began){
    LOG("net", "could not parse that URL - check it in the setup page");
    setFault("SETUP", "bad url");
    http.end();
    delete tls; delete plain;
    lastPoll = millis();
    return false;
  }

  http.addHeader("Accept", "application/json");
  if (configSigned()){
    /* A signature over a 1970 timestamp is a request the backend must reject
       — api.md's appendix rejects anything older than 60 s — so say why
       rather than send it.                                                */
    const time_t nowSec = time(nullptr);
    if (nowSec < 1700000000L){
      LOG("net", "signed mode needs a clock and SNTP has not answered yet - not sending an unsignable request");
      setFault("NO CLOCK", "waiting for time");
      http.end();
      delete tls; delete plain;
      lastPoll = millis();
      return false;
    }
    char path[LEN_URL], ts[24], sig[72], toSign[LEN_URL + 64];
    urlPathOf(url, path, sizeof path);
    snprintf(ts, sizeof ts, "%ld", (long)nowSec);
    snprintf(toSign, sizeof toSign, "GET\n%s\n%s\n", path, ts);   /* GET has an empty body */
    hmacSha256Hex(cfg.secret, toSign, sig, sizeof sig);
    http.addHeader("X-Api-Key", cfg.key);
    http.addHeader("X-Timestamp", ts);
    http.addHeader("X-Signature", sig);
    LOGF("net", "auth: signed - key %.4s..., ts %s", cfg.key, ts);
  } else if (cfg.key[0]){
    char bearer[LEN_KEY + 8];
    snprintf(bearer, sizeof bearer, "Bearer %s", cfg.key);
    http.addHeader("Authorization", bearer);
    LOGF("net", "auth: bearer token %.4s...", cfg.key);
  } else {
    LOG("net", "auth: none - no API key is set");
  }

  const int code = http.GET();
  bool ok = false;

  if (code == 200){
    char* body = nullptr; size_t len = 0;
    if (fetchBody(http, &body, &len)){
      LOGF("net", "200 in %lums, %u bytes", (unsigned long)(millis() - t0), (unsigned)len);
      ok = parseSnapshot(body);
      if (ok) clearFault();
      else    setFault("BAD DATA", "payload rejected");
    } else {
      setFault("BAD DATA", "body unreadable");
    }
  } else if (code > 0){
    char detail[22];
    const char* word = faultForStatus(code, detail, sizeof detail);
    LOGF("net", "%d %s - %s, keeping the last good data", code, word, detail);
    if (code == 429){
      /* api.md §7: honour Retry-After, clamped so a silly value cannot park
         the device for a day.                                             */
      long wait = http.header("Retry-After").toInt();
      if (wait <= 0) wait = POLL_MIN_S;
      if (wait > RETRY_AFTER_MAX_S) wait = RETRY_AFTER_MAX_S;
      pollHoldUntil = millis() + (uint32_t)wait * 1000UL;
      LOGF("net", "Retry-After %lds - next poll held off that long", wait);
    }
    setFault(word, detail);
  } else {
    LOGF("net", "request failed after %lums: %s", (unsigned long)(millis() - t0),
         HTTPClient::errorToString(code).c_str());
    setFault("OFFLINE", https && !configTlsVerified() ? "tls or timeout" : "no answer");
  }

  http.end();
  delete tls; delete plain;
  lastPoll = millis();
  if (!ok && snap.nrows)
    LOGF("net", "holding %u screen%s of data from %lus ago",
         (unsigned)snap.nrows, snap.nrows == 1 ? "" : "s",
         (unsigned long)((millis() - snap.polledAt) / 1000));
  return ok;
}

/* What the banner says before the first poll has had a chance to say
   anything — a blank screen with no explanation is the one thing a device
   with no keyboard must never do.                                         */
static void netBegin(){
  if (!configHasEndpoint())  setFault("SETUP", "hold LEFT 2 s");
  else if (!cfg.nnets)       setFault("SETUP", "no wi-fi saved");
  else                       setFault("OFFLINE", "joining wi-fi");
  LOGF("net", "waiting: %s - %s", faultWord, faultDetail);
}

/* How often the backend is asked: the cycle's lap time, floored at 30 s. */
static uint32_t pollIntervalMs(){
  uint32_t lap = (uint32_t)snap.nrows * SCREEN_MS;
  uint32_t min = (uint32_t)POLL_MIN_S * 1000UL;
  return lap > min ? lap : min;
}

/* A poll the person asked for — the glass held 2 s. Ignores the schedule,
   lights the status hairline and sweeps the graph back in. A forced refresh
   also clears a Retry-After hold: a person asking by hand outranks a backend
   that asked us to wait.                                                  */
static void refreshNow(){
  LOG("net", "forced refresh - glass held 2 s");
  pollHoldUntil = 0;
  beginCount();
  netFetch();
  if (curRow >= snap.nrows) curRow = 0;
  refreshT0 = sweepT0 = screenT0 = millis();
}

/* ------------------------------------------------------------------ cycle */
static void cycleBegin(){
  screenT0 = millis();
  lastPoll = millis();
}

/* A person just chose this screen by hand, or came back from settings — give
   it a full SCREEN_MS before the cycle moves on.                          */
static void cycleResetTimer(){ screenT0 = millis(); }

/* Called every frame. Advances the screen every 5 s, and when the cycle wraps
   back to the first screen it refetches — as long as the poll interval has
   actually elapsed. Both events log what they did and why.

   With nothing parsed yet there are no screens to turn, so the cycle just
   keeps asking on the interval until something answers.                   */
static void cycleTick(){
  if (view != VIEW_MAIN) return;              /* settings and hotspot hold the cycle */
  const uint32_t now = millis();
  if (now - screenT0 < SCREEN_MS) return;

  screenT0 = now;                             /* also resyncs the alert flash — ws_lcd_154.ino */
  const bool wrapped = (curRow + 1 >= snap.nrows);
  if (snap.nrows){
    nextScreen();                             /* dashboard.ino */
    LOGF("view", "auto -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
         snap.rows[curRow].gateway, snap.rows[curRow].name);
  }

  if (!wrapped) return;
  if (pollHoldUntil && (int32_t)(now - pollHoldUntil) < 0){
    LOGF("net", "held off by Retry-After for another %lus",
         (unsigned long)((pollHoldUntil - now) / 1000));
    return;
  }
  pollHoldUntil = 0;
  const uint32_t due = pollIntervalMs();
  if (now - lastPoll >= due){
    LOGF("net", "cycle complete after %lus - refetching", (unsigned long)((now - lastPoll) / 1000));
    netFetch();
    sweepT0 = millis();
  } else {
    LOGF("net", "cycle complete - next poll in %lus (interval %lus)",
         (unsigned long)((due - (now - lastPoll)) / 1000), (unsigned long)(due / 1000));
  }
}
