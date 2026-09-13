# 🟦 The 240 × 240 square build

Waveshare **ESP32-S3-LCD-1.54**. One metric screen at a time, turning on its own every 5 s.

🔧 Shared behaviour → **[device.md](../docs/device.md)** · 📡 Contract → **[api.md](../docs/api.md)** · 🖥️ Other build → **[ws_lcd_349](../ws_lcd_349/device.md)**
🔌 Flashing it → **[README.md](README.md)** · 🎨 Live mockup → **[mockup.html](mockup.html)**

---

## 1. 🔩 Hardware

Waveshare square kit, SKU **33867** with cell.

| Part    | Spec                                       |
| ------- | ------------------------------------------ |
| MCU     | ESP32-S3R8, dual LX7, 240 MHz              |
| Memory  | 16 MB flash, 8 MB PSRAM                    |
| Radio   | Wi-Fi 4, BLE 5.0, native USB               |
| Display | 1.54" IPS, **240 × 240**, ST7789           |
| Touch   | CST816 — touch SKUs only                   |
| Audio   | Speaker in the case, dual MEMS mics        |
| IMU     | 6-axis — unused                            |
| RTC     | Keeps time across a reboot — unused so far |
| Storage | TF (microSD)                               |
| Battery | Optional **1000 mAh** LiPo, fits the shell |
| Keys    | PLUS (GPIO4), PWR (GPIO5), BOOT (GPIO0)    |

⚠️ **Things that bite**

- 🔋 Battery is a SKU — `-EN` boards are USB only. Confirm the cell is _Included_
- 👆 Touch is a SKU. Without the touch controller there is no way to steer the screens by hand — they still turn on their own every 5 s, and a manual refresh is **future use** on this SKU
- 🔌 **GPIO2 is the power latch**, not only the battery divider enable. Drive it LOW and a battery-powered board is off — from Waveshare's `bsp_power_manager.c`
- 🐌 ST7789 on SPI: a `fillRect` is cheap, a full-screen redraw every poll is not

### 🔄 Rotated 90° right

The panel runs at `rotation = 1`.

- 🔌 **So it can be read while it charges.** Upright, the USB-C cable comes out of the bottom edge and fights the desk; turned a quarter turn it leaves the side and the screen still faces you
- 🔊 The **speaker hole faces out** instead of into the bench, which is the difference between hearing a critical and not
- 📐 The panel is square, so **no layout number changes** — only which edge is up

---

## 2. 👆 Input

The standard map → [device.md §2](../docs/device.md#2--input--the-standard-map). On this build:

| Input                | Tap                       | Double-tap              | Hold 2 s                     |
| -------------------- | ------------------------- | ----------------------- | ---------------------------- |
| **Glass** · anywhere | Next metric screen        | _future use_            | **Force refresh**            |
| **LEFT** key         | **Settings** on / off     | _future use_            | **Hotspot** — placeholder    |
| **PWR** · middle key | _future use_              | _future use_            | **Power off** · from off: on |
| **RIGHT** key        | **Display on / off**      | **Sound mute / unmute** | _future use_                 |

- ⏱️ **Every hold is 2 s** — keys and glass alike
- ⭕ Anything held past a tap shows a ring filling toward 2 s with the action written inside — `REFRESH`, `HOTSPOT`, `OFF`, `ON` — so you see it coming and can let go. A hold that is future use shows **no ring**: nothing is coming
- 👆 **One glass, no zones.** Anywhere counts. A single tap waits 450 ms to be sure it is not the first half of a double-tap, and a finger that slides is not a tap
- 🔙 **Off the main screen**, a tap on the glass goes back to it
- 🌑 **RIGHT tap sleeps the panel only.** The cycle keeps turning, the poll keeps polling, and a critical still sounds in the dark → [device.md §2](../docs/device.md#-display-off-is-not-power-off)
- 🔕 **RIGHT double-tap** mutes every sound until it is double-tapped again or the board restarts → [§5](#5--alert-sound-and-mute)
- 🔌 **Off and on are both a 2-second hold of PWR.** The key powers the board up by itself; the firmware latches power (GPIO2) only once the hold reaches 2 s, so a brush against it does nothing. Off drops the latch — on battery that is truly off
- 🔋 On USB the rail stays up whatever GPIO2 says, so off is the panel dark plus deep sleep, and PWR (GPIO5, an RTC pin) wakes it into the same 2 s hold
- ↔️ **LEFT/RIGHT are physical positions**, not the PLUS/BOOT silkscreen — this particular board is wired the other way round from Waveshare's own labelling, so `KEY_LEFT`/`KEY_RIGHT` are swapped in `ws_lcd_154.ino` to match. Every press still prints `key: …` on Serial with its GPIO

### 🔄 One cycle

```
Orders · Created → Dispatched → Cancelled
  → Payments · Paid → Declined → back to Created, and refetch
```

- ⏱️ **Each screen is up for 5 s** and then moves on by itself — no input needed at all
- 🔄 **The wrap is the poll.** When it returns to the first screen it refetches, but never faster than `max(30, metrics × 5)` seconds → [device.md §1](../docs/device.md#1--the-cycle)
- 👆 A tap on the glass steps on early and gives the chosen screen a fresh 5 s
- 📋 Every `metrics` entry in the order sent — up to five, each naming the `gateway` it came from
- 🧭 No menu, no back button — keep tapping and you return where you started

---

## 3. 🖥️ Screen

**Four** fixed bands, named. Nothing moves between them. See it drawn, live, with real numbers → **[mockup.html](mockup.html)**.

| Band   | y   | h   | Content                                                                          |
| ------ | --- | --- | -------------------------------------------------------------------------------- |
| STATUS | 0   | 20  | Battery + charge (with an explicit bolt icon), the reading's clock, Wi-Fi, mute icon, poll hairline |
| ALERT  | 20  | 36  | Solid level colour · level word at size 2 · `alert.message` under it at size 1    |
| BODY   | 56  | 160 | The heading tile, four more tiles — the `buckets` graph as their ground          |
| FOOTER | 216 | 24  | Position rule · gateway's first five letters + metric `name`, one line at size 2 |

- 🚫 **No gateway dots.** A response can name more than one `gateway` across its rows; the FOOTER names whichever one the row on screen came from, and the STATUS band is for the device alone

### 📊 The BODY

Offsets from the top of the band — the sketch and the mockup use the same numbers.
Every tile, heading included, is the same shape: the value on the left (its unit,
if it has one, right after it), and its name above its secondary — if it has one —
stacked to the right. Only the scale changes.

| y   | What                                                                          |
| --- | ------------------------------------------------------------------------------ |
| 10  | `aggregates[0]`, the heading — value size 4; name at this same y, the row's own span (`LAST 30M`) 14 px under it, both size 2, right-aligned |
| 44 – 157 | The graph: full width, a fill fading to the baseline, one thin cyan line |
| 58  | `aggregates[1]` and `[2]` — value size 2; name/secondary size 1, 9 px apart |
| 104 | `aggregates[3]` and `[4]` — same shape as the row above                    |

A row with fewer than 5 tiles leaves the remaining slots blank — the fixed
positions still exist, nothing reflows. Today's payload names its tiles `2XX` /
`4XX` / `5XX` / `AVG` / `P95` → [api.md §4](../docs/api.md#-aggregates--tiles) —
that name **is** what's drawn, there's no separate label — but a backend may
choose differently; only the position matters to this build.

- 🔢 Number and unit formatting → [AGENTS.md §10](../AGENTS.md#10--aggregate-tile-formatting)
- 🎨 **`4XX`/`5XX` colour by convention, not schema:** this build reads `secondary_value` as a share and goes orange past 2 % (`4XX`) or orange past 0.5 %/red past 1 % (`5XX`) — keyed on those exact `name`s, nothing else
- 📈 **The graph is the ground, not a panel.** It plots one count per bucket (`buckets_value_type: total_count`) with no headroom, so a steady series runs in the gap under the hero instead of through the text. A 1 px halo keeps every name readable where the line crosses it. It sweeps in left → right over ~900 ms
- 🩵 One graph colour at every level — the ALERT band carries the alarm, the graph only carries the shape
- 🔢 The numbers count up from the previous screen's over ~520 ms
- ⏱️ **The heading's secondary slot is always the row's span**, never that tile's own `secondary_value` — `LAST 30M`, computed from `bucket_size × bucket_count × bucket_unit`, never sent → [api.md §4](../docs/api.md#-the-clock--bucket_)
- 🚫 Not drawn: the per-minute rate

### 🚨 The ALERT band, and the whole-screen flash

Same y, same height, every frame. What changes is loudness, not position.

| `alert.level`  | ALERT band                             | Flash                | Sound                        |
| -------------- | -------------------------------------- | -------------------- | ---------------------------- |
| 🟢 `info`       | Solid lime `#5cf22e`, near-black words  | none                 | none                         |
| 🟠 `warning`    | Solid orange `#ff7a00`                  | **500 ms every 5 s** | none                         |
| 🔴 `critical`   | Solid red `#ff2626`                     | **500 ms every 5 s** | **500 ms, with every flash** |
| 🩶 fetch fault  | Solid grey `#8aa0c0`                    | **500 ms every 5 s** | none                         |

- 💡 **Full-strength colour, nothing mixed in, held steady.** The band never blinks — the level is always readable at a glance
- 🚨 **The flash is the entire screen, and it is synced to the cycle, not a clock of its own.** Under `warning`, `critical` or a lost connection **all four bands** flash the level colour for the **first 500 ms of every new 5 s screen** — right at the 5 s mark, every time, whatever else is happening. `info` never flashes. `critical` alone adds its sound, in the same frame → [§5](#5--alert-sound-and-mute)
- 🎯 **The alert belongs to the response, not to the metric on screen.** It follows you through every screen; tap through and look at whichever one you want
- 🖋️ **During the flash every foreground turns to near-black ink** — numbers, labels, hairlines, the graph, the battery. No one colour reads on black, red, orange and grey alike (white on orange is under 3:1), so the ink follows the ground. The rest of the 5 s it is the normal screen, thresholds and all
- ➖ Under an alert a rule parts the ALERT band from the BODY
- 🔤 **Heading and subscript.** The level word at size 2, the message small under it at size 1
- 🎨 These level colours are brighter than [device.md §7](../docs/device.md#7--colours) — this build paints them as whole bands, so they go to full strength
- ✅ `alert` is required in every response, so this band always has something to draw — there is no empty state to design
- 🔤 Levels spelled out, never abbreviated. `CRITICAL` is eight characters and there is room
- 🚫 No glyph. The word and the colour say it
- 🙅 No acknowledged level → [api.md §3](../docs/api.md#3--alert)

### 📐 Layout rules

- 📍 **Identity lives in the FOOTER, on one line.** The gateway's first five letters, uppercase and dim (`ORDER`), then the metric `name` bright — `ORDER Created`, `ORDER Dispatched`, `PAYME Paid`. A long name gives way first, and never leaves a one- or two-letter stub
- 🏷️ The gateway comes from the **row's own `gateway`** field — required on every row, never a top-level fallback — so a merged backend always says which platform a number came from
- ➖ **The rule above the FOOTER is the position** — one segment per metric screen, the current one lit
- 🕐 **The clock is `MM/DD hh:mm AM`** of `measured_at` — the time of the reading, not the time now
- ↔️ 6 px margin on every band
- 🎨 Palette → [device.md §7](../docs/device.md#7--colours), with the brighter level colours above

### 🔠 Text budget

Fixed **6 × 8** cell, multiplied by `setTextSize(n)`. Never wraps, never scrolls.

| Size | Cell    | Chars across 240 px | Used for                                 |
| ---- | ------- | ------------------- | ---------------------------------------- |
| 1    | 6 × 8   | **40**              | STATUS band, alert message               |
| 2    | 12 × 16 | **20**              | level word, labels, shares, FOOTER       |
| 3    | 18 × 24 | **13**              | tile values                              |
| 4    | 24 × 32 | **10**              | the 2xx hero, `PULSAR` in the boot intro |

This is where the [api.md §5](../docs/api.md#5--limits) limits come from: 14 / 16 / **20**.

---

## 4. ⚙️ Settings and hotspot

### Settings screen

Tap **LEFT** to open it, tap again to go back. Read-only — everything the board knows about itself. The 5 s cycle holds still while it is up. See it live in the mockup — it renders the identical layout.

| Row                     | Where it comes from                                                                                    |
| ----------------------- | ------------------------------------------------------------------------------------------------------ |
| Battery                 | The same ADC reading as the STATUS band, and the charge pin                                             |
| Name                    | `pulsar-` + the last three bytes of the MAC — also the hotspot's Wi-Fi name                             |
| Sound                   | Muted or not. RAM only, so it always says `on` after a restart                                          |
| Connected · Signal · IP | The radio after it joins. **This offline build has no Wi-Fi**, so it shows `no - offline build` and `-` |
| MAC                     | The chip's Wi-Fi station address, `esp_read_mac()` — real even without Wi-Fi                            |
| Gateways                | Every distinct `gateway` a row named, comma-joined (`Orders, Payments`) — the level dot beside them is `alert.level`, global to the response, not any one platform |
| Poll                    | The shared interval, `max(30, metrics × 5)` seconds                                                     |

- 🔔 The critical alert sound plays here too — it belongs to the response, not to the screen you happen to be on

### 📶 Hotspot mode (placeholder)

Hold **LEFT** 2 s. The real thing raises a Wi-Fi access point named after the device and serves a page to change the endpoint and token; **that UI and its spec come later**. For now the screen names the network and address it will use and says `NOT STARTED`, and `startHotspot()` / `stopHotspot()` in `settings.ino` are stubs marked `TODO(hotspot)`.

- 🔙 Tap LEFT, hold LEFT 2 s again, or tap the glass to leave
- 💾 When it lands, what it saves goes to `Preferences` → [device.md §6](../docs/device.md#6--configuration)

---

## 5. 🔔 Alert sound and mute

- 🔴 **Critical only.** While the response is `critical`, a 500 ms sound plays with every flash — started in the same frame the flash is drawn, faded to nothing exactly as the flash ends. `warning` and no connection flash silently
- 🌑 **It plays with the panel asleep**, and on the settings screen, and anywhere else. The speaker is for when you are not looking
- 📉 **A stressed dip, pitched where this speaker can play it:** a tone falling fast from **920 to 560 Hz** with two harmonics for body, a 14 Hz shiver on top, a short thump on the attack and a soft 1.76 kHz glint
- 🔈 An earlier version dipped 260 → 180 Hz. A laptop plays that; this speaker cannot move below a few hundred hertz, so on the board the dip never happened — only the top of the sound came through
- ⚙️ **Rendered once at boot, in float maths only**, into a 500 ms buffer — the S3's FPU is single precision, and double maths runs in software, slow enough to starve a core. Then a **250 Hz high-pass**: the speaker cannot move lower, and trying only rattles and distorts
- 🔊 Codec volume **74 / 100** (−1.5 dB; the scale is logarithmic, 75 is 0 dB), peaks at **85 %** of full scale — just under the ~75 ceiling where a small cell starts to sag. The amp stays on between flashes while a critical lasts, and switches off 6 s after the last sound
- 🔕 **Double-tap RIGHT** to silence every sound; double-tap again to bring it back. The screen says `SOUND OFF` / `SOUND ON` for a moment, and a small crossed speaker sits in the STATUS band while muted
- 🔄 **Mute is RAM only** — every restart, power-on or reset comes back with sound. One exception: straight after a **brown-out reset** it starts muted, so a weak supply cannot be looped by the sound
- 🔇 No codec answering on I²C → no sound, everything else works
- 🧩 `sound.ino` in the sketch folder, no extra library. `ESP_I2S` ships with the ESP32 core; the ES8311 driver (`es8311.cpp/.h`, Espressif, Apache-2.0) sits beside it

| Audio signal         | GPIO                       |
| -------------------- | -------------------------- |
| Speaker amp enable   | 7                          |
| I²S MCLK             | 8                          |
| I²S BCLK             | 9                          |
| I²S LRCK             | 10                         |
| I²S DOUT → ES8311    | 12                         |
| ES8311 on I²C `0x18` | 42 / 41, shared with touch |

---

## 6. 🌠 Boot intro

Once per power-on, **5 s**, **two screens**, then a cross-fade into the dashboard. See it replay any time → the **boot intro** button in the mockup.

| Time        | Screen                                                                                    |
| ----------- | -------------------------------------------------------------------------------------------- |
| 0.0 – 3.0 s | **Screen 1 — the pulsar, full screen.** A bright core and two straight beams reaching almost to every edge, turning steadily, humming low in step with them. No text |
| 3.0 – 5.0 s | **Screen 2 — the label.** Plain black, bold `PULSAR`, stencil-cut, centred, coloured with a light-blue → red gradient that drifts across the letters. Silent — the hum stopped with screen 1 |
| 4.4 – 5.0 s | **Cross-fade into the dashboard** — no flash, no cut, overlapping the last 0.6 s of screen 2  |

- ⭐ **Only two beams.** A real pulsar sweeps two opposite beams from its magnetic poles, and its spin does not speed up — so nothing else streams out, and the rate never changes. **Steady 1.3 turns a second**, filling the whole panel — centred on screen, reaching almost to every edge
- 📏 **The beams are walked pixel by pixel along their own ray** — `centre + r·(cos a, sin a)` for a single, unchanging `a` — rather than drawn as separate line segments, which is what keeps them perfectly straight at every angle
- 🔊 **A low hum rides the beams.** Rendered once into a buffer at boot and streamed, exactly like the critical alert — never synthesised live. Its loudness swells on the same phase the core's glow pulses on, so the ear and the eye read one thing, not two that happen to share a rate. It fades in over 150 ms, holds for the full 3 s, fades out over the last 300 ms, and stays off for the rest of the intro
- 🔤 **A stencil-cut label, not an effect.** Plain black background, bold `PULSAR`, larger than before — the built-in monospace font printed twice, one row apart, for extra weight — then two thin bands cut back to black straight across the whole word, the way a real stencil template needs "bridges" to hold its cut-out letters together. It appears fully formed the instant screen 2 starts; no per-letter motion, no plate behind it
- 🌈 **A travelling gradient, not a flat colour.** Every pixel the stencil cut left lit is recoloured live from light blue to red and back, the mix a function of its position and the time since screen 2 started, plus a small fast ripple for shimmer — so the whole word visibly drifts for as long as it's on screen, never static, never repeating the exact same frame twice
- 🎞️ **The cross-fade is real pixels:** the dashboard is painted once into a PSRAM copy and every intro frame for the last 0.6 s is blended toward it in RGB565. The graph arrives already whole, so it does not sweep in again after
- 🔇 **Silent past screen 1.** The hum stops exactly when the beams do; the critical alert (§5) is the only other sound this build makes, and it never plays during the intro. Both are rendered once into a buffer, never synthesised live — an earlier version generated a soundtrack live on core 0; stretched to 5 s it starved that core's idle task past the **5 s task watchdog**, and the board reset straight back into the intro, over and over
- 🧯 Every boot prints `boot: reset: …` on Serial — if the board ever loops again, that line names the cause

---

## 7. 📁 What this build adds

Cycle, input, bands, logging, speaker, hotspot and palette are shared → **[device.md](../docs/device.md)**. Particular to this one:

- 🧠 Sprite is 240 × 240 × 16 bpp = **113 KB**, PSRAM, one push per frame
- 🔄 `rotation = 1` so it reads while charging → [§1](#-rotated-90-right)

The sketch is modular — one concern per file, all flat in `ws_lcd_154/` (Arduino requires the main `.ino` to share its folder's name, hence `ws_lcd_154.ino` rather than a generic name):

| File            | Owns                                                     |
| --------------- | -------------------------------------------------------- |
| `ws_lcd_154.ino`    | Pins, model, palette, logging, `setup()` / `loop()`      |
| `intro.ino`     | The welcome screen                                       |
| `dashboard.ino` | The four bands                                           |
| `input.ino`     | Keys and touch — taps, double-taps, holds                |
| `power.ino`     | Power latch, off/on, display on/off                      |
| `settings.ino`  | Settings screen and hotspot screen                       |
| `sound.ino`     | The boot-intro hum, the alert sound, and mute            |
| `net.ino`       | Poll scheduling, the fetch placeholder, the test payload |
| `es8311.*`      | Vendor codec driver (Espressif, Apache-2.0)              |

---

## 8. ☑️ On-device checklist

**Serial Monitor at 115200** for all of it — every step below prints what it did.

- [ ] Boot intro plays once for 5 s — 3 s pulsar with a hum, 2 s the gradient `PULSAR` label — then the dashboard; Serial shows `boot: reset: power on`
- [ ] The two beams stay visibly straight through a full spin, at every angle
- [ ] The boot hum pulses twice a turn, in step with the core brightening — never a beat late or early, and stops the instant screen 2 starts
- [ ] The `PULSAR` label's gradient visibly drifts across the full 2 s — never a single static frame
- [ ] Screen is rotated 90° right — readable with the charging cable plugged in
- [ ] 240 × 240, black background, four bands
- [ ] Clock reads `MM/DD hh:mm AM` of `measured_at`
- [ ] No gateway dots anywhere — the FOOTER is the only place a gateway is named
- [ ] FOOTER reads `ORDER Created` on the first screen, and the gateway half comes from the row's own `gateway`
- [ ] No number wider than 5 characters, at any count
- [ ] **Screens turn on their own every 5 s** and wrap back to the first
- [ ] On the wrap it refetches — and `net:` says either `refetching` or how long until the next poll
- [ ] A glass tap steps on early and the chosen screen gets a fresh 5 s; a slide does nothing
- [ ] A glass double-tap does nothing and Serial says `future use`
- [ ] Glass held 2 s refreshes: `REFRESH` ring, hairline lights, graph sweeps in, `last poll` resets
- [ ] LEFT tap opens settings, tap again goes back; MAC and name are real, battery tracks the STATUS band, GATEWAYS lists both platforms
- [ ] LEFT held 2 s shows the hotspot placeholder; tap LEFT or the glass to leave
- [ ] **PWR tap does nothing** and Serial says `POWER tap -> future use`
- [ ] PWR held 2 s switches off; from off, PWR held 2 s switches on — shorter does nothing
- [ ] On USB, off goes dark and PWR held 2 s brings it back
- [ ] **RIGHT tap blanks the panel**; tap again and it comes back
- [ ] With the panel blank, a critical **still sounds**, right at every 5 s mark
- [ ] **RIGHT double-tap** shows `SOUND OFF` and the crossed speaker; double-tap again brings sound back
- [ ] **RIGHT held 2 s does nothing**, shows no ring, and Serial says `future use`
- [ ] Muted, then power off and on → sound is back
- [ ] `critical`, `warning` and no connection flash **the whole screen** for 500 ms right at the start of every 5 s screen — every number readable during the flash
- [ ] ALERT band occupies the same pixels at every level, in solid colour
- [ ] `critical` plays its 500 ms sound with every flash, in step with it; `warning` and no connection stay silent
- [ ] The battery icon shows an explicit bolt while charging, not just a colour change
- [ ] Sending a metric with no `gateway`, no `aggregates`, or a null field logs the problem on Serial and still renders — never a crash
- [ ] A one-metric payload shows one full-width position segment and polls every 30 s
- [ ] A sixth `metrics` entry is ignored rather than crashing, and Serial says so
- [ ] Critical alert sound on battery with no brown-out reboot, and no rattle or distortion from the speaker
