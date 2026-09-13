/* ===========================================================================
   Network — the poll, the screen cycle, and the test payload.

   ONE endpoint. The backend behind it may merge several platforms — every
   metric row carries its own `gateway`, and that is the ONLY place the word
   appears on the wire. There is no root-level `gateway` field: a device
   doesn't have "a" gateway any more than it has "a" metric, it shows
   whatever rows the backend sends, each one labelled.

   THIS BUILD HAS NO NETWORK. netFetch() is the placeholder: it parses the
   JSON literal below instead of talking to a server. Going online means
   filling `body` from an HTTPS GET and leaving everything else alone —
   the model, the layout and the invariant checks do not change. The
   simulator (../simulator) serves exactly this shape over real HTTP,
   with controls to fake faults, delays and bad data for testing.

   The cycle, and why the poll is tied to it:

     · each metric screen is shown for SCREEN_MS (5 s)
     · when the cycle wraps back to the first screen, the data is refetched
     · but never faster than POLL_MIN_S — so the interval is
           max(30, metric_count x 5) seconds
       Five metrics is a 25 s lap, so it polls on every second lap; ten
       metrics would be a 50 s lap and poll on every lap.
     · a 2 s hold on the glass forces a poll whatever the cycle is doing

   Every poll prints its outcome on Serial — success and failure alike, with
   the real error text. That is the only way to debug this board.
   =========================================================================== */

#define SCREEN_MS     5000      /* each metric screen is up this long */
#define POLL_MIN_S      30      /* never ask the backend faster than this */

/* TODO(online): the configured endpoint — Preferences, namespace "gw". */
#define POLL_URL "https://example.invalid/v1/gateway_health"

static uint32_t lastPoll  = 0;  /* millis() of the last completed poll */

/* ------------------------------------------------------------- the payload
   Two platforms behind one endpoint — Orders (Created, Dispatched, Cancelled)
   and Payments (Paid, Declined) — five metric screens, exactly what one GET
   would return. Every row names its own `gateway`; there is no root one.
   The up-to-5-tile `aggregates` array is what's drawn — tile 0 is the
   heading, the rest fill the four body slots. Field names, ordering and
   the tile schema follow docs/api.md §4; the formatting rules (name
   length, unit strings, number compaction) are AGENTS.md's aggregate-tile
   guideline, not repeated here.
   Healthy by default — change "level" to "warning" or "critical" to see the
   other states, or point this build at the simulator instead.             */
static const char PAYLOAD[] PROGMEM = R"JSON(
{
 "measured_at": 1770000100,
 "alert": { "level": "info", "message": "error budget healthy" },
 "metrics": [
  { "gateway": "Orders", "name": "Created",
    "bucket_unit": "m", "bucket_size": 1, "bucket_count": 30,
    "buckets_value_type": "total_count",
    "buckets": [649, 625, 606, 646, 647, 632, 636, 610, 640, 616, 607, 641, 621, 633, 633,
                 613, 614, 611, 607, 627, 648, 609, 630, 627, 632, 610, 631, 618, 630, 634],
    "aggregates": [
     { "name": "2XX", "primary_value": 18659, "secondary_value": 99.34, "secondary_unit": "%" },
     { "name": "4XX", "primary_value": 109,   "secondary_value": 0.58,  "secondary_unit": "%" },
     { "name": "5XX", "primary_value": 15,    "secondary_value": 0.08,  "secondary_unit": "%" },
     { "name": "AVG", "primary_value": 42,  "primary_unit": "ms" },
     { "name": "P95", "primary_value": 180, "primary_unit": "ms" }
    ] },
  { "gateway": "Orders", "name": "Dispatched",
    "bucket_unit": "m", "bucket_size": 1, "bucket_count": 30,
    "buckets_value_type": "total_count",
    "buckets": [304, 302, 302, 291, 302, 296, 313, 291, 309, 302, 302, 300, 285, 298, 289,
                 287, 285, 306, 302, 303, 314, 302, 291, 313, 288, 293, 303, 299, 290, 292],
    "aggregates": [
     { "name": "2XX", "primary_value": 8909, "secondary_value": 99.5,  "secondary_unit": "%" },
     { "name": "4XX", "primary_value": 40,   "secondary_value": 0.45,  "secondary_unit": "%" },
     { "name": "5XX", "primary_value": 5,    "secondary_value": 0.06,  "secondary_unit": "%" },
     { "name": "AVG", "primary_value": 55,  "primary_unit": "ms" },
     { "name": "P95", "primary_value": 230, "primary_unit": "ms" }
    ] },
  { "gateway": "Orders", "name": "Cancelled",
    "bucket_unit": "m", "bucket_size": 1, "bucket_count": 30,
    "buckets_value_type": "total_count",
    "buckets": [40, 39, 41, 37, 40, 38, 38, 40, 41, 39, 37, 39, 39, 40, 39,
                 38, 41, 38, 39, 40, 37, 38, 40, 39, 40, 37, 38, 40, 39, 38],
    "aggregates": [
     { "name": "2XX", "primary_value": 1156, "secondary_value": 98.89, "secondary_unit": "%" },
     { "name": "4XX", "primary_value": 11,   "secondary_value": 0.94,  "secondary_unit": "%" },
     { "name": "5XX", "primary_value": 2,    "secondary_value": 0.17,  "secondary_unit": "%" },
     { "name": "AVG", "primary_value": 61,  "primary_unit": "ms" },
     { "name": "P95", "primary_value": 240, "primary_unit": "ms" }
    ] },
  { "gateway": "Payments", "name": "Paid",
    "bucket_unit": "m", "bucket_size": 1, "bucket_count": 30,
    "buckets_value_type": "total_count",
    "buckets": [488, 490, 468, 496, 484, 469, 502, 489, 480, 470, 484, 500, 490, 481, 480,
                 495, 481, 480, 476, 500, 483, 470, 488, 468, 494, 483, 486, 487, 480, 483],
    "aggregates": [
     { "name": "2XX", "primary_value": 14467, "secondary_value": 99.6,  "secondary_unit": "%" },
     { "name": "4XX", "primary_value": 51,    "secondary_value": 0.35,  "secondary_unit": "%" },
     { "name": "5XX", "primary_value": 7,     "secondary_value": 0.05,  "secondary_unit": "%" },
     { "name": "AVG", "primary_value": 71,  "primary_unit": "ms" },
     { "name": "P95", "primary_value": 310, "primary_unit": "ms" }
    ] },
  { "gateway": "Payments", "name": "Declined",
    "bucket_unit": "m", "bucket_size": 1, "bucket_count": 30,
    "buckets_value_type": "total_count",
    "buckets": [27, 26, 28, 27, 28, 26, 27, 27, 27, 26, 28, 27, 27, 26, 27,
                 28, 27, 26, 27, 28, 27, 26, 27, 27, 26, 28, 27, 27, 28, 26],
    "aggregates": [
     { "name": "2XX", "primary_value": 798, "secondary_value": 98.64, "secondary_unit": "%" },
     { "name": "4XX", "primary_value": 9,   "secondary_value": 1.11,  "secondary_unit": "%" },
     { "name": "5XX", "primary_value": 2,   "secondary_value": 0.25,  "secondary_unit": "%" },
     { "name": "AVG", "primary_value": 68,  "primary_unit": "ms" },
     { "name": "P95", "primary_value": 260, "primary_unit": "ms" }
    ] }
 ]
}
)JSON";

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
        tl.primary = t["primary_value"] | 0.0;
        strlcpy(tl.primaryUnit, t["primary_unit"] | "", sizeof tl.primaryUnit);
        tl.hasSecondary = !t["secondary_value"].isNull();
        tl.secondary = t["secondary_value"] | 0.0;
        strlcpy(tl.secondaryUnit, t["secondary_unit"] | "", sizeof tl.secondaryUnit);
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
      snprintf(one, sizeof one, "%s%s=%g", k ? " " : "", r.tiles[k].name, r.tiles[k].primary);
      strlcat(tiles, one, sizeof tiles);
    }
    LOGF("data", "  %u/%u %s/%s  %s  %u buckets",
         (unsigned)(i + 1), (unsigned)snap.nrows, r.gateway, r.name, tiles, (unsigned)r.nbuckets);
  }
  return true;
}

/* ------------------------------------------------------------------ fetch
   The placeholder poll. Online this becomes:

       HTTPClient http; http.begin(client, POLL_URL);
       http.addHeader("Authorization", "Bearer " + token);
       int code = http.GET();                     // log it, always
       if (code == 200) parseSnapshot(http.getString().c_str());
       else  ... keep the last good snapshot and mark it held — api.md §6

   Whatever happens, it says so on Serial: the URL, the outcome, and the
   error text if there is one.                                            */
static bool netFetch(){
  LOGF("net", "poll -> %s", POLL_URL);
  const uint32_t t0 = millis();

  /* TODO(online): replace these two lines with the HTTPS GET above. */
  const char* body = PAYLOAD;
  LOG("net", "offline build - no radio, reading the embedded payload instead");

  const bool ok = parseSnapshot(body);
  lastPoll = millis();
  if (ok) LOGF("net", "poll OK in %lums - %u metrics, measured_at=%ld",
               (unsigned long)(millis() - t0), (unsigned)snap.nrows, snap.measured_at);
  else    LOGF("net", "poll FAILED after %lums - keeping the last good data",
               (unsigned long)(millis() - t0));
  return ok;
}

/* How often the backend is asked: the cycle's lap time, floored at 30 s. */
static uint32_t pollIntervalMs(){
  uint32_t lap = (uint32_t)snap.nrows * SCREEN_MS;
  uint32_t min = (uint32_t)POLL_MIN_S * 1000UL;
  return lap > min ? lap : min;
}

/* A poll the person asked for — the glass held 2 s. Ignores the schedule,
   lights the status hairline and sweeps the graph back in.                */
static void refreshNow(){
  LOG("net", "forced refresh - glass held 2 s");
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
   actually elapsed. Both events log what they did and why.                */
static void cycleTick(){
  if (view != VIEW_MAIN) return;              /* settings and hotspot hold the cycle */
  const uint32_t now = millis();
  if (now - screenT0 < SCREEN_MS) return;

  screenT0 = now;                             /* also resyncs the alert flash — ws_lcd_154.ino */
  const bool wrapped = (curRow + 1 >= snap.nrows);
  nextScreen();                               /* dashboard.ino */
  LOGF("view", "auto -> screen %u/%u %s/%s", (unsigned)(curRow + 1), (unsigned)snap.nrows,
       snap.rows[curRow].gateway, snap.rows[curRow].name);

  if (!wrapped) return;
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
