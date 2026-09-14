# 🔌 Flashing Pulsar onto the board — from Windows

Nothing about the backend is compiled in. Once it's flashed, hold **DOWN** for 2 s to
configure it → [docs/functional-requirements.md §5](../docs/functional-requirements.md#5--configuration).

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
  | `sound.ino`                          | The boot-intro torpedo fire, the critical alert sound, and mute    |
  | `net.ino`                            | Poll scheduling and the HTTPS `GET`                               |
  | `es8311.cpp` / `.h` / `es8311_reg.h` | Speaker codec driver (Espressif, Apache-2.0), from Waveshare's demo |
  | `README.md`                          | This file — flashing and troubleshooting                          |
  | `device.md`                          | This build's pixels and hardware                                  |
  | `mockup.html`                        | The interactive mockup, standalone, open it straight in a browser |

  Arduino requires the main `.ino` to share its containing folder's name —
  that's why it's `ws_lcd_154.ino` here rather than a generic name.

- 🖥️ Board → Waveshare **ESP32-S3-LCD-1.54** (ESP32-S3R8, 16 MB flash, 8 MB PSRAM)
- 🎨 What it should look like → [`mockup.html`](mockup.html)
- 🖥️ Screen layout, behaviour and hardware → [`device.md`](device.md)

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

🔊 **Nothing extra for the boot-intro torpedo fire or the critical alert sound.** `ESP_I2S` is part of the ESP32 board package from step 2, and the codec driver ships in the sketch folder.

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
| DOWN and UP feel swapped                       | This build already assumes PLUS/BOOT are wired backwards from Waveshare's own labelling — swap `KEY_LEFT`/`KEY_RIGHT`'s pin numbers back in `ws_lcd_154.ino` if yours isn't |
| Board keeps restarting into the intro          | An older sketch — its intro soundtrack tripped the task watchdog. Flash this version; serial `boot: reset:` names the cause  |
| Critical flashes but no sound                  | Look for the crossed speaker in the STATUS band — it is muted; double-tap the UP key. No icon: serial says `audio: NO CODEC` |
| Screen is locked and you don't know the code   | The code is set on the setup page and never shown back — hold DOWN 2 s to raise the hotspot and set a new one; that always works even while locked |
| Board dies as soon as PWR is let go            | Released before the ring closed — hold the full 2 s                                                                          |
| `es8311.h: No such file`                       | The three `es8311*` files are not beside `ws_lcd_154.ino` — keep the whole folder together                                          |
| Garbled or mirrored display                    | Wrong board variant — confirm it is the 1.54″ 240 × 240                                                                      |

🧪 **No backend yet?** Run the [simulator](../simulator) and point the board's URL at it —
real data over a real network, plus buttons to fake every fault.

Everything about what the device does once it's running — the setup flow, the dashboard,
controls, banners, serial log format — is in **[docs/functional-requirements.md](../docs/functional-requirements.md)**, not
here.

## 7. 📦 Producing a flashable binary

Everything above uploads straight from the IDE. To hand someone a file instead — flash a
second board without installing anything, or flash from a machine with no IDE at all —
export a `.bin` once and reuse it.

### Skipping Arduino entirely

Don't want to install any of §1–4 at all? Grab a ready-made [`ws_lcd_154.merged.bin`](#)
from the project's releases — someone else already ran the export below — and jump
straight to **flashing that file with `esptool`** further down. `esptool` (a small
Python tool) is the only thing this needs: no IDE, no board package, no libraries.

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

Contract → [`../docs/api.md`](../docs/api.md) · Behaviour and configuration →
[`../docs/functional-requirements.md`](../docs/functional-requirements.md) · This build's hardware → [`device.md`](device.md)
