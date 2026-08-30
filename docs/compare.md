# Compare

**Yes** = in the box, ready · **Partial** = chip/header only, you add a part · **No** = not there

| Feature | M5StickS3 | ESP32-C6 Waveshare 1.83 | ESP32-S3 Waveshare 3.49 |
| --- | --- | --- | --- |
| **Main board / SoC** | **ESP32-S3-PICO-1-N8R8** · dual-core LX7 · 240 MHz · 8 MB flash + 8 MB PSRAM | **ESP32-C6** · RISC-V · 160 MHz · 16 MB flash · no PSRAM | **ESP32-S3R8** · dual-core LX7 · 240 MHz · 16 MB flash + 8 MB PSRAM |
| **Released** | **23 Jan 2026** (M5Stack launch) | **~Nov 2025** (first shop listings; no press date) | **Sep 2025** (CNX 16 Sep 2025) |
| **Touch display** | **Partial** — 1.14" 135×240 rectangle, **no touch** | **Yes** — 1.83" 240×284 rectangle, **touch** | **Yes** — 3.49" 172×640 bar, **touch** |
| **Buttons** | **Yes** — A, B, power | **Yes** — PWR, BOOT | **Yes** — PWR, BOOT, RESET |
| **Wi-Fi** | **Yes** — 2.4 GHz | **Yes** — 2.4 GHz Wi-Fi 6 | **Yes** — 2.4 GHz |
| **Buzzer / audio** | **Yes** — speaker in the case | **Partial** — codec + amp + **speaker plug** | **Partial** — ES8311 + mics + **MX1.25 speaker plug**. No sealed-in speaker. |
| **Battery** | **Yes** — 250 mAh inside | **Partial** — charger + plug. Cell optional. | **Partial** — Case A = **18650 holder**. Case B = **LiPo header**. Cell optional on both. |
| **Inbuilt case** | **Yes** — molded stick | **Yes** — plastic kit | **Yes** — Case A (thick, 18650) or Case B (thin, polymer) |
| **Compact &lt; 3×3 in** | **Yes** — 48×24×15 mm | **Yes** — 1.83" + case | **No** — ~98.5 mm long (**3.9"**) |
| **Price** | $21.50 | $21.99 / $22.99 with cell | **$29.99** no cell · ~$31.99 with cell |

---

## ESP32-S3-Touch-LCD-3.49 — has / has not

[Official page](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?sku=32374) · [Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49)

| Need | Has? | Note |
| --- | --- | --- |
| Touch display | **Yes** | 3.49" IPS, 172 × 640, capacitive. Long rectangle. |
| Buttons | **Yes** | PWR, BOOT, RESET (back). |
| Wi-Fi | **Yes** | 2.4 GHz + BLE 5. ESP32-S3R8. |
| Speaker / alarm | **Partial** | Amp + **speaker header**. Plug an 8 Ω speaker. Dual mics are onboard. |
| Battery | **Partial** | Charger is on the board. Cell only if you pick a “with battery” SKU. |
| Case | **Yes** | Two shells. Same board family. |
| Under 3×3 in | **No** | Case A **98.5 × 34.8 × 31.8 mm**. Case B **98.5 × 34.8 × 22.8 mm**. |

---

## 18650 vs polymer — not the same

Same product page, **four SKUs**. Battery chemistry and case change. Capacity is **not** the same on paper.

| | Case A — 18650 | Case B — polymer (LiPo) |
| --- | --- | --- |
| SKU with cell | ESP32-S3-Touch-LCD-3.49 | ESP32-S3-Touch-LCD-3.49B |
| SKU no cell | ESP32-S3-Touch-LCD-3.49-EN (**32374**, $29.99) | ESP32-S3-Touch-LCD-3.49B-EN |
| How it mounts | Cylinder **holder** in a thicker case | Flat pouch on **MX1.25** in a thinner case |
| Official capacity | **Not listed** | Package name: **3.7 V 2000 mAh** (`3.7V-2000mA-MX1.25-2P-40mm`) |
| Typical range | A generic 18650 is often **2000–3500 mAh** — whatever cell they ship | **2000 mAh** (stated) |
| Same capacity? | **No guarantee.** Waveshare does not publish 18650 mAh. Polymer is 2000 mAh. An 18650 *can* be larger, but do not assume they match. |
| Size | Thicker (~32 mm) | Thinner (~23 mm) |
| Swap later | Any protected **18650** that fits the holder | Same **MX1.25 3.7 V** pouch, ~40 mm, ~2000 mAh |

**Pick Case A** if you want a cheap replaceable 18650 and do not mind bulk.  
**Pick Case B** if you want a known **2000 mAh** pack and a slimmer shell.

Without a “with battery” option, both only run on USB.

---

## Notes (other boards)

### M5StickS3

Closed stick. Speaker + 250 mAh already inside. No touch.

### ESP32-C6 Waveshare 1.83

Touch / Wi-Fi / buttons / case are real. Speaker and battery are plugs. Buy the **with battery** SKU.

---

## Buy

| Board | Link | Get |
| --- | --- | --- |
| **3.49 Case A + 18650** | [waveshare.com/esp32-s3-touch-lcd-3.49.htm](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm) → **Case A (with 18650 Lithium battery)** | Board + thick case + 18650 (mAh unknown) |
| **3.49 Case B + polymer** | same page → **Case B (with 3.7V Lithium polymer battery)** | Board + thin case + **2000 mAh** LiPo |
| 3.49 no battery (your SKU) | [sku=32374](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?sku=32374) | Case A, **no cell**, $29.99 |
| C6 1.83 + cell | [esp32-c6-touch-lcd-1.83](https://www.waveshare.com/product/esp32-c6-touch-lcd-1.83.htm) | Not `-EN` |
| StickS3 complete | [M5Stack shop](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit) | $21.50, everything inside |
