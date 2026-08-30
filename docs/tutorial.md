# Pulsar tutorial

Program the **M5StickS3** to call a REST API with HMAC auth, draw your own screen, remember Wi-Fi networks, manage power on battery, and play an alarm.

Official hardware notes: [m5sticks3.md](m5sticks3.md) · [M5Stack StickS3 docs](https://docs.m5stack.com/en/core/StickS3)

---

## What you are building

| Mode | Display | Poll | Alert |
| --- | --- | --- | --- |
| USB / charger plugged in | Always on | Every **20 s** | Speaker + on-screen detail |
| Battery (unplugged) | Off | Background poll | Speaker only |
| Battery + **Btn A** | On for **60 s** | Same background poll | You can read the last payload |

There is **no hardware RTC** on StickS3. Sync time with **NTP** after Wi-Fi connects (HMAC timestamps need a real clock).

On battery, keep speaker volume **under 75%** or the 250 mAh pack can brown out.

---

## 1. Toolchain

Use **PlatformIO in VS Code**. The StickS3 is an ESP32-S3 with 8 MB flash + 8 MB PSRAM.

`platformio.ini`

```ini
[env:m5stack-sticks3]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.partitions = default_8MB.csv
board_build.arduino.memory_type = qio_opi
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    m5stack/M5Unified
    m5stack/M5GFX
    m5stack/M5PM1
    bblanchon/ArduinoJson
```

**Flash (download) mode:** USB-C in, hold the **side power / reset** button until the internal green LED flashes, then upload.

| Button | Role in Pulsar |
| --- | --- |
| Power (side, PMIC) | Click = on. Double-click = off. Hold = download mode. Do not use this as a UI button. |
| **Btn A** (front) | Wake screen 60 s / confirm |
| **Btn B** (side) | Hold 2 s → Wi-Fi setup portal. Short press → next field / back |

Libraries you will use:

- `M5Unified` — display, buttons, speaker, power
- `WiFi` + `WiFiMulti` — connect to any saved network that is in range
- `Preferences` — remember SSIDs and secrets in flash
- `HTTPClient` + `mbedtls` — HTTPS + HMAC-SHA256
- `ArduinoJson` — parse the API payload

---

## 2. First flash (prove the board)

```cpp
#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);          // 240 × 135 landscape
  M5.Display.setBrightness(128);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 48);
  M5.Display.print("Pulsar");

  M5.Speaker.setVolume(64);           // ~25%, safe on battery
  M5.Speaker.tone(880, 80);
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) M5.Speaker.tone(1200, 40);
  delay(10);
}
```

If you see **Pulsar** and hear a beep, the display, buttons, and ES8311 speaker path all work.

---

## 3. Create your own interface

The panel is **135 × 240**. Landscape (`setRotation(1)`) gives **240 × 135**, which is easier for a dashboard.

There is no touch. Design for **two buttons**:

```
┌──────────────────────────────┐
│ PULSAR          82%  USB     │  header
│──────────────────────────────│
│ Title                        │
│ body line 1                  │  payload
│ body line 2                  │
│──────────────────────────────│
│ A wake   B wifi              │  footer
└──────────────────────────────┘
```

```cpp
static constexpr int W = 240;
static constexpr int H = 135;

void drawHeader(bool plugged, int batPct, bool wifiOk) {
  M5.Display.fillRect(0, 0, W, 22, 0x2104);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, 0x2104);
  M5.Display.setCursor(4, 6);
  M5.Display.print("PULSAR");
  M5.Display.setCursor(150, 6);
  M5.Display.printf("%s %d%%", plugged ? "USB" : "BAT", batPct);
  M5.Display.setCursor(220, 6);
  M5.Display.print(wifiOk ? "W" : "-");
}

void drawPayload(const char* title, const char* line1, const char* line2) {
  M5.Display.fillRect(0, 22, W, H - 40, TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 32);
  M5.Display.print(title);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(8, 64);
  M5.Display.print(line1);
  M5.Display.setCursor(8, 80);
  M5.Display.print(line2);
}

void drawFooter(const char* hint) {
  M5.Display.fillRect(0, H - 18, W, 18, 0x2104);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_YELLOW, 0x2104);
  M5.Display.setCursor(4, H - 12);
  M5.Display.print(hint);
}

void showDashboard(bool plugged, int batPct, bool wifiOk,
                   const char* title, const char* a, const char* b) {
  drawHeader(plugged, batPct, wifiOk);
  drawPayload(title, a, b);
  drawFooter(plugged ? "B: wifi setup" : "A: peek 60s   B: wifi");
}
```

**Layout rules that fit this screen**

- Header 22 px, footer 18 px, payload in the middle.
- Title `setTextSize(2)` (about 12–14 chars). Body `setTextSize(1)`.
- Colors: black background, cyan title, yellow hints. Alerts can use `TFT_RED`.
- Never `fillScreen` on every poll — only redraw the payload rect so it does not flicker.

Turn the panel fully off (battery idle):

```cpp
void displayOff() {
  M5.Display.setBrightness(0);
  M5.Display.sleep();
}

void displayOn() {
  M5.Display.wakeup();
  M5.Display.setBrightness(128);
}
```

---

## 4. Remember many Wi-Fi networks

Goal: store several SSIDs, join whichever is in range, add more later without reflashing.

### Store in flash

```cpp
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiMulti.h>

Preferences prefs;
WiFiMulti wifiMulti;

struct WifiCred { String ssid; String pass; };

void loadWifiList() {
  prefs.begin("wifi", true);
  const int n = prefs.getInt("count", 0);
  for (int i = 0; i < n; i++) {
    const String ssid = prefs.getString(("s" + String(i)).c_str());
    const String pass = prefs.getString(("p" + String(i)).c_str());
    if (ssid.length()) wifiMulti.addAP(ssid.c_str(), pass.c_str());
  }
  prefs.end();
}

bool saveWifi(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  const int n = prefs.getInt("count", 0);
  for (int i = 0; i < n; i++) {
    if (prefs.getString(("s" + String(i)).c_str()) == ssid) {
      prefs.putString(("p" + String(i)).c_str(), pass);
      prefs.end();
      return true;
    }
  }
  prefs.putString(("s" + String(n)).c_str(), ssid);
  prefs.putString(("p" + String(n)).c_str(), pass);
  prefs.putInt("count", n + 1);
  prefs.end();
  wifiMulti.addAP(ssid.c_str(), pass.c_str());
  return true;
}

bool connectWifi(uint32_t timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  return wifiMulti.run(timeoutMs) == WL_CONNECTED;
}
```

`WiFiMulti` tries every saved AP and stays on the one that answers. When you walk into another saved network, call `connectWifi()` again after a disconnect.

### Add a network (setup portal)

Typing a password with two buttons is painful. Hold **Btn B** for 2 seconds and raise a soft AP + tiny web form.

```cpp
#include <WebServer.h>

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html",
    "<form action='/save' method='post'>"
    "SSID <input name='ssid'><br>"
    "Pass <input name='pass' type='password'><br>"
    "<button>Save</button></form>");
}

void handleSave() {
  saveWifi(server.arg("ssid"), server.arg("pass"));
  server.send(200, "text/plain", "Saved. Rebooting.");
  delay(500);
  ESP.restart();
}

void startWifiPortal() {
  displayOn();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 40);
  M5.Display.setTextSize(1);
  M5.Display.print("Join WiFi: Pulsar-Setup\nOpen 192.168.4.1");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Pulsar-Setup");
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  while (true) {
    server.handleClient();
    M5.update();
    delay(10);
  }
}
```

On first boot, if `count == 0`, go straight to the portal.

After STA connects, sync the clock (needed for HMAC):

```cpp
#include <time.h>

void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 20 && time(nullptr) < 1700000000; i++) delay(250);
}
```

---

## 5. REST call with custom HMAC

Put secrets in `include/config.h` (do not commit this file):

```cpp
#pragma once
#define API_BASE     "https://api.example.com"
#define API_PATH     "/v1/pulsar/status"
#define API_KEY      "your-public-key"
#define API_SECRET   "your-hmac-secret"
```

**Signing rule** (document this with your backend so both sides match):

```
string_to_sign = METHOD + "\n" + PATH + "\n" + unix_timestamp + "\n" + body
signature      = hex( HMAC-SHA256(API_SECRET, string_to_sign) )
```

Headers:

| Header | Value |
| --- | --- |
| `X-Api-Key` | public key |
| `X-Timestamp` | unix seconds |
| `X-Signature` | hex HMAC |
| `Content-Type` | `application/json` (if you POST a body) |

```cpp
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <ArduinoJson.h>
#include "config.h"

String hmacSha256Hex(const char* secret, const String& msg) {
  unsigned char mac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg.c_str(), msg.length());
  mbedtls_md_hmac_finish(&ctx, mac);
  mbedtls_md_free(&ctx);

  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", mac[i]);
  hex[64] = 0;
  return String(hex);
}

struct ApiPayload {
  bool ok = false;
  bool alert = false;
  String title;
  String line1;
  String line2;
};

ApiPayload fetchStatus() {
  ApiPayload out;
  const String method = "GET";
  const String body = "";
  const String ts = String((uint32_t)time(nullptr));
  const String toSign = method + "\n" + API_PATH + "\n" + ts + "\n" + body;
  const String sig = hmacSha256Hex(API_SECRET, toSign);

  WiFiClientSecure client;
  client.setInsecure();   // replace with setCACert() in production

  HTTPClient http;
  http.begin(client, String(API_BASE) + API_PATH);
  http.addHeader("X-Api-Key", API_KEY);
  http.addHeader("X-Timestamp", ts);
  http.addHeader("X-Signature", sig);

  const int code = http.GET();
  if (code != 200) {
    http.end();
    return out;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getString())) {
    http.end();
    return out;
  }
  http.end();

  out.ok = true;
  out.alert = doc["alert"] | false;
  out.title = doc["title"] | "Pulsar";
  out.line1 = doc["line1"] | "";
  out.line2 = doc["line2"] | "";
  return out;
}
```

Example backend response:

```json
{
  "alert": true,
  "title": "Door open",
  "line1": "Front gate",
  "line2": "since 01:12"
}
```

Your server must rebuild the **same** `string_to_sign` and reject stale timestamps (for example older than 60 s). That is what makes this HMAC useful.

To POST JSON instead of GET, put the raw body string in `string_to_sign` and use `http.POST(body)`.

---

## 6. Alarm sound

The StickS3 speaker is **ES8311 + AW8737**, not a piezo. Use `M5.Speaker`.

```cpp
void playAlarm() {
  const uint8_t vol = isPluggedIn() ? 128 : 80;  // < 75% on battery
  M5.Speaker.setVolume(vol);
  for (int i = 0; i < 4; i++) {
    M5.Speaker.tone(1800, 180);
    delay(220);
    M5.Speaker.tone(900, 180);
    delay(220);
  }
}
```

Play this when `payload.alert` goes from false → true, even if the display is asleep. Do not loop forever on battery or you will kill the pack — four beeps is enough; poll again later if it is still alert.

Deduplicate so a sticky `alert: true` does not scream every cycle:

```cpp
static bool lastAlert = false;

void handleAlert(const ApiPayload& p) {
  if (p.alert && !lastAlert) playAlarm();
  lastAlert = p.alert;
}
```

---

## 7. Power management

Detect USB vs battery with the PMIC (`M5PM1`):

```cpp
bool isPluggedIn() {
  const auto chg = M5.Power.isCharging();
  if (chg == m5::Power_Class::is_charging) return true;
  const int vbus = M5.Power.getVBUSVoltage();
  return vbus > 4000;   // ~5 V present on USB
}

int batteryPct() {
  return M5.Power.getBatteryLevel();
}
```

### Plugged in

- `displayOn()`
- Poll every **20 s**
- Redraw payload when it changes
- Alarm if `alert` rises

### Battery (unplugged)

- `displayOff()` immediately
- Keep Wi-Fi up (or reconnect), poll in the background
- On important `alert`, **speaker only** — do not wake the panel
- **Btn A** → `displayOn()`, show last payload, start a 60 s timer, then `displayOff()`
- Between polls, `lightSleep` to save the 250 mAh cell

```cpp
static uint32_t screenUntil = 0;
static ApiPayload last;

void peekScreen() {
  displayOn();
  showDashboard(false, batteryPct(), WiFi.isConnected(),
                last.title.c_str(), last.line1.c_str(), last.line2.c_str());
  screenUntil = millis() + 60000;
}

void loop() {
  M5.update();
  const bool plugged = isPluggedIn();
  const uint32_t interval = plugged ? 20000 : 60000;

  if (M5.BtnB.pressedFor(2000)) startWifiPortal();
  if (!plugged && M5.BtnA.wasPressed()) peekScreen();

  if (!plugged && screenUntil && millis() > screenUntil) {
    displayOff();
    screenUntil = 0;
  }

  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= interval) {
    lastPoll = millis();
    if (WiFi.status() != WL_CONNECTED) connectWifi();
    last = fetchStatus();
    handleAlert(last);
    if (plugged || screenUntil) {
      showDashboard(plugged, batteryPct(), last.ok,
                    last.title.c_str(), last.line1.c_str(), last.line2.c_str());
    }
  }

  if (!plugged && !screenUntil) {
    M5.Display.sleep();
    // light sleep until next poll, or Btn A (GPIO 11) wakes
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_11, 0);
    M5.Power.lightSleep((uint64_t)interval * 1000);
    M5.Display.wakeup();   // only if you will draw; else stay asleep
  }

  delay(10);
}
```

Btn A is **GPIO 11** (KEY1). `lightSleep` keeps RAM, so `last` and `lastAlert` survive. `deepSleep` would reboot and lose that state unless you stash it in `Preferences` or RTC memory.

Suggested poll while on battery: **60 s**. Plugged in: **20 s**.

---

## 8. Suggested file layout

```
pulsar/
  platformio.ini
  include/config.h          // secrets, not in git
  src/
    main.cpp                // setup/loop, power + peek timer
    ui.cpp / ui.h           // header, payload, footer
    wifi_store.cpp          // Preferences + WiFiMulti + portal
    api.cpp                 // HMAC + HTTP + JSON
    alert.cpp               // speaker alarm
```

`setup()` order:

1. `M5.begin`
2. Load Wi-Fi list
3. If empty → portal
4. `connectWifi` → `syncTime`
5. If unplugged → `displayOff`, else draw empty dashboard
6. First `fetchStatus`

---

## 9. Checklist

- [ ] Hello World + beep
- [ ] Save two Wi-Fi networks, walk between them, auto-join
- [ ] Portal adds a third network without a reflash
- [ ] NTP time is valid before the first HMAC call
- [ ] GET returns 200 with a matching server signature
- [ ] Screen shows `title` / `line1` / `line2`
- [ ] `alert: true` plays the alarm once
- [ ] Unplug USB → screen goes black, alarm still works
- [ ] Btn A on battery → screen for 60 s, then off
- [ ] Plug USB back in → screen stays on, 20 s polls
