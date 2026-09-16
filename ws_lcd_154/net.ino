/* ===========================================================================
   Network — the poll, the screen cycle, and what happens when a poll fails.

   ONE endpoint. The backend behind it may merge several platforms — every
   metric row carries its own `gateway`, and that is the ONLY place the word
   appears on the wire. There is no root-level `gateway` field: a device
   doesn't have "a" gateway any more than it has "a" metric, it shows
   whatever rows the backend sends, each one labelled.

   NOTHING IS BAKED IN. The full URL, the API key and the API secret come out
   of NVS (config.ino), set over the setup hotspot (hotspot.ino) — the device
   polls exactly that URL, nothing appended, nothing assumed about its shape.
   A board with no URL shows SETUP and no numbers: a monitor that invents
   data is worse than one that admits it has none. The simulator
   (../simulator) serves this exact shape over real HTTP at /v1/gateway_health
   — a convention for testing, not a path the device requires — with controls
   to fake every fault below, so the whole error path is testable on a bench.

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
   5 s pattern as `warn`, and none of them sound: a Wi-Fi roam must not
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

/* One line off the stream, CRLF (or bare LF) stripped, bounded by `cap` and
   by `deadline` (an absolute millis()). Used only for chunk-size lines and
   the trailer after a chunk's data — never for the JSON itself, so a few
   bytes at a time is not a performance concern.                          */
static bool readStreamLine(WiFiClient* s, char* out, size_t cap, uint32_t deadline){
  size_t n = 0;
  for (;;){
    if ((int32_t)(millis() - deadline) > 0) return false;
    if (!s->available()){
      if (!s->connected()) return false;
      delay(2);
      continue;
    }
    const int c = s->read();
    if (c < 0) continue;
    if (c == '\r') continue;
    if (c == '\n'){ out[n] = 0; return true; }
    if (n + 1 < cap) out[n++] = (char)c;    /* an absurd line just gets truncated, never overrun */
  }
}

/* HTTP chunked transfer-encoding, decoded by hand: hex size, CRLF, that many
   bytes, CRLF, repeat until a zero-size chunk, then whatever trailer headers
   follow up to the blank line that ends them. Same ceiling and "refuse, do
   not truncate" rule as the Content-Length path below.                    */
static bool fetchChunkedBody(WiFiClient* s, char* buf, size_t cap, size_t* outLen, uint32_t deadline){
  size_t n = 0;
  char line[24];
  for (;;){
    if (!readStreamLine(s, line, sizeof line, deadline)){
      LOG("net", "chunked body: timed out or connection closed reading a chunk size");
      *outLen = n;
      return false;
    }
    char* semi = strchr(line, ';');           /* chunk-extensions — legal, always ignorable here */
    if (semi) *semi = 0;
    char* end = nullptr;
    const long size = strtol(line, &end, 16);
    if (end == line || size < 0){
      LOGF("net", "chunked body: not a valid chunk-size line: \"%s\"", line);
      *outLen = n;
      return false;
    }
    if (size == 0) break;                     /* the terminating chunk */
    if (n + (size_t)size > cap){
      LOGF("net", "chunked body: over the %u byte limit, refusing it", (unsigned)cap);
      *outLen = n;
      return false;
    }
    size_t got = 0;
    while (got < (size_t)size){
      if ((int32_t)(millis() - deadline) > 0){
        LOG("net", "chunked body: timed out mid-chunk");
        *outLen = n;
        return false;
      }
      const int avail = s->available();
      if (avail <= 0){
        if (!s->connected()){ LOG("net", "chunked body: connection closed mid-chunk"); *outLen = n; return false; }
        delay(4);
        continue;
      }
      size_t want = (size_t)size - got;
      if ((size_t)avail < want) want = (size_t)avail;
      const int r = s->readBytes(buf + n + got, want);
      if (r <= 0) break;
      got += (size_t)r;
    }
    if (got != (size_t)size){
      LOG("net", "chunked body: fewer bytes arrived than the chunk declared");
      *outLen = n;
      return false;
    }
    n += got;
    if (!readStreamLine(s, line, sizeof line, deadline)){   /* the CRLF after this chunk's data */
      LOG("net", "chunked body: timed out reading the chunk's trailing CRLF");
      *outLen = n;
      return false;
    }
  }
  for (;;){                                   /* optional trailer headers, ended by a blank line */
    if (!readStreamLine(s, line, sizeof line, deadline)) break;
    if (!line[0]) break;
  }
  *outLen = n;
  return true;
}

/* Read the body with a hard ceiling, so a backend that answers with a
   gigabyte cannot take the board down. api.md §5 caps a real body at 4 KB;
   anything past NET_MAX_BODY is refused rather than truncated, because half
   a JSON document is not a smaller JSON document.

   Two wire shapes are handled, because both are common and neither is
   this device's business to reject: a declared Content-Length, or
   Transfer-Encoding: chunked with no length declared at all — a plain
   `readBytes()` loop over the latter hands the parser raw chunk-size lines
   and a trailing "0" instead of JSON, which reads as "InvalidInput" and is
   nothing to do with the JSON itself. Content-Encoding (gzip et al.) is
   refused outright: this device has no decompressor, and parsing compressed
   bytes as text would fail just as confusingly.                          */
static bool fetchBody(HTTPClient& http, char** body, size_t* len){
  static char buf[NET_MAX_BODY + 1];          /* one poll at a time — no need for two */
  *body = buf; *len = 0;

  WiFiClient* s = http.getStreamPtr();
  if (!s){ LOG("net", "no stream to read the body from"); return false; }

  const String encoding = http.header("Content-Encoding");
  if (encoding.length()){
    LOGF("net", "body is Content-Encoding: %s - this device only reads plain JSON, cannot decode it",
         encoding.c_str());
    return false;
  }

  const uint32_t deadline = millis() + FETCH_MS;
  size_t n = 0;
  bool ok;

  if (http.header("Transfer-Encoding").equalsIgnoreCase("chunked")){
    ok = fetchChunkedBody(s, buf, NET_MAX_BODY, &n, deadline);
  } else {
    const int declared = http.getSize();
    if (declared > NET_MAX_BODY){
      LOGF("net", "body says it is %d bytes - over the %d limit, refusing to read it",
           declared, NET_MAX_BODY);
      return false;
    }
    while (n < NET_MAX_BODY && (int32_t)(millis() - deadline) <= 0){
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
    ok = true;                       /* the ceiling check right below still applies */
  }

  if (!ok || n >= NET_MAX_BODY){
    if (n >= NET_MAX_BODY) LOGF("net", "body hit the %d byte ceiling - refusing it", NET_MAX_BODY);
    buf[n < NET_MAX_BODY ? n : NET_MAX_BODY] = 0;
    *len = n;
    return false;
  }

  /* A leading UTF-8 BOM is legal UTF-8 but not legal JSON — some backends'
     JSON serializers add one without asking. Strip it before the parser
     ever sees it, rather than fail on three bytes that carry no data.    */
  if (n >= 3 && (uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF){
    LOG("net", "body starts with a UTF-8 BOM - stripped before parsing");
    memmove(buf, buf + 3, n - 3);
    n -= 3;
  }

  buf[n] = 0;
  *len = n;
  if (n > 4096) LOGF("net", "body is %u bytes - api.md caps it at 4 KB", (unsigned)n);
  return n > 0;
}

/* Turn an HTTP status into the banner api.md §6 asks for. Returns the word;
   fills `detail` with the line under it.                                   */
static const char* faultForStatus(int code, char* detail, size_t cap){
  if (code >= 300 && code < 400){ snprintf(detail, cap, "fix the url (%d)", code); return "REDIRECT"; }
  if (code == 401 || code == 403){ snprintf(detail, cap, "key rejected (%d)", code); return "NO ACCESS"; }
  if (code == 404){ strlcpy(detail, "wrong url", cap); return "NOT FOUND"; }
  if (code == 429){ strlcpy(detail, "backing off", cap); return "SERVER ERROR"; }
  if (code >= 500){ snprintf(detail, cap, "backend said %d", code); return "SERVER ERROR"; }
  snprintf(detail, cap, "http %d", code);
  return "SERVER ERROR";
}

/* One poll. Everything it decides, it logs — the URL, the auth it used, the
   status, the parse result, and the real error text when there is one.    */
bool netFetch(){
  const uint32_t t0 = millis();

  /* ---- the reasons not to even try, each of them said out loud */
  if (!configHasEndpoint()){
    LOG("net", "no endpoint configured - hold DOWN 2 s, join the hotspot, set the URL");
    setFault("SETUP", "hold DOWN 2 s");
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

  char url[LEN_URL];
  strlcpy(url, cfg.url, sizeof url);
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
  /* header() only answers for headers asked for in advance. Retry-After is
     what a 429 carries (api.md §7); Content-Type/-Length are logged on every
     response so a bad-data poll shows exactly what came back and why;
     Transfer-Encoding/Content-Encoding are what fetchBody() reads to decide
     how to read the body at all — chunked framing and a compressed body
     both look like "bad data" to the JSON parser if read as plain bytes.  */
  static const char* wanted[] = { "Retry-After", "Content-Type", "Content-Length", "Server",
                                   "Transfer-Encoding", "Content-Encoding" };
  http.collectHeaders(wanted, 6);
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
  /* Asked for plainly so a well-behaved backend never compresses a body this
     device has no decompressor for — fetchBody() still checks and refuses
     Content-Encoding outright if one shows up anyway.                     */
  http.addHeader("Accept-Encoding", "identity");
  /* Not part of api.md's contract — a real backend owes this device nothing
     back for these — but the simulator (and anything else that wants to
     tell devices apart) can key off them instead of just source IP.       */
  char macStr[18];
  snprintf(macStr, sizeof macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  http.addHeader("X-Device-Id",    deviceName);
  http.addHeader("X-Device-Mac",   macStr);
  http.addHeader("X-Device-Board", "ws_lcd_154");
  LOG("net", "> GET (path + query from the URL above)");
  LOG("net", ">   Accept: application/json");
  LOG("net", ">   Accept-Encoding: identity");
  LOGF("net", ">   X-Device-Id: %s", deviceName);
  LOGF("net", ">   X-Device-Mac: %s", macStr);
  LOG("net", ">   X-Device-Board: ws_lcd_154");
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
    LOGF("net", ">   X-Api-Key: %s", cfg.key);
    LOGF("net", ">   X-Timestamp: %s", ts);
    LOGF("net", ">   X-Signature: %s", sig);
    LOGF("net", "auth: signed - key %.4s..., ts %s", cfg.key, ts);
  } else if (cfg.key[0]){
    LOGF("net", ">   Authorization: Bearer %s", cfg.key);
    char bearer[LEN_KEY + 8];
    snprintf(bearer, sizeof bearer, "Bearer %s", cfg.key);
    http.addHeader("Authorization", bearer);
    LOGF("net", "auth: bearer token %.4s...", cfg.key);
  } else {
    LOG("net", "auth: none - no API key is set");
  }

  const int code = http.GET();
  bool ok = false;

  if (code > 0){
    /* Every response gets its status, headers and body logged before this
       function decides anything from them — a poll that looked "bad" on
       screen should never be a mystery on Serial.                        */
    LOGF("net", "< HTTP %d (%lums)", code, (unsigned long)(millis() - t0));
    LOGF("net", "<   Content-Type: %s",   http.header("Content-Type").length()   ? http.header("Content-Type").c_str()   : "(none)");
    LOGF("net", "<   Content-Length: %s", http.header("Content-Length").length() ? http.header("Content-Length").c_str() : "(none)");
    if (http.header("Server").length()) LOGF("net", "<   Server: %s", http.header("Server").c_str());
    if (http.header("Retry-After").length()) LOGF("net", "<   Retry-After: %s", http.header("Retry-After").c_str());

    char* body = nullptr; size_t len = 0;
    const bool gotBody = fetchBody(http, &body, &len);

    if (code == 200 && gotBody){
      LOGF("net", "< body (%u bytes): %s", (unsigned)len, body);
      ok = parseSnapshot(body);
      if (ok) clearFault();
      else    setFault("BAD DATA", "payload rejected");
    } else if (code == 200){
      LOG("net", "< body: could not be read - see the reason logged above");
      setFault("BAD DATA", "body unreadable");
    } else {
      char detail[22];
      const char* word = faultForStatus(code, detail, sizeof detail);
      LOGF("net", "%d %s - %s, keeping the last good data", code, word, detail);
      if (gotBody && len) LOGF("net", "< body (%u bytes): %s", (unsigned)len, body);
      else                LOG("net", "< body: (empty or unreadable)");
      if (code == 429){
        /* api.md §7: honour Retry-After, clamped so a silly value cannot park
           the device for a day.                                           */
        long wait = http.header("Retry-After").toInt();
        if (wait <= 0) wait = POLL_MIN_S;
        if (wait > RETRY_AFTER_MAX_S) wait = RETRY_AFTER_MAX_S;
        pollHoldUntil = millis() + (uint32_t)wait * 1000UL;
        LOGF("net", "Retry-After %lds - next poll held off that long", wait);
      }
      setFault(word, detail);
    }
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
void netBegin(){
  if (!configHasEndpoint())  setFault("SETUP", "hold DOWN 2 s");
  else if (!cfg.nnets)       setFault("SETUP", "no wi-fi saved");
  else                       setFault("OFFLINE", "joining wi-fi");
  LOGF("net", "waiting: %s - %s", faultWord, faultDetail);
}

/* How often the backend is asked: the cycle's lap time, floored at 30 s. */
uint32_t pollIntervalMs(){
  uint32_t lap = (uint32_t)snap.nrows * SCREEN_MS;
  uint32_t min = (uint32_t)POLL_MIN_S * 1000UL;
  return lap > min ? lap : min;
}

/* A poll the person asked for — the glass held 2 s, or three shakes
   (imu.ino). Ignores the schedule, lights the status hairline and sweeps
   the graph back in. A forced refresh also clears a Retry-After hold: a
   person asking by hand outranks a backend that asked us to wait. `why`
   is just what goes in the log line — every caller names its own gesture. */
void refreshNow(const char* why){
  LOGF("net", "forced refresh - %s", why);
  pollHoldUntil = 0;
  beginCount();
  netFetch();
  if (curRow >= snap.nrows) curRow = 0;
  refreshT0 = sweepT0 = screenT0 = millis();
}

/* ------------------------------------------------------------------ cycle */
void cycleBegin(){
  screenT0 = millis();
  lastPoll = millis();
}

/* A person just chose this screen by hand, or came back from settings — give
   it a full SCREEN_MS before the cycle moves on.                          */
void cycleResetTimer(){ screenT0 = millis(); }

/* Called every frame. Advances the screen every 5 s, and when the cycle wraps
   back to the first screen it refetches — as long as the poll interval has
   actually elapsed. Both events log what they did and why.

   With nothing parsed yet there are no screens to turn, so the cycle just
   keeps asking on the interval until something answers.                   */
void cycleTick(){
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
