# 🟨 The 640 × 172 bar build

Waveshare **ESP32-S3-Touch-LCD-3.49**, run landscape. Every metric is on screen at once — there is no cycle to walk.

🔧 Shared behaviour → **[device.md](../docs/device.md)** · 📡 Contract → **[api.md](../docs/api.md)** · 🖥️ Other build → **[ws_lcd_154](../ws_lcd_154/device.md)**
🎨 Live mockup → **[mockup.html](mockup.html)**

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
| IMU       | 6-axis — unused                                  |
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
- ❓ Square build asks *"what is this metric doing?"* and it steps through them for you. This one asks *"which metric is the problem?"* and the answer is one glance
- 📐 Five is the cap, so five is what the layout budgets: **128 px per column**, fixed, filled or not
- ✅ Twenty-one characters is tight but sufficient — `4XX 169 0.92%` is thirteen, a 16-char `name` lands
- 🚫 Sizing columns to whatever arrived would move every number on screen each time a row appeared

### 🎨 STATUS and ALERT share the top bar

A landscape board needs a strip for the gateway name, battery and clock anyway — the **STATUS** band. So that same strip carries **ALERT** as well, and it costs no rows to say so. The other two bands are the columns (**BODY**) and each column's own name line (**FOOTER**) → [device.md §3](../docs/device.md#3--the-four-bands).

| Option            | Cost                              |
| ----------------- | --------------------------------- |
| Full-width banner | 40 of 172 rows, to say one word   |
| Vertical rail     | 42 of 640 columns                 |
| ✅ **The top bar** | **nothing** — already chrome      |

- 🎯 26 rows were already spent, so **all 146 remaining rows go to content**
- 🌑 **At `info` the bar stays dark** — a level-coloured edge, the word, the message. The board is calm at rest
- 🚨 **`warning`, `critical` and a fetch fault tint it**, and then flash **the whole screen** — bar and every column alike — the level colour in near-black ink for 500 ms every 5 s → [device.md §3](../docs/device.md#-the-alert-is-the-whole-screen)
- 🏷️ Needs no glyph: the level word sits right on it, and the colour is the rest of the sentence
- 📍 Same pixels either way — what changes is loudness, never position
- 🔋 **Charging shows an explicit bolt** beside the battery percentage, not just a colour change

---

## 3. 🖥️ Screen

See it drawn, live, with real numbers → **[mockup.html](mockup.html)**.

| Region  | Rows | Bands           | Content                                                                                              |
| ------- | ---- | --------------- | ---------------------------------------------------------------------------------------------------- |
| top bar | 26   | ALERT + STATUS  | Level word + message left; `gateway`, battery, clock (`MM/DD hh:mm AM` of `measured_at`), Wi-Fi right |
| columns | 146  | BODY + FOOTER   | One per `metrics` row, same scale, left to right as sent; each names its own gateway and metric       |

Inside a column, top to bottom — 128 px wide, rows 26 → 172:

| y   | h  | Content                                                                 |
| --- | -- | ----------------------------------------------------------------------- |
| 34  | 8  | `name`, ≤ 18 chars (the summary column in accent)                        |
| 48  | 24 | `aggregates[0]` (the heading) per `bucket_unit` — the headline, size 3   |
| 76  | 8  | Unit label                                                               |
| 92  | 8  | the tile named `P95`                                                    |
| 104 | 8  | the tile named `4XX` + its share                                        |
| 116 | 8  | the tile named `5XX` + its share                                        |
| 130 | 30 | `buckets` (`total_count`), 110 × 30                                      |
| 164 | 8  | Row's own `gateway`, dim — the FOOTER. Span caption on the summary column |

- 🈳 **Only 4 of a row's up-to-5 tiles are drawn** — the heading, then whichever tiles are named `P95`, `4XX` and `5XX`. A tile named `AVG` may still be sent; this column just has no line for it → [AGENTS.md §10](../AGENTS.md#10--aggregate-tile-formatting)
- 🎯 **The share beside `4XX`/`5XX` is computed here, not sent** — `value / Σ(buckets)`, the same kind of client-side arithmetic as a tile's throughput. Its colour comes from the tile's own `level` (`critical` red, `warning` orange, `info` this build's own text colour) — the backend's call, never a threshold this board recomputes
- 📏 **Same scale in every column.** Five graphs at five y-scales is not a comparison, it is five pictures — one ceiling across the row
- 📋 Column order is `metrics` order, which is why [api.md §4](../docs/api.md#4--metrics) asks for most-important-first and stable names
- 🈳 Fewer than five rows leaves columns **empty**, never stretched. Four rows shown above, fifth slot unused
- 🔵 The summary column carries a slightly lighter ground and an accent name, so the aggregate is not mistaken for a part
- 🏷️ The span caption is drawn **once**, under the summary column — every column shares the clock, so five copies would be five lies waiting to disagree
- 🎨 Palette → [device.md §7](../docs/device.md#7--colours). This build leans on it harder: the level colour backs a 640 px strip, so the near-black ink rule is what makes one bar work on green, orange, red *and* grey

### 🔠 Text budget

| Size | Cell    | Across 640 px | In a 128 px column | Used for             |
| ---- | ------- | ------------- | ------------------ | -------------------- |
| 1    | 6 × 8   | **106**       | **21**             | column rows          |
| 2    | 12 × 16 | **53**        | 10                 | top bar, headings    |
| 3    | 18 × 24 | **35**        | 7                  | the hero numbers     |

- 📐 [api.md §5](../docs/api.md#5--limits) limits are set by the narrower build, so everything fits here with room over
- ✅ The whole alert line — level word, gateway, message — lands inside 53 characters at size 2

---

## 4. 👆 Input

The standard map → [device.md §2](../docs/device.md#2--input--the-standard-map). This board has **no PLUS key** — only PWR, BOOT and RESET — so what LEFT does on the square build falls to the glass here.

| Input                | Tap                  | Double-tap              | Hold 2 s                     |
| -------------------- | -------------------- | ----------------------- | ---------------------------- |
| **Glass** · a column | _future use_         | _future use_            | **Force refresh**            |
| **Glass** · top bar  | **Settings** on/off  | _future use_            | **Hotspot** — placeholder    |
| **PWR**              | _future use_         | _future use_            | **Power off** · from off: on |
| **BOOT**             | **Display on / off** | **Sound mute / unmute** | _future use_                 |

- 🖥️ **There is no metric cycle to walk** — every metric is already on screen, so a tap on a column has nothing to go to. It stays future use rather than inventing a gesture
- 🔄 The data still refetches on the shared interval, `max(30, metric_count × 5)` seconds → [device.md §1](../docs/device.md#1--the-cycle)
- ⏱️ **Every hold is 2 s**, exactly as on the square build
- 🚫 **No gateway cycling.** One endpoint, one response — whatever `gateway` names its rows carry → [api.md §2](../docs/api.md#2--the-object)

---

## 5. ➕ What this build adds

Cycle, input, bands, logging, speaker, configuration and palette are shared → **[device.md](../docs/device.md)**. Particular to this one:

- 🧠 Sprite is 640 × 172 × 16 bpp = **220 KB**, twice the square build's. `fillScreen()` in `loop()` is the one call that costs you the frame here
- ↔️ **No screen transition at all.** Everything is on screen already; the only motion is the count-up, the graph sweep and the alert flash
- 🔇 **The speaker is not fitted.** Nothing sounds until you add one, which makes [device.md §5](../docs/device.md#5--the-speaker) theoretical until you do
- 🔋 An 18650 is a much bigger cell, so a dark screen is less about survival and more about not lighting a desk at night — and it is a **BOOT tap** away
- 🪵 Serial at **115200**, same log lines as the square build → [device.md §4](../docs/device.md#4--logging)

---

## 6. ☑️ On-device checklist

**Serial Monitor at 115200** throughout → [device.md §4](../docs/device.md#4--logging).

- [ ] 640 × 172 landscape, columns, ALERT and STATUS share the top bar
- [ ] Every column shares one graph scale
- [ ] A two-row payload leaves three columns empty, no reflow
- [ ] Each column names its own row `gateway` under the graph
- [ ] Top bar stays readable on green, orange, red and grey
- [ ] `critical` flashes **the whole screen** — bar and every column — for 500 ms every 5 s
- [ ] Refetches on the shared interval, never faster than 30 s
- [ ] Glass held 2 s forces a poll
- [ ] A tap on a column does nothing, and Serial says `future use`
- [ ] Speaker fitted on MX1.25 and audible at arm's length
- [ ] `critical` plays the 500 ms alert sound with every flash; `warning` and a fetch fault stay silent
- [ ] BOOT double-tap silences every sound until it is repeated or the board restarts
- [ ] BOOT tap blanks the panel, and a critical still sounds in the dark
- [ ] RTC holds the clock across a reboot with no Wi-Fi
- [ ] Dropped Wi-Fi greys the bar and holds the numbers
