# 🔌 Flashing Pulsar onto the board — from Windows

Offline build. No Wi-Fi, no backend — the payload is a JSON literal inside the sketch.

- 📄 Sketch → [`pulsar_lcd154/pulsar_lcd154.ino`](pulsar_lcd154/pulsar_lcd154.ino)
- 🖥️ Board → Waveshare **ESP32-S3-LCD-1.54** (ESP32-S3R8, 16 MB flash, 8 MB PSRAM)
- 🎨 What it should look like → [`../mockups/device-ui.html`](../mockups/device-ui.html)

---

## 1. 📥 Install the Arduino IDE

- Download **Arduino IDE 2.x** → <https://www.arduino.cc/en/software> · run the installer, accept the driver prompts

## 2. 🧩 Add ESP32 board support

1. **File → Preferences**
2. In *Additional boards manager URLs* paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **OK**, then **Tools → Board → Boards Manager**, search `esp32`, install **esp32 by Espressif Systems** (v3.x)

## 3. 📚 Install the three libraries

**Tools → Manage Libraries**, search and install each:

| Library                     | Author            | Used for               |
| --------------------------- | ----------------- | ---------------------- |
| **GFX Library for Arduino** | Moon On Our Nation | ST7789 panel + canvas |
| **SensorLib**               | lewisxhe          | CST816 touch           |
| **ArduinoJson**             | Benoit Blanchon   | parsing the payload    |

> If SensorLib asks to install dependencies, say yes.

## 4. 🔧 Board settings — the two that matter

Open `pulsar_lcd154/pulsar_lcd154.ino`, then set **Tools** to:

| Setting                     | Value                          |
| --------------------------- | ------------------------------ |
| Board                       | **ESP32S3 Dev Module**         |
| **PSRAM**                   | **OPI PSRAM** ← required       |
| **Flash Size**              | **16MB (128Mb)** ← required    |
| Partition Scheme            | 16M Flash (3MB APP/9.9MB FATFS) |
| USB CDC On Boot             | **Enabled**                    |
| Flash Mode                  | QIO 80MHz                      |
| Upload Speed                | 921600                         |
| Port                        | the COM port that appears when you plug the board in |

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

| Symptom                            | Fix                                                          |
| ---------------------------------- | ------------------------------------------------------------- |
| No COM port at all                 | Try another USB-C cable — many are charge-only                |
| `A fatal error occurred: ...`      | Lower **Upload Speed** to 115200                              |
| Uploads fine, screen stays black   | Check **PSRAM = OPI PSRAM**, then look at Serial Monitor @ 115200 |
| Screen draws, touch does nothing   | Non-touch SKU (33867). Use the **PLUS** key instead — serial prints `touch: not found` |
| Garbled or mirrored display        | Wrong board variant — confirm it is the 1.54″ 240 × 240       |

## 7. ✅ What you should see

Serial Monitor at **115200** prints:

```
Pulsar - ESP32-S3-LCD-1.54 - offline build
touch: CST816 ok
battery: 4.05 V  100%  charging
gateway Payouts        4 rows  level=critical
gateway Fraud VAS      2 rows  level=info
```

On the panel — six bands, top to bottom:

| Band   | Shows                                                     |
| ------ | --------------------------------------------------------- |
| status | Live battery %, charge state, and the clock from `measured_at` |
| health | Red `CRITICAL` band, blinking, `5xx 2.14% max 0.50%`      |
| hero   | `595` and `2XX / MIN`                                     |
| graph  | 30 buckets sweeping in, with `LAST 30 MIN`                |
| stats  | `P95 210ms` · `4XX 169 0.92%` · `5XX 389 2.14%`           |
| footer | `PAYOUTS` over `Overview`, fleet dots, `1/4`              |

**Controls**

- 👆 Tap the **right half** → next panel · **left half** → previous
- 👆 Tap the **footer** → next gateway · **status strip** → setup screen
- 🔘 **PLUS** key → next panel (works on non-touch boards too)

---

## 🔋 Where the live values come from

Everything except battery is from the embedded JSON. The battery is read off the hardware:

| Reading  | How                                                                    |
| -------- | ---------------------------------------------------------------------- |
| Voltage  | `analogReadMilliVolts(GPIO1)` × 3.0 — GPIO1 sits behind a 1/3 divider   |
| Enable   | GPIO2 driven HIGH powers that divider                                   |
| Charging | GPIO3, pull-up, **LOW = charging**                                      |
| Percent  | Waveshare's own voltage bands: <3.52 V → 1 %, then 20/40/60/80/100      |

⏰ **There is no clock.** No RTC is read and there is no NTP, so the time in the status band is `measured_at` from the payload, formatted `HH:MM`. Change `TZ_OFFSET_HOURS` at the top of the sketch to shift it.

## 📌 Pin map

Taken from Waveshare's own demos for this board, not guessed:

| Signal      | GPIO | Signal        | GPIO |
| ----------- | ---- | ------------- | ---- |
| LCD DC      | 45   | I²C SDA       | 42   |
| LCD CS      | 21   | I²C SCL       | 41   |
| LCD SCK     | 38   | Touch RST     | 47   |
| LCD MOSI    | 39   | Touch INT     | 48   |
| LCD RST     | 40   | Battery ADC   | 1    |
| Backlight   | 46   | Battery enable| 2    |
| BOOT key    | 0    | Charging sense| 3    |
| User keys   | 5, 4 |               |      |

> Both 5 and 4 are treated as "next panel", so PLUS works whichever one it is on your unit.

## 🔜 Going online later

`loadPayload()` parses a string. Swapping the mock for the real thing means fetching
`GET /v1/gateway_health` into that string and calling the same function — the model,
the layout and the invariant checks do not change. Contract → [`../../docs/api.md`](../../docs/api.md).
