# 🔌 Flashing Pulsar onto the board — from Windows

Nothing about the backend is compiled in. Once it is flashed, hold **LEFT** for 2 s and the
board serves its own setup page → [§7](#7--first-run--setting-it-up).

- 📄 Sketch → this same folder — keep **every** `.ino`/`.cpp`/`.h` file here, flat, the IDE opens them all as tabs. The mockup and the build's own docs live right beside them, since Arduino ignores extensions it doesn't compile:

  | File                                 | What                                                              |
  | ------------------------------------ | ----------------------------------------------------------------- |
  | `ws_lcd_154.ino`                          | Pins, model, palette, logging, `setup()` / `loop()`               |
  | `intro.ino`                          | The welcome screen — the pulsar animation and the PULSAR label     |
  | `dashboard.ino`                      | The four bands: STATUS · ALERT · BODY · FOOTER                    |
  | `input.ino`                          | Keys and touch — taps, double-taps, holds                         |
  | `power.ino`                          | Power latch, off / on, display on / off                           |
  | `config.ino`                         | The stored endpoint and saved networks (NVS)                      |
  | `wifi.ino`                           | Joining saved networks — priority, enterprise, captive portals    |
  | `hotspot.ino`                        | The setup hotspot, the page it serves, and its screen             |
  | `settings.ino`                       | The settings screen                                               |
  | `sound.ino`                          | The boot-intro hum, the critical alert sound, and mute             |
  | `net.ino`                            | Poll scheduling and the HTTPS `GET`                               |
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

📶 **Nothing extra for the radio or the setup page either.** `WiFi`, `HTTPClient`, `WebServer`, `DNSServer`, `Preferences` and mbedtls all come with the ESP32 core. WPA2-Enterprise needs **core 3.x** — that is where `WiFi.begin(ssid, WPA2_AUTH_PEAP, …)` lives.

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
| Screen says `SETUP`, no numbers                | Correct on a fresh board: no URL is stored. Hold **LEFT** 2 s → [§7](#7--first-run--setting-it-up)                            |
| Screen says `OFFLINE`                          | No saved network is in range, or none of them accepted the password. Serial `wifi:` names the one it tried and why it failed  |
| Screen says `PORTAL`                           | It joined, but a Wi-Fi sign-in page is in the way. Tick "a sign-in page stands between…" for that network and add the login   |
| Screen says `NO ACCESS`                        | The backend answered 401/403 — the API key is wrong, or the secret is set when the backend wanted a bearer token             |
| Screen says `NOT FOUND` or `REDIRECT`          | The base URL is wrong. `/v1/gateway_health` is appended for you, so the URL should stop before it                            |
| Screen says `NO CLOCK`                         | Signed mode needs the time and SNTP has not answered. It clears on its own once it does; if it never does, the network blocks NTP |
| Hotspot never appears on my phone              | Look at the device screen — it prints the exact network name and this session's password. The password is new every time     |
| Joined the hotspot, no page appeared           | Open `192.168.4.1` by hand. Some phones only pop the page when they have no mobile data                                      |
| The setup page saved, nothing changed          | Saving does not rejoin — use **Save & restart**, or hold PWR 2 s twice                                                       |
| Enterprise (802.1X) network never joins        | Try the outer identity blank first (it then uses the username). Serial says `rejected - wrong password, or 802.1X refused it` |
| Settings screen says `TLS UNVERIFIED`          | No CA root is pasted, so the connection is encrypted but unauthenticated. Paste your CA's PEM into the setup page's TLS box  |

## 7. 🔧 First run — setting it up

A freshly flashed board knows nothing: no URL, no networks. It boots, plays the intro, and
then says so — `SETUP` in the ALERT band, `HOLD LEFT 2 S` in the middle of the screen.
**It shows no numbers at all**, and that is deliberate: a monitor that renders invented data
while it is misconfigured is worse than one that admits it has none.

**Hold LEFT for 2 s.** The screen becomes:

```
SETUP
JOIN THIS WI-FI
pulsar-3fa2c1
PASSWORD
k7fmq2xw
THEN OPEN
192.168.4.1
WAITING FOR A PHONE
```

1. Join that network from a phone or laptop, with **the password on the screen** — it is new
   every time the hotspot is raised, so it cannot be guessed from the outside
2. The setup page should open by itself. If it doesn't, browse to `192.168.4.1`
3. Fill in the **base URL** (stop before `/v1/gateway_health` — it is appended for you) and
   the **API key**. Leave the **API secret** empty for a bearer token; fill it in and every
   request gets signed instead
4. Add your Wi-Fi networks. **Tap "scan for networks"** and pick from what is actually in the
   room. Top of the list is tried first
5. **Save & restart**

The device screen is the test. Within a few seconds of the restart it either shows real
numbers, or names what stopped it — the table in [§6](#6--if-the-port-never-appears-or-upload-fails)
lists every banner. Serial narrates all of it:

```
Pulsar - ESP32-S3-LCD-1.54 - serial 115200 baud
[    210ms] boot: reset: power on
[    228ms] boot: panel ok - 240x240, rotation 1 (90 right)
[    244ms] boot: device: pulsar-3fa2c1  mac 3C:84:27:3F:A2:C1
[    251ms] cfg: url https://api.example.com
[    252ms] cfg: auth bearer token, TLS NOT VERIFIED (no CA pasted)
[    253ms] cfg: 2 saved networks
[    253ms] cfg:   1. "Office" psk
[    254ms] cfg:   2. "HotelGuest" open + sign-in page
[    261ms] boot: touch: CST816 ok
[    268ms] boot: battery: 4.05 V  100%  charging
[    275ms] net: waiting: OFFLINE - joining wi-fi
[    280ms] wifi: 2 saved networks, in priority order
[    290ms] wifi: scanning for the saved networks
[   1980ms] wifi: scan found 9 networks in range
[   1981ms] wifi:   candidate 1: "Office" psk, -52 dBm
[   1982ms] wifi: joining "Office" (psk)
[   4130ms] wifi: joined "Office" - 192.168.1.42, -52 dBm, took 2148ms
[   4390ms] wifi: probe: 204 - the way out is clear
[   4391ms] wifi: online
[   4392ms] net: poll -> https://api.example.com/v1/gateway_health [TLS NOT VERIFIED - no CA pasted]
[   4393ms] net: auth: bearer token 7f3a...
[   4712ms] net: 200 in 320ms, 2614 bytes
[   4750ms] data: level=info "error budget healthy" - 5 metric screens
[   4751ms] data:   1/5 Orders/Created  2XX=18659 4XX=109 5XX=15 AVG=42 P95=180  30 buckets
[   5312ms] wifi: clock set from SNTP - signed requests can be timestamped
[   5400ms] boot: ready - 2 saved networks, endpoint https://api.example.com, poll every 30s
```

🧪 **No backend yet?** Run the [simulator](../simulator) on your laptop, put its
`http://<laptop-ip>:<port>` in as the base URL, and you get real data over a real network —
plus buttons to fake every fault, so you can watch `NO ACCESS`, `THROTTLED` and `BACKEND`
land on the screen without breaking anything real.

## 8. ✅ What you should see once it is pointing somewhere

First, once per power-on: the **boot intro**, two screens, 5 seconds total. A pulsar turning steadily on black for 3 seconds, its two beams straight and even, filling the whole panel, humming low in step with them; then 2 seconds of a plain black screen with bold `PULSAR`, stencil-cut, coloured with a light-blue → red gradient drifting across the letters, silent. Then it cross-fades into the dashboard.

Then the dashboard, **rotated a quarter turn right** so you can read it with the charger plugged in. With the simulator's default data, two platforms are merged — **Orders** and **Payments** — and it is `info`: calm, no flashing, no sound.

| Band   | Orders · Created                                                                             |
| ------ | ---------------------------------------------------------------------------------------------- |
| STATUS | Battery % with a charging bolt if plugged in, `02/02 02:41 AM` of the reading, Wi-Fi bars lit by signal |
| ALERT  | Solid lime `INFO`, `error budget healthy`                                                     |
| BODY   | `18659` · `LAST 30M`, `4XX 109`, `5XX 15`, `AVG 42ms`, `P95 180ms`, a steady graph             |
| FOOTER | Position rule with five segments, `ORDER Created`                                             |

**It runs with no input at all.** Every 5 seconds the next metric comes up; after the last one it wraps to the first and polls again. Serial narrates all of it:

```
[  10430ms] view: auto -> screen 2/5 Orders/Dispatched
[  30450ms] net: cycle complete - next poll in 5s (interval 30s)
```

🎨 **Want to see the other levels?** Arm them in the [simulator](../simulator) — `warning`,
`critical`, and every fault from [api.md §6](../docs/api.md#6--errors). Critical is the one
that flashes the whole screen and sounds, right at the start of every 5 s screen.

**Controls**

| Input         | Tap                   | Double-tap        | Hold 2 s                       |
| ------------- | ---------------------- | ----------------- | ------------------------------- |
| **Glass**     | Next metric screen     | _future use_      | Force refresh                   |
| **LEFT** key  | Settings on / off      | _future use_      | **Setup hotspot**               |
| **PWR** key   | _future use_           | _future use_      | Power off · when off, power on  |
| **RIGHT** key | **Display on / off**   | **Mute / unmute** | _future use_                    |

- ⭕ Holding shows a ring filling toward 2 s with the action inside — let go before it closes and nothing happens. A hold that is future use shows no ring
- 🌑 **RIGHT tap blanks the panel.** Nothing else stops — it keeps polling, and a critical still sounds in the dark. Tap again to bring it back
- 🔕 **RIGHT double-tap** mutes — `SOUND OFF` flashes up and a crossed speaker sits in the STATUS band. Double-tap again, or restart, and sound is back
- ⚙️ The **settings screen** is the one to read when something is wrong: battery, device name and MAC; the network it joined, its signal and IP; the backend host, whether the key is sent as a bearer token or signed, and whether TLS is actually verified; then every distinct `gateway` the payload named, with the screen count and how old the numbers are
- 📶 **Hold LEFT 2 s** for the setup hotspot at any time — including from the settings screen. The access point goes down the moment you leave it
- 🔎 Every press prints `key: LEFT down (GPIO0)` and then what it did — this build's LEFT/RIGHT are already swapped from the PLUS/BOOT silkscreen to match a board wired backwards; see the pin map below

## 9. 📦 Producing a flashable binary

Everything above uploads straight from the IDE. To hand someone a file instead — flash a
second board without installing anything, or flash from a machine with no IDE at all —
export a `.bin` once and reuse it.

### From the IDE (no new tools)

1. Set the board menu exactly as in [§4](#4--board-settings--the-two-that-matter) — the export
   bakes in whatever is selected right then
2. **Sketch → Export Compiled Binary** (`Ctrl+Alt+S` / `Cmd+Alt+S`)
3. The IDE drops a `build/esp32.esp32.esp32s3/` folder beside the sketch. The one file that
   matters is **`ws_lcd_154.ino.merged.bin`** — bootloader, partition table and the app itself,
   already combined at their correct flash offsets. That single file *is* the board's flash,
   byte for byte, from address `0x0`

### Flashing that file with `esptool`, no IDE involved

```
pip install esptool
esptool.py --chip esp32s3 -p <PORT> -b 921600 write_flash 0x0 ws_lcd_154.ino.merged.bin
```

Same download-mode dance as [§6](#6--if-the-port-never-appears-or-upload-fails) applies if the
port doesn't show up on its own: hold **BOOT**, tap **RESET**, release **BOOT**, then flash.

### Building from the command line (`arduino-cli`, no GUI at all)

```
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino" "SensorLib" "ArduinoJson"
```

- 🔎 **Confirm the exact menu option names first** — they're stable per core release but do
  shift between major versions: `arduino-cli board details -b esp32:esp32:esp32s3` lists every
  key (`PSRAM`, `FlashSize`, `PartitionScheme`, …) and its valid values. Match each one to the
  [§4](#4--board-settings--the-two-that-matter) table before trusting a copied FQBN string
- Then compile with every option folded into the FQBN, one string, comma-separated:
  ```
  arduino-cli compile \
    --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,PartitionScheme=<match §4>,FlashMode=qio,FlashFreq=80,FlashSize=16M,UploadSpeed=921600" \
    --export-binaries .
  ```
  drops the same `.bin` files (merged one included, on core 3.x) into `build/.../` — flash with
  the `esptool.py` command above, or skip the middle step with `arduino-cli upload -p <PORT> --fqbn "<same string>" .`

### If your core doesn't produce a merged binary

Older core releases export the three pieces separately instead. Merge them yourself —
`boot_app0.bin` ships with the core package, not the sketch:

```
esptool.py --chip esp32s3 merge_bin -o ws_lcd_154-full.bin \
  --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0     ws_lcd_154.ino.bootloader.bin \
  0x8000  ws_lcd_154.ino.partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 ws_lcd_154.ino.bin
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 ws_lcd_154-full.bin
```

---

## 🔋 Where the live values come from

Everything except battery comes from the backend, over the URL you set. The battery is read off the hardware:

| Reading  | How                                                                       |
| -------- | --------------------------------------------------------------------------- |
| Voltage  | `analogReadMilliVolts(GPIO1)` × 3.0 — GPIO1 sits behind a 1/3 divider        |
| Enable   | GPIO2 driven HIGH powers that divider                                       |
| Charging | GPIO3, pull-up, **LOW = charging** — also what lights the STATUS band's bolt icon |
| Percent  | Waveshare's own voltage bands: <3.52 V → 1 %, then 20/40/60/80/100           |

⏰ **The STATUS band's clock is the data's, not the wall's.** There is no RTC, and the time shown is the payload's own `measured_at`, formatted `MM/DD hh:mm AM` — the time the numbers were measured, which is the one worth knowing. Change `TZ_OFFSET_HOURS` at the top of the sketch to shift it.

SNTP is asked for the real time on every join, but only so a **signed** request can carry a timestamp the backend will accept — it never moves the clock on screen.

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

## 🧷 What happens when the backend misbehaves

`parseSnapshot()` in `net.ino` is written to survive whatever a real server sends: a missing
field, a `null`, a wrong type, five hundred metrics, or the connection dropping mid-body. It
logs the problem and keeps the last good screen rather than crash — and it only ever replaces
what is on screen once a payload has parsed **completely**, so a half-read response can never
leave a torn mix of old and new numbers up.

Every failure gets a banner over the held numbers rather than a blank screen or a silent
stale one → [api.md §6](../docs/api.md#6--errors). The [simulator](../simulator) can arm
every one of them on demand, which is the cheapest way to see what each looks like before it
happens for real at 3 a.m.

Contract → [`../docs/api.md`](../docs/api.md) · Configuration → [`../docs/device.md`](../docs/device.md#6--configuration)
