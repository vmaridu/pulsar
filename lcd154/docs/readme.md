# 🟦 The 240 × 240 square build

Waveshare **ESP32-S3-LCD-1.54**. One panel at a time, walked with touch or the PLUS key.

🔧 Shared behaviour → **[device.md](../../docs/device.md)** · 📡 Contract → **[api.md](../../docs/api.md)** · 🖥️ Other build → **[lcd349.md](../../lcd349/docs/readme.md)**
🎨 Live mockup → **[../mockups/ui-lcd154.html](../mockups/device-ui.html)**

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
| **Shake**        | Force a poll → [device.md §3](../../docs/device.md#3--shake-to-refresh) | — |

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
│ ▪82%              14:02            ᯤ   │  status  20
├────────────────────────────────────────┤ 20
│▌CRITICAL                               │  health  40
│▌5xx 2.14% max 0.50%                    │
├────────────────────────────────────────┤ 60
│ 595                                    │  hero    56
│ 2XX / MIN                              │
├────────────────────────────────────────┤ 116
│  ▁▂▃▅▆▇█▇▆▅▃▂▁                         │  graph   64
│                          LAST 30 MIN   │
├────────────────────────────────────────┤ 180
│ P95      4XX       5XX                 │  stats   30
│ 210ms    169 0.92% 389 2.14%           │
├────────────────────────────────────────┤ 210
│ PAYOUTS                          ● ○   │  footer  30
│ Overview                           1/4 │
└────────────────────────────────────────┘ 240
```

| Band   | y   | h  | Content                                                    |
| ------ | --- | -- | ---------------------------------------------------------- |
| status | 0   | 20 | Battery + charge, clock, Wi-Fi, poll hairline              |
| health | 20  | 40 | Level edge + level word + **one** line of `health.message` |
| hero   | 60  | 56 | 2xx per `bucket_unit` — the headline, size 4               |
| graph  | 116 | 64 | `buckets`, **228 × 42**, directly under the headline       |
| stats  | 180 | 30 | `p95` · `4xx` · `5xx` in three columns, label over value   |
| footer | 210 | 30 | `gateway` over panel `name`, position, fleet dots          |

- 🔗 **Graph sits directly under the hero, nothing between them.** They are one metric at two time scales, so a second label or a stats row in the gap would break the reading → [api.md §4](../../docs/api.md#4--metrics)
- 🏷️ `LAST 30 MIN` is computed from the clock, not sent
- 📊 Stats are three columns, not a run-on line — dim label over bright value, so the eye lands on numbers
- 🎨 `4xx` goes amber past 2 %; `5xx` amber past 0.5 %, red past 1 % — they raise their own hand without an alarm
- 🚫 `avg_latency_ms` is the figure that does not fit here. It is sent and not drawn

### 🚨 The health band is the anchor

Same y, same height, every frame. What changes is loudness, not position.

| `health.level`  | Band                                          | Blink        |
| --------------- | --------------------------------------------- | ------------ |
| 🟢 `info`        | Near-black, green edge, `INFO` + message      | none         |
| 🟡 `warning`     | Amber tint, amber edge, `WARNING` + message   | **~0.7 Hz**  |
| 🔴 `critical`    | Red tint, red edge, `CRITICAL` + message      | **~2.3 Hz**  |
| 🩶 fetch fault    | Grey tint, grey edge, numbers below held      | **~0.7 Hz**  |

- 📏 **A 3 px level-colour edge down the left, not a filled banner.** At rest the screen stays dark and calm; `warning` and `critical` tint the band and blink it, which is what carries across a room
- ✅ `health` is required in every response, so this band always has something to draw — there is no empty state to design
- 🔤 Levels spelled out, never abbreviated. `CRITICAL` is eight characters and there is room
- 🚫 No glyph. The word and the colour say it; an icon beside a spelled-out level is a third copy of the same fact
- 🩶 A fault adds `HELD` on the right — the numbers below are the last good payload
- 🚫 Fault words say *what*, not *where*: `OFFLINE` and `NO ACCESS`, not `NET` and `AUTH`
- 🙅 No acknowledged level → [api.md §3](../../docs/api.md#3--health)

### 📐 Layout rules

- 📍 **Identity lives at the bottom.** `gateway` small, dim and uppercase above the panel `name` at size 2 — the panel name changes as you tap, the gateway is only context. One dot per gateway, coloured by that gateway's level
- 📈 **The graph is 228 × 42** — 7.6 px per bucket. Enough to read a *shape*, not a value, which is why the numbers sit above it. Filled area, one flat tone, lit surface line, taking the level colour when in trouble
- 🎨 Palette → [device.md §6](../../docs/device.md#6--colours). Here the level colour fills the health band with near-black ink

### 🔠 Text budget

Fixed **6 × 8** cell, multiplied by `setTextSize(n)`. Never wraps, never scrolls.

| Size | Cell    | Chars across 240 px | Used for                  |
| ---- | ------- | ------------------- | ------------------------- |
| 1    | 6 × 8   | **40**              | stats line, captions      |
| 2    | 12 × 16 | **20**              | gateway name, level word  |
| 3    | 18 × 24 | **13**              | the hero number           |

This is where the [api.md §5](../../docs/api.md#5--limits) limits come from: 14 / 16 / **20** — one line at size 2.

---

## 4. ➕ What this build adds

Power, shake, speaker, portal and palette are shared → **[device.md](../../docs/device.md)**. Particular to this one:

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
