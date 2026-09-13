# 🔌 Flashing Pulsar onto the board — from Windows

Offline build. No Wi-Fi, no backend — the payload is a JSON literal inside `net.ino`.

- 📄 Sketch → this same folder — keep **every** `.ino`/`.cpp`/`.h` file here, flat, the IDE opens them all as tabs. The mockup and the build's own docs live right beside them, since Arduino ignores extensions it doesn't compile:

  | File                                 | What                                                              |
  | ------------------------------------ | ----------------------------------------------------------------- |
  | `ws_lcd_154.ino`                          | Pins, model, palette, logging, `setup()` / `loop()`               |
  | `intro.ino`                          | The welcome screen — the pulsar animation and the PULSAR label     |
  | `dashboard.ino`                      | The four bands: STATUS · ALERT · BODY · FOOTER                    |
  | `input.ino`                          | Keys and touch — taps, double-taps, holds                         |
  | `power.ino`                          | Power latch, off / on, display on / off                           |
  | `settings.ino`                       | Settings screen and hotspot screen                                |
  | `sound.ino`                          | The boot-intro hum, the critical alert sound, and mute             |
  | `net.ino`                            | Poll scheduling, the fetch placeholder, the test payload          |
  | `es8311.cpp` / `.h` / `es8311_reg.h` | Speaker codec driver (Espressif, Apache-2.0), from Waveshare's demo |
  | `README.md`                          | This file — flashing and troubleshooting                          |
  | `device.md`                          | This build's pixels and hardware                                  |
  | `mockup.html`                        | The interactive mockup, standalone, open it straight in a browser |

  Arduino requires the main `.ino` to share its containing folder's name —
  that's why it's `ws_lcd_154.ino` here rather than a generic name.

- 🖥️ Board → Waveshare **ESP32-S3-LCD-1.54** (ESP32-S3R8, 16 MB flash, 8 MB PSRAM)
- 🎨 What it should look like → [`mockup.html`](mockup.html)
- 🖥️ Screen layout and behaviour → [`device.md`](device.md)

---

## 1. 📥 Install the Arduino IDE

- Download **Arduino IDE 2.x** → <https://www.arduino.cc/en/software> · run the installer, accept the driver prompts

## 2. 🧩 Add ESP32 board support

1. **File → Preferences**
2. In _Additional boards manager URLs_ paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **OK**, then **Tools → Board → Boards Manager**, search `esp32`, install **esp32 by Espressif Systems** (v3.x)

## 3. 📚 Install the three libraries

**Tools → Manage Libraries**, search and install each:

| Library                     | Author             | Used for              |
| --------------------------- | ------------------ | --------------------- |
| **GFX Library for Arduino** | Moon On Our Nation | ST7789 panel + canvas |
| **SensorLib**               | lewisxhe           | CST816 touch          |
| **ArduinoJson**             | Benoit Blanchon    | parsing the payload   |

> If SensorLib asks to install dependencies, say yes.

🔊 **Nothing extra for the boot-intro hum or the critical alert sound.** `ESP_I2S` is part of the ESP32 board package from step 2, and the codec driver ships in the sketch folder.

## 4. 🔧 Board settings — the two that matter

Open `ws_lcd_154.ino`, then set **Tools** to:

| Setting                   | Value                                                |
| ------------------------- | ---------------------------------------------------- |
| Board                     | **ESP32S3 Dev Module** — not Arduino Nano ESP32      |
| **PSRAM**                 | **OPI PSRAM** ← required                             |
| **Flash Size**            | **16MB (128Mb)** ← required                          |
| Partition Scheme          | 16M Flash (3MB APP/9.9MB FATFS)                      |
| USB CDC On Boot           | **Enabled**                                          |
| Pin Numbering (if listed) | **By GPIO number (legacy)**                          |
| Flash Mode                | QIO 80MHz                                            |
| Upload Speed              | 921600                                               |
| Port                      | the COM port that appears when you plug the board in |

⚠️ **PSRAM must be ON.** The sketch draws into a 113 KB off-screen frame. With PSRAM off the screen shows `PSRAM OFF` and stops.

## 5. 🔌 Plug in and upload

1. Connect the board with the USB-C cable
2. **Tools → Port** → pick the new `COM*` entry
3. Click **Upload** (→)

## 6. 🆘 If the port never appears, or upload fails

Put the board in download mode by hand:

1. Hold **BOOT**
2. Tap **RESET** (or unplug/replug the cable) while still holding BOOT
3. Release **BOOT**
4. Re-pick the port and Upload again
5. After it flashes, tap **RESET** once to run it

Other things that bite:

| Symptom                                       | Fix                                                                                                                          |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| No COM port at all                             | Try another USB-C cable — many are charge-only                                                                               |
| `A fatal error occurred: ...`                  | Lower **Upload Speed** to 115200                                                                                             |
| Uploads fine, screen stays black               | Check **PSRAM = OPI PSRAM**, then look at Serial Monitor @ 115200                                                             |
| Screen is sideways from what you expect        | It is meant to be: `PANEL_ROTATION 1`, a quarter turn right so it reads while charging. Change that one line to undo it       |
| Screen draws, touch does nothing               | Non-touch SKU. Screens still turn by themselves every 5 s — serial prints `touch: NOT FOUND`                                 |
| LEFT and RIGHT feel swapped                    | This build already assumes PLUS/BOOT are wired backwards from Waveshare's own labelling — swap `KEY_LEFT`/`KEY_RIGHT`'s pin numbers back in `ws_lcd_154.ino` if yours isn't |
| Nothing happens when I tap PWR                 | Correct — PWR tap is future use. Serial says so. Hold it 2 s to switch off                                                   |
| Screen went black and won't come back          | You tapped the RIGHT key — that is display off. Tap it again. It was still polling the whole time                            |
| Board keeps restarting into the intro          | An older sketch — its intro soundtrack tripped the task watchdog. Flash this version; serial `boot: reset:` names the cause  |
| Starts with `SOUND OFF` after boot             | The last reset was a brown-out (weak USB port or low cell) — sound starts muted so it cannot loop. Double-tap the RIGHT key  |
| Critical flashes but no sound                  | Look for the crossed speaker in the STATUS band — it is muted; double-tap the RIGHT key. No icon: serial says `audio: NO CODEC` |
| PWR hold does not switch it off on USB         | Expected: USB keeps it powered, so it goes dark and deep-sleeps instead. Hold PWR 2 s to wake                                |
| Board dies as soon as PWR is let go            | Released before the ring closed — hold the full 2 s                                                                          |
| `es8311.h: No such file`                       | The three `es8311*` files are not beside `ws_lcd_154.ino` — keep the whole folder together                                          |
| Garbled or mirrored display                    | Wrong board variant — confirm it is the 1.54″ 240 × 240                                                                      |

## 7. ✅ What you should see

**Serial Monitor at 115200.** Everything the board does prints there — that is the whole debugging story, so leave it open.

```
Pulsar - ESP32-S3-LCD-1.54 - offline build - serial 115200 baud
[    210ms] boot: reset: power on
[    228ms] boot: panel ok - 240x240, rotation 1 (90 right)
[    231ms] boot: keys ready - LEFT settings | POWER future use | RIGHT display, double-tap mute
[    244ms] boot: device: pulsar-3fa2c1  mac 3C:84:27:3F:A2:C1
[    261ms] boot: touch: CST816 ok
[    268ms] boot: battery: 4.05 V  100%  charging
[    275ms] net: poll -> https://example.invalid/v1/gateway_health
[    276ms] net: offline build - no radio, reading the embedded payload instead
[    298ms] data: level=info "error budget healthy" - 5 metric screens
[    299ms] data:   1/5 Orders/Created  2XX=18659 4XX=109 5XX=15 AVG=42 P95=180  30 buckets
[    300ms] data:   2/5 Orders/Dispatched  2XX=8909 4XX=40 5XX=5 AVG=55 P95=230  30 buckets
[    303ms] net: poll OK in 28ms - 5 metrics, measured_at=1770000100
[    308ms] boot: audio: ES8311 ok
[   5312ms] boot: ready - 5 metric screens, poll every 30s
```

First, once: the **boot intro**, two screens, 5 seconds total. A pulsar turning steadily on black for 3 seconds, its two beams straight and even, filling the whole panel, humming low in step with them; then 2 seconds of a plain black screen with bold `PULSAR`, stencil-cut, coloured with a light-blue → red gradient drifting across the letters, silent. Then it cross-fades into the dashboard.

Then the dashboard, **rotated a quarter turn right** so you can read it with the charger plugged in. The test data merges two platforms — **Orders** and **Payments** — and it is `info`: calm, no flashing, no sound.

| Band   | Orders · Created                                                                             |
| ------ | ---------------------------------------------------------------------------------------------- |
| STATUS | Battery % with a charging bolt if plugged in, `02/02 02:41 AM` of the reading, Wi-Fi bars (flat — no radio) |
| ALERT  | Solid lime `INFO`, `error budget healthy`                                                     |
| BODY   | `18659` · `LAST 30M`, `4XX 109 0.58%`, `5XX 15 0.08%`, `AVG 42ms`, `P95 180ms`, a steady graph |
| FOOTER | Position rule with five segments, `ORDER Created`                                             |

**It runs with no input at all.** Every 5 seconds the next metric comes up; after the fifth it wraps to the first and polls again. Serial narrates all of it:

```
[  10430ms] view: auto -> screen 2/5 Orders/Dispatched
[  30450ms] net: cycle complete - next poll in 5s (interval 30s)
```

🎨 **Want to see the other levels?** Change `"level"` in the payload in `net.ino` to `"warning"` or `"critical"` and re-upload — critical is the one that flashes the whole screen and sounds, right at the start of every 5 s screen.

**Controls**

| Input         | Tap                   | Double-tap        | Hold 2 s                       |
| ------------- | ---------------------- | ----------------- | ------------------------------- |
| **Glass**     | Next metric screen     | _future use_      | Force refresh                   |
| **LEFT** key  | Settings on / off      | _future use_      | Hotspot mode (placeholder)      |
| **PWR** key   | _future use_           | _future use_      | Power off · when off, power on  |
| **RIGHT** key | **Display on / off**   | **Mute / unmute** | _future use_                    |

- ⭕ Holding shows a ring filling toward 2 s with the action inside — let go before it closes and nothing happens. A hold that is future use shows no ring
- 🌑 **RIGHT tap blanks the panel.** Nothing else stops — it keeps polling, and a critical still sounds in the dark. Tap again to bring it back
- 🔕 **RIGHT double-tap** mutes — `SOUND OFF` flashes up and a crossed speaker sits in the STATUS band. Double-tap again, or restart, and sound is back
- ⚙️ The **settings screen** lists battery, device name, sound, Wi-Fi (`no - offline build` here), MAC, and every distinct `gateway` the payload named (`Orders, Payments`) with the metric count, last poll and poll interval
- 📶 **Hotspot mode** is a placeholder screen for now — the settings page it will serve is still to be specified
- 🔎 Every press prints `key: LEFT down (GPIO0)` and then what it did — this build's LEFT/RIGHT are already swapped from the PLUS/BOOT silkscreen to match a board wired backwards; see the pin map below

---

## 🔋 Where the live values come from

Everything except battery is from the embedded JSON in `net.ino`. The battery is read off the hardware:

| Reading  | How                                                                       |
| -------- | --------------------------------------------------------------------------- |
| Voltage  | `analogReadMilliVolts(GPIO1)` × 3.0 — GPIO1 sits behind a 1/3 divider        |
| Enable   | GPIO2 driven HIGH powers that divider                                       |
| Charging | GPIO3, pull-up, **LOW = charging** — also what lights the STATUS band's bolt icon |
| Percent  | Waveshare's own voltage bands: <3.52 V → 1 %, then 20/40/60/80/100           |

⏰ **There is no clock.** No RTC is read and there is no NTP, so the time in the STATUS band is the payload's `measured_at`, formatted `MM/DD hh:mm AM`. Change `TZ_OFFSET_HOURS` at the top of the sketch to shift it.

## 📌 Pin map

Taken from Waveshare's own demos for this board, not guessed. This is the **physical silkscreen**, independent of firmware: `ws_lcd_154.ino` assigns `KEY_LEFT` to GPIO0 (silkscreened BOOT) and `KEY_RIGHT` to GPIO4 (silkscreened PLUS) — swapped from this table, to match a board wired backwards from Waveshare's own labelling. Swap them back if yours isn't.

| Signal         | GPIO | Signal         | GPIO |
| -------------- | ---- | -------------- | ---- |
| LCD DC         | 45   | I²C SDA        | 42   |
| LCD CS         | 21   | I²C SCL        | 41   |
| LCD SCK        | 38   | Touch RST      | 47   |
| LCD MOSI       | 39   | Touch INT      | 48   |
| LCD RST        | 40   | Battery ADC    | 1    |
| Backlight      | 46   | Power latch    | 2    |
| BOOT key       | 0    | Charging sense | 3    |
| PWR key        | 5    | PLUS key       | 4    |
| Speaker amp EN | 7    |                |      |
| I²S MCLK       | 8    | I²S BCLK       | 9    |
| I²S LRCK       | 10   | I²S DOUT       | 12   |

> GPIO2 latches the battery's power as well as feeding the divider — HIGH keeps the board on, LOW switches it off.

## 🔜 Going online later

`netFetch()` in `net.ino` fills a string and hands it to `parseSnapshot()`. Swapping the mock for the real thing means replacing two lines with an HTTPS `GET` — the model, the layout, the cycle and the invariant checks do not change. The function already logs the URL and the outcome, so a failing endpoint names itself — and `parseSnapshot()` is written to survive whatever a real server sends: a missing field, a null, a wrong type, or the connection just dropping. It logs the problem and keeps the last good screen rather than crash.

Want to point this build at something that behaves like a real backend, faults and all, before you have one? → **[../simulator](../simulator)**

Contract → [`../docs/api.md`](../docs/api.md)
