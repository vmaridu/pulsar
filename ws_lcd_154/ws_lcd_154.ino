/* ===========================================================================
   Pulsar — Waveshare ESP32-S3-LCD-1.54  (240 x 240, ST7789, CST816)
   ---------------------------------------------------------------------------
   ONE device, ONE endpoint. The backend behind it may merge several
   platforms; every metric carries its own `gateway` attribute saying where
   it came from — there is no root `gateway` field, only the per-metric one.

   Offline build: no Wi-Fi yet. net.ino holds the test payload and the poll
   placeholder — wiring the real GET replaces one function.

   Screen — four fixed bands, named. Say these names:
       STATUS 0..20 | ALERT 20..56 | BODY 56..216 | FOOTER 216..240
   An alert flashes the WHOLE screen, not one band.

   Panel is rotated 90 degrees right, so the board can be read while it charges
   and the speaker hole faces out instead of into the bench.

   Files in this folder (the IDE opens the .ino/.cpp/.h ones as tabs, and
   concatenates them — globals live HERE, functions may live anywhere); this
   build's own docs and mockup sit right beside them. Arduino requires the
   main .ino to share its folder's name, hence `ws_lcd_154.ino` rather than a
   generic name:
       ws_lcd_154.ino         pins, model, palette, logging, setup/loop
       intro.ino          the welcome screen — pulsar, then the PULSAR label
       dashboard.ino      the four bands
       input.ino          keys and touch — taps, double-taps, holds
       power.ino          power latch, off/on, display on/off
       settings.ino       settings screen, hotspot screen
       sound.ino          the alert sound and mute
       net.ino            poll scheduling, fetch placeholder, test payload
       es8311.*           vendor codec driver, do not edit

   Input — the standard map, the whole contract. Anything not here is FUTURE USE:
       GLASS  tap: next metric screen · double-tap: future · hold 2 s: refresh
       LEFT   tap: settings on/off    · double-tap: future · hold 2 s: hotspot
       POWER  tap: future             · double-tap: future · hold 2 s: off / on
       RIGHT  tap: display on/off     · double-tap: mute   · hold 2 s: future

   Serial is the debugger: 115200 baud, every input, action and poll is logged.

   Libraries (Arduino Library Manager):
       GFX Library for Arduino   by Moon On Our Nation   (Arduino_GFX)
       SensorLib                 by lewisxhe             (CST816 touch)
       ArduinoJson               by Benoit Blanchon      (v7)

   Pin map below is taken from Waveshare's own demos for this board:
       Arduino-3.2.0/examples/04_gfx_helloworld, 08_lvgl_arduino_v8
       ESP-IDF-5.5.1/01_factory/components/esp_bsp/bsp_power_manager.c
   =========================================================================== */

#include <Arduino.h>
/* ESP32 core 3.x (and Nano ESP32 pin remapping) turns these into macros.
   GFX and SensorLib declare methods of the same name, so the macros explode
   into `'digitalPinToGPIONumber' is not a type`. This board's pin numbers
   ARE GPIO numbers — drop the macros, then include those libraries. */
#undef pinMode
#undef digitalWrite
#undef digitalRead

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <ctype.h>
#include <ESP_I2S.h>                /* ships with the ESP32 core — the alert sound */
#include <esp_system.h>                /* esp_reset_reason() */
#include <esp_mac.h>                   /* esp_read_mac() — the settings screen */
#include "es8311.h"                 /* codec driver, beside this sketch — Espressif, Apache-2.0 */
#if __has_include("TouchDrvCST.hpp")
#include "TouchDrvCST.hpp"          /* SensorLib 2026+ */
#elif __has_include("TouchDrv.hpp")
#include "TouchDrv.hpp"             /* mid versions; silences the CSTXXX warning */
#else
#include "TouchDrvCSTXXX.hpp"       /* older SensorLib — deprecation warning is harmless */
#endif

#if ARDUINOJSON_VERSION_MAJOR < 7
#error "Install ArduinoJson 7.x - v6 uses a different document API"
#endif

/* ------------------------------------------------------------------ serial
   This board has no screen for us to debug on, so Serial out is the whole
   story. 115200 baud. Every key, every touch, the action each one caused,
   every poll and every failure prints one line:

       [   1234ms] key: RIGHT tap -> display off

   Log state changes and actions, never frames — the render loop runs at
   ~25 fps and would bury everything that matters.                         */
#define SERIAL_BAUD 115200
#define LOGF(cat, fmt, ...) Serial.printf("[%7lums] " cat ": " fmt "\n", \
                                          (unsigned long)millis(), ##__VA_ARGS__)
#define LOG(cat, msg)       LOGF(cat, "%s", msg)

/* ---------------------------------------------------------------- pin map */
#define PIN_LCD_DC    45
#define PIN_LCD_CS    21
#define PIN_LCD_SCK   38
#define PIN_LCD_MOSI  39
#define PIN_LCD_RST   40
#define PIN_LCD_BL    46

#define PIN_I2C_SDA   42
#define PIN_I2C_SCL   41
#define PIN_TP_RST    47
#define PIN_TP_INT    48

#define PIN_BAT_ADC    1     /* ADC1_CH0, through a 1/3 divider   */
#define PIN_PWR_HOLD   2     /* power latch: HIGH keeps the board on from the cell,
                                LOW cuts it. It also feeds the divider — power.ino */
#define PIN_CHARGING   3     /* input pull-up, LOW = charging     */

#define PIN_PA_CTRL    7     /* NS4150B speaker amp enable, HIGH = on */
#define PIN_I2S_MCLK   8     /* ES8311 codec — pins from Waveshare's 01_i2s_audio */
#define PIN_I2S_BCLK   9
#define PIN_I2S_LRCK  10
#define PIN_I2S_DOUT  12

/* Keys, left to right on the case. This board's PLUS/BOOT keys are wired the
   opposite of Waveshare's own silkscreen — GPIO4 sits physically on the
   right, GPIO0 physically on the left — so the two are swapped here rather
   than in input.ino: KEY_LEFT and KEY_RIGHT are the physical positions,
   whichever name is printed on the key. Every press prints on Serial with
   its GPIO. KEY_POWER is wired to the power circuit and has to stay on 5. */
#define KEY_LEFT       0     /* physically left  — tap: settings on / off · hold 2 s: hotspot     */
#define KEY_POWER      5     /* PWR              — tap: future use        · hold 2 s: off, and on */
#define KEY_RIGHT      4     /* physically right — tap: display on / off  · double-tap: mute      */

/* Clock offset applied to measured_at. No RTC and no NTP in this build,
   so the time on screen is the time the payload says it was measured,
   shown MM/DD hh:mm AM.                                                   */
#define TZ_OFFSET_HOURS  0

/* ----------------------------------------------------------------- model
   One gateway. Every metric row carries the gateway it came from, so a
   backend that merged several platforms still says which is which.        */
#define MAX_ROWS    5
#define MAX_BUCKETS 30
#define MAX_TILES   5

/* One number card for the BODY band — up to 5 per row, position 0 is the
   heading, the rest fill the four body slots in order. `name` IS the text
   drawn — no separate label, truncated here if a backend sent more than
   5 chars. `*Unit` is one of "ms" / "s" / "%" / "" (a plain count) —
   AGENTS.md's aggregate tile guideline says what each does to
   fmtTileValue()'s output. What `secondary` MEANS is the backend's
   choice, not this schema's — api.md §4.                                */
struct Tile {
  char   name[6];
  double primary;
  char   primaryUnit[4];
  bool   hasSecondary;
  double secondary;
  char   secondaryUnit[4];
};

struct Row {
  char      gateway[16];                /* where this row came from — api.md §4 */
  char      name[18];                   /* IS the FOOTER text — no separate label */
  char      unit[8];
  int       size, count;
  Tile      tiles[MAX_TILES];
  uint8_t   ntiles;
  int       buckets[MAX_BUCKETS];
  uint8_t   nbuckets;
};

/* The whole of what the device knows — one response, parsed. No root
   `gateway` — api.md only carries the word per metric row, since a backend
   may merge several platforms into the one response this device shows.   */
struct Snapshot {
  uint32_t polledAt;          /* millis() of the last reading — the settings screen */
  long     measured_at;
  char     level[10];
  char     message[22];
  Row      rows[MAX_ROWS];
  uint8_t  nrows;
};
static Snapshot snap;

/* ---------------------------------------------------------------- palette
   Authored in hex, folded to RGB565 the same way the panel does it.       */
static constexpr uint16_t rgb(uint32_t h){
  return (uint16_t)((((h >> 19) & 0x1F) << 11) | (((h >> 10) & 0x3F) << 5) | ((h >> 3) & 0x1F));
}
static constexpr uint16_t C_BG    = rgb(0x05070c);
static constexpr uint16_t C_LINE  = rgb(0x1c2740);
static constexpr uint16_t C_LINE2 = rgb(0x28344a);
static constexpr uint16_t C_TX    = rgb(0xe6edf7);
static constexpr uint16_t C_DIM   = rgb(0x7f8fa8);
static constexpr uint16_t C_DIM2  = rgb(0x4d5c76);
static constexpr uint16_t C_CY    = rgb(0x22d3ee);
static constexpr uint16_t C_GR    = rgb(0x5cf22e);   /* lime */
static constexpr uint16_t C_OR    = rgb(0xff7a00);   /* warning — orange, well clear of the red */
static constexpr uint16_t C_RD    = rgb(0xff2626);   /* bright red, nothing mixed in */
static constexpr uint16_t C_GY    = rgb(0x8aa0c0);
static constexpr uint16_t C_INK   = rgb(0x04060a);   /* words printed on a level colour */

/* level → word, solid colour, dark shade, flashes, sounds */
struct Level { const char* word; uint16_t c, dark; bool flashes, sounds; };
static const Level LV_INFO  = { "INFO",     C_GR, rgb(0x12380c), false, false };
static const Level LV_WARN  = { "WARNING",  C_OR, rgb(0x3d1c00), true,  false };
static const Level LV_CRIT  = { "CRITICAL", C_RD, rgb(0x4a0c0c), true,  true  };
static const Level LV_FAULT = { "OFFLINE",  C_GY, rgb(0x18202d), true,  false };

/* One alert pattern for warning, critical and no connection alike: the WHOLE
   screen flashes the level colour for the first ALERT_FLASH_MS of every new
   metric screen — synced to screenT0 (net.ino), never a free-running clock
   of its own. Five-second screens, so in practice: flash right at the 5 s
   mark, for 500 ms, every time. Critical adds a sound of exactly the same
   length, started in the same frame.                                     */
#define ALERT_FLASH_MS   500

/* What the screen is painted with. Normally black, with the graph as a quiet
   cyan ground under the numbers. During an alert flash every band goes to the
   level colour and every foreground on it turns to near-black ink — no one
   colour reads on black, red, orange and grey alike (white on orange is under
   3:1). Mixed once in setup(), the same maths as the mockup.              */
#define FADE 12                 /* graph fill steps, top → baseline */
#define SWEEP_MS 900            /* graph columns draw in left → right */
struct Theme {
  uint16_t bg, tx, dim, rule;   /* ground, values, labels, hairlines */
  uint16_t gLine, gFill[FADE];  /* graph line, and its fill fading to the baseline */
  bool     ink;                 /* ink foreground: thresholds do not recolour it */
};
static Theme TH_DARK, TH_RED, TH_ORANGE, TH_GREY;
static uint8_t mixCh(uint32_t a, uint32_t b, int shift, float t){
  int x = (a >> shift) & 255, y = (b >> shift) & 255;
  return (uint8_t)lround(x + (y - x) * t);
}
static uint16_t mixHex(uint32_t a, uint32_t b, float t){
  return rgb(((uint32_t)mixCh(a, b, 16, t) << 16) | ((uint32_t)mixCh(a, b, 8, t) << 8) | mixCh(a, b, 0, t));
}
static void litTheme(struct Theme& t, uint32_t hex){
  t.bg    = rgb(hex);
  t.tx    = C_INK;
  t.dim   = mixHex(hex, 0x04060a, 0.62f);
  t.rule  = mixHex(hex, 0x04060a, 0.35f);
  t.gLine = mixHex(hex, 0x04060a, 0.70f);
  for (int i = 0; i < FADE; i++) t.gFill[i] = mixHex(hex, 0x04060a, 0.22f - i * 0.016f);
  t.ink   = true;
}
static void buildThemes(){
  TH_DARK.bg = C_BG; TH_DARK.tx = C_TX; TH_DARK.dim = C_DIM; TH_DARK.rule = C_LINE;
  TH_DARK.gLine = mixHex(0x05070c, 0x22d3ee, 0.62f);
  for (int i = 0; i < FADE; i++) TH_DARK.gFill[i] = mixHex(0x05070c, 0x22d3ee, 0.15f - i * 0.012f);
  TH_DARK.ink = false;
  litTheme(TH_RED,    0xff2626);
  litTheme(TH_ORANGE, 0xff7a00);
  litTheme(TH_GREY,   0x8aa0c0);
}

/* ------------------------------------------------------------------ bands
   STATUS · ALERT · BODY · FOOTER. Names, not numbers — refer to them by name
   in code, comments, docs and conversation.                               */
#define X_L             6     /* left and right margin, every band */
#define X_R           234
#define BAND_STATUS_Y   0
#define BAND_STATUS_H  20
#define BAND_ALERT_Y   20
#define BAND_ALERT_H   36
#define BAND_BODY_Y    56
#define BAND_BODY_H   160
#define BAND_FOOT_Y   216
#define BAND_FOOT_H    24

/* inside the BODY band — offsets from BAND_BODY_Y, the mockup's numbers */
#define COL2          128     /* second tile column */
/* every tile — the heading and the four body slots alike — draws its value
   on the left and its name (above its secondary, if it has one) stacked
   to the right; only the scale differs. dashboard.ino's drawTile().      */
#define Y_HERO         10     /* heading: value size 4, name/secondary size 2 */
#define Y_HERO_SHARE   24
#define Y_TILE1        58     /* body row 1 (tiles[1], tiles[2]): value size 2, name/secondary size 1 */
#define Y_TILE2        104    /* body row 2 (tiles[3], tiles[4]) */
#define Y_GRAPH_TOP    44
#define Y_BASE        157

/* ---------------------------------------------------------------- display
   Rotation 1 = 90 degrees right. The panel is square, so every layout number
   above is unchanged; what moves is which edge is "up". Read it on the desk
   while it charges — the cable leaves the side instead of the bottom, and the
   speaker hole faces out.                                                 */
#define PANEL_ROTATION 1
Arduino_DataBus* bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS,
                                            PIN_LCD_SCK, PIN_LCD_MOSI, -1 /* MISO */);
Arduino_GFX* panel = new Arduino_ST7789(bus, PIN_LCD_RST, PANEL_ROTATION, true /*IPS*/,
                                        240, 240);
/* One off-screen frame so a redraw lands in a single push. 240*240*2 = 113 KB,
   which lives in PSRAM. Never draw straight to the panel inside loop().     */
Arduino_Canvas* cv = new Arduino_Canvas(240, 240, panel);

TouchDrvCSTXXX touch;
static bool touchOK = false;

/* ------------------------------------------------------------------ state */
static uint8_t  curRow = 0;                  /* which metric screen is up */
/* which screen is up — input.ino moves between them */
#define VIEW_MAIN      0
#define VIEW_SETTINGS  1
#define VIEW_HOTSPOT   2
static uint8_t  view = VIEW_MAIN;
static char     deviceName[20] = "pulsar";   /* pulsar-xxxxxx from the MAC — set in setup() */
static uint8_t  macAddr[6];

/* What the radio would report. This build has no Wi-Fi, so it stays empty and
   the settings screen says so; an online build fills it after connecting.  */
struct NetInfo { bool connected; int rssi; char ip[16]; };
static NetInfo net = { false, 0, "" };

static uint32_t countT0 = 0;
static double   countFromP[MAX_TILES];      /* each tile's primary value, last screen */
static double   countFromS[MAX_TILES];      /* and its secondary, if it had one */
static uint32_t screenT0 = 0;      /* when the current metric screen came up — net.ino owns the cycle,
                                       but this lives here so alertFlash() below can see it too */
static uint32_t sweepT0 = 0;
static uint32_t refreshT0 = 0;     /* last forced refresh — lights the status hairline */
static bool     audioOK = false;   /* the codec answered at boot — sound.ino */
static bool     soundMuted = false;/* BOOT double-tap; RAM only, so every restart has sound */
static bool     displayAwake = true;/* BOOT tap; the panel only — polling and alerts carry on */
static uint32_t toastT0 = 0;       /* a short message over the screen — "SOUND OFF" */
static const char* toastText = "";

/* ------------------------------------------------------------- small utils */
static float easeOut(float t){ t = 1 - t; return 1 - t*t*t; }
static float clampf(float v, float a, float b){ return v < a ? a : v > b ? b : v; }

/* Every number on screen is at most 5 characters. Exact while it fits, then
   k · m · b (thousand, million, billion) with as many decimals as still fit:
   99999 · 123k · 1.23m · 12.3m · 123m · 1.5b                                */
static void fmtCount(double n, char* out, size_t cap){
  long long v = n < 0 ? 0 : llround(n);
  snprintf(out, cap, "%lld", v);
  if (strlen(out) <= 5) return;
  static const double unit[3] = { 1e3, 1e6, 1e9 };
  static const char   suf[3]  = { 'k', 'm', 'b' };
  for (int u = 0; u < 3; u++){
    for (int d = 2; d >= 0; d--){
      char t[24]; snprintf(t, sizeof t, "%.*f", d, v / unit[u]);
      if (atof(t) >= 1000) break;                        /* rounds up out of this unit */
      if (strlen(t) + 1 > 5) continue;
      if (d){                                            /* 1.50m → 1.5m, 100k stays */
        char* e = t + strlen(t) - 1;
        while (*e == '0') *e-- = 0;
        if (*e == '.') *e = 0;
      }
      snprintf(out, cap, "%s%c", t, suf[u]);
      return;
    }
  }
  strlcpy(out, ">999b", cap);
}
/* 96.97% · 0.92% · 12.3% — the hero keeps two decimals at any size */
static void fmtShare(double p, int digits, char* out, size_t cap){
  if (p >= 99.995)                snprintf(out, cap, "100%%");
  else if (p >= 10 && digits < 2) snprintf(out, cap, "%.1f%%", p);
  else                            snprintf(out, cap, "%.2f%%", p);
}
/* latency keeps its unit beside it, so it gets four characters: 9999ms, then 18.3s.
   Writes the number, returns the unit.                                    */
static const char* fmtLatency(double ms, char* out, size_t cap){
  long v = ms < 0 ? 0 : lround(ms);
  if (v < 10000){ snprintf(out, cap, "%ld", v); return "ms"; }
  double sec = v / 1000.0;
  if (sec < 100) snprintf(out, cap, "%.1f", sec);
  else { long s = lround(sec); snprintf(out, cap, "%ld", s > 9999 ? 9999L : s); }
  return "s";
}
/* One tile's primary or secondary value, formatted by its declared unit —
   the aggregate-tile guideline in AGENTS.md. "ms" reuses fmtLatency (which
   itself moves to "s" past 9999); "%" is a share, baked into the string;
   anything else (or no unit) is a plain compacted count. Returns the unit
   text to draw after the value, or NULL when there is none to draw
   separately (a share, or a bare count).                                 */
static const char* fmtTileValue(double v, const char* unit, char* out, size_t cap){
  if (unit && !strcmp(unit, "ms")) return fmtLatency(v, out, cap);
  if (unit && !strcmp(unit, "%")){ fmtShare(v, 1, out, cap); return nullptr; }
  if (unit && !strcmp(unit, "s")){ snprintf(out, cap, "%.1f", v); return "s"; }
  fmtCount(v, out, cap);
  return nullptr;
}
/* How long the row's own clock covers — "30M" · "5M" · "24H" · "45S" —
   `bucket_size × bucket_count` in `bucket_unit`, compacted so it never
   needs more than 2 digits: 60+ s becomes minutes, 60+ m becomes hours.
   Replaces the heading tile's own secondary in the BODY band — the row's
   span, not that tile's `secondary_value`, is what belongs beside the
   headline number. api.md §4.                                            */
static void spanCaption(int size, int count, const char* unit, char* out, size_t cap){
  long span = (long)size * count;
  char u = (unit && unit[0]) ? (char)toupper(unit[0]) : 'M';
  if (u == 'S' && span >= 60){ span = (span + 30) / 60; u = 'M'; }
  if (u == 'M' && span >= 60){ span = (span + 30) / 60; u = 'H'; }
  snprintf(out, cap, "%ld%c", span, u);
}
/* 4s ago · 3m ago · 2h ago */
static void ageText(uint32_t ms, char* out, size_t cap){
  const uint32_t s = ms / 1000;
  if (s < 5)          snprintf(out, cap, "just now");
  else if (s < 60)    snprintf(out, cap, "%lus ago", (unsigned long)s);
  else if (s < 3600)  snprintf(out, cap, "%lum ago", (unsigned long)(s / 60));
  else                snprintf(out, cap, "%luh ago", (unsigned long)(s / 3600));
}

/* text: x,y is the TOP-LEFT of the 6x8 cell, matching the mockup's coords.
   halo: a 1 px outline in that colour (the ground under it), printed at the
   neighbours first, so text stays readable where it crosses the graph.
   0 = no halo.                                                            */
static int txt(const char* s, int x, int y, uint8_t size, uint16_t c,
               char align = 'l', bool bold = false, uint16_t halo = 0){
  int w = (int)strlen(s) * 6 * size;
  if (align == 'c') x -= w / 2;
  if (align == 'r') x -= w;
  cv->setTextSize(size);
  if (halo){
    cv->setTextColor(halo);
    const int right = bold ? 2 : 1;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= right; dx++){
        if (dy == 0 && dx >= 0 && dx < right) continue;  /* the glyph itself covers these */
        cv->setCursor(x + dx, y + dy); cv->print(s);
      }
  }
  cv->setTextColor(c);
  cv->setCursor(x, y);
  cv->print(s);
  if (bold){ cv->setCursor(x + 1, y); cv->print(s); }
  return w;
}

/* `struct` in these signatures is required: Arduino IDE injects prototypes
   above the type definitions, and without it the compile dies with
   `'Level' does not name a type`. */
static const struct Level& levelNow(){
  if (!strcmp(snap.level, "critical")) return LV_CRIT;
  if (!strcmp(snap.level, "warning"))  return LV_WARN;
  if (!strcmp(snap.level, "info"))     return LV_INFO;
  return LV_FAULT;
}
/* There is no root `gateway` any more — a device shows whatever rows the
   backend sends, each named on its own. For the one place that still wants
   a single label (the settings screen), the first row stands for all of
   them, same as `metrics[0]` stands as the summary elsewhere in api.md.   */
static const char* deviceGateway(){
  return snap.nrows ? snap.rows[0].gateway : "-";
}
/* The first ALERT_FLASH_MS of every metric screen, and nothing outside it —
   one clock, shared with the 5 s cycle, so the flash (and the sound that
   rides it) always lands right at the 5 s mark and never drifts loose of it. */
static bool alertFlash(const struct Level& L){
  return L.flashes && millis() - screenT0 < ALERT_FLASH_MS;
}
static const struct Theme& themeFor(const struct Level& L){
  if (!alertFlash(L)) return TH_DARK;
  if (&L == &LV_CRIT) return TH_RED;
  if (&L == &LV_WARN) return TH_ORANGE;
  return TH_GREY;                  /* no connection */
}

/* ---------------------------------------------------------------- battery */
static void batteryBegin(){
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);        // latch power on, and feed the divider
  pinMode(PIN_CHARGING, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}
static float batteryVolts(){
  /* analogReadMilliVolts applies the chip's factory ADC calibration, then the
     board's 1/3 divider is undone — same maths as Waveshare's BSP.          */
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  return (mv / 8.0f) / 1000.0f * 3.0f;
}
static int batteryPercent(){
  float v = batteryVolts();
  if (v < 3.52f) return 1;
  if (v < 3.64f) return 20;
  if (v < 3.76f) return 40;
  if (v < 3.88f) return 60;
  if (v < 4.00f) return 80;
  return 100;
}
static bool charging(){ return digitalRead(PIN_CHARGING) == LOW; }

/* ------------------------------------------------------------------ toast */
static void showToast(const char* s){ toastText = s; toastT0 = millis(); }

/* =========================================================================
   Render
   ========================================================================= */
static void render(){
  if (!displayAwake) return;                 /* panel asleep — polls and alerts carry on */
  if (view == VIEW_SETTINGS) drawSettings();
  else if (view == VIEW_HOTSPOT) drawHotspot();
  else drawDashboard();
  drawHoldOverlay();        /* a key or a finger on its way to a 2 s hold — input.ino */
  drawToast();
  cv->flush();
}

/* The alert sound rides the flash, and runs whether or not the panel is awake
   and whatever screen is up — the point of the speaker is to reach you when
   you are not looking at it.                                              */
static void alertTick(){
  static bool wasFlash = false;
  const Level& L = levelNow();
  const bool flash = alertFlash(L);
  if (flash && !wasFlash && L.sounds) alertSound();       /* sound.ino */
  wasFlash = flash;
}

/* =========================================================================
   Lifecycle
   ========================================================================= */
/* why the chip last reset — the first thing to look at if the board loops */
static const char* resetName(){
  switch (esp_reset_reason()){
    case ESP_RST_POWERON:   return "power on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "crash (panic)";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "woke from deep sleep";
    case ESP_RST_BROWNOUT:  return "brown-out - the supply sagged";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_EXT:       return "reset pin";
    default:                return "other";
  }
}
void setup(){
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.printf("\nPulsar - ESP32-S3-LCD-1.54 - offline build - serial %d baud\n", SERIAL_BAUD);
  LOGF("boot", "reset: %s", resetName());          /* a boot loop names itself here */

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  if (!panel->begin()){ LOG("boot", "panel begin FAILED"); }
  panel->setTextWrap(false);
  if (!cv->begin()){
    LOG("boot", "canvas alloc FAILED - is PSRAM enabled in Tools?");
    panel->fillScreen(RGB565_BLACK);
    panel->setCursor(8, 100); panel->setTextColor(RGB565_RED); panel->setTextSize(2);
    panel->println("PSRAM OFF");
    while (true) delay(1000);
  }
  LOGF("boot", "panel ok - 240x240, rotation %d (90 right)", PANEL_ROTATION);

  cv->setTextWrap(false);          // clients truncate, never wrap - api.md §5

  powerOnHold();                   /* switched on by the power key? it must be held 2 s — power.ino */
  batteryBegin();                  /* latches power */
  keysBegin();
  esp_read_mac(macAddr, ESP_MAC_WIFI_STA);          /* the radio's own address, readable without Wi-Fi */
  snprintf(deviceName, sizeof deviceName, "pulsar-%02x%02x%02x", macAddr[3], macAddr[4], macAddr[5]);
  LOGF("boot", "device: %s  mac %02X:%02X:%02X:%02X:%02X:%02X", deviceName,
       macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  touch.setPins(PIN_TP_RST, PIN_TP_INT);
  touchOK = touch.begin(Wire, CST816_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  LOGF("boot", "touch: %s", touchOK ? "CST816 ok" : "NOT FOUND (non-touch SKU?)");
  LOGF("boot", "battery: %.2f V  %d%%  %s",
       batteryVolts(), batteryPercent(), charging() ? "charging" : "on battery");

  if (!netFetch()){                /* net.ino — the placeholder poll */
    LOG("boot", "no usable payload - stopping");
    panel->fillScreen(RGB565_BLACK);
    panel->setCursor(8, 100); panel->setTextColor(RGB565_RED); panel->setTextSize(2);
    panel->println("BAD PAYLOAD");
    while (true) delay(1000);
  }
  buildThemes();
  soundBegin();             /* codec, the critical alert and the intro hum — before the intro needs either — sound.ino */
  bootIntro();              /* once per power-on, hums with the beams, fades into the dashboard — intro.ino */
  if (soundMuted) showToast("SOUND OFF");  /* the brown-out mute check inside soundBegin() ran before
                                               anything was on screen to show it on — say it now instead */
  beginCount();             /* sweepT0 stays 0: the graph is already whole, as it faded in */
  cycleBegin();             /* start the 5 s screen rotation — net.ino */
  LOGF("boot", "ready - %u metric screens, poll every %lus",
       (unsigned)snap.nrows, (unsigned long)(pollIntervalMs() / 1000));
}

void loop(){
  handleTouch();            /* input.ino */
  handleKeys();             /* input.ino */
  cycleTick();              /* 5 s per screen, refetch when the loop wraps — net.ino */
  alertTick();
  render();                 /* ~25 fps; the alert flash and the count-up need it */
  delay(28);
}
