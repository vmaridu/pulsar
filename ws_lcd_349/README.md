# 🔌 Flashing Pulsar onto the wide board — from Windows

Nothing about the backend is compiled in. Once it's flashed, hold the **left part of the glass** for 2 s to
configure it → [docs/functional-requirements.md §5](../docs/functional-requirements.md#5--configuration).
What this build does and what is still a placeholder → [`device.md`](device.md).

- 📄 Sketch → this same folder — keep **every** `.ino`/`.cpp`/`.h` file here, flat, the IDE opens them all as tabs. The mockup and the build's own docs live right beside them, since Arduino ignores extensions it doesn't compile:

  | File                                 | What                                                              |
  | ------------------------------------ | ----------------------------------------------------------------- |
  | `ws_lcd_349.ino`                     | Pins, model, palette, logging, `setup()` / `loop()`               |
  | `intro.ino`                          | The welcome screen — the pulsar animation and the PULSAR label     |
  | `dashboard.ino`                      | The three parts of the screen                                     |
  | `input.ino`                          | Keys and touch — taps, double-taps, holds                         |
  | `power.ino`                          | Power latch, off / on, display on / off, the backlight            |
  | `config.ino`                         | The stored endpoint and saved networks (NVS)                      |
  | `wifi.ino`                           | Joining saved networks — priority, enterprise, captive portals    |
  | `hotspot.ino` / `hotspot_page.h`     | The setup hotspot, the page it serves, and its screen             |
  | `settings.ino`                       | The settings screen                                               |
  | `sound.ino`                          | The boot-intro torpedo fire, the alert sounds, and mute           |
  | `lock.ino`                           | The privacy lock — code, keypad, auto-relock                      |
  | `fonts.h`                            | ProFont at four sizes, Adafruit GFX format — the screen's one font |
  | `net.ino`                            | Poll scheduling, the 5 s cycle and the HTTPS `GET`                |
  | `es8311.cpp` / `.h` / `es8311_reg.h` | Speaker codec driver (Espressif, Apache-2.0), from Waveshare's demo |
  | `README.md`                          | This file — flashing and troubleshooting                          |
  | `device.md`                          | This build's pixels and hardware                                  |
  | `mockup.html`                        | The interactive mockup, standalone, open it straight in a browser |

  Arduino requires the main `.ino` to share its containing folder's name —
  that's why it's `ws_lcd_349.ino` here rather than a generic name.

- 🖥️ Board → Waveshare **ESP32-S3-Touch-LCD-3.49** (ESP32-S3R8, 16 MB flash, 8 MB PSRAM), any case, with or without the cell
- 🏷️ **Which revision is it?** Boards shipped from June 2026 are **V2** — `Rev1.1` on the PCB silkscreen, `V2` on the case label. The sketch is set for V2; an original board needs one line changed → [§6](#6--if-the-port-never-appears-or-upload-fails)
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

## 3. 📚 Install the two libraries

**Tools → Manage Libraries**, search and install each:

| Library                     | Author             | Used for                                  |
| --------------------------- | ------------------ | ----------------------------------------- |
| **GFX Library for Arduino** | Moon On Our Nation | the AXS15231B panel over QSPI + the canvas |
| **ArduinoJson**             | Benoit Blanchon    | parsing the payload                       |

> Install **GFX Library for Arduino 1.5 or newer** — that is where the AXS15231B driver and the QSPI bus arrived. If the compile stops on `Arduino_AXS15231B` or `Arduino_ESP32QSPI`, the library is older than that; update it.

👆 **No touch library.** This panel's touch controller is part of the display driver chip and the sketch talks to it directly — nothing to install.

🔊 **Nothing extra for sound.** `ESP_I2S` is part of the ESP32 board package from step 2, and the codec driver ships in the sketch folder.

📶 **Nothing extra for the radio or the setup page either.** `WiFi`, `HTTPClient`, `WebServer`, `DNSServer`, `Preferences` and mbedtls all come with the ESP32 core. WPA2-Enterprise needs **core 3.x**.

## 4. 🔧 Board settings — the two that matter

Open `ws_lcd_349.ino`, then set **Tools** to:

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

⚠️ **PSRAM must be ON.** The sketch draws into a 220 KB off-screen frame. With PSRAM off the panel never comes up and Serial says `panel begin FAILED`.

## 5. 🔌 Plug in and upload

1. Connect the board with the USB-C cable
2. **Tools → Port** → pick the new `COM*` entry
3. Click **Upload** (→)
4. **Tools → Serial Monitor**, **115200 baud** — every boot step, key, touch and screen change prints there

## 6. 🆘 If the port never appears, or upload fails

Put the board in download mode by hand:

1. Hold **BOOT**
2. Tap **RESET** (or unplug/replug the cable) while still holding BOOT
3. Release **BOOT**
4. Re-pick the port and Upload again
5. After it flashes, tap **RESET** once to run it

Other things that bite:

| Symptom                                        | Fix                                                                                                                          |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| No COM port at all                             | Try another USB-C cable — many are charge-only                                                                               |
| `A fatal error occurred: ...`                  | Lower **Upload Speed** to 115200                                                                                             |
| Uploads fine, screen stays black, Serial says `panel begin FAILED` | Check **PSRAM = OPI PSRAM**                                                                                     |
| Backlight on, glass stays blank                 | Look for `touch: NOT FOUND` in the boot log: the display chip is hung, and the firmware cuts its power to recover it — on the cell the board switches itself off, so hold LEFT 2 s to boot it again. Still blank on an original (V1) board: set `BOARD_REV` to `1` at the top of `ws_lcd_349.ino` — V1 wires the backlight and the panel reset differently |
| Stray bright dots along the top edge            | The glass mangles the last pixel of every write. The frame goes out in ten strips, each with its first two pixels repeated after it so the real last pixel is never last — `panelFlush()` in `ws_lcd_349.ino`. Dots back means that tail is gone; dots at the bottom-left corner instead means the glass wraps the tail to the start of the frame — send the frame's own first two pixels there |
| Pressing LEFT restarts the board                | Expected: that key is on the power circuit, and a press pulses the rail. Setup is a 2 s hold on the left part of the glass, and settings a double-tap there, for exactly that reason |
| The picture is upside-down on the desk         | Change `PANEL_ROTATION` from `1` to `3` in `ws_lcd_349.ino`. Touch follows automatically                                      |
| Colours look inverted (white ground, dark text) | Flip the `false /* not inverted */` argument on the `Arduino_AXS15231B` line in `ws_lcd_349.ino` to `true`                  |
| Screen draws, a tap does nothing               | Serial says `touch: NOT FOUND` — screens still turn on their own every 5 s. A tap that lands far from where you pressed is the other rotation: see the row above |
| Board keeps restarting into the intro          | Serial `boot: reset:` names the cause on the first line of every boot                                                        |
| Intro plays but no sound                       | Serial says `audio: NO CODEC` — everything else works. Sound also starts muted straight after a brown-out reset              |
| Board dies as soon as PWR is let go            | Released before the ring closed — hold the full 2 s                                                                          |
| `undefined reference to` some function at link time | A file is missing from the sketch folder — copy **every** file listed at the top of this page, not just the ones that changed |
| `es8311.h: No such file`                       | The three `es8311*` files are not beside `ws_lcd_349.ino` — keep the whole folder together                                  |
| Screen is locked and you don't know the code   | The code is set on the setup page and never shown back — hold the left part of the glass 2 s to raise the hotspot and set a new one |
| You want a completely clean board              | **Tools → Erase All Flash Before Sketch Upload → Enabled**, upload once, then set it back to Disabled. That wipes every stored setting and saved network along with the old firmware. The factory reset on the setup page does the same without a reflash |
| Garbled or mirrored display                    | Wrong board variant — confirm it is the 3.49″ 172 × 640                                                                      |

Everything about what the device does once it's running — the screen, the cycle, the intro,
the serial log format — is in **[`device.md`](device.md)** and
**[docs/functional-requirements.md](../docs/functional-requirements.md)**, not here.

## 7. 📦 Producing a flashable binary

Everything above uploads straight from the IDE. To hand someone a file instead — flash a
second board without installing anything, or flash from a machine with no IDE at all —
export a `.bin` once and reuse it.

### From the IDE (no new tools)

1. Set the board menu exactly as in [§4](#4--board-settings--the-two-that-matter) — the export
   bakes in whatever is selected right then
2. **Sketch → Export Compiled Binary** (`Ctrl+Alt+S` / `Cmd+Alt+S`)
3. The IDE drops a `build/esp32.esp32.esp32s3/` folder beside the sketch. The one file that
   matters is **`ws_lcd_349.ino.merged.bin`** — bootloader, partition table and the app itself,
   already combined at their correct flash offsets. That single file *is* the board's flash,
   byte for byte, from address `0x0`

### Flashing that file with `esptool`, no IDE involved

```
pip install esptool
esptool.py --chip esp32s3 -p <PORT> -b 921600 write_flash 0x0 ws_lcd_349.ino.merged.bin
```

Same download-mode dance as [§6](#6--if-the-port-never-appears-or-upload-fails) applies if the
port doesn't show up on its own: hold **BOOT**, tap **RESET**, release **BOOT**, then flash.

### If your core doesn't produce a merged binary

Older core releases export the three pieces separately instead. Merge them yourself —
`boot_app0.bin` ships with the core package, not the sketch:

```
esptool.py --chip esp32s3 merge_bin -o ws_lcd_349-full.bin \
  --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0     ws_lcd_349.ino.bootloader.bin \
  0x8000  ws_lcd_349.ino.partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 ws_lcd_349.ino.bin
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 ws_lcd_349-full.bin
```

---

Contract → [`../docs/api.md`](../docs/api.md) · Behaviour → [`../docs/functional-requirements.md`](../docs/functional-requirements.md) · This build's hardware → [`device.md`](device.md)
