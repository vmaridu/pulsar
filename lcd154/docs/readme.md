# 🟦 The 240 × 240 square build

Waveshare **ESP32-S3-LCD-1.54**. One panel at a time, walked with touch or the PLUS key.

🔧 Shared behaviour → **[device.md](device.md)** · 📡 Contract → **[api.md](api.md)** · 🖥️ Other build → **[lcd349.md](lcd349.md)**
🎨 Live mockup → **[../mockups/ui-lcd154.html](../mockups/ui-lcd154.html)**

---

## 1. 🔩 Hardware

Waveshare square kit, SKU **33867** with cell.

| Part       | Spec                                          |
| ---------- | --------------------------------------------- |
| MCU        | ESP32-S3R8, dual LX7, 240 MHz                 |
| Memory     | 16 MB flash, 8 MB PSRAM                       |
| Radio      | Wi-Fi 4, BLE 5.0, native USB                  |
| Display    | 1.54" IPS, **240 × 240**, ST7789              |
| Touch      | CST816 — touch SKUs only                      |
| Audio      | Speaker in the case, dual MEMS mics           |
| IMU        | 6-axis — shake to refresh                     |
| RTC        | Keeps time across a reboot                    |
| Storage    | TF (microSD)                                  |
| Battery    | Optional **1000 mAh** LiPo, fits the shell    |
| Keys       | PLUS, BOOT, PWR                               |

⚠️ **Things that bite**

- 🔋 Battery is a SKU — `-EN` boards are USB only. Confirm the cell is *Included*
- 👆 Touch is a SKU — on 33867 the PLUS key walks panels on its own
- 🐌 ST7789 on SPI: a `fillRect` is cheap, a full-screen redraw every poll is not

---

## 2. 👆 Input

| Input            | Tap                                | Hold                      |
| ---------------- | ---------------------------------- | ------------------------- |
| **Right half**   | Next panel (wakes the panel first) | —                         |
| **Left half**    | Previous panel                     | —                         |
| **Footer**       | Next gateway                       | —                         |
| **PLUS**         | Same as a right tap                | ~0.9 s → **Wi-Fi portal** |
| **Status strip** | Setup screen                       | —                         |
| **Power**        | On                                 | Off                       |
| **Shake**        | Force a poll → [device.md §3](device.md#3--shake-to-refresh) | — |

- ⛔ Never bind UI actions to the power key — the PMIC owns it
- 🔌 PLUS is the light-sleep wake pin (active low) — wire `esp_sleep_enable_ext0_wakeup` to it

### 🔄 One cycle

```
Payouts · Overview → Init Payout → Disburse Payout → Acct Validations
  → Fraud VAS · Overview → Eval → back to the start
```

- 📋 Every `metrics` entry in the order sent, for each gateway
- 📐 3 gateways × 5 panels = **15 screens**, then it wraps
- ↔️ Crossing gateways slides sideways · ↕️ moving within one slides up
- 🧭 No menu, no back button — keep tapping and you return where you started

---

## 3. 🖥️ Screen

**Six** fixed bands. Nothing moves between them; only the band that changed is redrawn.

```
0                                      240
┌────────────────────────────────────────┐ 0
│ ▮82%              14:02            ᯤ   │  status    22
├────────────────────────────────────────┤ 22
│ ⚠ CRITICAL                             │  HEALTH    44
│ 5xx 2.14% max 0.50%                    │  one line
├────────────────────────────────────────┤ 66
│ 595                                    │  hero      64
│ 2XX / MIN                              │
├────────────────────────────────────────┤ 130
│ P95 210ms   4XX 169 0.92%   5XX 389    │  stats     22
├────────────────────────────────────────┤ 152
│    ▁▂▃▅▆▇█▇▆▅▃▂▁          LAST 30 MIN  │  graph     58
├────────────────────────────────────────┤ 210
│ Payouts                           ● ○  │  footer    30
│ Overview                           1/5 │
└────────────────────────────────────────┘ 240
```

| Band   | y   | h  | Content                                                    |
| ------ | --- | -- | ---------------------------------------------------------- |
| status | 0   | 22 | Battery + charge, clock, Wi-Fi                             |
| health | 22  | 44 | Glyph + level word + **one** line of `health.message`      |
| hero   | 66  | 64 | 2xx per minute — the headline                              |
| stats  | 130 | 22 | `p95` · `4xx` + rate · `5xx` + rate on **one** 40-char line |
| graph  | 152 | 58 | `buckets`, **228 × 42** — the same metric over time         |
| footer | 210 | 30 | `gateway` bold over panel `name`, position, fleet dots     |

- 🔗 The hero and the graph are **one metric at two time scales**; both come from the same row → [api.md §4](api.md#4--metrics)
- 🏷️ `LAST 30 MIN` is computed from the clock, not sent
- 📏 Forty characters is what buys the one-line stats block instead of four stacked rows
- 🚫 `avg_latency_ms` is the figure that does not fit here. It is sent and not drawn

### 🚨 The health band is the anchor

Same y, same height, every frame. What changes is loudness, not position.

| `health.level`  | Band                                        | Blink        |
| --------------- | ------------------------------------------- | ------------ |
| 🟢 `info`        | Green, tick glyph, `INFO` + message         | none         |
| 🟡 `warning`     | Yellow, warning triangle, `WARNING` + msg   | **~0.7 Hz**  |
| 🔴 `critical`    | Red, warning triangle, `CRITICAL` + msg     | **~2.3 Hz**  |
| 🩶 fetch fault    | Grey, no-link glyph, numbers below held     | **~0.7 Hz**  |

- ✅ `health` is required in every response, so this band always has something to draw — there is no empty state to design
- 🔤 Levels spelled out, never abbreviated. `CRITICAL` is eight characters and there is room
- 🚫 Fault words say *what*, not *where*: `OFFLINE` and `NO ACCESS`, not `NET` and `AUTH`
- ✏️ Glyphs drawn from primitives, not a font — ~6 draw calls each, near-black on the banner. At 12 px a flat shape reads where colour art turns to mud
- 🙅 No acknowledged level → [api.md §3](api.md#3--health)

### 📐 Layout rules

- 📍 **Identity lives at the bottom.** `gateway` bold above its panel `name`, same size, one dot per gateway coloured by that gateway's level — the top two thirds belong to the health band, the number and the graph
- 📈 **The graph is 228 × 42** — 7.6 px per bucket. Enough to read a *shape*, not a value, which is why the numbers sit above it. Filled area, one flat tone, lit surface line, taking the level colour when in trouble
- 🎨 Palette → [device.md §6](device.md#6--colours). Here the level colour fills the health band with near-black ink

### 🔠 Text budget

Fixed **6 × 8** cell, multiplied by `setTextSize(n)`. Never wraps, never scrolls.

| Size | Cell    | Chars across 240 px | Used for                  |
| ---- | ------- | ------------------- | ------------------------- |
| 1    | 6 × 8   | **40**              | stats line, captions      |
| 2    | 12 × 16 | **20**              | gateway name, level word  |
| 3    | 18 × 24 | **13**              | the hero number           |

This is where the [api.md §5](api.md#5--limits) limits come from: 14 / 16 / **20** — one line at size 2.

---

## 4. ➕ What this build adds

Power, shake, speaker, portal and palette are shared → **[device.md](device.md)**. Particular to this one:

- 🧠 Sprite is 240 × 240 × 16 bpp = **113 KB**, PSRAM, one push per frame
- ↔️↕️ **Two slide directions** — within a gateway slides up (~240 ms), crossing gateways slides sideways (~300 ms). The bar build has no panel slide at all

---

## 5. ☑️ On-device checklist

- [ ] 240 × 240, black background, six bands
- [ ] Status band never flickers on poll
- [ ] Next-panel walks every screen and wraps; footer tap changes gateway
- [ ] A deliberate shake forces a poll and the hairline lights
- [ ] A hard desk knock does **not** trigger a poll
- [ ] Two shakes inside 5 s produce one request
- [ ] PLUS held raises the AP; status tap opens setup
- [ ] Health band occupies the same pixels at every level
- [ ] `critical` rising edge → speaker once, not once per poll
- [ ] Unplug → panel black in under 1 s
- [ ] Wake on battery → 60 s, then black
- [ ] A one-row gateway shows `1/1` and is stepped over correctly
- [ ] A sixth `metrics` entry is ignored rather than crashing
- [ ] Dropped Wi-Fi shows `OFFLINE` and holds the numbers
- [ ] Quiet speaker on battery with no brownout reboot
