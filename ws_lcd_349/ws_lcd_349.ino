/* ===========================================================================
   Pulsar — Waveshare ESP32-S3-Touch-LCD-3.49  (172 x 640 AXS15231B, QSPI)
   ---------------------------------------------------------------------------
   The wide build. The panel is a 172 x 640 portrait strip; this firmware
   draws it landscape — 640 wide, 172 tall — lying on its long edge with the
   keys at the top right, the way it sits on a desk. PANEL_ROTATION below
   is the one number that turns it; touch follows the same number.

   Nothing about the endpoint is baked in. The full URL, the API key and the
   API secret are set over the device's own Wi-Fi hotspot — hold the left part of the glass 2 s —
   and kept in NVS with the saved networks; config.ino owns the store,
   hotspot.ino the page, wifi.ino the joining, net.ino the poll. An
   unconfigured board says SETUP and shows no numbers at all: a monitor that
   invents data is worse than one that admits it has none.

   Input — two keys, named by where they sit on the case (RESET, the third,
   is a hard reset). LEFT is on the board's power circuit and a press of it
   resets the chip on this hardware, so it is power and nothing else; the
   settings and setup-hotspot gestures live on the glass's left part — the
   STATUS / ALERT part, on screen whenever the dashboard is. No gesture
   waits for a double-tap any more — it read as unreliable on this hardware,
   on the keys and on the glass alike — so sound's mute toggle lives on the
   settings screen now, not a RIGHT double-tap. input.ino:
       GLASS  left part: tap settings · hold 2 s setup hotspot
              right parts: tap next · hold 2 s refresh
       RIGHT  tap: display on/off · hold 2 s: lock, or the unlock keypad
       LEFT   hold 2 s: power off · from off, hold 2 s: on

   Screen — the four bands, folded into three parts across 640 px:
       part 1  x   0..212   STATUS strip on top, ALERT beneath it
       part 2  x 213..426   BODY: the heading tile over the graph
       part 3  x 427..639   BODY's other four tiles, and the FOOTER's job —
                            gateway + metric name up top, position rule below
   An alert flashes the WHOLE screen, all three parts at once.

   Files in this folder (Arduino opens every .ino/.cpp/.h as a tab and
   concatenates them — globals live HERE, functions may live anywhere; the
   main .ino must share the folder's name):
       ws_lcd_349.ino     pins, model, palette, logging, setup/loop
       intro.ino          the welcome screen
       dashboard.ino      the three parts
       input.ino          keys and touch — taps and holds
       power.ino          power latch, off/on, display on/off, the backlight
       config.ino         the stored endpoint and saved networks (NVS)
       wifi.ino           joining saved networks — priority, enterprise, portals
       hotspot.ino        the setup hotspot, the page it serves, its screen
       settings.ino       the settings screen
       sound.ino          the sounds, and mute
       lock.ino           the privacy lock — code, keypad, auto-relock
       net.ino            poll scheduling, the 5 s cycle and the HTTPS GET
       es8311.*           vendor codec driver, do not edit
       fonts.h            ProFont in four sizes, generated — do not edit

   Serial is the debugger: 115200 baud, every input and action prints.

   Libraries (Arduino Library Manager):
       GFX Library for Arduino   by Moon On Our Nation   (Arduino_GFX ≥ 1.5)
       ArduinoJson               by Benoit Blanchon      (v7)
   Touch needs no library: the AXS15231B answers on I²C directly.

   Every pin, address and sequence below comes from Waveshare's own demos:
       github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2  (Arduino/examples)
   =========================================================================== */

#include <Arduino.h>
/* ESP32 core 3.x turns these into macros; Arduino_GFX declares methods of
   the same name and the macros explode inside them. This board's pin
   numbers ARE GPIO numbers — drop the macros, then include the library.  */
#undef pinMode
#undef digitalWrite
#undef digitalRead

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <ctype.h>
#include <ESP_I2S.h>                /* ships with the ESP32 core — the sounds */
#include <esp_system.h>             /* esp_reset_reason() */
#include <esp_mac.h>                /* esp_read_mac() — the device name */
#include <esp_heap_caps.h>          /* esp_ptr_external_ram() — the boot log says where the frame lives */
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

#if ARDUINOJSON_VERSION_MAJOR < 7
#error "Install ArduinoJson 7.x - v6 uses a different document API"
#endif

/* ------------------------------------------------------------------ serial */
#define SERIAL_BAUD 115200
#define LOGF(cat, fmt, ...) Serial.printf("[%7lums] " cat ": " fmt "\n", \
                                          (unsigned long)millis(), ##__VA_ARGS__)
#define LOG(cat, msg)       LOGF(cat, "%s", msg)

/* ------------------------------------------------------------ board revision
   Two revisions of this board exist and they wire the backlight and the
   panel's reset differently. Boards shipped from June 2026 on are V2 —
   "Rev1.1" on the PCB silkscreen, "V2" on the case label. Set this to 1
   for an original board.                                                 */
#define BOARD_REV 2

/* ---------------------------------------------------------------- pin map */
#define PIN_LCD_CS     9      /* QSPI: chip select, clock, four data lines */
#define PIN_LCD_SCK   10
#define PIN_LCD_D0    11
#define PIN_LCD_D1    12
#define PIN_LCD_D2    13
#define PIN_LCD_D3    14
#if BOARD_REV >= 2
#define PIN_LCD_BL    42      /* backlight PWM — ACTIVE LOW: duty 0 is full brightness */
#define PIN_LCD_RST   -1      /* V2 resets the panel through the I/O expander (EXIO_LCD_RST) */
#else
#define PIN_LCD_BL     8
#define PIN_LCD_RST   21
#endif

#define PIN_I2C_SDA   47      /* the board's own bus: I/O expander, codec, RTC, IMU */
#define PIN_I2C_SCL   48
#define PIN_TP_SDA    17      /* the touch controller has a bus to itself */
#define PIN_TP_SCL    18
#define TP_I2C_ADDR   0x3B    /* AXS15231B — touch is inside the display driver */

/* TCA9554 I/O expander at 0x20. Eight lines; the ones this build drives: */
#define EXIO_I2C_ADDR 0x20
#define EXIO_TOUCH_INT 0      /* input — unused, touch is polled */
#define EXIO_BL_EN     1      /* backlight enable, HIGH = on */
#define EXIO_LCD_RST   5      /* the panel's reset line on V2, active LOW */
#define EXIO_SYS_EN    6      /* the power latch: HIGH keeps the board on from the cell */
#define EXIO_NS_MODE   7      /* audio path enable — driven HIGH like Waveshare's audio demo */

#define PIN_BAT_ADC    4      /* ADC1_CH3, through a 1/3 divider */

#define KEY_BOOT       0      /* the RIGHT key on the case (silkscreen BOOT), active LOW */
#define KEY_POWER     16      /* the LEFT key on the case (silkscreen PWR), active LOW — also
                                 the key the power circuit listens to, so it switches on and off */

#define PIN_I2S_MCLK   7      /* ES8311 codec — from Waveshare's codec board config */
#define PIN_I2S_BCLK  15
#define PIN_I2S_LRCK  46
#define PIN_I2S_DOUT  45

/* backlight PWM — the driver input is active low on this board, so duty
   255 is dark and duty 0 is full. Bounds match the setup page's two
   sliders — power.ino / config.ino.                                     */
#define LEDC_BL_FREQ   20000
#define LEDC_BL_RES        8
#define BRIGHTNESS_MIN          30
#define BRIGHTNESS_MAX         100
#define BRIGHTNESS_DEFAULT_BATTERY   50
#define BRIGHTNESS_DEFAULT_CHARGING  90

/* --------------------------------------------------------------- config
   What a person sets over the setup page, and the only place the endpoint
   and the networks come from — nothing here is baked into the firmware.
   config.ino loads and saves it, hotspot.ino edits it, wifi.ino and net.ino
   read it. docs/functional-requirements.md §5 is the owner of what each
   field means. Say "networks", and the number is six.                   */
#define MAX_NETWORKS  6
#define LEN_SSID     33    /* 32 + NUL, the 802.11 maximum                 */
#define LEN_PSK      65    /* 64 + NUL, a WPA2 passphrase or raw PSK       */
#define LEN_CRED     49    /* enterprise and sign-in names and passwords   */
#define LEN_URL     129
#define LEN_KEY      97    /* the API key, and the API secret              */
#define LEN_CA     2048    /* one pasted PEM root — a real one is ~1.2 KB  */

/* How a network is joined. The setup page picks it per network. */
enum { SEC_OPEN = 0, SEC_PSK = 1, SEC_ENT = 2 };

/* One saved network. Order in the array IS the priority — wifi.ino joins the
   first one it can actually see. WPA2-Enterprise wants an identity and an
   inner username and password for the join itself; a captive portal joins
   fine and then holds every request until a sign-in form is posted —
   `portal` says to expect that, and the two portal fields get posted.   */
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

/* The STATUS clock is display-only, and a raw UTC offset cannot show it
   correctly for half the year in any zone that observes daylight saving.
   A POSIX TZ string carries the transition rule, so localtime_r() applies
   daylight saving on its own, forever. Never touches what net.ino signs:
   time(nullptr) is raw UTC regardless of TZ.                            */
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

/* The endpoint, its credentials and the networks to reach it over. One key,
   one secret, and the secret is the mode switch: empty means the key is a
   bearer token, set means each request is signed — api.md §1 and §8.    */
struct Config {
  char     url[LEN_URL];              /* the full URL to poll, exactly as configured */
  char     key[LEN_KEY];
  char     secret[LEN_KEY];
  char     ca[LEN_CA];                /* optional PEM root; empty = TLS unverified */
  WifiNet  nets[MAX_NETWORKS];
  uint8_t  nnets;
  uint16_t muteTimeoutMin;            /* minutes until a mute clears itself; 0 = never — sound.ino */
  uint8_t  tzIndex;                    /* index into TZ_TABLE — the display clock's zone */
  char     lockCode[7];                /* the privacy lock's 6-digit code, plus NUL — stored for lock.ino,
                                          never sent back to the setup page or printed to Serial     */
  uint16_t lockTimeoutMin;             /* minutes until an unlocked screen re-locks itself; 0 = never */
  uint8_t  brightnessBattery;          /* backlight %, on the cell — BRIGHTNESS_MIN..MAX, default 40 */
  uint8_t  brightnessCharging;         /* backlight %, on the charger — BRIGHTNESS_MIN..MAX, default 90 */
};
static Config cfg;

/* ----------------------------------------------------------------- model
   One gateway. Every metric row carries the gateway it came from.        */
#define MAX_ROWS    5
#define MAX_BUCKETS 30
#define MAX_TILES   5

struct Tile {
  char   name[6];               /* IS the text drawn — ≤ 5 chars, api.md §4 */
  double value;
  char   unit[4];               /* "ms" / "s" / "%" / "" */
  char   level[10];             /* the backend's own call — never computed here */
};
struct Row {
  char      gateway[16];
  char      name[18];
  char      unit[8];
  int       size, count;
  Tile      tiles[MAX_TILES];
  uint8_t   ntiles;
  int       buckets[MAX_BUCKETS];
  uint8_t   nbuckets;
};
struct Snapshot {
  uint32_t polledAt;
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
static constexpr uint16_t C_GR    = rgb(0x5cf22e);
static constexpr uint16_t C_OR    = rgb(0xff7a00);
static constexpr uint16_t C_RD    = rgb(0xff2626);
static constexpr uint16_t C_GY    = rgb(0x8aa0c0);
static constexpr uint16_t C_INK   = rgb(0x04060a);   /* words printed on a level colour */

/* level → word, solid colour, dark shade, flashes, sounds (the full alert),
   soft (the quieter notice instead — never both on the same level) */
struct Level { const char* word; uint16_t c, dark; bool flashes, sounds, soft; };
static const Level LV_INFO  = { "INFO",    C_GR, rgb(0x12380c), false, false, false };
static const Level LV_WARN  = { "WARN",    C_OR, rgb(0x3d1c00), true,  false, true  };
static const Level LV_CRIT  = { "CRIT",    C_RD, rgb(0x4a0c0c), true,  true,  false };
static const Level LV_FAULT = { "OFFLINE", C_GY, rgb(0x18202d), true,  false, true  };

/* The WHOLE screen flashes the level colour for the first ALERT_FLASH_MS of
   every new metric screen — synced to screenT0, never a clock of its own. */
#define ALERT_FLASH_MS   500
#define SCREEN_MS       5000      /* each metric screen is up this long */

/* What the screen is painted with. Normally black, with the graph as a quiet
   cyan ground under the numbers. During an alert flash every part goes to
   the level colour and every foreground on it turns to near-black ink.    */
#define FADE 12                 /* graph fill steps, top → baseline */
#define SWEEP_MS 900            /* graph columns draw in left → right */
struct Theme {
  uint16_t bg, tx, dim, rule;
  uint16_t gLine, gFill[FADE];
  bool     ink;
};
static Theme TH_DARK, TH_RED, TH_ORANGE, TH_GREY;
static uint8_t mixCh(uint32_t a, uint32_t b, int shift, float t){
  int x = (a >> shift) & 255, y = (b >> shift) & 255;
  return (uint8_t)lround(x + (y - x) * t);
}
static uint16_t mixHex(uint32_t a, uint32_t b, float t){
  return rgb(((uint32_t)mixCh(a, b, 16, t) << 16) | ((uint32_t)mixCh(a, b, 8, t) << 8) | mixCh(a, b, 0, t));
}
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

/* ---------------------------------------------------------------- display
   The panel is 172 x 640 portrait. The canvas is built on those raw
   dimensions and ROTATED, so everything else in this sketch draws on a
   640 x 172 landscape surface and the canvas lays each pixel down where
   the panel wants it. PANEL_ROTATION 1 or 3 — the two landscape turns.
   If the picture comes up upside-down on your desk, use the other one;
   touch reads through the same number, so it follows automatically.     */
#define PANEL_W        172
#define PANEL_H        640
#define W              640
#define H              172
#define PANEL_ROTATION   1

Arduino_DataBus* bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCK,
                                             PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
/* The init this glass actually wants — the sequence ESPHome ships for this
   exact board (its AXS15231 driver), which is also what Waveshare's own
   demo amounts to: NO software reset, a short unlock / config / lock,
   then sleep-out, MADCTL, COLMOD, display-on. Arduino_GFX's built-in
   AXS15231B sequence is for a different 180 x 640 glass, and its default
   tftInit() sends a software reset (0x01) whenever no reset GPIO is given
   — this panel is reset by its line on the expander instead, and a 0x01
   over QSPI leaves it dark. So the panel class is subclassed below to
   run exactly this and nothing else.                                     */
static const uint8_t AXS_INIT[] = {
  BEGIN_WRITE,
  WRITE_C8_BYTES, 0xBB, 8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5,   /* unlock */
  WRITE_C8_D8, 0xC1, 0x33,
  WRITE_C8_BYTES, 0xBB, 8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* lock */
  WRITE_COMMAND_8, 0x11,          /* sleep out */
  END_WRITE,
  DELAY, 120,
  BEGIN_WRITE,
  WRITE_C8_D8, 0x36, 0x00,        /* MADCTL: no mirror, RGB order */
  WRITE_C8_D8, 0x3A, 0x55,        /* COLMOD: 16 bits per pixel */
  WRITE_COMMAND_8, 0x29,          /* display on */
  END_WRITE,
  DELAY, 100,
};
class PulsarAXS15231B : public Arduino_AXS15231B {
public:
  using Arduino_AXS15231B::Arduino_AXS15231B;
protected:
  void tftInit() override {
    /* no hardware pulse here (done through the expander before begin()),
       no software reset, no INVOFF — just the sequence above */
    _bus->batchOperation(AXS_INIT, sizeof AXS_INIT);
  }
};
Arduino_GFX* panel = new PulsarAXS15231B(bus, PIN_LCD_RST, 0 /* raw orientation */, false /* not inverted */,
                                         PANEL_W, PANEL_H);
/* One off-screen frame so a redraw lands in a single push. 172*640*2 = 220 KB,
   which lives in PSRAM. Never draw straight to the panel inside loop().     */
Arduino_Canvas* cv = new Arduino_Canvas(PANEL_W, PANEL_H, panel, 0, 0, PANEL_ROTATION);

/* Where a landscape (x, y) lives in the canvas's raw 172 x 640 framebuffer —
   for the intro's per-pixel recolouring, which reads the buffer directly.
   Same arithmetic Arduino_Canvas uses for its own rotated writes.        */
static inline uint32_t fbIndex(int x, int y){
#if PANEL_ROTATION == 1
  return (uint32_t)x * PANEL_W + (PANEL_W - 1 - y);
#else
  return (uint32_t)(W - 1 - x) * PANEL_W + y;
#endif
}

/* Push the canvas to the glass in strips of 64 panel rows, each its own
   write — the same shape Waveshare's demo uses.

   The glass mangles the LAST pixel of every write: whatever closes the
   transaction, that pixel comes out wrong. One stream of the whole frame
   left stray dots along the top edge; ten strips left ten — one at the
   end of each, and the end of a strip is the top edge of the picture (the
   canvas is turned a quarter, so a panel row's last pixel is a landscape
   column's top). So every strip is sent with its own first two pixels
   repeated after it: the real last pixel is no longer last, and the two
   extras either fall off the end of the window or land back on the
   pixels they copy — right either way. That cleared nine of the ten.

   The tenth dot — always the same one, in the strip nearest the top-left
   corner — outlived that fix and a first attempt at this one (priming the
   transaction with a throwaway pixel before the real loop, on the theory
   that the FIRST write after startWrite() was the odd one out — it
   wasn't; the dot came back exactly the same afterward). The other
   natural asymmetry is the LAST write: it alone sits right up against
   endWrite(), whatever that does to actually close the transaction out.
   So the last strip is sent twice — identically, tail padding and all —
   right before endWrite() runs. If closing the transaction is what
   mangles a pixel, it now mangles the second, throwaway copy; the first,
   real copy already landed correctly, the same way every other strip's
   single copy already does.                                             */
#define FLUSH_ROWS 64
#define FLUSH_TAIL  2
static uint16_t* flushBuf = NULL;   /* one strip plus the tail, internal RAM */
void panelFlush(){
  uint16_t* fb = cv->getFramebuffer();
  if (!flushBuf) flushBuf = (uint16_t*)malloc(((size_t)PANEL_W * FLUSH_ROWS + FLUSH_TAIL) * sizeof(uint16_t));
  Arduino_TFT* tft = (Arduino_TFT*)panel;
  tft->startWrite();
  int lastY = 0, lastH = 0;
  for (int y = 0; y < PANEL_H; y += FLUSH_ROWS){
    const int h = (PANEL_H - y) < FLUSH_ROWS ? (PANEL_H - y) : FLUSH_ROWS;
    const uint16_t* strip = fb + (size_t)y * PANEL_W;
    const size_t n = (size_t)PANEL_W * h;
    tft->writeAddrWindow(0, y, PANEL_W, h);
    if (flushBuf){
      memcpy(flushBuf, strip, n * sizeof(uint16_t));
      memcpy(flushBuf + n, strip, FLUSH_TAIL * sizeof(uint16_t));
      bus->writePixels(flushBuf, n + FLUSH_TAIL);
    } else {
      bus->writePixels((uint16_t*)strip, n);   /* no room for the copy — the dots come back, nothing else does */
    }
    lastY = y; lastH = h;
  }
  if (flushBuf){                      /* the redundant resend — see the comment above */
    const uint16_t* strip = fb + (size_t)lastY * PANEL_W;
    const size_t n = (size_t)PANEL_W * lastH;
    tft->writeAddrWindow(0, lastY, PANEL_W, lastH);
    memcpy(flushBuf, strip, n * sizeof(uint16_t));
    memcpy(flushBuf + n, strip, FLUSH_TAIL * sizeof(uint16_t));
    bus->writePixels(flushBuf, n + FLUSH_TAIL);
  }
  tft->endWrite();
}

/* soft dividers — fade in from `bg`, peak at `c` in the middle, fade back */
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

/* ------------------------------------------------------------- the layout
   Three equal parts across the width. TOP_BAR_H is part 1's STATUS strip
   and part 3's header both — one height, so they line up.               */
static const int COL_X[4] = { 0, 213, 427, 640 };
#define TOP_BAR_H 26

/* ---------------------------------------------------------- I/O expander
   A TCA9554 on the board's own I²C bus holds the lines the chip ran out
   of: the power latch, the backlight enable, the panel reset. Two shadow
   bytes, written whole — the chip has no per-bit set.                    */
static uint8_t exioOut = 0xFF, exioCfg = 0xFF;    /* the chip's own power-up state */
static bool    exioOK  = false;
static bool exioReg(uint8_t reg, uint8_t v){
  Wire.beginTransmission(EXIO_I2C_ADDR);
  Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}
void exioWrite(uint8_t pin, bool hi){
  if (hi) exioOut |= (1 << pin); else exioOut &= ~(1 << pin);
  if (exioOK) exioReg(0x01, exioOut);
}
/* read one register back — proof that a write landed, for the boot log */
static int exioReadReg(uint8_t reg){
  Wire.beginTransmission(EXIO_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)EXIO_I2C_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}
/* Does the chip hold what was last written? Silent when it does; one
   line naming both values when it does not — the failure worth hearing. */
static bool exioVerify(const char* when){
  const int out = exioReadReg(0x01), cfg = exioReadReg(0x03);
  if (out == exioOut && cfg == exioCfg) return true;
  LOGF("boot", "expander MISMATCH %s: out=0x%02X (wrote 0x%02X) cfg=0x%02X (wrote 0x%02X) - latch, backlight or reset line may not have moved",
       when, out & 0xFF, exioOut, cfg & 0xFF, exioCfg);
  return false;
}
static void exioBegin(){
  Wire.beginTransmission(EXIO_I2C_ADDR);
  exioOK = Wire.endTransmission() == 0;
  if (!exioOK){ LOG("boot", "expander: NOT FOUND at 0x20 - no power latch, no backlight enable"); return; }
  /* levels first, then directions, so nothing glitches on the way to output:
     latch ON, backlight enable OFF until the panel is ready, reset idle HIGH,
     audio path enabled */
  exioOut = 0xFF;
  exioOut &= ~(1 << EXIO_BL_EN);
  exioReg(0x01, exioOut);
  exioCfg = 0xFF & ~((1 << EXIO_BL_EN) | (1 << EXIO_LCD_RST) | (1 << EXIO_SYS_EN) | (1 << EXIO_NS_MODE));
  exioReg(0x03, exioCfg);
  if (exioVerify("at boot")) LOG("boot", "expander: TCA9554 ok - power latched");
}

/* ------------------------------------------------------------------ touch
   Polled, not interrupt-driven. The controller answers an 11-byte request
   with up to 32 bytes: [1] is the finger count, [2..5] the first point.  */
static bool touchOK = false;
struct TouchPoint { bool down; int16_t x, y, rawX, rawY; };
static void touchBegin(){
  Wire1.beginTransmission(TP_I2C_ADDR);        /* the bus was begun in panelBegin() */
  touchOK = Wire1.endTransmission() == 0;
}
bool touchRead(struct TouchPoint& tp){
  static const uint8_t cmd[11] = { 0xb5, 0xab, 0xa5, 0x5a, 0, 0, 0, 0x0e, 0, 0, 0 };
  Wire1.beginTransmission(TP_I2C_ADDR);
  Wire1.write(cmd, sizeof cmd);
  if (Wire1.endTransmission(false) != 0) return false;
  uint8_t buf[32] = { 0 };
  const int n = Wire1.requestFrom((uint8_t)TP_I2C_ADDR, (uint8_t)sizeof buf);
  if (n < 6) return false;
  for (int i = 0; i < n && i < (int)sizeof buf; i++) buf[i] = Wire1.read();
  const uint8_t fingers = buf[1];
  tp.rawX = ((buf[2] & 0x0f) << 8) | buf[3];      /* runs along the 640 axis */
  tp.rawY = ((buf[4] & 0x0f) << 8) | buf[5];      /* runs along the 172 axis */
  tp.down = fingers > 0 && fingers < 5;
  /* raw → the panel's own portrait frame, the way Waveshare's demo reads it */
  int nx = tp.rawY, ny = PANEL_H - tp.rawX;
  if (nx < 0) nx = 0;
  if (nx > PANEL_W - 1) nx = PANEL_W - 1;
  if (ny < 0) ny = 0;
  if (ny > PANEL_H - 1) ny = PANEL_H - 1;
  /* portrait → the landscape the canvas draws in, through PANEL_ROTATION */
#if PANEL_ROTATION == 1
  tp.x = ny; tp.y = PANEL_W - 1 - nx;
#else
  tp.x = PANEL_H - 1 - ny; tp.y = nx;
#endif
  return true;
}

/* ------------------------------------------------------------------ state */
static uint8_t  curRow = 0;                  /* which metric screen is up */
/* which screen is up — input.ino moves between them */
#define VIEW_MAIN      0
#define VIEW_SETTINGS  1
#define VIEW_HOTSPOT   2
#define VIEW_LOCK      3
static uint8_t  view = VIEW_MAIN;
static char     deviceName[20] = "pulsar";   /* pulsar-xxxxxx from the MAC */
static uint8_t  macAddr[6];

/* What the radio reports — filled by wifi.ino as it joins and drops, read by
   the STATUS bars and the settings screen. `portalBlocked` is the awkward
   middle state a captive portal puts us in: associated, with an IP, and
   still unable to reach anything until a sign-in form is posted.        */
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
   While this is set the ALERT part shows it instead of `alert`, the last
   good numbers stay exactly where they were, and the whole screen flashes
   grey. Only ever set by net.ino and wifi.ino, and always logged there.  */
static char faultWord[14]   = "";
static char faultDetail[22] = "";

/* ----------------------------------------------- radio and hotspot state
   wifi.ino and hotspot.ino own all of this; it lives here because the IDE
   concatenates the .ino files and a global has to be declared before the
   first file that reads it. Joining is a state machine stepped from
   loop(), never a blocking connect.                                      */
enum { WS_IDLE, WS_SCAN, WS_JOIN, WS_PORTAL, WS_ONLINE, WS_WAIT, WS_SUSPENDED };
static uint8_t  wifiState   = WS_IDLE;
static uint32_t wifiStateT0 = 0;
static uint8_t  wifiCand[MAX_NETWORKS];   /* cfg.nets indices worth trying, best first */
static uint8_t  wifiNcand   = 0;
static uint8_t  wifiTry     = 0;          /* how far down wifiCand we are */
static uint32_t wifiJoined  = 0;          /* millis() we went online — the settings screen */

/* The setup hotspot. Both servers are raised only while it is up. */
static WebServer* apServer  = nullptr;
static DNSServer* apDns     = nullptr;
static char       apPass[10] = "";        /* this session's AP password, shown on screen */
static uint8_t    apSaves   = 0;          /* how many times the page has saved */
static uint32_t   apLastHit = 0;          /* millis() of the last request served */

static uint32_t countT0 = 0;
static double   countFromP[MAX_TILES];
static uint32_t screenT0 = 0;      /* when the current metric screen came up — net.ino owns the cycle */
static uint32_t sweepT0 = 0;
static uint32_t refreshT0 = 0;     /* last forced refresh — lights the STATUS hairline */
static bool     audioOK = false;
static bool     soundMuted = false;/* toggled from settings' sound icon; RAM only, so every restart has sound */
static bool     displayAwake = true;/* RIGHT tap; the panel only — polling and alerts carry on */
static uint32_t toastT0 = 0;
static const char* toastText = "";

/* ------------------------------------------------------------- small utils */
float easeOut(float t){ t = 1 - t; return 1 - t*t*t; }
float clampf(float v, float a, float b){ return v < a ? a : v > b ? b : v; }

/* Every number on screen is at most 5 characters — AGENTS.md §10 */
static void fmtCount(double n, char* out, size_t cap){
  long long v = n < 0 ? 0 : llround(n);
  snprintf(out, cap, "%lld", v);
  if (strlen(out) <= 5) return;
  static const double unit[3] = { 1e3, 1e6, 1e9 };
  static const char   suf[3]  = { 'k', 'm', 'b' };
  for (int u = 0; u < 3; u++){
    for (int d = 2; d >= 0; d--){
      char t[24]; snprintf(t, sizeof t, "%.*f", d, v / unit[u]);
      if (atof(t) >= 1000) break;
      if (strlen(t) + 1 > 5) continue;
      if (d){
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
static void fmtShare(double p, int digits, char* out, size_t cap){
  if (p >= 99.995)                snprintf(out, cap, "100%%");
  else if (p >= 10 && digits < 2) snprintf(out, cap, "%.1f%%", p);
  else                            snprintf(out, cap, "%.2f%%", p);
}
static const char* fmtLatency(double ms, char* out, size_t cap){
  long v = ms < 0 ? 0 : lround(ms);
  if (v < 10000){ snprintf(out, cap, "%ld", v); return "ms"; }
  double sec = v / 1000.0;
  if (sec < 100) snprintf(out, cap, "%.1f", sec);
  else { long s = lround(sec); snprintf(out, cap, "%ld", s > 9999 ? 9999L : s); }
  return "s";
}
/* One tile's value by its declared unit. Returns the unit text to draw
   after it, or NULL when there is none to draw separately.               */
const char* fmtTileValue(double v, const char* unit, char* out, size_t cap){
  if (unit && !strcmp(unit, "ms")) return fmtLatency(v, out, cap);
  if (unit && !strcmp(unit, "%")){ fmtShare(v, 1, out, cap); return nullptr; }
  if (unit && !strcmp(unit, "s")){ snprintf(out, cap, "%.1f", v); return "s"; }
  fmtCount(v, out, cap);
  return nullptr;
}
/* "LAST 30M" — bucket_size × bucket_count in bucket_unit, compacted */
void spanCaption(int size, int count, const char* unit, char* out, size_t cap){
  long span = (long)size * count;
  char u = (unit && unit[0]) ? (char)toupper(unit[0]) : 'M';
  if (u == 'S' && span >= 60){ span = (span + 30) / 60; u = 'M'; }
  if (u == 'M' && span >= 60){ span = (span + 30) / 60; u = 'H'; }
  snprintf(out, cap, "LAST %ld%c", span, u);
}

/* ------------------------------------------------------------------- text
   One font, ProFont (fonts.h), in four sizes that stand in for the
   old 6x8-cell sizes 1 / 2 / 3, plus the alert message's own size:

       size 1   profont12   6 px wide, cap 8    labels, units, hints
       size 2   profont17   9 px wide, cap 11   names, the clock, values in settings
       message  profont22   12 px wide, cap 14  the alert message, up to two lines
       size 3   profont29   16 px wide, cap 19  the level word, the big numbers
       size 8   the built-in 6x8 font at x8 — the intro's stencil PULSAR only

   x,y is the TOP-LEFT of the capitals, as it always was; the library draws
   these fonts from the baseline, so `cap` is added underneath. halo: a
   1 px outline in that colour, printed at the neighbours first, so text
   stays readable where it crosses the graph. 0 = no halo.               */
struct Font { const GFXfont* data; uint8_t adv, cap; };
static const Font F_SMALL = { &ProFont12,  6,  8 };
static const Font F_MED   = { &ProFont17,  9, 11 };
static const Font F_MSG   = { &ProFont22, 12, 14 };
static const Font F_BIG   = { &ProFont29, 16, 19 };
static const struct Font& fontFor(uint8_t size){ return size >= 3 ? F_BIG : size == 2 ? F_MED : F_SMALL; }

static int txtFont(const char* s, int x, int y, const struct Font& f, uint16_t c,
                   char align, bool bold, uint16_t halo){
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
        if (dy == 0 && dx >= 0 && dx < right) continue;
        cv->setCursor(x + dx, base + dy); cv->print(s);
      }
  }
  cv->setTextColor(c);
  cv->setCursor(x, base);
  cv->print(s);
  if (bold){ cv->setCursor(x + 1, base); cv->print(s); }
  return w;
}
static int txt(const char* s, int x, int y, uint8_t size, uint16_t c,
               char align = 'l', bool bold = false, uint16_t halo = 0){
  if (size >= 8){                            /* the intro's stencil word — the built-in font, scaled */
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
  return txtFont(s, x, y, fontFor(size), c, align, bold, halo);
}
/* the alert message's own size — MSG_CHARS characters to a line in a part */
#define MSG_CHARS 16
static int txtMsg(const char* s, int x, int y, uint16_t c, char align = 'l', bool bold = false){
  return txtFont(s, x, y, F_MSG, c, align, bold, 0);
}

/* `struct` in these signatures is required: the IDE injects prototypes
   above the type definitions, and without it the compile dies.          */
const struct Level& levelNow(){
  /* NO CLOCK is this device waiting on its own SNTP, not a connectivity
     problem with the backend — orange, not the grey every other fault
     gets. Still a fault: still holds the last good data.               */
  if (faultWord[0] && !strcmp(faultWord, "NO CLOCK")) return LV_WARN;
  if (faultWord[0])                    return LV_FAULT;   /* cannot reach the backend */
  if (!strcmp(snap.level, "crit")) return LV_CRIT;
  if (!strcmp(snap.level, "warn")) return LV_WARN;
  if (!strcmp(snap.level, "info")) return LV_INFO;
  return LV_FAULT;
}
const char* alertWord(const struct Level& L){ return faultWord[0] ? faultWord : L.word; }
const char* alertDetail(){ return faultWord[0] ? faultDetail : snap.message; }
void setFault(const char* word, const char* detail){
  strlcpy(faultWord,   word   ? word   : "", sizeof faultWord);
  strlcpy(faultDetail, detail ? detail : "", sizeof faultDetail);
}
void clearFault(){ faultWord[0] = faultDetail[0] = 0; }
static bool alertFlash(const struct Level& L){
  return L.flashes && millis() - screenT0 < ALERT_FLASH_MS;
}
const struct Theme& themeFor(const struct Level& L){
  if (!alertFlash(L)) return TH_DARK;
  if (&L == &LV_CRIT) return TH_RED;
  if (&L == &LV_WARN) return TH_ORANGE;
  return TH_GREY;
}

/* ---------------------------------------------------------------- battery
   The cell's voltage off the ADC pin behind a 1/3 divider, calibrated by
   the chip. Nothing on this board tells the firmware whether the charger
   is plugged in, so charging() is a fixed "no" until a way turns up.    */
static void batteryBegin(){
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}
static float batteryVolts(){
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  return (mv / 8.0f) / 1000.0f * 3.0f;
}
/* Interpolated between five calibration points so a slow discharge reads
   as a slow decline, not a jump between fixed values.                    */
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
bool charging(){ return false; }

/* ------------------------------------------------------------------ toast */
void showToast(const char* s){ toastText = s; toastT0 = millis(); }

/* ----------------------------------------------------- forward declarations
   Every function called from a file other than the one that defines it is
   declared here, so the build never depends on the IDE's prototype scanner
   (it silently misses whole batches on a sketch shaped like this). None of
   these are `static` — a static definition cannot satisfy an extern-linkage
   prototype. Grouped by the file that defines them.                       */

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
bool        lockArmed();
void        lockBegin();
void        lockCloseKeypad();
void        lockEngage();
void        lockHandleTap(int16_t x, int16_t y);
bool        lockIsLocked();
void        lockOpenKeypad();
void        lockTick();

/* net.ino */
void        cycleBegin();
void        cycleResetTimer();
void        cycleTick();
bool        netFetch();
void        netBegin();
uint32_t    pollIntervalMs();
void        refreshNow();

/* power.ino */
void        applyBacklight();
void        batteryTick();
void        displayToggle();
void        powerOff();
void        powerOnHold();

/* settings.ino */
void        drawSettings();
void        settingsHandleTap(int16_t x, int16_t y);

/* sound.ino */
void        alertSound();
void        introSound();
void        noticeSound();
void        soundBegin();
void        soundTick();
void        toggleMute();

/* wifi.ino */
void        wifiBegin();
void        wifiResume();
const char* wifiStateName(uint8_t s);
void        wifiSuspend();
void        wifiTick();

/* 4s ago · 3m ago · 2h ago — the settings screen */
void ageText(uint32_t ms, char* out, size_t cap){
  const uint32_t s = ms / 1000;
  if (s < 5)          snprintf(out, cap, "just now");
  else if (s < 60)    snprintf(out, cap, "%lus ago", (unsigned long)s);
  else if (s < 3600)  snprintf(out, cap, "%lum ago", (unsigned long)(s / 60));
  else                snprintf(out, cap, "%luh ago", (unsigned long)(s / 3600));
}

/* =========================================================================
   Render
   ========================================================================= */
static void render(){
  if (!displayAwake) return;                 /* panel asleep — the cycle and the sounds carry on */
  if (view == VIEW_SETTINGS)     drawSettings();
  else if (view == VIEW_HOTSPOT) drawHotspot();
  else if (view == VIEW_LOCK)    drawLock();
  else                           drawDashboard();
  drawHoldOverlay();        /* a key or a finger on its way to a 2 s hold — input.ino */
  drawToast();
  panelFlush();
}

/* The alert sound (or the quieter notice) rides the flash. */
static void alertTick(){
  static bool wasFlash = false;
  const Level& L = levelNow();
  const bool flash = alertFlash(L);
  if (flash && !wasFlash){
    if (L.sounds)     alertSound();
    else if (L.soft)  noticeSound();
  }
  wasFlash = flash;
}

/* =========================================================================
   Lifecycle
   ========================================================================= */
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

/* The panel, in the order Waveshare's demo brings it up: backlight held
   off, a reset pulse, then the QSPI bus and the init sequence, and only
   then the backlight — so the glass never shows the panel's power-on
   noise. On V2 the reset line is on the expander; on V1 it is a GPIO and
   Arduino_GFX pulses it itself inside begin().                          */
/* One line when it all works — with the figures that would explain a
   blank glass: where the frame lives and how long a push takes. A line
   of its own only for the step that failed.                              */
/* Does the display chip answer on I²C? Its touch controller lives inside
   it, so the touch address answering is the chip being alive — the one
   health check available before anything is drawn.                      */
static bool panelChipAlive(){
  Wire1.beginTransmission(TP_I2C_ADDR);
  return Wire1.endTransmission() == 0;
}

static bool panelBegin(){
  ledcAttach(PIN_LCD_BL, LEDC_BL_FREQ, LEDC_BL_RES);
  ledcWrite(PIN_LCD_BL, 255);                 /* active low: 255 is dark */
  Wire1.begin(PIN_TP_SDA, PIN_TP_SCL, 400000);
  /* 40 MHz, SPI mode 0 — the library's own default, proven on this chip.
     The bus is begun by hand so the panel skips its bus begin and just
     runs the init sequence.                                              */
  if (!bus->begin(40000000)){ LOG("boot", "panel: QSPI bus begin FAILED"); return false; }
  /* Reset, init, then check the chip is actually there: its touch
     controller answers on I²C once the chip is up, and a chip that came
     through a power glitch hung answers nothing — and shows nothing,
     however well it is initialised. So reset and init again, up to three
     times, and say so either way.                                        */
  bool alive = false;
  for (int attempt = 1; attempt <= 3 && !alive; attempt++){
#if BOARD_REV >= 2
    exioWrite(EXIO_LCD_RST, true);  delay(30);
    exioWrite(EXIO_LCD_RST, false); delay(250);
    exioWrite(EXIO_LCD_RST, true);  delay(50);
#endif
    if (!panel->begin(GFX_SKIP_DATABUS_BEGIN)){ LOG("boot", "panel: init sequence FAILED"); return false; }
    delay(50);
    alive = panelChipAlive();
    if (!alive) LOGF("boot", "panel: chip not answering on I2C after reset + init %d of 3", attempt);
  }
  exioVerify("after the panel reset");
  if (!alive){
    /* A hung chip only comes back when its power is cut, and its power
       is the latch. So cut it. On the cell that switches the whole board
       off — the next LEFT press boots it with a fresh panel, which is the
       point. On USB the chip loses power while the board stays up, so
       restore the latch and try once more.                              */
    LOG("boot", "panel: chip NOT ANSWERING - hung. Cutting its power: on the cell the board switches off now - press LEFT 2 s to boot it clean");
    Serial.flush();
    exioWrite(EXIO_SYS_EN, false);
    delay(800);
    exioWrite(EXIO_SYS_EN, true);              /* still here: USB holds the board up */
    delay(300);
    for (int attempt = 1; attempt <= 3 && !alive; attempt++){
#if BOARD_REV >= 2
      exioWrite(EXIO_LCD_RST, true);  delay(30);
      exioWrite(EXIO_LCD_RST, false); delay(250);
      exioWrite(EXIO_LCD_RST, true);  delay(50);
#endif
      if (!panel->begin(GFX_SKIP_DATABUS_BEGIN)){ LOG("boot", "panel: init sequence FAILED"); return false; }
      delay(50);
      alive = panelChipAlive();
    }
    LOGF("boot", "panel: after the power cut the chip %s", alive ? "answers again" : "STILL does not answer - unplug USB and remove the cell for 10 s");
  }
  if (!cv->begin(GFX_SKIP_OUTPUT_BEGIN)){      /* allocates the 220 KB frame */
    LOGF("boot", "panel: frame alloc FAILED - free psram %u KB, free heap %u KB - is PSRAM set to OPI PSRAM?",
         (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getFreeHeap() / 1024));
    return false;
  }
  const bool inPsram = esp_ptr_external_ram(cv->getFramebuffer());
  cv->setTextWrap(false);                     /* clients truncate, never wrap */
  cv->fillScreen(RGB565_BLACK);
  const uint32_t tf = millis();
  panelFlush();
  const uint32_t flushMs = millis() - tf;
  /* full brightness until configLoad() in setup() picks the configured duty */
  ledcWrite(PIN_LCD_BL, 0);
  exioWrite(EXIO_BL_EN, true);
  exioVerify("after the backlight");
  LOGF("boot", "panel ok - %dx%d as %dx%d landscape (rotation %d), QSPI 40 MHz mode 0, frame in %s, one push %lu ms",
       PANEL_W, PANEL_H, W, H, PANEL_ROTATION, inPsram ? "PSRAM" : "INTERNAL RAM (slow - check PSRAM)",
       (unsigned long)flushMs);
  return true;
}

void setup(){
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.printf("\nPulsar - ESP32-S3-Touch-LCD-3.49 (V%d) - serial %d baud\n", BOARD_REV, SERIAL_BAUD);
  LOGF("boot", "reset: %s", resetName());

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  exioBegin();                     /* latches power first of all — on the cell the board dies without it */
  delay(150);                      /* let the rails settle before the panel is reset */
  batteryBegin();

  if (!panelBegin()){
    LOG("boot", "panel begin FAILED - stopping here");
    while (true) delay(1000);
  }

  powerOnHold();                   /* switched on by the power key? it must be held 2 s — power.ino */
  keysBegin();
  esp_read_mac(macAddr, ESP_MAC_WIFI_STA);
  snprintf(deviceName, sizeof deviceName, "pulsar-%02x%02x%02x", macAddr[3], macAddr[4], macAddr[5]);
  LOGF("boot", "device: %s  mac %02X:%02X:%02X:%02X:%02X:%02X", deviceName,
       macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

  touchBegin();
  LOGF("boot", "touch: %s", touchOK ? "AXS15231B ok" : "NOT FOUND - screens still turn on their own");
  LOGF("boot", "battery: %.2f V  %d%%", batteryVolts(), batteryPercent());

  /* No payload is baked in, so there is nothing to show yet and nothing to
     stop for either: a board with no config, or one whose networks are all
     out of range, is a working board with an honest banner on it. The first
     poll happens the moment wifi.ino gets online.                        */
  // configFactoryReset();         /* uncomment for ONE flash to wipe every stored setting and
  //                                   saved network, then comment it out again — config.ino */
  configLoad();                    /* config.ino — the endpoint and the saved networks */
  applyBacklight();                /* the configured duty, now that it's loaded — power.ino */
  lockBegin();                     /* lock.ino — the persisted lockout count, if any */
  /* Display-only: the STATUS clock reads through this; net.ino's signing
     clock (time(nullptr)) never does — see the TZ_TABLE comment.        */
  setenv("TZ", TZ_TABLE[cfg.tzIndex < TZ_COUNT ? cfg.tzIndex : TZ_DEFAULT_INDEX].posix, 1);
  tzset();
  buildThemes();
  netBegin();                      /* net.ino — the banner starts out saying what we wait for */
  wifiBegin();                     /* wifi.ino — joining runs in the background from here */
  soundBegin();                    /* codec, the sounds, the intro's torpedo fire — before the intro needs them */
  bootIntro();                     /* once per power-on, fades into the dashboard — intro.ino */
  if (soundMuted) showToast("SOUND OFF");
  beginCount();
  cycleBegin();                    /* start the 5 s screen rotation — net.ino */
  LOGF("boot", "ready - %u saved network%s, endpoint %s, poll every %lus",
       (unsigned)cfg.nnets, cfg.nnets == 1 ? "" : "s",
       cfg.url[0] ? cfg.url : "NOT SET - hold the left part of the glass 2 s",
       (unsigned long)(pollIntervalMs() / 1000));
}

/* The pause between frames, spent watching the inputs. Reading only once
   a frame (~28 ms) would blur every debounce window in input.ino down to
   that same coarse step — a touch-down would settle late, a mid-hold
   blink might outlast its own bridge window entirely. Every 7 ms keeps
   each of them meaningfully finer than what it's timed against.        */
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
  wifiTick();               /* wifi.ino — scan, join, watch for drops */
  hotspotTick();            /* hotspot.ino — serves the setup page while the AP is up */
  cycleTick();              /* 5 s per screen, refetch when the loop wraps — net.ino */
  alertTick();
  batteryTick();            /* shuts the board down 2 min after the battery hits 10%, unless it recovers — power.ino */
  soundTick();              /* auto-clears a mute once its configured timeout elapses — sound.ino */
  lockTick();               /* auto-relocks an unlocked screen once its timeout elapses — lock.ino */
  render();                 /* ~25 fps; the alert flash and the count-up need it */
  idleWait(28);
}
