# 🟨 The 640 × 172 bar build

Waveshare **ESP32-S3-Touch-LCD-3.49**, run landscape. Every panel of a gateway is on screen at once — there is no cycle to walk.

🔧 Shared behaviour → **[device.md](../../docs/device.md)** · 📡 Contract → **[api.md](../../docs/api.md)** · 🖥️ Other build → **[lcd154.md](../../lcd154/docs/readme.md)**
🎨 Live mockup → **[../mockups/ui-lcd349.html](../mockups/device-ui.html)**

---

## 1. 🔩 Hardware

| Part      | Spec                                             |
| --------- | ------------------------------------------------ |
| MCU       | ESP32-S3R8, dual LX7, 240 MHz                    |
| Memory    | 16 MB flash, 8 MB PSRAM                          |
| Radio     | Wi-Fi 4, BLE 5.0, native USB                     |
| Display   | 3.49" IPS, **640 × 172** landscape, AXS15231B    |
| Touch     | Capacitive, whole panel                          |
| Audio out | **Header only** — ES8311 codec, MX1.25 plug      |
| Audio in  | Dual MEMS mics                                   |
| IMU       | 6-axis — shake to refresh                        |
| RTC       | **PCF85063** — time survives a reboot without NTP |
| Battery   | 18650 holder or LiPo on the header               |
| Case      | A (thick, takes the 18650) or B (thin)           |
| Size      | ~98.5 mm long — no 3 × 3 in footprint            |
| Keys      | PWR, BOOT, RESET                                 |

⚠️ **Things that bite**

- 🔇 **No speaker in the box.** Alerts are silent until one is on the MX1.25 header. Not optional
- 🔄 Panel is native portrait 172 × 640 — rotating is the whole design
- 📏 Desk-width, not pocket-size. Wall or shelf, not a bag

---

## 2. 🔭 Why landscape

- 📊 **106 characters across is enough for five columns of 21.** That is the threshold: under ~500 px of width you design a *sequence*, over it a *comparison*
- ❓ Square build asks *"what is this panel doing?"* and you step through. This one asks *"which panel is the problem?"* and the answer is one glance
- 📐 Five is the cap, so five is what the layout budgets: **128 px per column**, fixed, filled or not
- ✅ Twenty-one characters is tight but sufficient — `4XX 169 0.92%` is thirteen, a 16-char `name` lands
- 🚫 Sizing columns to whatever arrived would move every number on screen each time a row appeared

### 🎨 Health is the top bar

A landscape board needs a strip for the gateway name, battery and clock anyway. So that strip **is** `health` — and it costs no rows to say so.

| Option            | Cost                              |
| ----------------- | --------------------------------- |
| Full-width banner | 40 of 172 rows, to say one word   |
| Vertical rail     | 42 of 640 columns                 |
| ✅ **The top bar** | **nothing** — already chrome      |

- 🎯 26 rows were already spent, so **all 146 remaining rows go to content**
- 🌑 **At `info` the bar stays dark** — a level-coloured edge, the word, the message. The board is calm at rest
- 🚨 **`warning` and `critical` fill it** with the level colour in near-black ink, and blink it. A full-width strip is far harder to miss than a rail down one edge
- 🏷️ Needs no glyph: the level word sits right on it, and the colour is the rest of the sentence
- 📍 Same pixels either way — what changes is loudness, never position

---

## 3. 🖥️ Screen

```
0                                                              640
┌──────────────────────────────────────────────────────────────────┐ 0
│ ⚠ CRITICAL · Payouts · 5xx 2.14% max 0.50%    ▮82%  14:02   ᯤ    │  health bar  26
├────────────┬────────────┬────────────┬────────────┬──────────────┤ 26
│ Overview   │ Init Payo… │ Disburse … │ Acct Vali… │              │  columns    146
│ 595        │ 214        │ 176        │ 205        │              │  2xx / min
│ 2XX / MIN  │ 2XX / MIN  │ 2XX / MIN  │ 2XX / MIN  │   (unused)   │
│ P95  210ms │ P95  165ms │ P95  390ms │ P95   62ms │              │
│ 4XX 169    │ 4XX  61    │ 4XX  74    │ 4XX  34    │              │
│ 5XX 389    │ 5XX  44    │ 5XX 338    │ 5XX   7    │              │
│ ▁▂▃▅▆▇█▇▆▅ │ ▁▂▃▅▆▇█▇▆▅ │ ▁▂▃▅▆▇█▇▆▅ │ ▁▂▃▅▆▇█▇▆▅ │              │  buckets
└────────────┴────────────┴────────────┴────────────┴──────────────┘ 172
```

| Region     | Rows | Content                                                        |
| ---------- | ---- | -------------------------------------------------------------- |
| health bar | 26   | Level word + message left; `gateway`, battery, clock, Wi-Fi right |
| columns    | 146  | One per `metrics` row, same scale, left to right as sent        |

Inside a column, top to bottom — 128 px wide, rows 26 → 172:

| y   | h  | Content                                        |
| --- | -- | ---------------------------------------------- |
| 34  | 8  | `name`, ≤ 18 chars (the summary column in accent) |
| 48  | 24 | 2xx per `bucket_unit` — the headline, size 3   |
| 76  | 8  | Unit label                                      |
| 92  | 8  | `p95`                                           |
| 104 | 8  | `4xx` + rate                                    |
| 116 | 8  | `5xx` + rate                                    |
| 130 | 30 | `buckets`, 110 × 30                             |
| 164 | 8  | Span caption — **on the summary column only**   |

- 📏 **Same scale in every column.** Five graphs at five y-scales is not a comparison, it is five pictures — one ceiling across the row
- 📋 Column order is `metrics` order, which is why [api.md §4](../../docs/api.md#4--metrics) asks for most-important-first and stable names
- 🈳 Fewer than five rows leaves columns **empty**, never stretched. Four rows shown above, fifth slot unused
- 🔵 The summary column carries a slightly lighter ground and an accent name, so the aggregate is not mistaken for a part
- 🏷️ The span caption is drawn **once**, under the summary column — every column shares the clock, so five copies would be five lies waiting to disagree
- 🎨 Palette → [device.md §6](../../docs/device.md#6--colours). This build leans on it harder: the level colour backs a 640 px strip, so the near-black ink rule is what makes one bar work on green, amber, red *and* grey

### 🔠 Text budget

| Size | Cell    | Across 640 px | In a 128 px column | Used for             |
| ---- | ------- | ------------- | ------------------ | -------------------- |
| 1    | 6 × 8   | **106**       | **21**             | column rows          |
| 2    | 12 × 16 | **53**        | 10                 | health bar, headings |
| 3    | 18 × 24 | **35**        | 7                  | the hero numbers     |

- 📐 [api.md §5](../../docs/api.md#5--limits) limits are set by the narrower build, so everything fits here with room over
- ✅ The whole health line — level word, gateway, message — lands inside 53 characters at size 2

---

## 4. 👆 Input

| Input             | Tap                           | Hold                      |
| ----------------- | ----------------------------- | ------------------------- |
| **Gateway block** | Next gateway                  | ~0.9 s → **Wi-Fi portal** |
| **A column**      | Nothing — it is already shown | —                         |
| **Health bar**    | Setup screen                  | —                         |
| **Power**         | On                            | Off                       |
| **Shake**         | Force a poll                  | —                         |

- 🔄 The cycle is **gateways, not panels** — three gateways is three taps around the loop
- 🤝 Shake rules are shared → [device.md §3](../../docs/device.md#3--shake-to-refresh). They matter more here: this build gets nudged rather than picked up, so the three-jolt pattern does real work

---

## 5. ➕ What this build adds

Power, shake, speaker, portal and palette are shared → **[device.md](../../docs/device.md)**. Particular to this one:

- 🧠 Sprite is 640 × 172 × 16 bpp = **220 KB**, twice the square build's. `fillScreen()` in `loop()` is the one call that costs you the frame here
- ↔️ **No panel transition.** Everything is already on screen, so the only slide is sideways on gateway change — all five columns move together
- 🔇 **The speaker is not fitted.** Nothing sounds until you add one, which makes [device.md §4](../../docs/device.md#4--the-speaker) theoretical until you do
- 🔋 An 18650 is a much bigger cell, so screen-off on battery is less about survival and more about not lighting a desk at night

---

## 6. ☑️ On-device checklist

- [ ] 640 × 172 landscape, columns, `health` **is** the top bar
- [ ] Every column shares one graph scale
- [ ] A two-row gateway leaves three columns empty, no reflow
- [ ] Health bar stays readable on green, amber, red and grey
- [ ] Tapping the gateway block cycles gateways and wraps
- [ ] A deliberate shake forces a poll; a hard desk knock does not
- [ ] Two shakes inside 5 s produce one request
- [ ] Speaker fitted on MX1.25 and audible at arm's length
- [ ] `critical` rising edge → speaker once, not once per poll
- [ ] RTC holds the clock across a reboot with no Wi-Fi
- [ ] Unplug → panel black in under 1 s; tap → 60 s
- [ ] Dropped Wi-Fi greys the bar and holds the numbers
