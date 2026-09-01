# The devices

Everything about the hardware: what each board draws, and how you drive it.

The backend contract is separate and knows nothing about any of this → **[api.md](api.md)**
Firmware → **[code.md](code.md)** · Board choice → **[compare.md](compare.md)**

## Three screens, one contract

Every board polls the same [api.md](api.md) endpoint and shows the same five numbers. What changes is how much fits at once — and that changes the interaction more than the layout.

| Board | Screen | Input | Live mockup |
| --- | --- | --- | --- |
| **M5StickS3** | 1.14" **135 × 240** portrait, ST7789P3, no touch | One blue key + shake | **[ui-sticks3.html](ui-sticks3.html)** |
| **ESP32-S3-Touch-LCD-3.49** | 3.49" **640 × 172 landscape**, AXS15231B, capacitive | Touch + shake | **[ui-lcd349.html](ui-lcd349.html)** |
| **ESP32-S3-LCD-1.54** | 1.54" **240 × 240** square, ST7789, CST816 | Touch halves + PLUS key | **[ui-lcd154.html](ui-lcd154.html)** |

### What each size increase actually buys

| Board | Diagonal | Pixels | Count | vs stick | Chars per line | Panels visible |
| --- | --- | --- | --- | --- | --- | --- |
| M5StickS3 | 1.14" | 135 × 240 | 32,400 | 1.00× | 22 | **1** |
| S3-LCD-1.54 | 1.54" | 240 × 240 | 57,600 | **1.78×** | 40 | **1** |
| S3-Touch-LCD-3.49 | 3.49" | 640 × 172 | 110,080 | **3.40×** | 106 | **4** |

The pixel count is not the interesting column. **Panels visible** is:

- **135 × 240 → 240 × 240** is 1.78× the pixels and buys *comfort*, not capability. The same one panel, but four numbers land on one 40-character line instead of stacking into four rows, the graph doubles in height, and touch splits the square into prev/next halves. Still one panel at a time, still a cycle.
- **240 × 240 → 640 × 172** is only 1.9× again, but it crosses a threshold: **106 characters across a line is enough for four columns**, so every panel fits side by side and the cycle disappears entirely. That is a different product, not a bigger screen.

The lesson worth carrying: below ~500 px of width you are designing a *sequence*, above it you are designing a *comparison*. Doubling the pixels inside one of those regimes only makes things roomier; the jump between them changes what the device is for.

Height matters less than you would expect. All three fit 21–30 text lines, and none of them can use that many — the constraint is always width.

### The 3.49 in landscape

The panel is native portrait at 172 × 640, but rotating it is the whole point. Stood on end it is a tall ribbon still showing one thing at a time, which is what the stick already does with less hardware. Laid flat you read *across* to see which panel is dragging the service down, instead of stepping through and holding numbers in your head.

**The alarm is the top bar.** Not a banner, not a rail — the bar that was going to be there anyway.

A landscape board still needs a strip for the service name, the battery and the clock. So that strip *is* the alarm: it takes the level colour, blinks at the level's rate, and everything on it is drawn in near-black ink so it stays readable on green, amber, red or grey. A full-width banner would have cost 40 of 172 rows to say one word; a vertical rail cost 42 of 640 columns. This costs **nothing** — the 26 rows were already spent on chrome, and all 146 remaining rows go to content.

It is also the most visible option of the three. A blinking strip the full width of the screen is far harder to miss from the corner of your eye than a rail down one edge, and it needs no glyph to explain itself because the level word sits right there on it.

The stick cannot do this. At 135 px wide its status bar has room for a battery and a Wi-Fi icon and nothing else, so the level word and message need a band of their own. Give the same bar 640 px and it swallows the service name, the alarm, the message, the battery and the clock with room to spare.

On the stick you ask "what is this panel doing?" and step through. On the 3.49 you ask "which panel is the problem?" and the answer is one glance.

Two things the Waveshare boards have that the stick does not: a **real RTC** (PCF85063 on the 3.49, so time survives a reboot without NTP) and **capacitive touch**. Two things they lose: the 3.49 has no speaker in the box — only an MX1.25 header — and neither has the stick's magnetic back.

---

# The M5StickS3

Detailed below because it is the tightest constraint. The other two relax it.

---

## 1. Hardware — M5StickS3

Compact stick from M5Stack, January 2026. Successor to the StickC-Plus2.

|               |                                                         |
| ------------- | ------------------------------------------------------- |
| MCU           | ESP32-S3-PICO-1-N8R8, dual-core Xtensa LX7, 240 MHz     |
| Memory        | 8 MB flash, 8 MB PSRAM                                  |
| Radio         | 2.4 GHz Wi-Fi, Bluetooth 5.0 LE, native USB (OTG & CDC) |
| Display       | 1.14" colour TFT, **135 × 240**, ST7789P3               |
| Audio out     | ES8311 codec + AW8737 amp, 8 Ω / 1 W cavity speaker     |
| Audio in      | MEMS microphone                                         |
| IMU           | 6-axis (accel + gyro) — **shake to refresh**            |
| IR            | Transmitter and receiver                                |
| Ports         | Hat2-Bus 2.54-16P (top), HY2.0-4P Grove (bottom)        |
| Battery       | 250 mAh LiPo                                            |
| Size / weight | 48.0 × 24.0 × 15.0 mm, 20 g                             |
| Mount         | Magnetic back                                           |
| Keys          | Blue front key, right side key, power (PMIC)            |

Things that bite:

| Fact             | Consequence                                          |
| ---------------- | ---------------------------------------------------- |
| No hardware RTC  | Sync over NTP after Wi-Fi. Nothing time-based before that. |
| No SD slot       | Use the Hat2-Bus if you need one.                    |
| 250 mAh          | Screen off on battery. Speaker under 75 % or the rail sags and it reboots. |
| IR receive       | Turn the speaker amp off first or IR RX fails.       |
| ST7789P3 on SPI  | A `fillRect` is cheap. A full-screen redraw every poll is not. |

---

## 2. Keys

Three keys, and only the blue one is used to look around.

| Key                        | Tap                                             | Hold                          |
| -------------------------- | ----------------------------------------------- | ----------------------------- |
| **Blue front** (GPIO 11)   | Next screen. Wakes the panel first if it is dark. | ~0.7 s → **force a poll now** |
| **Right side** (GPIO 12)   | Setup screen                                     | ~0.9 s → **Wi-Fi portal**     |
| **Power** (PMIC, left)     | Power on                                        | Power off                     |

Never bind UI actions to the power key — the PMIC owns it.

### One key, one cycle

The blue key does exactly one thing, so there is nothing to learn and nothing to hold. A tap walks every screen on the stick in order and wraps:

```
Payouts · Overview → Payouts · Payout → Payouts · Account
  → Fraud VAS · Overview → Fraud VAS · Eval → back to the start
```

That is `overview` followed by each `metrics` entry, for each configured service, in the order the backend sent them. A stick holds **at most 3 services** and shows **at most 3 panels** per service — nine screens, which is as far as one key can walk before a person loses their place. Crossing into a new service slides sideways; moving within one slides up. No menu, no back button, no way to get lost — keep tapping and you return to where you started.

### Shake to refresh

Forcing a fresh poll is a **shake**, not a button. Picking the stick up and shaking it is the gesture people already make at a thing that looks stuck, it needs no label on a 135 px panel, and it finally gives the 6-axis IMU a job.

Detection has to reject the desk being bumped, so it looks for a *pattern*, not a threshold:

| Rule            | Value    | Why                                                  |
| --------------- | -------- | ---------------------------------------------------- |
| Jolt threshold  | > 0.9 g deviation from rest | A knock on the desk is well under this |
| Jolts required  | **3**    | One bump is an accident; three reversals is intent   |
| Inside          | 700 ms   | Spread them out and the count resets                 |
| Per-jolt refractory | 80 ms | The loop runs far faster than a hand moves           |
| Cooldown        | **5 s**  | One shake is one request. Your backend is not a toy  |

A shake also wakes the panel, so on battery it is a single gesture for *show me, and make it current*. The cooldown matters more than it looks: without it a device rattling in a bag would hammer every configured endpoint, and the one thing a monitoring device must never do is become the incident.

If the stick lives somewhere it gets knocked around, raise the jolt count rather than the threshold — a higher threshold just means shaking harder, while more jolts means only a deliberate gesture qualifies.

The blue key is GPIO 11, which is also the light-sleep wake pin:

```cpp
esp_sleep_enable_ext0_wakeup(GPIO_NUM_11, 0);   // active low
```

---

## 3. Screen

The panel is 135 × 240 and Pulsar runs it **portrait** (`setRotation(0)`), USB-C down — the stick stands the way it is shaped.

**Five** fixed bands. Nothing ever moves between them; only the band that changed is redrawn.

```
0                          135
┌──────────────────────────┐ 0
│ ▮82%                 ᯤ   │  status    14
├──────────────────────────┤ 14
│ ⚠ CRITICAL               │  ALARM     30
│ 5xx 2.14% max 0.50%      │  one line
├──────────────────────────┤ 44
│ P95              210ms   │  stats     76
│ AVG               48ms   │  4 rows
│ 4XX        169    0.92%  │  size-2 values
│ 5XX        389    2.14%  │
├──────────────────────────┤ 120
│ 595                      │  graph     94
│ 2XX / MIN                │  headline drawn
│    ▁▂▃▅▆▇█▇▆▅▃▂▁         │  ON the plot
│              LAST 30 MIN │  127 × 80
├──────────────────────────┤ 214
│ Payouts             ● ○  │  footer    26
│ Overview             1/3 │
└──────────────────────────┘ 240
```

| Band    | y    | h  | Content                                                       |
| ------- | ---- | -- | ------------------------------------------------------------- |
| status  | 0    | 14 | Battery + charge state, Wi-Fi link. Nothing else.             |
| alarm   | 14   | 30 | Glyph + level word + **one** line of `alarm.message`          |
| stats   | 44   | 76 | `p95` · `avg` · `4xx` + rate · `5xx` + rate, values at size 2 |
| graph   | 120  | 94 | `trend`, with the **2XX/MIN headline drawn inside it**        |
| footer  | 214  | 26 | `service` in bold over the panel `name`, position, fleet dots |

### The headline lives on the graph

The big number is **2xx throughput** — `count_2xx / window_minutes` — and it is drawn *on top of* the plot rather than in a band of its own, on a knocked-out plate so it stays legible over the fill. The graph beneath it is `trend`, which the backend sends as 2xx per minute: the same metric, at two time scales. Headline is *now*, graph is *how it got here*.

Folding the headline into the graph is what pays for the stats block. It gives back a whole 50 px band, which is spent making the four supporting numbers **size 2** instead of size 1 — twice the height, readable from across a desk instead of at arm's length.

`LAST 30 MIN` sits bottom-right on the same kind of plate. Every number on the screen covers that one window, and saying so once, on the graph, is cheaper than qualifying four rows.

### Status classes, not "errors"

The stats block counts `4XX` and `5XX` separately, each with its raw count and its share of traffic:

- **4XX** — the caller's fault. Amber past 2 %.
- **5XX** — **your** fault. Amber past 0.5 %, red past 1 %. This is what alarms fire on.

One blended "error rate" would hide the only distinction that matters when the device goes off at 3 a.m. A client shipping a bad release and your own service falling over look identical in a single number and completely different here.

Counts arrive raw and are abbreviated on the device: `18420` → `18.4k`, `2456789` → `2.5M`. Nothing over five characters ever reaches the panel.

### Identity lives at the bottom

The service name and the panel name sit together in the footer at the **same size**, the service in bold above the panel it belongs to, with one dot per configured service coloured by that service's alarm. Putting identity at the bottom costs nothing — you already know which stick you are looking at — and hands the top two thirds of the panel to the alarm, the number and the graph.

### The alarm band is the anchor

It is drawn on every frame, at the same y, at the same height, whatever the state. What changes is loudness, not position:

The band holds a level word and, when something is wrong, a message. No icon, no chip, no badge. What changes between states is the *background*, and that is the whole signal:

| State                    | Band                                                                    | Blink        |
| ------------------------ | ----------------------------------------------------------------------- | ------------ |
| `ok`              | Green banner, tick glyph, `OK` + message                                        | none         |
| `warning` active  | **Yellow** banner, warning triangle, `WARNING` + message                        | **~0.7 Hz**  |
| `critical` active | **Red** banner, warning triangle, `CRITICAL` + message                          | **~2.3 Hz**  |
| Any fetch fault   | Grey banner, no-link glyph, numbers below held                                   | **~0.7 Hz**, the warning rate |

Every non-`200` blinks at the **warning** rate — `OFFLINE`, `REDIRECT`, `NO ACCESS`, `NOT FOUND`, `THROTTLED`, `BACKEND`. Fast enough to notice, never the critical rate, and never a beep: a Wi-Fi roam must not look like an outage. But it is never steady either, because a stick quietly showing stale numbers reads as good news, which is the worst thing it could do. Grey keeps it distinct from a real `warning` — "I cannot reach you" and "you say you are degraded" have different owners.

Levels are spelled out, not abbreviated. Beside the 12 px glyph there is room for **nine characters at size 2**, and `CRITICAL` is eight — so there is no reason to make someone expand `CRIT` in their head at a glance. The fault words follow the same rule: `OFFLINE` and `NO ACCESS` say what is wrong; `NET` and `AUTH` only say where.

There is **no acknowledged state**. The stick reads and never writes, so it cannot acknowledge anything — see [api.md §3](api.md#3-alarm). A live alarm blinks until the backend says `ok`.

Every state is the same 30 px banner: a **12 × 12 glyph**, a level word at size 2, and **one line of message**.

The glyphs are drawn from primitives, not from a font — a triangle with a bang for a warning, a tick in a disc for OK, a slashed disc for a fetch fault — in near-black on the banner colour. Two `fillTriangle` calls or a `fillCircle` and a knocked-out shape; no glyph table, no character set, nothing to fall back on. They are legible at 12 px in a way small colour art is not, and they cost about six draw calls each.

Every non-`200` blinks at the **warning** rate — `OFFLINE`, `REDIRECT`, `NO ACCESS`, `NOT FOUND`, `THROTTLED`, `BACKEND`. Fast enough to notice, never the critical rate, and never a beep: a Wi-Fi roam must not look like an outage. But it is never steady either, because a stick quietly showing stale numbers reads as good news, which is the worst thing it could do. Grey keeps it distinct from a real `warning` — "I cannot reach you" and "you say you are degraded" have different owners.

Levels are spelled out, not abbreviated. Beside the 12 px glyph there is room for **nine characters at size 2**, and `CRITICAL` is eight — so there is no reason to make someone expand `CRIT` in their head at a glance. The fault words follow the same rule: `OFFLINE` and `NO ACCESS` say what is wrong; `NET` and `AUTH` only say where.

There is **no acknowledged state**. The stick reads and never writes, so it cannot acknowledge anything — see [api.md §3](api.md#3-alarm). A live alarm blinks until the backend says `ok`.

Every state is the same 30 px banner: a **12 × 12 glyph**, a level word at size 2, and **one line of message**.

The glyphs come from primitives, not a font — a triangle with a bang for a warning, a tick in a disc for OK, a slashed disc for a fetch fault — in near-black on the banner colour. Two `fillTriangle` calls, or a `fillCircle` with a shape knocked out of it; about six draw calls each, no glyph table and nothing to fall back on. At 12 px a flat shape reads where small colour art turns to mud.

### The graph really does fit

The plot is **127 × 74 px** — 4.2 px per minute across a 30-minute window, 1.4 % of full scale per pixel of height. That is enough to read a *shape*: a plateau, a cliff, a ramp. It is not enough to read a value off, which is why the numbers are above it. It is drawn as a filled area in one flat tone with a lit surface line, not as bars and not stacked, and it takes the alarm colour when the service is in trouble.

### Colours

Authored in hex, quantised to RGB565 — what you pick is not quite what the panel shows.

| Use              | Colour    | Use            | Colour    |
| ---------------- | --------- | -------------- | --------- |
| Background       | `#05070c` | `info` / accent| `#22d3ee` |
| Panel            | `#0c1422` | `ok`           | `#22c55e` |
| Text             | `#e6edf7` | `warning`      | `#f5a524` |
| Dim              | `#7f8fa8` | `critical`     | `#ff4d5e` |
| Rule             | `#1c2740` | fetch fault    | `#8aa0c0` |

### Text budget

The built-in font is a fixed **6 × 8** cell. `setTextSize(n)` multiplies it. The panel never wraps and never scrolls, so the client truncates.

| Size | Cell    | Chars across 135 px | Used for                    |
| ---- | ------- | ------------------- | --------------------------- |
| 1    | 6 × 8   | 22                  | everything small            |
| 2    | 12 × 16 | 11                  | service name, alarm level   |
| 3    | 18 × 24 | 7                   | the hero number             |

This is where the [api.md §5 limits](api.md#5-limits) come from: `service` ≤ 14, `name` ≤ 16, `alarm.message` ≤ 40 (two lines of 21). Longer strings are cut, not wrapped.

---

## 4. Power

The firmware detects USB vs battery at runtime and switches behaviour.

| Power         | Screen                      | Poll           | Alarm            |
| ------------- | --------------------------- | -------------- | ---------------- |
| USB / charger | Always on                   | every **60 s** | Speaker + screen |
| Battery       | **Off**                     | every **60 s** | **Speaker only** |
| Battery + tap | On for **60 s**, then off   | every **60 s** | You read it      |

**The poll interval never changes.** 60 seconds, plugged in or not. Power changes what the screen does, never what the radio does — so the alarm is exactly as fresh on battery as it is docked, and there is one number in the firmware instead of two.

All services are polled every cycle, not just the one on screen — a service you are not looking at still has to be able to wake the speaker.

```cpp
bool plugged() {
  if (M5.Power.isCharging() == m5::Power_Class::is_charging) return true;
  return M5.Power.getVBUSVoltage() > 4000;      // ~5 V on VBUS
}
void displayOff() { M5.Display.setBrightness(0); M5.Display.sleep(); }
void displayOn()  { M5.Display.wakeup(); M5.Display.setBrightness(128); }
```

Speaker volume: `128` plugged, `80` on battery. Four tones then stop — anything longer browns out the 250 mAh cell.

---

## 5. Animation budget

Motion is allowed because it is cheap: one `M5Canvas` sprite pushed per frame at ~25 fps, and all of it stops the instant the panel sleeps.

| Motion              | How                                                  |
| ------------------- | ---------------------------------------------------- |
| Alarm blink         | The banner toggles two shades — 220 ms crit, 700 ms warn   |
| Service change      | Two sprites pushed at an x-offset, ~300 ms           |
| Screen change       | Same, y-offset, ~240 ms                              |
| Numbers count up    | Redraw the hero and sub bands only, ~520 ms          |
| Graph sweep-in      | Draw columns left → right on new data, ~300 ms       |
| Poll in flight      | The status-band hairline brightens                   |
| Wake / sleep        | `setBrightness()` ramp                               |

A full frame is roughly **420 primitive calls**. Never `fillScreen()` inside `loop()`; never animate with the panel dark.

---

## 6. Configuration

Held in `Preferences`, namespace `svc`. Added over the Wi-Fi portal — hold the right key, join `Pulsar-Setup`, open `192.168.4.1`. No reflash to add a service.

| Key     | Meaning                          |
| ------- | -------------------------------- |
| `count` | how many services are configured, **max 3** |
| `u{i}`  | base URL, `https://…`            |
| `t{i}`  | bearer token                     |

Wi-Fi networks live in namespace `wifi` and are joined with `WiFiMulti`, so the stick roams between saved SSIDs on its own.

The service **name** is not stored — it arrives in every payload. There is one less thing to keep in sync.

---

## 7. On-device checklist

- [ ] Portrait 135 × 240, black background, five bands
- [ ] Status band never flickers on poll
- [ ] Blue key taps through all five screens and wraps
- [ ] A deliberate shake forces a poll and the hairline lights
- [ ] Bumping the desk hard does **not** trigger a poll
- [ ] Two shakes inside 5 s produce one request, not two
- [ ] Right key opens setup; held, it raises the AP
- [ ] Alarm band occupies the same pixels in every state
- [ ] `critical` rising edge → speaker once, not once per poll
- [ ] Unplug → panel black in under 1 s
- [ ] Blue key on battery → 60 s, then black
- [ ] A service with no `metrics` shows `1/1` and is stepped over correctly
- [ ] A fourth configured service, or a third `metrics` entry, is ignored rather than crashing
- [ ] A dropped Wi-Fi shows `NET` and holds the numbers
- [ ] Volume 80 on battery with no brownout reboot
