# Proposed code

Firmware for the M5StickS3, structured to match [api.md](api.md) and [stick.md](stick.md).

This is the shape of the implementation, not a copy-paste build. It compiles against **M5Unified** + **ArduinoJson 7** on PlatformIO.

---

## 1. Project

```ini
; platformio.ini
[env:sticks3]
platform      = espressif32
board         = m5stack-stamps3
framework     = arduino
monitor_speed = 115200
build_flags   = -DARDUINO_USB_CDC_ON_BOOT=1 -DBOARD_HAS_PSRAM
lib_deps =
  m5stack/M5Unified @ ^0.2.7
  bblanchon/ArduinoJson @ ^7.1.0
```

```
src/
  main.cpp        setup / loop, keys, power state machine
  model.h         Row, Service — the only structs
  api.cpp         HTTP + bearer + parse
  store.cpp       Preferences: Wi-Fi list, service list, setup portal
  ui.cpp          the five bands
  alert.cpp       speaker
include/
  config.h        defaults only, no secrets in git
```

---

## 2. Model

The payload's `overview` and `metrics[]` are the same shape, so there is one struct and the screens are just an array.

```cpp
// model.h
#pragma once
#include <Arduino.h>

static constexpr int MAX_ROWS   = 3;    // overview + 2 metrics — api.md §5
static constexpr int MAX_TREND  = 30;
static constexpr int MAX_SVCS   = 3;    // 3 services x 3 panels = 9 screens

enum Level  : uint8_t { LV_INFO, LV_WARNING, LV_CRITICAL };
enum AState : uint8_t { AS_OK, AS_ACTIVE };   // no ACK: the device never writes back
enum Fault  : uint8_t { F_NONE, F_NET, F_REDIR, F_AUTH, F_404, F_BUSY, F_SRV };

struct Row {
  char    name[17];
  int32_t avg_ms, p95_ms;
  int32_t c2xx, c4xx, c5xx;       // disjoint: every request lands in one
  int16_t trend[MAX_TREND];       // c2xx per minute
  uint8_t trend_n;
  int32_t total()   const { return c2xx + c4xx + c5xx; }
  float   pct4()    const { return total() ? c4xx * 100.0f / total() : 0.0f; }
  float   pct5()    const { return total() ? c5xx * 100.0f / total() : 0.0f; }
  float   okPerMin(int w) const { return w ? c2xx / (float)w : 0.0f; }
};

struct Service {
  char     url[96];
  char     token[80];
  char     name[17];              // from the payload, not from flash
  uint16_t window_min = 30;

  AState   state = AS_OK;
  Level    level = LV_INFO;
  char     message[41];

  Row      rows[MAX_ROWS];        // rows[0] is the overview
  uint8_t  row_n = 0;

  Fault    fault    = F_NONE;
  uint32_t recvAt   = 0;          // millis() of the last 200
  uint32_t updated  = 0;          // updated_at from the payload
  uint8_t  lastKey  = 0xFF;       // rising-edge memory for the speaker
};

extern Service gSvc[MAX_SVCS];
extern uint8_t gSvcCount;
```

---

## 3. Fetch and parse

Bearer, one GET, straight into the struct. No JSON is retained.

```cpp
// api.cpp
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "model.h"

static Level parseLevel(const char* s) {
  if (!s) return LV_INFO;
  if (!strcmp(s, "critical")) return LV_CRITICAL;
  if (!strcmp(s, "warning"))  return LV_WARNING;
  return LV_INFO;
}
static AState parseState(const char* s) {
  return (s && !strcmp(s, "active")) ? AS_ACTIVE : AS_OK;
}

static void readRow(JsonObjectConst o, Row& r) {
  strlcpy(r.name, o["name"] | "-", sizeof r.name);
  r.avg_ms = o["avg_latency_ms"] | 0;
  r.p95_ms = o["p95_latency_ms"] | 0;
  r.c2xx   = o["count_2xx"] | 0;
  r.c4xx   = o["count_4xx"] | 0;
  r.c5xx   = o["count_5xx"] | 0;
  r.trend_n  = 0;
  JsonArrayConst t = o["trend"];
  if (!t.isNull()) {
    const int skip = max(0, (int)t.size() - MAX_TREND);   // keep the newest 30
    int i = 0;
    for (JsonVariantConst v : t)
      if (i++ >= skip && r.trend_n < MAX_TREND) r.trend[r.trend_n++] = v.as<int>();
  }
}

bool fetch(Service& s) {
  WiFiClientSecure net;
  net.setInsecure();                      // pin a CA in production
  HTTPClient http;
  http.setTimeout(5000);

  String url = String(s.url) + "/v1/status";
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);   // a 3xx is a config bug
  if (!http.begin(net, url)) { s.fault = F_NET; return false; }
  http.addHeader("Authorization", String("Bearer ") + s.token);
  http.addHeader("Accept", "application/json");

  const int code = http.GET();
  if (code != 200) {
    s.fault = code >= 300 && code < 400   ? F_REDIR      // not followed, by design
            : code == 401 || code == 403  ? F_AUTH
            : code == 404                 ? F_404
            : code == 429                 ? F_BUSY
            : code >= 500                 ? F_SRV : F_NET;
    http.end();
    return false;                         // last good payload stays on screen
  }

  JsonDocument doc;                        // ~4 KB payload, PSRAM-backed
  const auto err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) { s.fault = F_SRV; return false; }

  strlcpy(s.name, doc["service"] | "-", sizeof s.name);
  s.window_min = doc["window_minutes"] | 30;
  s.updated    = doc["updated_at"]     | 0;

  JsonObjectConst al = doc["alarm"];
  s.state = parseState(al["state"]);
  s.level = parseLevel(al["level"]);
  strlcpy(s.message, al["message"] | "", sizeof s.message);

  s.row_n = 0;
  readRow(doc["overview"], s.rows[s.row_n++]);
  for (JsonObjectConst m : doc["metrics"].as<JsonArrayConst>())
    if (s.row_n < MAX_ROWS) readRow(m, s.rows[s.row_n++]);   // extras dropped

  s.fault  = F_NONE;
  s.recvAt = millis();
  return true;
}
```

Note what is **not** here: no error-rate field (it is `Row::errPct()`), no service id, no display strings. The device derives what it can.

---

## 4. Speaker — rising edge only

```cpp
// alert.cpp
#include <M5Unified.h>
#include "model.h"

void maybeAlert(Service& s, bool plugged) {
  // note: alarms come from the backend and are driven by 5xx, never by 4xx
  const bool loud = s.fault == F_NONE && s.state == AS_ACTIVE &&
                    (s.level == LV_WARNING || s.level == LV_CRITICAL);
  const uint8_t key = loud ? (uint8_t)(1 + s.level) : 0;
  if (key == s.lastKey) { s.lastKey = key; return; }   // same alarm, stay quiet
  s.lastKey = key;
  if (!loud) return;

  M5.Speaker.setVolume(plugged ? 128 : 80);            // >75 % browns out the cell
  const int n = s.level == LV_CRITICAL ? 4 : 2;
  for (int i = 0; i < n; i++) { M5.Speaker.tone(2400, 90); delay(140); }
  M5.Speaker.stop();
}
```

Alerting is keyed on the transition, so an alarm that stays `active` sounds once, not every 20 seconds.

---

## 5. Drawing

Everything goes through one off-screen sprite so a frame lands in a single SPI push, and so the two-sprite slide transitions in [stick.md §5](stick.md#5-animation-budget) are free.

```cpp
// ui.cpp
#include <M5Unified.h>
#include "model.h"

static constexpr int W = 135, H = 240;
struct Band { int y, h; };
static constexpr Band B_STATUS{0,14}, B_ALARM{14,30}, B_STATS{44,76},
                      B_GRAPH{120,94}, B_FOOT{214,26};

static M5Canvas fb(&M5.Display);        // 135x240x16bpp = 64 KB, PSRAM

void uiBegin() {
  M5.Display.setRotation(0);            // portrait
  fb.setColorDepth(16);
  fb.createSprite(W, H);
}

static uint16_t C_BG, C_TX, C_DIM, C_RULE, C_CY, C_OK, C_WARN, C_CRIT;
void uiColors() {
  C_BG   = fb.color565(0x05,0x07,0x0c);  C_TX   = fb.color565(0xe6,0xed,0xf7);
  C_DIM  = fb.color565(0x7f,0x8f,0xa8);  C_RULE = fb.color565(0x1c,0x27,0x40);
  C_CY   = fb.color565(0x22,0xd3,0xee);  C_OK   = fb.color565(0x22,0xc5,0x5e);
  C_WARN = fb.color565(0xf5,0xa5,0x24);  C_CRIT = fb.color565(0xff,0x4d,0x5e);
}

/* the graph: one flat fill to the baseline, one lit surface pixel */
static void drawTrend(const Row& r, uint16_t body, uint16_t edge) {
  const int X0 = 4, PW = 127, YB = B_GRAPH.y + 86, PH = 80;
  if (r.trend_n < 2) return;
  int hi = 1;
  for (int i = 0; i < r.trend_n; i++) hi = max(hi, (int)r.trend[i]);
  hi = hi * 114 / 100;                                   // 14 % headroom

  for (int px = 0; px < PW; px++) {
    const int   pos = px * (r.trend_n - 1) * 256 / (PW - 1);
    const int   i   = pos >> 8, fr = pos & 255;
    const int   a   = r.trend[i], b = r.trend[min(i + 1, r.trend_n - 1)];
    const int   val = a + ((b - a) * fr >> 8);
    const int   h   = val * (PH - 1) / hi;
    fb.drawFastVLine(X0 + px, YB - h, h + 1, body);
    fb.drawPixel(X0 + px, YB - h, edge);
  }
  fb.drawFastHLine(X0, YB + 1, PW, C_RULE);
}

/* raw counts arrive from the backend; the panel has room for five characters */
static void compact(int32_t n, char* out, size_t cap) {   // 18420->18.4k  2456789->2.5M
  if (n < 0) n = 0;
  if      (n >= 10000000) snprintf(out, cap, "%dM",   (int)(n / 1000000));
  else if (n >= 1000000)  snprintf(out, cap, "%.1fM", n / 1e6);
  else if (n >= 100000)   snprintf(out, cap, "%dk",   (int)(n / 1000));
  else if (n >= 1000)     snprintf(out, cap, "%.1fk", n / 1e3);
  else                    snprintf(out, cap, "%d",    (int)n);
}

void uiDraw(const Service& s, uint8_t row, bool plugged, int batPct, int wifiBars) {
  const Row& r = s.rows[row];
  fb.fillSprite(C_BG);

  uiStatus(batPct, plugged, wifiBars);        // battery + charge bolt + Wi-Fi bars
  uiAlarm(s);                                 // THE ANCHOR — glyph, word, one line
  uiStats(r);                                 // p95 / avg / 4xx / 5xx, size 2
  drawTrend(r, /*body*/ C_RULE, /*edge*/ C_CY);
  uiHeadline(r, s.window_min);                // 2xx/min drawn ON the plot
  uiFooter(s, row);                           // service (bold) / panel / dots

  fb.pushSprite(0, 0);
}
```

Every band writes only inside its own `{y,h}`, and `uiFooter` is the only one that draws the service name — identity lives at the bottom so the top belongs to the alarm and the graph. Text is the built-in 6 × 8 cell, so a string's width is exactly `len * 6 * size` — which is how the [api.md §5](api.md#5-limits) character limits were chosen. Nothing wraps; over-long strings are cut with `strlcpy`.

---

## 6. Main loop

```cpp
// main.cpp
#include <M5Unified.h>
#include "model.h"

Service gSvc[MAX_SVCS];
uint8_t gSvcCount = 0;

static uint8_t  curSvc = 0, curRow = 0;
static bool     screenOn = true, settings = false;
static uint32_t wakeUntil = 0, nextPoll = 0;

/* Shake to refresh. Rejects desk bumps by requiring a pattern, not a spike:
   three jolts inside 700 ms, then five seconds of quiet before it can fire
   again. Without that cooldown a stick rattling in a bag would hammer every
   configured endpoint — the monitor must never become the incident.        */
static bool shaken() {
  static uint32_t lastFire = 0, firstJolt = 0, lastJolt = 0;
  static uint8_t  jolts = 0;
  if (!M5.Imu.update()) return false;

  const auto a  = M5.Imu.getImuData().accel;
  const float g = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
  const float dev = fabsf(g - 1.0f);            // 1 g sitting still
  const uint32_t now = millis();

  if (dev > 0.9f && now - lastJolt > 80) {      // 80 ms: faster than any hand
    lastJolt = now;
    if (now - firstJolt > 700) { jolts = 0; firstJolt = now; }
    jolts++;
  }
  if (jolts >= 3 && now - lastFire > 5000) {
    lastFire = now; jolts = 0;
    return true;
  }
  return false;
}

static bool plugged() {
  if (M5.Power.isCharging() == m5::Power_Class::is_charging) return true;
  return M5.Power.getVBUSVoltage() > 4000;
}

/* one tap walks every screen on the stick, then wraps */
static void nextScreen() {
  if (++curRow >= gSvc[curSvc].row_n) {
    curRow = 0;
    curSvc = (curSvc + 1) % gSvcCount;
  }
}

static void pollAll() {
  for (uint8_t i = 0; i < gSvcCount; i++) {
    fetch(gSvc[i]);
    maybeAlert(gSvc[i], plugged());
  }
  nextPoll = millis() + 60000;      // same interval plugged or not
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  uiBegin(); uiColors();
  storeLoad();                       // Wi-Fi list + service list from Preferences
  wifiJoin();                        // WiFiMulti over every saved SSID
  syncTime();                        // NTP — nothing time-based works before this
  screenOn = plugged();
  if (!screenOn) displayOff();
  pollAll();
}

void loop() {
  M5.update();                       // never delay() past this

  if (M5.BtnA.wasPressed()) {                        // blue key: one job
    if (!screenOn) { displayOn(); screenOn = true; wakeUntil = millis() + 60000; }
    else if (settings) settings = false;
    else nextScreen();
  }
  if (shaken()) {                                    // shake -> refresh now
    if (!screenOn) { displayOn(); screenOn = true; }
    wakeUntil = millis() + 60000;
    settings = false;
    nextPoll = 0;
  }

  if (M5.BtnB.wasPressed())     settings = !settings; // right key, tap
  if (M5.BtnB.pressedFor(900))  startPortal();        // right key, hold -> Wi-Fi AP

  if (millis() >= nextPoll) pollAll();

  if (!plugged() && screenOn && millis() > wakeUntil) {
    displayOff(); screenOn = false;                   // back to dark idle
  }
  if (screenOn) {
    if (settings) uiSettings();
    else uiDraw(gSvc[curSvc], curRow, plugged(),
                M5.Power.getBatteryLevel(), wifiBars());
  }
}
```

`M5.BtnA` is the blue front key (GPIO 11), `M5.BtnB` the right side key (GPIO 12). The power key belongs to the PMIC and is never read here. Forcing a refresh is the IMU's job, not a button's.

---

## 7. Storage and the setup portal

```cpp
// store.cpp — namespaces: "wifi" (count, s{i}, p{i}) and "svc" (count, u{i}, t{i})
void storeLoad() {
  Preferences p;
  p.begin("svc", true);
  gSvcCount = min<uint8_t>(p.getUChar("count", 0), MAX_SVCS);
  for (uint8_t i = 0; i < gSvcCount; i++) {
    char k[8];
    snprintf(k, sizeof k, "u%u", i); p.getString(k, gSvc[i].url,   sizeof gSvc[i].url);
    snprintf(k, sizeof k, "t%u", i); p.getString(k, gSvc[i].token, sizeof gSvc[i].token);
    strlcpy(gSvc[i].name, "…", sizeof gSvc[i].name);   // replaced by the first payload
  }
  p.end();
}
```

Holding the right key raises AP `Pulsar-Setup`; `192.168.4.1` serves a form that appends either a Wi-Fi network or a service (URL + token), saves, and reboots. Service **names** are never stored — they arrive in every payload, so there is nothing to keep in sync.

---

## 8. Build order

1. `M5.begin`, portrait, a filled sprite pushed to the panel, one beep.
2. Save two Wi-Fi networks, walk between them, confirm auto-join.
3. NTP, then a real `GET` with a bearer token against a mock server.
4. Parse into `Service` and draw the five bands with hard-coded data.
5. Two services, blue-key cycle across all screens, wrap.
6. Shake detection: fires on a deliberate shake, ignores a hard desk bump, honours the 5 s cooldown.
7. Alarm rising edge → speaker exactly once.
8. Power: unplug → black in under 1 s; blue key → 60 s; replug → always on.
9. Unplug the network mid-poll → `OFFLINE`, numbers held, no beep.
10. Portal adds a third service with no reflash.

Full acceptance list → [stick.md §7](stick.md#7-on-device-checklist).
