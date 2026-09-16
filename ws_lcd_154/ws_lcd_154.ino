/* ===========================================================================
   Pulsar — Waveshare ESP32-S3-LCD-1.54  (240 x 240, ST7789, CST816)
   ---------------------------------------------------------------------------
   ONE device, ONE endpoint. The backend behind it may merge several
   platforms; every metric carries its own `gateway` attribute saying where
   it came from — there is no root `gateway` field, only the per-metric one.

   Nothing about the endpoint is baked in. The full URL, the API key and the
   API secret are set over the device's own Wi-Fi hotspot — hold DOWN 2 s —
   and kept in NVS with the saved networks; config.ino owns the store,
   hotspot.ino the page, wifi.ino the joining, net.ino the poll. An
   unconfigured board says SETUP and shows no numbers at all: a monitor that
   invents data is worse than one that admits it has none.

   Once real data is showing, the screen locks itself down to the STATUS and
   ALERT bands only — BODY and FOOTER, the actual stats, stay hidden behind a
   lock icon until a 6-digit code is entered on an on-screen keypad. Hold UP
   2 s to lock it (no code needed) or, while locked, to raise that keypad.
   lock.ino owns all of it; it never touches sound, or any other button.

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
       ws_lcd_154.ino     pins, model, palette, logging, setup/loop
       intro.ino          the welcome screen — pulsar, then the PULSAR label
       dashboard.ino      the four bands
       input.ino          keys and touch — taps, double-taps, holds
       imu.ino            the motion sensor — shake-to-refresh
       power.ino          power latch, off/on, display on/off
       config.ino         the stored endpoint and saved networks (NVS)
       wifi.ino           joining saved networks — priority, enterprise, portals
       hotspot.ino        the setup hotspot, the page it serves, its screen
       settings.ino       the settings screen
       sound.ino          the alert sound and mute
       lock.ino           the privacy lock — code, keypad, auto-relock
       net.ino            poll scheduling and the HTTPS GET
       es8311.*           vendor codec driver, do not edit
       fonts.h            ProFont in four sizes, generated — do not edit

   Input — this build's own map, the whole contract. Anything not here is FUTURE USE:
       GLASS  tap: next metric screen · double-tap: settings on/off · hold 2 s: refresh
       DOWN   tap: future             · double-tap: settings on/off · hold 2 s: setup hotspot
       POWER  tap: future             · double-tap: future · hold 2 s: off / on
       UP     tap: display on/off     · double-tap: mute   · hold 2 s: lock / unlock

   DOWN and UP are physical positions on the case, not silkscreen names — see
   KEY_LEFT/KEY_RIGHT below. UP hold 2 s locks the screen (BODY and FOOTER
   hidden behind a lock icon) with no code needed, or — while already
   locked — raises a keypad to enter the 6-digit code and clear it. Only
   armed once real data has shown at least once, and only on a touch SKU:
   there is no way to type a code with three keys alone, so a non-touch board
   never locks itself at all — see ws_lcd_154/device.md.

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
/* Everything below ships with the ESP32 core — nothing to install for the
   radio, the setup page or the signed-request mode.                       */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>              /* the setup page — hotspot.ino */
#include <DNSServer.h>              /* so joining the hotspot pops that page */
#include <Preferences.h>            /* the config store — config.ino */
#include <mbedtls/md.h>             /* HMAC-SHA256 when an API secret is set */
#if __has_include(<esp_random.h>)
#include <esp_random.h>             /* IDF 5 moved it out of esp_system.h */
#endif
/* WPA2-Enterprise: the EAP client moved headers between IDF 4 and 5, and the
   call names moved with it. wifi.ino uses whichever is here.              */
#if __has_include(<esp_eap_client.h>)
#include <esp_eap_client.h>
#define PULSAR_EAP_IDF5 1
#elif __has_include(<esp_wpa2.h>)
#include <esp_wpa2.h>
#define PULSAR_EAP_IDF5 0
#else
#error "No EAP client header - update the ESP32 core (3.x) for WPA2-Enterprise"
#endif
#include "es8311.h"                 /* codec driver, beside this sketch — Espressif, Apache-2.0 */
#include "fonts.h"                  /* ProFont at four sizes — the screen's one font, Adafruit GFX format */
#if __has_include("TouchDrvCST.hpp")
#include "TouchDrvCST.hpp"          /* SensorLib 2026+ */
#elif __has_include("TouchDrv.hpp")
#include "TouchDrv.hpp"             /* mid versions; silences the CSTXXX warning */
#else
#include "TouchDrvCSTXXX.hpp"       /* older SensorLib — deprecation warning is harmless */
#endif
#include "SensorQMI8658.hpp"        /* SensorLib again — the shake gesture, imu.ino */

#if ARDUINOJSON_VERSION_MAJOR < 7
#error "Install ArduinoJson 7.x - v6 uses a different document API"
#endif

/* ------------------------------------------------------------------ serial
   This board has no screen for us to debug on, so Serial out is the whole
   story. 115200 baud. Every key, every touch, the action each one caused,
   every poll and every failure prints one line:

       [   1234ms] key: UP tap -> display off

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

/* backlight PWM — a dimmable panel, not just on/off. 5 kHz/8-bit is a
   standard, flicker-free LEDC duty for driving an LED backlight this way.
   Bounds match the setup page's slider — power.ino/config.ino.           */
#define LEDC_BL_FREQ            5000
#define LEDC_BL_RES             8
#define BRIGHTNESS_MIN          30
#define BRIGHTNESS_MAX          100
#define BRIGHTNESS_DEFAULT_BATTERY   50
#define BRIGHTNESS_DEFAULT_CHARGING  90

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
   than in input.ino: KEY_LEFT and KEY_RIGHT name the physical positions,
   whichever silkscreen name is printed on the key. The logical roles input.ino
   assigns them are DOWN and UP, not LEFT/RIGHT — a physical-position macro
   feeding a differently-named logical key is intentional, not a mismatch.
   Every press prints on Serial with its GPIO. KEY_POWER is wired to the
   power circuit and has to stay on 5. */
#define KEY_LEFT       0     /* physically left  — DOWN: settings on / off · hold 2 s: hotspot    */
#define KEY_POWER      5     /* PWR              — tap: future use         · hold 2 s: off, and on */
#define KEY_RIGHT      4     /* physically right — UP: display on / off    · double-tap: mute      */

/* --------------------------------------------------------------- config
   What a person sets over the setup page, and the only place the endpoint
   and the networks come from — nothing here is baked into the firmware.
   config.ino loads and saves it, hotspot.ino edits it, wifi.ino and net.ino
   read it. docs/functional-requirements.md §5 is the owner of what each field means.

   MAX_NETWORKS is deliberately not 5: api.md already has two different
   caps of five — 5 metric screens and 5 tiles per screen (AGENTS.md §10) —
   and a third would get conflated with them in conversation. Say
   "networks", and the number is six.                                     */
#define MAX_NETWORKS  6
#define LEN_SSID     33    /* 32 + NUL, the 802.11 maximum                 */
#define LEN_PSK      65    /* 64 + NUL, a WPA2 passphrase or raw PSK       */
#define LEN_CRED     49    /* enterprise and sign-in names and passwords   */
#define LEN_URL     129
#define LEN_KEY      97    /* the API key, and the API secret              */
#define LEN_CA     2048    /* one pasted PEM root — a real one is ~1.2 KB  */

/* How a network is joined. The setup page picks it per network; a scan can
   suggest it, but only a person knows whether an "enterprise" SSID wants
   PEAP or the guest PSK printed on the wall.                             */
enum { SEC_OPEN = 0, SEC_PSK = 1, SEC_ENT = 2 };

/* One saved network. Order in the array IS the priority — wifi.ino joins the
   first one it can actually see, so the array is the preference list.

   Two kinds of network need more than an SSID and a password, and they are
   not the same kind of "more":
     · WPA2-Enterprise (SEC_ENT) authenticates during the join itself —
       802.1X wants an identity and an inner username and password, and
       without them the association never completes
     · a captive portal joins fine and then holds every request until a
       sign-in form is posted, so the join succeeds and the backend is
       still unreachable. `portal` says to expect that, and the two
       portal fields are what gets posted — a different credential from
       the enterprise one, often a different account entirely            */
struct WifiNet {
  char    ssid[LEN_SSID];
  uint8_t security;                  /* SEC_OPEN · SEC_PSK · SEC_ENT */
  char    psk[LEN_PSK];              /* SEC_PSK */
  char    identity[LEN_CRED];        /* SEC_ENT — outer identity, often anonymous@realm */
  char    user[LEN_CRED];            /* SEC_ENT — inner username */
  char    pass[LEN_CRED];            /* SEC_ENT — inner password */
  uint8_t portal;                    /* 1 = a sign-in page stands after the join */
  char    portalUser[LEN_CRED];
  char    portalPass[LEN_CRED];
};

/* The endpoint, its credentials and the networks to reach it over.

   One key, one secret, and the secret is the mode switch: secret empty means
   the key is a bearer token, secret set means the key is the public
   X-Api-Key and the secret signs each request — api.md §1 and its HMAC
   appendix. Two fields, no third field to contradict them.               */

/* The STATUS band's clock is display-only, and a raw UTC offset cannot show
   it correctly for half the year in any zone that observes daylight saving
   — the clock would read an hour wrong every spring and every autumn until
   someone remembered to change a number. A POSIX TZ string carries the
   actual transition RULE (which week, which month), so the C library's own
   localtime_r() applies daylight saving automatically, forever, for
   whichever zone is picked — one setenv("TZ", ...); tzset(); at boot
   (ws_lcd_154.ino's setup(), right after configLoad()) is the whole cost.
   This never touches what net.ino signs: time(nullptr) always returns raw
   UTC seconds regardless of TZ — only localtime_r() reads it, gmtime_r()
   and time() never do.                                                   */
struct TzZone { const char* label; const char* posix; };
static const TzZone TZ_TABLE[] = {
  { "UTC",                          "UTC0" },
  { "US Eastern (New York)",        "EST5EDT,M3.2.0,M11.1.0" },
  { "US Central (Chicago)",         "CST6CDT,M3.2.0,M11.1.0" },
  { "US Mountain (Denver)",         "MST7MDT,M3.2.0,M11.1.0" },
  { "US Mountain, no DST (Phoenix)","MST7" },
  { "US Pacific (Los Angeles)",     "PST8PDT,M3.2.0,M11.1.0" },
  { "US Alaska (Anchorage)",        "AKST9AKDT,M3.2.0,M11.1.0" },
  { "US Hawaii (Honolulu)",         "HST10" },
  { "UK (London)",                  "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Central Europe (Berlin)",      "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "India (Kolkata)",              "IST-5:30" },
  { "Japan (Tokyo)",                "JST-9" },
  { "Australia Eastern (Sydney)",   "AEST-10AEDT,M10.1.0,M4.1.0/3" },
};
#define TZ_COUNT ((uint8_t)(sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0])))
#define TZ_DEFAULT_INDEX 1     /* US Eastern (New York) — this device's own default */

struct Config {
  char     url[LEN_URL];              /* the full URL to poll, exactly as configured */
  char     key[LEN_KEY];
  char     secret[LEN_KEY];
  char     ca[LEN_CA];                /* optional PEM root; empty = TLS unverified */
  WifiNet  nets[MAX_NETWORKS];
  uint8_t  nnets;
  uint16_t muteTimeoutMin;            /* minutes until a mute clears itself; 0 = never — sound.ino */
  uint8_t  tzIndex;                    /* index into TZ_TABLE — display-only clock zone, applied via
                                          setenv("TZ",...)/tzset() at boot. NEVER read by net.ino:
                                          signed requests always sign raw UTC seconds — time(nullptr),
                                          never localtime_r() — unaffected by whatever this is set to. */
  char     lockCode[7];                /* the privacy lock's 6-digit code, plus NUL — lock.ino.
                                          Defaults to "123456"; never sent back to the setup page or
                                          printed to Serial, same rule as the API secret.            */
  uint16_t lockTimeoutMin;             /* minutes until an unlocked screen re-locks itself on its own;
                                          0 = never (only a manual lock or a restart) — lock.ino      */
  uint8_t  brightnessBattery;          /* backlight %, on the cell — BRIGHTNESS_MIN..MAX, default 40 */
  uint8_t  brightnessCharging;         /* backlight %, on the charger — BRIGHTNESS_MIN..MAX, default 90 */
};
static Config cfg;

/* ----------------------------------------------------------------- model
   One gateway. Every metric row carries the gateway it came from, so a
   backend that merged several platforms still says which is which.        */
#define MAX_ROWS    5
#define MAX_BUCKETS 30
#define MAX_TILES   5

/* One number card for the BODY band — up to 5 per row, position 0 is the
   heading, the rest fill the four body slots in order. `name` IS the text
   drawn — no separate label, truncated here if a backend sent more than
   5 chars. `unit` is one of "ms" / "s" / "%" / "" (a plain count) —
   AGENTS.md's aggregate tile guideline says what each does to
   fmtTileValue()'s output. `level` colours the value — "crit" red,
   "warn" orange, "info" (the default) the theme's own colour — the
   backend's own judgement call, never computed here — api.md §4.        */
struct Tile {
  char   name[6];
  double value;
  char   unit[4];
  char   level[10];
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
static constexpr uint16_t C_OR    = rgb(0xff7a00);   /* warn — orange, well clear of the red */
static constexpr uint16_t C_RD    = rgb(0xff2626);   /* bright red, nothing mixed in */
static constexpr uint16_t C_GY    = rgb(0x8aa0c0);
static constexpr uint16_t C_INK   = rgb(0x04060a);   /* words printed on a level colour */

/* level → word, solid colour, dark shade, flashes, sounds (the full alert),
   soft (the quieter notice instead — never both on the same level) */
struct Level { const char* word; uint16_t c, dark; bool flashes, sounds, soft; };
static const Level LV_INFO  = { "INFO",    C_GR, rgb(0x12380c), false, false, false };
static const Level LV_WARN  = { "WARN",    C_OR, rgb(0x3d1c00), true,  false, true  };
static const Level LV_CRIT  = { "CRIT",    C_RD, rgb(0x4a0c0c), true,  true,  false };
static const Level LV_FAULT = { "OFFLINE", C_GY, rgb(0x18202d), true,  false, true  };

/* One alert pattern for warn, crit and no connection alike: the WHOLE
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
/* the same blend as mixHex(), but between two colours already in RGB565 —
   for blending a divider against whatever it's sitting on, not a fixed
   hex constant. */
static uint16_t mix565(uint16_t a, uint16_t b, float t){
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  return (uint16_t)((lround(ar + (br - ar) * t) << 11) | (lround(ag + (bg - ag) * t) << 5) | lround(ab + (bb - ab) * t));
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
   on the left and its name stacked to the right; only the heading also
   draws a span line under its name. Only the scale differs.
   dashboard.ino's drawTile().                                            */
#define Y_HERO         10     /* heading: value size 4, name/span size 2 */
#define Y_HERO_SHARE   28     /* a few px under the name row — not flush against it */
#define Y_TILE1        58     /* body row 1 (tiles[1], tiles[2]): value/name both adaptive */
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

/* a soft divider instead of a flat rule — fades in from `bg`, peaks at `c`
   in the middle, fades back to `bg`, one pixel at a time (this canvas has
   no gradient fill of its own, unlike the mockup's vFade()/hFade()). Must
   come after `cv` above — these draw directly to it.                    */
static void hFade(int x, int y, int w, uint16_t bg, uint16_t c){
  for (int i = 0; i < w; i++){
    const float t = w > 1 ? (float)i / (w - 1) : 0;
    cv->drawPixel(x + i, y, mix565(bg, c, t < 0.5f ? t * 2 : (1 - t) * 2));
  }
}
static void vFade(int x, int y, int h, uint16_t bg, uint16_t c){
  for (int i = 0; i < h; i++){
    const float t = h > 1 ? (float)i / (h - 1) : 0;
    cv->drawPixel(x, y + i, mix565(bg, c, t < 0.5f ? t * 2 : (1 - t) * 2));
  }
}

TouchDrvCSTXXX touch;
static bool touchOK = false;

SensorQMI8658 imu;              /* imu.ino — same bus, same board, shake detection only */
static bool imuOK = false;

/* ------------------------------------------------------------------ state */
static uint8_t  curRow = 0;                  /* which metric screen is up */
/* which screen is up — input.ino moves between them */
#define VIEW_MAIN      0
#define VIEW_SETTINGS  1
#define VIEW_HOTSPOT   2
#define VIEW_LOCK      3
static uint8_t  view = VIEW_MAIN;
static char     deviceName[20] = "pulsar";   /* pulsar-xxxxxx from the MAC — set in setup() */
static uint8_t  macAddr[6];

/* What the radio reports — filled by wifi.ino as it joins and drops, read by
   the STATUS band's bars and the settings screen. `portalBlocked` is the
   awkward middle state a captive portal puts us in: associated, with an IP,
   and still unable to reach anything until a sign-in form is posted.     */
struct NetInfo {
  bool connected;
  int  rssi;
  char ip[16];
  char ssid[LEN_SSID];
  bool portalBlocked;
  bool timeSynced;                   /* SNTP answered — the signed mode needs a clock */
};
static NetInfo net = { false, 0, "", "", false, false };

/* The fetch layer's own verdict, separate from the payload's — api.md §6.
   While this is set the ALERT band shows it instead of `alert`, the last
   good numbers stay exactly where they were, and the whole screen flashes
   grey: "I cannot reach you" and "you say you are degraded" have different
   owners, and neither may be silent. A poll that works clears it.        */
static char faultWord[14]   = "";    /* "" = the last poll was good */
static char faultDetail[22] = "";

/* ----------------------------------------------- radio and hotspot state
   wifi.ino and hotspot.ino own all of this; it lives here because the IDE
   concatenates the .ino files and a global has to be declared before the
   first file that reads it.

   Joining is a state machine stepped from loop(), never a blocking connect:
   the render loop runs at ~25 fps and a board frozen for ten seconds on a
   network that is not there looks broken.                                */
enum { WS_IDLE, WS_SCAN, WS_JOIN, WS_PORTAL, WS_ONLINE, WS_WAIT, WS_SUSPENDED };
static uint8_t  wifiState   = WS_IDLE;
static uint32_t wifiStateT0 = 0;
static uint8_t  wifiCand[MAX_NETWORKS];   /* cfg.nets indices worth trying, best first */
static uint8_t  wifiNcand   = 0;
static uint8_t  wifiTry     = 0;          /* how far down wifiCand we are */
static uint32_t wifiJoined  = 0;          /* millis() we went online — the settings screen */

/* The setup hotspot. Both servers are raised only while it is up, so a board
   that never opens it never pays for them.                                */
static WebServer* apServer  = nullptr;
static DNSServer* apDns     = nullptr;
static char       apPass[10] = "";        /* this session's AP password, shown on screen */
static uint8_t    apSaves   = 0;          /* how many times the page has saved */
static uint32_t   apLastHit = 0;          /* millis() of the last request served */

static uint32_t countT0 = 0;
static double   countFromP[MAX_TILES];      /* each tile's value, last screen */
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
float easeOut(float t){ t = 1 - t; return 1 - t*t*t; }
float clampf(float v, float a, float b){ return v < a ? a : v > b ? b : v; }

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
/* One tile's value, formatted by its declared unit —
   the aggregate-tile guideline in AGENTS.md. "ms" reuses fmtLatency (which
   itself moves to "s" past 9999); "%" is a share, baked into the string;
   anything else (or no unit) is a plain compacted count. Returns the unit
   text to draw after the value, or NULL when there is none to draw
   separately (a share, or a bare count).                                 */
const char* fmtTileValue(double v, const char* unit, char* out, size_t cap){
  if (unit && !strcmp(unit, "ms")) return fmtLatency(v, out, cap);
  if (unit && !strcmp(unit, "%")){ fmtShare(v, 1, out, cap); return nullptr; }
  if (unit && !strcmp(unit, "s")){ snprintf(out, cap, "%.1f", v); return "s"; }
  fmtCount(v, out, cap);
  return nullptr;
}
/* How long the row's own clock covers — "LAST 30M" · "LAST 5M" ·
   "LAST 24H" · "LAST 45S" — `bucket_size × bucket_count` in `bucket_unit`,
   compacted so the number never needs more than 2 digits: 60+ s becomes
   minutes, 60+ m becomes hours. Sits in the heading tile's own second
   line — the row's span, not anything the tile itself carries. api.md §4. */
void spanCaption(int size, int count, const char* unit, char* out, size_t cap){
  long span = (long)size * count;
  char u = (unit && unit[0]) ? (char)toupper(unit[0]) : 'M';
  if (u == 'S' && span >= 60){ span = (span + 30) / 60; u = 'M'; }
  if (u == 'M' && span >= 60){ span = (span + 30) / 60; u = 'H'; }
  snprintf(out, cap, "LAST %ld%c", span, u);
}
/* 4s ago · 3m ago · 2h ago */
void ageText(uint32_t ms, char* out, size_t cap){
  const uint32_t s = ms / 1000;
  if (s < 5)          snprintf(out, cap, "just now");
  else if (s < 60)    snprintf(out, cap, "%lus ago", (unsigned long)s);
  else if (s < 3600)  snprintf(out, cap, "%lum ago", (unsigned long)(s / 60));
  else                snprintf(out, cap, "%luh ago", (unsigned long)(s / 3600));
}

/* ------------------------------------------------------------------- text
   One font, ProFont (fonts.h) — the same face as the wide build — in three
   sizes that stand in for the old 6x8-cell sizes:

       size 1     profont12   6 px wide, cap 8    STATUS, tile names, units, hints, the alert message
       size 2     profont22   12 px wide, cap 14  the level word, names, shares, FOOTER — the old width exactly
       size 3, 4  profont29   16 px wide, cap 19  every big value; 2 and 3 characters draw the same

   x,y is the TOP-LEFT of the capitals, as it always was; the library
   draws these fonts from the baseline, so `cap` is added underneath. The
   boot intro's stencil PULSAR alone keeps the built-in cell font, through
   txtCell(). halo: a 1 px outline in that colour (the ground under it),
   printed at the neighbours first, so text stays readable where it
   crosses the graph. 0 = no halo.                                       */
struct Font { const GFXfont* data; uint8_t adv, cap; };
static const Font F_SMALL = { &ProFont12,  6,  8 };
static const Font F_MED   = { &ProFont22, 12, 14 };
static const Font F_BIG   = { &ProFont29, 16, 19 };
static const struct Font& fontFor(uint8_t size){ return size >= 3 ? F_BIG : size == 2 ? F_MED : F_SMALL; }
/* how wide a string draws at a size — for layout decisions before drawing */
static int txtW(const char* s, uint8_t size){ return (int)strlen(s) * fontFor(size).adv; }

static int txt(const char* s, int x, int y, uint8_t size, uint16_t c,
               char align = 'l', bool bold = false, uint16_t halo = 0){
  const struct Font& f = fontFor(size);
  const int w = (int)strlen(s) * f.adv;
  if (align == 'c') x -= w / 2;
  if (align == 'r') x -= w;
  const int base = y + f.cap;
  cv->setFont(f.data);
  cv->setTextSize(1);
  if (halo){
    cv->setTextColor(halo);
    const int right = bold ? 2 : 1;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= right; dx++){
        if (dy == 0 && dx >= 0 && dx < right) continue;  /* the glyph itself covers these */
        cv->setCursor(x + dx, base + dy); cv->print(s);
      }
  }
  cv->setTextColor(c);
  cv->setCursor(x, base);
  cv->print(s);
  if (bold){ cv->setCursor(x + 1, base); cv->print(s); }
  return w;
}
/* the built-in 6x8 cell font, scaled — the intro's stencil word only */
static int txtCell(const char* s, int x, int y, uint8_t size, uint16_t c, char align = 'l', bool bold = false){
  const int w = (int)strlen(s) * 6 * size;
  if (align == 'c') x -= w / 2;
  if (align == 'r') x -= w;
  cv->setFont();
  cv->setTextSize(size);
  cv->setTextColor(c);
  cv->setCursor(x, y); cv->print(s);
  if (bold){ cv->setCursor(x + 1, y); cv->print(s); }
  return w;
}

/* `struct` in these signatures is required: Arduino IDE injects prototypes
   above the type definitions, and without it the compile dies with
   `'Level' does not name a type`. */
const struct Level& levelNow(){
  /* NO CLOCK is this device waiting on its own SNTP, not a connectivity
     problem with the backend — orange, not the grey every other fault
     gets, so it doesn't read as "can't reach you" when it's really
     "give me a second." Still a fault: still holds the last good data. */
  if (faultWord[0] && !strcmp(faultWord, "NO CLOCK")) return LV_WARN;
  if (faultWord[0])                    return LV_FAULT;   /* cannot reach the backend */
  if (!strcmp(snap.level, "crit")) return LV_CRIT;
  if (!strcmp(snap.level, "warn")) return LV_WARN;
  if (!strcmp(snap.level, "info")) return LV_INFO;
  return LV_FAULT;
}
/* What the ALERT band actually prints. A fault speaks over the payload's
   alert — the numbers under it are held, and saying "INFO" above held
   numbers would read as good news. api.md §6.                            */
const char* alertWord(const struct Level& L){
  return faultWord[0] ? faultWord : L.word;
}
const char* alertDetail(){
  return faultWord[0] ? faultDetail : snap.message;
}
/* Set or clear the fault banner. Only ever called by net.ino and wifi.ino,
   and every change is logged there — never set silently.                 */
void setFault(const char* word, const char* detail){
  strlcpy(faultWord,   word   ? word   : "", sizeof faultWord);
  strlcpy(faultDetail, detail ? detail : "", sizeof faultDetail);
}
void clearFault(){ faultWord[0] = faultDetail[0] = 0; }
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
const struct Theme& themeFor(const struct Level& L){
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
/* The same five voltage calibration points this always had (3.52/3.64/
   3.76/3.88/4.00 V), now interpolated linearly between them instead of
   stepped — a slow discharge should read as a slow, smooth decline on
   screen, not a sudden jump from 100% to 80% the moment it crosses one of
   these. Below the lowest point there's no calibration data to
   interpolate against, so it stays flat at the original "critically low"
   reading rather than guessing further.                                 */
int batteryPercent(){
  const float v = batteryVolts();
  static const float VOLTS[] = { 3.52f, 3.64f, 3.76f, 3.88f, 4.00f };
  static const int   PCT[]   = {    20,    40,    60,    80,   100 };
  const int n = sizeof(VOLTS) / sizeof(VOLTS[0]);
  if (v <= VOLTS[0])     return 1;
  if (v >= VOLTS[n - 1]) return 100;
  for (int i = 1; i < n; i++){
    if (v > VOLTS[i]) continue;
    const float t = (v - VOLTS[i - 1]) / (VOLTS[i] - VOLTS[i - 1]);
    return (int)lround(PCT[i - 1] + (PCT[i] - PCT[i - 1]) * t);
  }
  return 100;
}
bool charging(){ return digitalRead(PIN_CHARGING) == LOW; }

/* ------------------------------------------------------------------ toast */
void showToast(const char* s){ toastText = s; toastT0 = millis(); }

/* ----------------------------------------------------- forward declarations
   The IDE's ctags-based prototype scanner reads every .ino file as if it
   were all C++, and does not reliably parse this sketch's mix of nested
   structs, references and multi-file calls — it was found silently failing
   to prototype whole batches of functions, in ways that shifted with
   unrelated edits elsewhere. Every function called from a file other than
   the one that defines it is declared explicitly here instead, so the build
   no longer depends on that scanner at all. None of these are `static`: a
   `static` definition can't satisfy an extern-linkage prototype, and that
   mismatch is what an "undefined reference" at link time, rather than a
   compile error, means — so the matching definitions had `static` dropped
   too. Purely-internal helpers, used only within their own file, keep it.

   Grouped by the file that defines them; alphabetical within each group. */

/* config.ino */
bool        configApplyJson(JsonObjectConst in, char* err, size_t errcap);
bool        configHasEndpoint();
void        configJson(JsonDocument& doc);
void        configLoad();
bool        configSave();
bool        configFactoryReset();
bool        configSigned();
bool        configTlsVerified();
const char* secName(uint8_t s);
void        urlHostOf(const char* url, char* out, size_t cap);

/* dashboard.ino */
void        beginCount();
void        drawDashboard();
void        drawToast();
void        nextScreen();

/* hotspot.ino */
void        drawHotspot();
void        hotspotStart();
void        hotspotStop();
void        hotspotTick();

/* imu.ino */
void        imuBegin();
void        imuTick();

/* input.ino */
void        drawHoldOverlay();
void        handleKeys();
void        handleTouch();
void        holdRing(float p, const char* label);
void        keysBegin();

/* intro.ino */
void        bootIntro();

/* lock.ino */
void        drawLock();
void        drawLockedBody(const struct Theme& th);
void        lockBegin();
void        lockDigit(char d);
void        lockEngage();
bool        lockIsLocked();
void        lockHandleTap(int16_t x, int16_t y);
void        lockOpenKeypad();
void        lockTick();
bool        lockArmed();

/* net.ino */
void        cycleBegin();
void        cycleResetTimer();
void        cycleTick();
bool        netFetch();
void        netBegin();
uint32_t    pollIntervalMs();
void        refreshNow(const char* why);

/* power.ino */
void        applyBacklight();
void        backlightTick();
void        batteryTick();
void        displayToggle();
void        powerOff();
void        powerOnHold();

/* settings.ino */
void        drawSettings();

/* sound.ino */
void        alertSound();
void        introSound();
void        noticeSound();
void        shakeSound();
void        soundBegin();
void        soundTick();
void        toggleMute();

/* wifi.ino */
void        wifiBegin();
void        wifiResume();
const char* wifiStateName(uint8_t s);
void        wifiSuspend();
void        wifiTick();

/* =========================================================================
   Render
   ========================================================================= */
static void render(){
  if (!displayAwake) return;                 /* panel asleep — polls and alerts carry on */
  if (view == VIEW_SETTINGS) drawSettings();
  else if (view == VIEW_HOTSPOT) drawHotspot();
  else if (view == VIEW_LOCK) drawLock();
  else drawDashboard();
  drawHoldOverlay();        /* a key or a finger on its way to a 2 s hold — input.ino */
  drawToast();
  cv->flush();
}

/* The alert sound (or the quieter notice) rides the flash, and runs whether
   or not the panel is awake and whatever screen is up — the point of the
   speaker is to reach you when you are not looking at it.                */
static void alertTick(){
  static bool wasFlash = false;
  const Level& L = levelNow();
  const bool flash = alertFlash(L);
  if (flash && !wasFlash){
    if (L.sounds)     alertSound();     /* sound.ino — crit only */
    else if (L.soft)  noticeSound();    /* sound.ino — warn, and any connection fault */
  }
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
  Serial.printf("\nPulsar - ESP32-S3-LCD-1.54 - serial %d baud\n", SERIAL_BAUD);
  LOGF("boot", "reset: %s", resetName());          /* a boot loop names itself here */

  ledcAttach(PIN_LCD_BL, LEDC_BL_FREQ, LEDC_BL_RES);   /* dimmable, not just on/off — power.ino */
  ledcWrite(PIN_LCD_BL, 255);           /* full brightness until configLoad() below picks the real duty */

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
  imuBegin();                       /* imu.ino — same bus, shake detection only */
  LOGF("boot", "battery: %.2f V  %d%%  %s",
       batteryVolts(), batteryPercent(), charging() ? "charging" : "on battery");

  /* No payload is baked in, so there is nothing to show yet and nothing to
     stop for either: a board with no config, or one whose networks are all
     out of range, is a working board with an honest banner on it. The first
     poll happens the moment wifi.ino gets online.                        */
  // configFactoryReset();         /* uncomment for ONE flash to wipe every stored setting and
  //                                   saved network, then comment it out again — config.ino */
  configLoad();                    /* config.ino — the endpoint and the saved networks */
  applyBacklight();                /* the configured duty, now that it's loaded — power.ino */
  lockBegin();                     /* lock.ino — the persisted lockout count, if any */
  /* Display-only: the STATUS band's clock reads through this; net.ino's
     signing clock (time(nullptr)) never does — see the TZ_TABLE comment. */
  setenv("TZ", TZ_TABLE[cfg.tzIndex < TZ_COUNT ? cfg.tzIndex : TZ_DEFAULT_INDEX].posix, 1);
  tzset();
  buildThemes();
  netBegin();                      /* net.ino — the banner starts out saying what we wait for */
  wifiBegin();                     /* wifi.ino — joining runs in the background from here */
  soundBegin();             /* codec, the crit alert and the intro's torpedo fire — before the intro needs either — sound.ino */
  bootIntro();              /* once per power-on, hums with the beams, fades into the dashboard — intro.ino */
  if (soundMuted) showToast("SOUND OFF");  /* the brown-out mute check inside soundBegin() ran before
                                               anything was on screen to show it on — say it now instead */
  beginCount();             /* sweepT0 stays 0: the graph is already whole, as it faded in */
  cycleBegin();             /* start the 5 s screen rotation — net.ino */
  LOGF("boot", "ready - %u saved network%s, endpoint %s, poll every %lus",
       (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s",
       cfg.url[0] ? cfg.url : "NOT SET - hold DOWN 2 s",
       (unsigned long)(pollIntervalMs() / 1000));
}

/* The pause between frames, spent watching the inputs. A quick double-tap
   lifts and lands again inside one frame; read once a frame, the two taps
   merged into one long touch and the double-tap was hit or miss.        */
static void idleWait(uint32_t ms){
  const uint32_t t0 = millis();
  do {
    delay(7);
    handleTouch();          /* input.ino */
    handleKeys();
  } while (millis() - t0 < ms);
}

void loop(){
  handleTouch();            /* input.ino */
  handleKeys();             /* input.ino */
  imuTick();                /* shake x3 -> force a poll — imu.ino */
  wifiTick();               /* wifi.ino — scan, join, watch for drops */
  hotspotTick();            /* hotspot.ino — serves the setup page while the AP is up */
  cycleTick();              /* 5 s per screen, refetch when the loop wraps — net.ino */
  alertTick();
  backlightTick();          /* re-applies the duty only when the charge state actually changes — power.ino */
  batteryTick();            /* shuts the board down 2 min after the battery hits 10%, unless it recovers — power.ino */
  soundTick();              /* auto-clears a mute once its configured timeout elapses — sound.ino */
  lockTick();               /* auto-relocks an unlocked screen once its timeout elapses — lock.ino */
  render();                 /* ~25 fps; the alert flash and the count-up need it */
  idleWait(28);
}
