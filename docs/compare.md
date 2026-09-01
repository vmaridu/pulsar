# Hardware comparison

Everything about picking the board: what Pulsar needs, the boards that meet it, the chips underneath, and what to check on anything not listed here.

Current hardware and why → **[stick.md](stick.md)**

---

## 1. What Pulsar actually needs

The firmware uses five things. Everything else on a spec sheet is noise.

| Need                  | Why                                                      |
| --------------------- | -------------------------------------------------------- |
| 2.4 GHz Wi-Fi         | Polls every service over HTTPS                            |
| Colour TFT            | The seven-band dashboard                                  |
| **Speaker in the box**| Alarms have to be heard with the screen off               |
| At least one button   | The whole UI is one key                                   |
| **Battery in the box**| Plus USB-vs-battery detection to switch power modes       |

The two in bold are what disqualify most boards. Plenty of kits have a screen and a LiPo *header*; very few ship a sealed speaker and a cell inside a case.

**Yes** = in the box, ready · **Plug** = header/holder, you add a part · **No** = not there

---

## 2. Head to head

| Feature | **M5StickS3** | ESP32-S3 Waveshare 1.54 | ESP32-C6 Waveshare 1.54 | ESP32-C6 Waveshare 1.83 | ESP32-S3 Waveshare 3.49 |
| --- | --- | --- | --- | --- | --- |
| **SoC** | **ESP32-S3-PICO-1-N8R8** · dual LX7 · 240 MHz · 8 MB flash + **8 MB PSRAM** | **ESP32-S3R8** · dual LX7 · 240 MHz · 16 MB flash + **8 MB PSRAM** | **ESP32-C6** · RISC-V · 160 MHz · 16 MB flash · **no PSRAM** | **ESP32-C6** · RISC-V · 160 MHz · 16 MB flash · no PSRAM | **ESP32-S3R8** · dual LX7 · 240 MHz · 16 MB + 8 MB PSRAM |
| **Released** | 23 Jan 2026 | 2025 (SKU **33867**) | 2026 (SKU 34659) | ~Nov 2025 | Sep 2025 |
| **Display** | 1.14" 135×240, **no touch** | 1.54" 240×240 square IPS, **no touch** (ST7789) | 1.54" 240×240 square IPS, **touch** (CST816) | 1.83" 240×284, touch | 3.49" 172×640 bar, touch |
| **Buttons** | A, B, power | PLUS, BOOT, PWR | PLUS, BOOT, PWR | PWR, BOOT | PWR, BOOT, RESET |
| **Wi-Fi** | 2.4 GHz Wi-Fi 4 | 2.4 GHz Wi-Fi 4 + BLE 5 | 2.4 GHz **Wi-Fi 6** + Thread/Zigbee | 2.4 GHz Wi-Fi 6 | 2.4 GHz |
| **Speaker** | **Yes**, in the case | **Yes**, in the case + dual mics | **Yes**, in the case + dual mics | **Plug** — codec + amp, speaker plug | **Plug** — ES8311 + mics, MX1.25 plug |
| **Battery** | **Yes** — 250 mAh inside | **Plug** — optional **1000 mAh** fits the shell | **Plug** — optional **1000 mAh** fits the shell | **Plug** — cell optional | **Plug** — 18650 holder or LiPo header |
| **Case** | Moulded stick, magnetic back | Square moulded shell | Square moulded shell | Plastic kit | Case A (thick) or B (thin) |
| **Under 3 × 3 in** | **Yes** — 48×24×15 mm | Yes | Yes | Yes | **No** — ~98.5 mm long |
| **Price** | $21.50 | **$15.99** | $17.99 | $21.99–22.99 | $29.99–31.99 |
| **Firmware** | **M5Unified, runs today** | Port pins; **same S3 + PSRAM** | Port to C6, no M5 libs | Port to C6 | Port, S3 keeps the CPU |

### The verdict

**Stay on StickS3** for the current firmware, the stick shape, Grove and IR — and because it is the only board in this document that ships screen + speaker + cell + case at stick size with M5Unified already running.

**The S3 1.54 is the strongest Waveshare alternative.** Same square shell as the C6 1.54, cheaper (**$15.99**), dual-core S3 with 8 MB PSRAM, onboard speaker, dual mics, TF slot, IMU, and a 1000 mAh cell that fits the case. SKU **33867** is the no-touch + battery kit (Pulsar does not need touch). Firmware is still a pin-level port — not M5Unified — but you stay on Xtensa S3, so Arduino / PlatformIO and the HTTP/JSON stack do not change CPU.

**The C6 1.54 is the same idea on Wi-Fi 6.** Only pick it if you want Thread/Zigbee. RISC-V, no PSRAM, display/speaker/power all get rewritten.

**The 3.49 is the wrong shape.** 98.5 mm is not a stick, and the speaker is a plug.

### S3 1.54 SKUs — read carefully

| SKU       | Name                         | Touch | 1000 mAh cell |
| --------- | ---------------------------- | ----- | ------------- |
| **33867** | ESP32-S3-LCD-1.54            | No    | Yes           |
| 33866     | ESP32-S3-LCD-1.54-EN         | No    | **No**        |
| 33869     | ESP32-S3-Touch-LCD-1.54      | Yes   | Yes           |
| 33868     | ESP32-S3-Touch-LCD-1.54-EN   | Yes   | **No**        |

`-EN` means no cell — USB only. Confirm **Included** under "3.7V MX1.25 Lithium Batt" at checkout. Wiki: [ESP32-S3-Touch-LCD-1.54](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.54).

### C6 1.54 SKUs — read carefully

| SKU       | Name                       | Touch | 1000 mAh cell |
| --------- | -------------------------- | ----- | ------------- |
| **34659** | ESP32-C6-Touch-LCD-1.54    | Yes   | Yes           |
| 34661     | ESP32-C6-Touch-LCD-1.54-EN | Yes   | **No**        |
| 34658     | ESP32-C6-LCD-1.54          | No    | Yes           |
| 34660     | ESP32-C6-LCD-1.54-EN       | No    | **No**        |

`-EN` means no cell — USB only. Confirm **Included** under "3.7V MX1.25 Lithium Batt" at checkout.

### 3.49 battery SKUs — not the same cell

| | Case A — 18650 | Case B — polymer |
| --- | --- | --- |
| SKU with cell | ESP32-S3-Touch-LCD-3.49 | ESP32-S3-Touch-LCD-3.49B |
| SKU without | -EN (**32374**, $29.99) | 3.49B-EN |
| Mounting | Cylinder holder, thicker case (~32 mm) | Flat pouch on MX1.25, thinner (~23 mm) |
| Capacity | **Not published** — a generic 18650 is 2000–3500 mAh | **2000 mAh** (stated) |
| Swap later | Any protected 18650 that fits | MX1.25 3.7 V pouch, ~40 mm |

Waveshare does not publish the 18650's capacity. Do not assume the two cases ship equivalent packs.

---

## 3. Pulsar fit — the wider field

| Board                        | Wi-Fi         | Colour TFT       | Speaker  | Button | Battery      | Case | Fit                                 |
| ---------------------------- | ------------- | ---------------- | -------- | ------ | ------------ | ---- | ----------------------------------- |
| **M5StickS3**                | Yes           | Yes, no touch    | **Yes**  | A + B  | **Yes**      | Yes  | **Current hardware**                |
| M5CoreS3                     | Yes           | Yes, touch       | **Yes**  | Touch  | **Yes**      | Yes  | Works — brick, not a stick          |
| M5Cardputer                  | Yes           | Yes              | **Yes**  | Keyboard | Yes        | Yes  | Works — card with a keyboard        |
| M5StickC Plus2               | Yes           | Yes              | **Yes**  | A + B  | 200 mAh      | Yes  | Older stick, no native USB OTG      |
| M5Dial                       | Yes           | Round, touch     | No       | Encoder| Yes          | Yes  | Rotary UI, no speaker               |
| Waveshare S3 1.54            | Yes           | Yes, no touch    | **Yes**  | 3      | Optional SKU | Yes  | Best alternative — stay on S3       |
| Waveshare C6 1.54            | Wi-Fi 6       | Yes, touch       | **Yes**  | 3      | Optional SKU | Yes  | Same shell, port to C6              |
| Waveshare C6 1.83            | Wi-Fi 6       | Yes, touch       | Plug     | 2      | Optional SKU | Yes  | Port + add a speaker                |
| Waveshare S3 3.49            | Yes           | Bar, touch       | Plug     | 3      | Optional SKU | Yes  | Too long for a stick                |
| LilyGO T-Display S3          | Yes           | Yes, no touch    | No       | 2      | Header       | No   | Add amp + cell + shell              |
| Adafruit Reverse TFT Feather | Yes           | 1.14" on the back| No       | 3      | Header       | No   | Same                                |
| ESP32-S3-DevKitC-1           | Yes           | No               | No       | BOOT   | No           | No   | Prototype MCU only                  |
| ESP32-C6-DevKitC-1           | Wi-Fi 6       | No               | No       | BOOT   | No           | No   | Radio evaluation only               |

---

## 4. Choosing the chip

Espressif ships **chips**; M5Stack, Waveshare, LilyGO and others wrap them into products. The chip decides radio and CPU; the product PCB decides whether Pulsar is flash-and-go or a wiring project.

### Naming

| Letter | Meaning | | Suffix | Meaning |
| --- | --- | --- | --- | --- |
| (none) | Original ESP32 — Xtensa, Wi-Fi 4, BT Classic | | `WROOM`/`MINI` | Module with flash on board |
| **S** | Feature-rich — displays, USB OTG, camera, audio | | `WROVER` | Legacy module with PSRAM |
| **C** | Cost / RISC-V, Wi-Fi + BLE | | `N8`/`N16` | 8 / 16 MB flash |
| **H** | BLE + Thread/Zigbee, **no Wi-Fi** | | `R2`/`R8` | 2 / 8 MB PSRAM |
| **P** | High-performance HMI/video, **no radio** | | `U` | U.FL antenna connector |

`ESP32-S3-WROOM-1-N16R8` = S3 chip, WROOM module, 16 MB flash, 8 MB PSRAM.

### The SoCs

| SoC | CPU | Cores | Clock | SRAM | Wi-Fi | Bluetooth | 802.15.4 | USB | PSRAM | Best for |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **ESP32** | Xtensa LX6 | 2 + ULP | 240 MHz | 520 KB | Wi-Fi 4 | Classic + BLE 4.2 | No | External | Optional | Legacy, BT Classic, Ethernet |
| **ESP32-S2** | Xtensa LX7 | 1 | 240 MHz | 320 KB | Wi-Fi 4 | **None** | No | OTG FS | Optional | USB HID, no BLE |
| **ESP32-S3** | Xtensa LX7 | 2 + ULP | 240 MHz | 512 KB | Wi-Fi 4 | BLE 5.0 | No | OTG FS | Optional | **Pulsar / displays / audio** |
| **ESP32-S31** | RISC-V | 2 + LP | 320 MHz | 512 KB | Wi-Fi 6 | Classic + BLE 5.4 | Yes | OTG HS | Yes (DDR) | Hubs, LE Audio, GbE |
| **ESP32-C2** | RISC-V | 1 | 120 MHz | 272 KB | Wi-Fi 4 | BLE 5.0 | No | UART | No | Cheapest ESP8266 replacement |
| **ESP32-C3** | RISC-V | 1 | 160 MHz | 400 KB | Wi-Fi 4 | BLE 5.0 | No | Serial/JTAG | No | Budget IoT |
| **ESP32-C5** | RISC-V | 1 + LP | 240 MHz | 384 KB | **2.4 + 5 GHz** Wi-Fi 6 | BLE 5 | Yes | Serial/JTAG | Optional | Only ESP32 with 5 GHz |
| **ESP32-C6** | RISC-V | 1 + LP | 160 MHz | 512 KB | Wi-Fi 6 | BLE 5 | Yes | Serial/JTAG | No | Matter / Thread + Wi-Fi 6 |
| **ESP32-C61** | RISC-V | 1 | 160 MHz | 320 KB | Wi-Fi 6 | BLE 5.4 | **No** | Serial/JTAG | Optional | Cheap Wi-Fi 6, no mesh |
| **ESP32-H2 / H21** | RISC-V | 1 | 96 MHz | 320 KB | **None** | BLE 5.3 | Yes | Serial/JTAG | No | Thread / Zigbee endpoint |
| **ESP32-H4** | RISC-V | 2 | 96 MHz | 384 KB | **None** | BLE 5.4 LE Audio | Yes | OTG FS | Optional | BLE Audio + mesh |
| **ESP32-P4** | RISC-V | 2 + LP | 400 MHz | 768 KB | **None** (needs C5/C6) | **None** | No | OTG HS | Yes, often 32 MB | MIPI display/camera, H.264 |
| **ESP32-E22** | RISC-V | 2 | 500 MHz | 1 MB | Tri-band Wi-Fi 6E | Classic + BLE 5.4 | No | PCIe/SDIO host | — | Radio co-processor only |

ESP8266 is the older Wi-Fi-only predecessor, not an ESP32. E22 is a co-processor, not a board MCU.

### One-line picker

| Need | Chip |
| --- | --- |
| Colour TFT + speaker + USB CDC, Arduino/PlatformIO today | **ESP32-S3** |
| 5 GHz Wi-Fi | **ESP32-C5** |
| Matter / Thread / Zigbee **and** Wi-Fi | **ESP32-C6** (C5 if you also need 5 GHz) |
| Thread / Zigbee only | **ESP32-H2 / H21 / H4** |
| Bluetooth Classic | **ESP32** or **ESP32-S31** |
| Big MIPI panel / camera / H.264 | **ESP32-P4** + C5/C6 companion |
| Lowest-cost Wi-Fi node | **ESP32-C2** or **C3** |

Pulsar needs Wi-Fi, a colour TFT, a speaker, a battery and buttons. That is **S3 today**. C6 handles the radio and a small LCD with less CPU headroom for UI. P4 is the wrong class and needs a second chip for Wi-Fi.

### Toolchain maturity

| SoC | ESP-IDF | Arduino / PlatformIO | Notes |
| --- | --- | --- | --- |
| ESP32, S2, S3, C3 | Mature | Mature | StickS3 uses Arduino + `espressif32` + M5Unified |
| C6, H2 | Mature | Good, growing | Fine if you leave Arduino |
| C5, C61, P4 | Current IDF | Uneven | P4 is mostly ESP-IDF |
| S31, H4, H21, E22 | New (IDF 6.x) | Early | Do not pick these in 2026 unless you want to fight the toolchain |

---

## 5. Official Espressif DevKits

Bare boards: headers, USB, BOOT/RESET, sometimes an RGB LED. **No** display, speaker or battery unless the name says so. Flash/PSRAM is the common SKU; several variants exist of each PCB.

### Entry-level

| Board | SoC | Flash | PSRAM | Radio |
| --- | --- | --- | --- | --- |
| ESP32-DevKitC / DevKitM-1 | ESP32 | 4 MB | — | Wi-Fi 4 + BT Classic |
| ESP32-PICO-KIT-1 / PICO-DevKitM-2 | ESP32-PICO | 4–8 MB | 0–2 MB | Wi-Fi 4 + BT Classic |
| ESP32-S2-DevKitC-1 | S2 | 8 MB | 2 MB | Wi-Fi only, no BT |
| **ESP32-S3-DevKitC-1** | S3-WROOM-1 | 8/16/32 MB | 0/8/16 MB | Wi-Fi 4 + BLE 5 |
| ESP32-S3-DevKitM-1 | S3-MINI-1 | 8 MB | — | Wi-Fi 4 + BLE 5 |
| ESP32-C3-DevKitM-1 | C3-MINI-1 | 4 MB | — | Wi-Fi 4 + BLE 5 |
| ESP8684-DevKitC-02 / M-1 | C2 | 4 MB | — | Wi-Fi 4 + BLE 5 |
| ESP32-C5-DevKitC-1 | C5 | 8 MB | 8 MB | Dual-band Wi-Fi 6 + Thread |
| ESP32-C6-DevKitC-1 / M-1 | C6 | 4–8 MB | — | Wi-Fi 6 + Thread |
| ESP32-C61-DevKitC-1 | C61 | 8 MB | 2 MB | Wi-Fi 6, no Thread |
| ESP32-H2 / H21-DevKitM-1 | H2 / H21 | 4 MB | — | BLE + Thread, no Wi-Fi |
| ESP32-H4-DevKitC-1 | H4 | 8 MB | 2 MB | BLE 5.4 + Thread, no Wi-Fi |

### With a display or audio

| Board | SoC | Display | Audio | Camera | Battery |
| --- | --- | --- | --- | --- | --- |
| ESP32-S3-BOX-3 | S3 | 2.4" 320×240 touch | Dual mic + speaker | No | No |
| ESP-VoCat | S3 | 1.85" 360×360 round touch | Dual mic + speaker | Expand | Lithium pack |
| ESP32-S3-EYE | S3 | 1.3" 240×240 | Mic | 2 MP DVP | LiPo header |
| ESP32-S3-Korvo-2 | S3 | Optional LCD | Dual mic + speaker | DVP | — |
| ESP32-S3-LCD-EV-Board | S3 | 3.95" 480×480 RGB touch | Dual mic + speaker | — | — |
| ESP32-C3-LCDkit | C3 | 1.28" 240×240 round | Speaker | — | — |
| ESP-SensairShuttle | C5 | 1.93" 240×284 touch | Mic + speaker | — | LiPo |
| ESP32-P4-Function-EV-Board | P4 + C6 | 7" 1920×1080 MIPI touch | Mic + speaker | MIPI CSI | — |
| ESP32-P4-EYE | P4 | 1.54" 240×240 | Mic | 2 MP MIPI | LiPo |
| ESP-Mosaico | S31 | 2.16" 480×480 AMOLED touch | Mic + speaker + haptic | Expand | 65 mAh |

### Gateway / other

| Board | SoC | Extra |
| --- | --- | --- |
| ESP32-Ethernet-Kit | ESP32-WROVER | 10/100 PHY, optional PoE |
| ESP Thread Border Router / Zigbee Gateway | S3 + H2 RCP | Wi-Fi ↔ Thread / Zigbee |
| ESP32-C3-DevKit-RUST-2 | C3 | Temp/humidity, IMU, Li-ion charger |
| ESP-Prog-2 | S3 (on tool) | JTAG/UART programmer, not a product MCU |

---

## 6. Third-party complete boards

### Compact sticks and cores

| Board | SoC | Display | Touch | Speaker | Buttons | Battery / case | Size |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **M5StickS3** | S3-PICO N8R8 | 1.14" 135×240 | No | **Yes** | A, B, PWR | **250 mAh** + stick case | 48×24×15 mm |
| M5StickC Plus2 | ESP32-PICO-V3 N8 | 1.14" 135×240 | No | **Yes** | A, B, PWR | 200 mAh + case | ~48×24×13 mm |
| M5AtomS3 / S3R | S3 | 0.85" 128×128 | No | No | 1 | No | 24×24×13 mm |
| M5Dial | S3 | 1.28" 240×240 round | Yes | No | Encoder | 300 mAh + case | ~36 mm round |
| M5Cardputer | S3 | 1.14" 240×135 | No | **Yes** | Keyboard | 120 mAh + case | card |
| M5CoreS3 | S3 | 2.0" 320×240 | Yes | **Yes** | Touch | 500 mAh + brick | 54×54×16 mm |
| M5NanoC6 | C6 | No | — | No | 1 | No | 24×18 mm |
| M5StampS3 | S3 | No | — | No | 1 | No | 26×18 mm |

### Waveshare LCD kits

| Board | SoC | Display | Touch | Speaker | Battery | Under 3×3 in |
| --- | --- | --- | --- | --- | --- | --- |
| ESP32-S3-LCD-1.54 | S3R8 | 1.54" 240×240 | No | **Yes** | Optional 1000 mAh | Yes |
| ESP32-S3-Touch-LCD-1.54 | S3R8 | 1.54" 240×240 | Yes | **Yes** | Optional 1000 mAh | Yes |
| ESP32-C6-Touch-LCD-1.54 | C6 | 1.54" 240×240 | Yes | **Yes** | Optional 1000 mAh | Yes |
| ESP32-C6-Touch-LCD-1.83 | C6 | 1.83" 240×284 | Yes | Plug | Optional | Yes |
| ESP32-S3-Touch-LCD-3.49 | S3R8 | 3.49" 172×640 bar | Yes | Plug | 18650 or 2000 mAh | **No** |
| ESP32-S3-Touch-LCD-2 | S3 | 2.0" 320×240 | Yes | No | Header | Yes |
| ESP32-S3-Touch-LCD-1.28 | S3 | 1.28" 240×240 round | Yes | No | Header | Yes |
| ESP32-S3-Touch-AMOLED-1.8 | S3 | 1.8" AMOLED | Yes | No | Header | Yes |

### LilyGO

| Board | SoC | Display | Extra | Battery |
| --- | --- | --- | --- | --- |
| T-Display / T-Display S3 | ESP32 / S3 N16R8 | 1.14" 135×240 / 1.9" 170×320 | 2 buttons | Header |
| T-Display S3 AMOLED | S3 | 1.9" / 2.41" AMOLED | Touch on some | Header |
| T-Watch S3 | S3 | Round watch TFT | IMU, RTC, mic, speaker | Watch cell |
| T-Embed / CC1101 | S3 | 1.9" + encoder | Optional sub-GHz | LiPo |
| T-Beam / Supreme | ESP32 / S3 | OLED on some | LoRa, GPS | 18650 holder |

### Small form factors

| Board | SoC | Form | Flash / PSRAM | Charger | Extra |
| --- | --- | --- | --- | --- | --- |
| Arduino Nano ESP32 | S3 | Nano | 16 / 8 MB | No | Arduino pinout |
| Adafruit ESP32-S3 Feather | S3 | Feather | 8 / 2 MB | **Yes** | STEMMA QT |
| Adafruit Reverse TFT Feather | S3 | Feather | 4 / 2 MB | **Yes** | 1.14" 240×135 on the back, 3 buttons |
| Adafruit QT Py S3 / C3 | S3 / C3 | QT Py | 4–8 MB | No | STEMMA QT |
| SparkFun Thing Plus S3 / C6 | S3 / C6 | Feather | 4 MB | **Yes** | Qwiic, microSD |
| Seeed XIAO S3 / C3 / C6 / C5 | various | 21×17.5 mm | 4–8 MB | No | Tiny |
| DFRobot FireBeetle 2 C5 | C5 | FireBeetle | 4 MB | **Yes** | GDI display header |

T-Display S3 and the Reverse TFT Feather have a screen and a LiPo connector but **no sealed speaker**. StickS3 remains the only row that ships screen + speaker + cell + case at stick size.

---

## 7. Buying

| Board | Where | What you get |
| --- | --- | --- |
| **StickS3, complete** | [shop.m5stack.com](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit) | $21.50, everything inside |
| **S3 1.54 no-touch + cell** | [waveshare.com sku=33867](https://www.waveshare.com/esp32-s3-lcd-1.54.htm?sku=33867) | **$15.99** — confirm battery **Included** |
| S3 1.54, no cell | same page, SKU 33866 (`-EN`) | USB only |
| S3 1.54 touch + cell | same page, SKU 33869 | Touch (CST816) + 1000 mAh |
| **C6 1.54 touch + cell** | [waveshare.com sku=34659](https://www.waveshare.com/esp32-c6-lcd-1.54.htm?sku=34659) | $17.99 — confirm battery **Included** |
| C6 1.54, no cell | same page, SKU 34661 (`-EN`) | USB only |
| C6 1.83 + cell | [esp32-c6-touch-lcd-1.83](https://www.waveshare.com/product/esp32-c6-touch-lcd-1.83.htm) | Not the `-EN` SKU |
| 3.49 Case A + 18650 | [waveshare.com](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm) → Case A | Thick case, 18650 of unpublished capacity |
| 3.49 Case B + polymer | same page → Case B | Thin case, **2000 mAh** LiPo |
| 3.49 no battery | [sku=32374](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?sku=32374) | $29.99, USB only |

---

## 8. Evaluating a board that is not listed

PlatformIO's `espressif32` package alone carries **200+ board IDs**, and clones appear weekly. There is no complete list. Work it out in three steps:

1. Read the **module marking** — `ESP32-S3-WROOM-1-N16R8`, `ESP32-C6-MINI-1`, …
2. Match it against the **SoC table** in §4 for radio and CPU.
3. Check whether **display, speaker, battery and case** are on the PCB or only a header.

Step 3 is where most boards fail. A LiPo header is not a battery, and an amp with a speaker plug is not a speaker.

Sources: [Espressif SoCs](https://www.espressif.com/en/products/socs) · [Espressif DevKits](https://www.espressif.com/en/products/devkits) · [ESP-Techpedia board selection](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/get-started/board-selection.html) · [M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3)
