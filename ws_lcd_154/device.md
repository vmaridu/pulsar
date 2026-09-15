# 🟦 The 240 × 240 square build

Waveshare **ESP32-S3-LCD-1.54**. One metric screen at a time, turning on its own every 5 s.

🔧 Shared behaviour → **[functional-requirements.md](../docs/functional-requirements.md)** · 📡 Contract → **[api.md](../docs/api.md)** · 🖥️ Other build → **[ws_lcd_349](../ws_lcd_349/device.md)**
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
- 🔊 The **speaker hole faces out** instead of into the bench, which is the difference between hearing a crit and not
- 📐 The panel is square, so **no layout number changes** — only which edge is up

📌 **Exact pin assignments** (the physical silkscreen, LCD/touch/I²C/I²S/battery lines
included) live in the `#define`s at the top of [`ws_lcd_154.ino`](ws_lcd_154.ino) — the
source is the one copy of this fact, not restated here. `KEY_LEFT`/`KEY_RIGHT` are
swapped there from the BOOT/PLUS silkscreen, to match a board wired backwards from
Waveshare's own labelling; swap them back if yours isn't. Those two macro names track
physical position on the case only — the logical keys they feed are called **DOWN**
and **UP** everywhere else, in code, on screen and in this doc.

### 🔋 Where the live values come from

Everything except battery comes from the backend, over the configured URL. The battery
is read off the ADC pin, behind a 1/3 divider (`analogReadMilliVolts() × 3.0`), gated by
a divider-enable pin; a separate pull-up pin reads **LOW = charging** — also what lights
the STATUS band's bolt icon. Percent is interpolated between five voltage calibration
points (<3.52 V → 1 %, then 3.64/3.76/3.88/4.00 V → 20/40/60/80 %, ≥4.00 V → 100 %)
rather than stepped, so a slow discharge reads as a slow decline, not a jump between
fixed values.

🔋 **Backlight brightness has two figures, one for on-battery and one for on-charger** —
each 30-100 %, defaulting to 50 % on battery and 90 % on the charger, set on the setup
page → [§4](#4--settings-and-hotspot). The board switches between them the moment it
notices the charge state change, no restart needed.

🪫 **Below 10 % and not on the charger, the board shuts itself down after a 2-minute
grace period** rather than run the cell past a safe restart point — a toast and a soft
notice sound mark the moment the timer starts, and plugging in during those 2 minutes
cancels it.

⏰ **The STATUS band's clock is the data's, not the wall's.** There is no RTC, and the
time shown is the payload's own `measured_at`, formatted `MM/DD hh:mm AM` — the time the
numbers were measured, which is the one worth knowing. Shown in whichever zone is picked
on the setup page (**Clock** section, a list of real zones, not a raw offset) — daylight
saving applied automatically for the life of the board, no reflash and no twice-a-year
manual change needed. Defaults to US Eastern (New York). SNTP is asked for the real time
on every join, so a **signed** request can carry a timestamp the backend will accept, but
that clock is always raw UTC seconds —
the display zone never touches it, in either direction.

---

## 2. 👆 Input

This build's own input map — the physical keys and the glass, and what each one does:

| Input                | Tap                       | Double-tap              | Hold 2 s                                |
| -------------------- | ------------------------- | ----------------------- | ----------------------------------------- |
| **Glass** · anywhere | Next metric screen — or, locked, **open the unlock keypad** | **Settings** on / off   | **Force refresh**                        |
| **DOWN** key         | _future use_ — off the main screen, back | **Settings** on / off | **Setup hotspot**                    |
| **PWR** · middle key | _future use_              | _future use_            | **Power off** · from off: on             |
| **UP** key           | **Display on / off**      | **Sound mute / unmute** | **Lock the screen**, or open the unlock keypad → [§6](#6--privacy-lock) |

- ⏱️ **Every hold is 2 s** — keys and glass alike
- ⭕ Anything held past a tap shows a ring filling toward 2 s with the action written inside — `REFRESH`, `SETUP`, `EXIT`, `OFF`, `ON`, `LOCK`, `UNLOCK` — so you see it coming and can let go. A hold that is future use shows **no ring**: nothing is coming
- 👆 **One glass, no zones** on the main screen; the unlock keypad is the one place a tap position matters at all → [§6](#6--privacy-lock). Elsewhere a single tap waits 450 ms to be sure it is not the first half of a double-tap, and a finger that slides is not a tap
- 🔓 **A tap on the glass while the dashboard is showing locked opens the unlock keypad** — the same destination UP's own hold already reaches, not the next metric screen (which stays hidden behind the padlock anyway). A double-tap still opens **Settings** either way, locked or not — the lock never gates diagnostics → [§6](#6--privacy-lock)
- 🔙 **Off the main screen**, a tap on the glass goes back to it — except the keypad, which reads a tap as a digit; DOWN backs out of that one too
- 🌑 **UP tap sleeps the panel only.** The cycle keeps turning, the poll keeps polling, and a crit still sounds in the dark → [functional-requirements.md §1](../docs/functional-requirements.md#-display-off-is-not-power-off)
- 🔕 **UP double-tap** mutes every sound until it is double-tapped again or the board restarts → [§5](#5--alert-sound-and-mute)
- 🔌 **Off and on are both a 2-second hold of PWR.** The key powers the board up by itself; the firmware latches power (GPIO2) only once the hold reaches 2 s, so a brush against it does nothing. Off drops the latch — on battery that is truly off
- 🔋 On USB the rail stays up whatever GPIO2 says, so off is the panel dark plus deep sleep, and PWR (GPIO5, an RTC pin) wakes it into the same 2 s hold
- ↔️ **DOWN/UP are physical positions**, not the PLUS/BOOT silkscreen — this particular board is wired the other way round from Waveshare's own labelling, so `KEY_LEFT`/`KEY_RIGHT` are swapped in `ws_lcd_154.ino` to match. Every press still prints `key: …` on Serial with its GPIO

### 🔄 One cycle

```
Orders · Created → Dispatched → Cancelled
  → Payments · Paid → Declined → back to Created, and refetch
```

- ⏱️ **Each screen is up for 5 s** and then moves on by itself — no input needed at all
- 🔄 **The wrap is the poll.** When it returns to the first screen it refetches, but never faster than `max(30, metrics × 5)` seconds → [functional-requirements.md §1](../docs/functional-requirements.md#1--screens)
- 👆 A tap on the glass steps on early and gives the chosen screen a fresh 5 s
- 📋 Every `metrics` entry in the order sent — up to five, each naming the `gateway` it came from
- 🧭 No menu, no back button — keep tapping and you return where you started

---

## 3. 🖥️ Screen

**Four** fixed bands, named. Nothing moves between them. See it drawn, live, with real numbers → **[mockup.html](mockup.html)**.

| Band   | y   | h   | Content                                                                          |
| ------ | --- | --- | -------------------------------------------------------------------------------- |
| STATUS | 0   | 20  | Battery + charge (with an explicit bolt icon), the reading's clock, Wi-Fi, speaker icon (crossed while muted), poll hairline |
| ALERT  | 20  | 36  | Solid level colour · level word at size 3, left · `alert.message` beside it at size 2, scrolling if it's too long for the width left |
| BODY   | 56  | 160 | The heading tile, four more tiles — the `buckets` graph as their ground          |
| FOOTER | 216 | 24  | Position rule · gateway's first five letters + metric `name`, one line at size 2 |

- 🚫 **No gateway dots.** A response can name more than one `gateway` across its rows; the FOOTER names whichever one the row on screen came from, and the STATUS band is for the device alone

### 📊 The BODY

Offsets from the top of the band — the sketch and the mockup use the same numbers.
Every tile, heading included, is the same shape: the value on the left (its unit,
if it has one, right after it) and its name stacked to the right. The heading alone
carries a second line under its name, for the row's own span. Only the scale changes.

| y   | What                                                                          |
| --- | ------------------------------------------------------------------------------ |
| 10  | `aggregates[0]`, the heading — value size 4; name at this same y size 2, the row's own span (`LAST 30M`) 18 px under it, also size 2, right-aligned |
| 44 – 157 | The graph: full width, a fill fading to the baseline, one thin cyan line |
| 58  | `aggregates[1]` and `[2]` — value and name, unit 9 px under the value |
| 104 | `aggregates[3]` and `[4]` — same shape as the row above                    |

A row with fewer than 5 tiles leaves the remaining slots blank — the fixed
positions still exist, nothing reflows. Today's payload names its tiles `2XX` /
`4XX` / `5XX` / `AVG` / `P95` → [api.md §4](../docs/api.md#-aggregates--tiles) —
that name **is** what's drawn, there's no separate label — but a backend may
choose differently; only the position matters to this build.

- 📐 **The four body tiles size themselves to fit.** A short value (2 digits or
  fewer) draws at size 4, three digits at size 3, four or five digits fall back
  to size 2 — same tiering the hero already uses. The name independently tries
  size 2 first and drops to size 1 only if that would run into the value or the
  unit beside it, so the common case (short numbers, `2XX`/`4XX`/`5XX`/`AVG`/`P95`)
  fills its slot instead of sitting tiny in a corner of it. The unit itself never
  grows — it's already as small as it needs to be

- 🔢 Number and unit formatting → [AGENTS.md §10](../AGENTS.md#10--aggregate-tile-formatting)
- 🎨 **A tile's colour is its own `level`**, nothing this build computes: `crit` red, `warn` orange, `info` (or omitted) the theme's own colour. Any tile may carry one — a backend that wants `4XX` to read as trouble sends `"level": "warn"` itself
- 📈 **The graph is the ground, not a panel.** It plots one count per bucket (`buckets_value_type: total_count`) with no headroom, so a steady series runs in the gap under the hero instead of through the text. A 1 px halo keeps every name readable where the line crosses it. It sweeps in left → right over ~900 ms
- 🩵 One graph colour at every level — the ALERT band carries the alarm, the graph only carries the shape
- 🔢 The numbers count up from the previous screen's over ~520 ms
- ⏱️ **The heading's second line is always the row's span** — `LAST 30M`, computed client-side from `bucket_size × bucket_count × bucket_unit`, never a field the tile itself carries → [api.md §4](../docs/api.md#-the-clock--bucket_)
- 🚫 Not drawn: the per-minute rate

### 🚨 The ALERT band, and the whole-screen flash

Same y, same height, every frame. What changes is loudness, not position.

| `alert.level`  | ALERT band at rest                     | Flash                | Sound                             |
| -------------- | -------------------------------------- | -------------------- | ---------------------------------- |
| 🟢 `info`       | Plain background, bold lime word       | none                 | none                               |
| 🟠 `warn`       | Plain background, bold orange word     | **500 ms every 5 s** | short, quiet notice, every flash   |
| 🔴 `crit`       | Plain background, bold red word        | **500 ms every 5 s** | **full alert, every flash**       |
| 🩶 fetch fault  | Plain background, bold grey word       | **500 ms every 5 s** | short, quiet notice, every flash   |

- 💡 **The level word is always full-strength colour, bold, held steady** — that is the whole identification at rest, not a coloured background. The band never blinks — the level is always readable at a glance
- 🚨 **The flash is the entire screen, and it is synced to the cycle, not a clock of its own.** Under `warn`, `crit` or a lost connection **all four bands** — including this one's own background — flash the level colour for the **first 500 ms of every new 5 s screen** — right at the 5 s mark, every time, whatever else is happening. `info` never flashes. `crit` gets the full alert; `warn` and a fetch fault get a shorter, quieter notice instead — in the same frame either way → [§5](#5--alert-sound-and-mute)
- 🕰️ **One fetch fault borrows this table's `warn` row instead of `fetch fault`'s:** `NO CLOCK` (a signed board waiting on its own SNTP) is a local hold-up, not "can't reach the backend," so it reads as orange, same as a real `warn` — still a held-data fault underneath
- 🎯 **The alert belongs to the response, not to the metric on screen.** It follows you through every screen; tap through and look at whichever one you want
- 🖋️ **During the flash every foreground turns to near-black ink** — numbers, labels, hairlines, the graph, the battery. No one colour reads on black, red, orange and grey alike (white on orange is under 3:1), so the ink follows the ground. The rest of the 5 s it is the normal screen, thresholds and all
- ➖ **A soft rule always parts the ALERT band from the BODY** — not just during a flash, at rest too, `hFade()` in `dashboard.ino`
- 🔤 **Side by side, not stacked.** The level word at size 3, bold, left-aligned; `alert.message` at size 2 right beside it, baseline-matched, taking whatever width is left. A message too long for that width scrolls slowly (one character every 260 ms) instead of cutting off — [§3's text budget](#-text-budget) has the detail
- ✅ `alert` is required in every response, so this band always has something to draw — there is no empty state to design
- 🔤 Levels drawn abbreviated — `INFO` / `WARN` / `CRIT` — the same short form `alert.level` itself carries on the wire, so the screen never spells out anything the backend didn't send
- 🚫 No glyph. The word and the colour say it
- 🙅 No acknowledged level → [api.md §3](../docs/api.md#3--alert)

### 📐 Layout rules

- 📍 **Identity lives in the FOOTER, on one line.** The gateway's first five letters, uppercase and dim, sit at the left edge; the metric `name`, bright, sits at the right edge — `ORDER` left / `Created` right, `ORDER` left / `Dispatched` right, `PAYME` left / `Paid` right. A long name gives way first, and never leaves a one- or two-letter stub
- 🏷️ The gateway comes from the **row's own `gateway`** field — required on every row, never a top-level fallback — so a merged backend always says which platform a number came from
- ➖ **The rule above the FOOTER is the position** — one segment per metric screen, the current one lit
- 🕐 **The clock is `MM/DD hh:mm AM`** of `measured_at` — the time of the reading, not the time now
- ↔️ 6 px margin on every band
- 🎨 The shared palette, with the brighter level colours above

### 🔠 Text budget

One font, **ProFont** — the same monospaced terminal face the wide build draws — in three sizes, shipped in `fonts.h` in Adafruit GFX format and picked by `txt(…, size)`. Every glyph is the same width, so the counts below are exact. Bold is the same glyph printed twice, one pixel apart. Never wraps. One exception scrolls instead of truncating: the ALERT band's detail, when it's too long for the width left beside the level word — a whole-character marquee, one character every 260 ms, `drawScrolling()` in `dashboard.ino`. Nothing else on this build scrolls.

| Size | Face      | Advance × cap | Chars across 240 px | Used for                                 |
| ---- | --------- | ------------- | ------------------- | ---------------------------------------- |
| 1    | ProFont12 | 6 × 8         | **40**              | STATUS band, tile names, units, hints    |
| 2    | ProFont22 | 12 × 14       | **20**              | alert message, labels, shares, FOOTER    |
| 3, 4 | ProFont29 | 16 × 19       | **15**              | level word, every tile value — two and three characters draw the same |
| 5    | built-in  | 30 × 40       | —                   | `PULSAR` in the boot intro, the one place the classic 6 × 8 cell font still draws |

This is where the [api.md §5](../docs/api.md#5--limits) limits come from: 14 / 16 / **20**.

---

## 4. ⚙️ Settings and hotspot

### Settings screen

Double-tap **DOWN**, or double-tap the glass, to open it; a tap on either goes back. Read-only — everything the board knows about itself. The 5 s cycle holds still while it is up. See it live in the mockup — it renders the identical layout. What every build's settings screen answers → [functional-requirements.md §7](../docs/functional-requirements.md#7--settings-screen); the rows below are this build's.

Four groups, in the order you would ask the questions: what am I holding, what did it join, where is it pointing, and what came back.

| Row                     | Where it comes from                                                                                    |
| ----------------------- | ------------------------------------------------------------------------------------------------------ |
| **DEVICE** Battery      | The same ADC reading as the STATUS band, and the charge pin                                             |
| Name                    | `pulsar-` + the last three bytes of the MAC — also the setup hotspot's Wi-Fi name                       |
| MAC                     | The chip's Wi-Fi station address, `esp_read_mac()` — the one a network's allowlist wants                |
| **WI-FI** Network       | The saved network it actually joined. Not joined → what the radio is doing instead (`scanning`, `joining`, `waiting to retry`), or `none saved` in orange. Joined but walled in by a sign-in page → the name with `SIGN-IN` beside it, in orange |
| Signal · IP             | `WiFi.RSSI()` and the address DHCP gave it, `-` when not joined                                         |
| **BACKEND** Host        | Just the host out of the configured URL — orange `NOT SET` on a board nobody has set up yet             |
| Auth                    | `bearer` · `signed` · `no key`, then `tls ok` · `TLS UNVERIFIED` · `NO TLS`. Orange unless all of it is right |
| **GATEWAYS**            | Every distinct `gateway` a row named, comma-joined (`Orders, Payments`) — the level dot beside them is `alert.level`, global to the response, not any one platform, and grey while a fetch fault stands |
| ↳ right of it           | Screen count and how old the numbers are. With nothing parsed yet, the poll interval instead            |

- 🔕 Sound is not a row here — the crossed-speaker icon in the STATUS band already says it, and the space is worth more to the radio
- 🔔 The crit alert sound plays here too — it belongs to the response, not to the screen you happen to be on

### 📶 The setup hotspot

Hold **DOWN** 2 s. The board raises a WPA2 access point named after itself, answers every DNS name with its own address so the page opens on its own, and serves the setup page at `192.168.4.1` → [functional-requirements.md §5](../docs/functional-requirements.md#5--configuration).

| Line                 | What                                                                       |
| -------------------- | -------------------------------------------------------------------------- |
| `SETUP`              | Title, cyan                                                                |
| `JOIN THIS WI-FI`    | then the device name at size 2 — the AP's SSID                             |
| `PASSWORD`           | then this session's password at size 2, in cyan. **New every time**        |
| `THEN OPEN`          | then `192.168.4.1` at size 2                                               |
| status               | `WAITING FOR A PHONE`, or `n CONNECTED` in lime, and `PAGE OPENED` once it has been |
| below                | `SAVED n TIMES` in lime once the page saves, else the stored network count and whether a URL is set |

- 🔴 If the access point will not come up, the screen says `HOTSPOT FAILED` rather than showing a password that would not work
- 🔙 Tap DOWN, hold DOWN 2 s again, or tap the glass to leave — **the AP goes down the moment you do**
- 📻 The station side stays enabled but unassociated while it is up, so the page can scan for networks; it never joins one, because one radio cannot follow your network's channel and hold this AP still at the same time
- 🔆 **Two backlight brightness figures, one for on-battery and one for on-charger** — each
  30-100 %, defaulting to 50 % on battery and 90 % on the charger → [§1](#1--hardware). The
  board switches between them the moment it notices the charge state change, no restart
  needed
- 🗑️ **A factory reset**, behind its own confirmation on the page — a checkbox plus typing
  the word **YES**, not just a click. Wipes the endpoint, every credential, the lock code
  and every saved network, then restarts into the same state as a fresh board: `SETUP`,
  nothing configured, no undo

---

## 5. 🔔 Alert sound and mute

- 🔴 **`crit` gets the full alert; `warn` and any connection fault get a
  shorter, quieter notice instead.** Whichever it is, it plays with every flash —
  started in the same frame the flash is drawn, faded to nothing exactly as the
  flash ends. `info` stays silent
- 🌑 **It plays with the panel asleep**, and on the settings screen, and anywhere else. The speaker is for when you are not looking
- 📉 **The alert is a stressed dip, pitched where this speaker can play it:** a tone falling fast from **920 to 560 Hz** with two harmonics for body, a 14 Hz shiver on top, a short thump on the attack and a soft 1.76 kHz glint. 500 ms, peaking at 85 % of full scale
- 🔔 **The notice is one plain tone at 660 Hz**, a touch of its own octave for body, quick in and quicker out — 220 ms, peaking at just 35 % of full scale. Deliberately nothing like the alert: a nudge, not an alarm
- 🔈 An earlier version of the alert dipped 260 → 180 Hz. A laptop plays that; this speaker cannot move below a few hundred hertz, so on the board the dip never happened — only the top of the sound came through
- ⚙️ **Both rendered once at boot, in float maths only** — the S3's FPU is single precision, and double maths runs in software, slow enough to starve a core. Then a **250 Hz high-pass**: the speaker cannot move lower, and trying only rattles and distorts
- 🔊 Codec volume **74 / 100** (−1.5 dB; the scale is logarithmic, 75 is 0 dB) — just under the ~75 ceiling where a small cell starts to sag. The amp stays on between flashes while an alert lasts, and switches off 6 s after the last sound
- 🔕 **Double-tap UP** to silence every sound; double-tap again to bring it back. The screen says `SOUND OFF` / `SOUND ON` for a moment; the STATUS band's speaker icon is always there, dim, and gains a cross over it while muted
- 🔄 **Mute clears itself three ways**: the double-tap; a restart (RAM only, so every power-on or reset comes back with sound — except straight after a **brown-out reset**, which starts muted so a weak supply cannot loop the sound); or a configured timeout elapsing on its own — 5 m / 10 m / 30 m / 1 h / 6 h / 12 h / 24 h / never, set on the setup page, default 30 m → [functional-requirements.md §5](../docs/functional-requirements.md#5--configuration)
- 🔇 No codec answering on I²C → no sound, everything else works
- 🧩 `sound.ino` in the sketch folder, no extra library. `ESP_I2S` ships with the ESP32 core; the ES8311 driver (`es8311.cpp/.h`, Espressif, Apache-2.0) sits beside it. Exact GPIOs → [`ws_lcd_154.ino`](ws_lcd_154.ino)

---

## 6. 🔒 Privacy lock

Holds BODY and FOOTER — the actual stats and graph — off screen behind a lock icon
until a 6-digit code is typed on an on-screen keypad. STATUS and ALERT are drawn
exactly as they always are either way, and neither sound nor any other key change
at all — this is strictly about who can read the numbers.

- 🔐 **Hold UP 2 s to lock it — no code needed.** There is nothing to prove to take
  a screen *out* of view. Held again while already locked, the same gesture raises
  the keypad instead — **so does a plain tap on the glass** while the dashboard is
  showing locked, a second way to the same keypad → [§2](#2--input)
- 🔢 **Six digits, entered on a 3x3 grid of 1-9** (no 0, no clear, no backspace — the
  buttons stay big enough to hit reliably), auto-submitting on the sixth. Right
  unlocks; wrong just clears the entry and counts as a try. `DOWN` backs out of the
  keypad without entering anything
- 👁️ **The digit just pressed shows briefly, then folds into a masked dot** — the
  same way a phone's PIN entry does — so a mis-tap is visible immediately. Only ever
  the one most recent digit, never enough to read the whole code at a glance
- 🔟 **Default code `123456`**, changed on the setup page like any other secret —
  1-9 only, never shown back once set, only that one exists → [functional-requirements.md §5](../docs/functional-requirements.md#5--configuration)
- 🚫 **Five wrong codes inside a rolling 5 minutes** blocks the keypad until enough
  of that window has passed — kept in NVS (count, plus when it last grew), not just
  RAM, so a restart can't be used to dodge it. A correct code clears it outright
- 🤐 **The code is never printed on Serial** — stored or typed, right or wrong.
  Only that an attempt happened, and whether it was accepted. The brief on-screen
  reveal above is screen-only and never touches Serial either
- 🔁 **Every restart locks it again**, whether or not it was unlocked before —
  the safe state is the default, not something a board has to earn back
- ⏲️ **An unlocked screen re-locks itself on its own**, after a configurable time
  set on the setup page — 5 m / 10 m / 30 m / 1 h / 6 h / 12 h / 24 h, or never.
  Defaults to 30 minutes → [functional-requirements.md §5](../docs/functional-requirements.md#5--configuration)
- 🙈 **Armed only once real data has shown at least once, and only on a touch
  SKU** — there is nothing to hide before then, and no way to type a code back
  in with three keys alone. A non-touch board never locks itself; UP hold 2 s
  stays future use there, same as it always has been
- 🩶 Settings and the setup hotspot are unaffected — this only ever hides the
  stats, never the device's own diagnostics

---

## 7. 🌠 Boot intro

Once per power-on, **5 s**, **two screens**, then a cross-fade into the dashboard. See it replay any time → the **boot intro** button in the mockup.

| Time        | Screen                                                                                    |
| ----------- | -------------------------------------------------------------------------------------------- |
| 0.0 – 3.0 s | **Screen 1 — the pulsar, full screen.** A bright core and two straight beams reaching almost to every edge, turning steadily, firing a torpedo-launch burst every time a beam sweeps past top. No text |
| 3.0 – 5.0 s | **Screen 2 — the label.** Plain black, bold `PULSAR`, stencil-cut, centred, coloured with a light-blue → red gradient that drifts across the letters. Silent — the fire stopped with screen 1 |
| 4.4 – 5.0 s | **Cross-fade into the dashboard** — no flash, no cut, overlapping the last 0.6 s of screen 2  |

- ⭐ **Only two beams.** A real pulsar sweeps two opposite beams from its magnetic poles, and its spin does not speed up — so nothing else streams out, and the rate never changes. **Steady 1.3 turns a second**, filling the whole panel — centred on screen, reaching almost to every edge
- 📏 **The beams are walked pixel by pixel along their own ray** — `centre + r·(cos a, sin a)` for a single, unchanging `a` — rather than drawn as separate line segments, which is what keeps them perfectly straight at every angle
- 🔊 **A torpedo-launch burst fires with every beam pass** — twice a turn, a soft whoosh and a rounder thump, audible but soft-edged on purpose (well under the crit alert's own volume) — a launch, not a siren, so it never reads as trouble. Rendered once into a buffer at boot and streamed, exactly like the crit alert — never synthesised live. Each burst lands on the same instant the core's glow pulses on, so the ear and the eye read one thing, not two that happen to share a rate. The whole run fades in over 150 ms, fades out over the last 300 ms, and stays off for the rest of the intro
- 🔤 **A stencil-cut label, not an effect.** Plain black background, bold `PULSAR`, larger than before — the built-in monospace font printed twice, one row apart, for extra weight — then two thin bands cut back to black straight across the whole word, the way a real stencil template needs "bridges" to hold its cut-out letters together. It appears fully formed the instant screen 2 starts; no per-letter motion, no plate behind it
- 🌈 **A travelling gradient, not a flat colour.** Every pixel the stencil cut left lit is recoloured live from light blue to red and back, the mix a function of its position and the time since screen 2 started, plus a small fast ripple for shimmer — so the whole word visibly drifts for as long as it's on screen, never static, never repeating the exact same frame twice
- 🎞️ **The cross-fade is real pixels:** the dashboard is painted once into a PSRAM copy and every intro frame for the last 0.6 s is blended toward it in RGB565. The graph arrives already whole, so it does not sweep in again after
- 🔇 **Silent past screen 1.** The torpedo fire stops exactly when the beams do; the crit alert (§5) is the only other sound this build makes, and it never plays during the intro. Both are rendered once into a buffer, never synthesised live — an earlier version generated a soundtrack live on core 0; stretched to 5 s it starved that core's idle task past the **5 s task watchdog**, and the board reset straight back into the intro, over and over
- 🧯 Every boot prints `boot: reset: …` on Serial — if the board ever loops again, that line names the cause

---

## 8. 📁 What this build adds

Cycle, input, bands, logging, speaker, hotspot and palette are shared → **[functional-requirements.md](../docs/functional-requirements.md)**. Particular to this one:

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
| `config.ino`    | The stored endpoint and saved networks (NVS)             |
| `wifi.ino`      | Joining saved networks — priority, enterprise, portals   |
| `hotspot.ino`   | The setup hotspot, the page it serves, its screen        |
| `settings.ino`  | The settings screen                                      |
| `sound.ino`     | The boot-intro torpedo fire, the alert sound, and mute   |
| `lock.ino`      | The privacy lock — code, keypad, auto-relock             |
| `net.ino`       | Poll scheduling and the HTTPS `GET`                      |
| `es8311.*`      | Vendor codec driver (Espressif, Apache-2.0)              |

---

## 9. ☑️ On-device checklist

**Serial Monitor at 115200** for all of it — every step below prints what it did.

- [ ] Boot intro plays once for 5 s — 3 s pulsar firing torpedo bursts, 2 s the gradient `PULSAR` label — then the dashboard; Serial shows `boot: reset: power on`
- [ ] The two beams stay visibly straight through a full spin, at every angle
- [ ] The torpedo fire bursts twice a turn, in step with the core brightening — never a beat late or early, and stops the instant screen 2 starts
- [ ] The `PULSAR` label's gradient visibly drifts across the full 2 s — never a single static frame
- [ ] Screen is rotated 90° right — readable with the charging cable plugged in
- [ ] 240 × 240, black background, four bands
- [ ] Clock reads `MM/DD hh:mm AM` of `measured_at`
- [ ] No gateway dots anywhere — the FOOTER is the only place a gateway is named
- [ ] FOOTER reads `ORDER` at the left edge and `Created` at the right edge on the first screen, and the gateway half comes from the row's own `gateway`
- [ ] No number wider than 5 characters, at any count
- [ ] **Screens turn on their own every 5 s** and wrap back to the first
- [ ] On the wrap it refetches — and `net:` says either `refetching` or how long until the next poll
- [ ] A glass tap steps on early and the chosen screen gets a fresh 5 s; a slide does nothing
- [ ] A glass double-tap opens settings, and a tap goes back; a DOWN tap on the main screen says `future use`
- [ ] Glass held 2 s refreshes: `REFRESH` ring, hairline lights, graph sweeps in, `last poll` resets
- [ ] DOWN double-tap opens settings, tap goes back; MAC and name are real, battery tracks the STATUS band, WI-FI names the network it joined with its real IP, BACKEND names the configured host, GATEWAYS lists both platforms
- [ ] DOWN held 2 s raises the hotspot: the screen names the network, a fresh password and `192.168.4.1`
- [ ] A phone joining it makes the count on screen go to `1 CONNECTED`, and the setup page opens by itself
- [ ] The page lists the saved networks in priority order and shows **no** stored password, only `saved - leave blank to keep`
- [ ] Scanning from the page lists what is really in the room; tapping one adds it
- [ ] Save & restart: the board comes back, joins, and shows real numbers within a few seconds
- [ ] Tap DOWN or the glass to leave the hotspot — the AP disappears from the phone's list straight away
- [ ] With no URL set at all: `SETUP` in the ALERT band, `NO DATA YET` and `HOLD DOWN 2 S` in the BODY, and **no numbers anywhere**
- [ ] With a URL set but every saved network out of range: `OFFLINE`, and the last good numbers stay up if there were any
- [ ] Wrong API key: `NO ACCESS`, held numbers stay, and it keeps retrying every cycle
- [ ] **PWR tap does nothing** and Serial says `POWER tap -> future use`
- [ ] PWR held 2 s switches off; from off, PWR held 2 s switches on — shorter does nothing
- [ ] On USB, off goes dark and PWR held 2 s brings it back
- [ ] **UP tap blanks the panel**; tap again and it comes back
- [ ] With the panel blank, a crit **still sounds**, right at every 5 s mark
- [ ] **UP double-tap** shows `SOUND OFF` and crosses the STATUS band's speaker icon (visible even unmuted); double-tap again brings sound back and clears the cross
- [ ] Muted, then power off and on → sound is back
- [ ] `crit`, `warn` and no connection flash **the whole screen** for 500 ms right at the start of every 5 s screen — every number readable during the flash
- [ ] `crit` plays the full alert with every flash; `warn` and no connection play the shorter, quieter notice instead — never both, and `info` stays silent
- [ ] Before any data has ever arrived, or on a non-touch SKU, UP held 2 s shows **no ring** and Serial says `future use` — the lock never engages
- [ ] Once real data is showing (touch SKU only), UP held 2 s locks it instantly, no code asked for: BODY and FOOTER are replaced by a lock icon, STATUS and ALERT keep drawing normally, and sound is unaffected
- [ ] UP held 2 s again, locked, raises a 6-digit keypad; the default code `123456` unlocks it and shows `UNLOCKED`
- [ ] A plain tap on the glass while locked also raises the same keypad — no hold needed; a double-tap still opens Settings instead, locked or not
- [ ] A wrong code clears the entry and shows `WRONG CODE` rather than unlocking
- [ ] Five wrong codes in a row lock the keypad out, with a wait-time message, until the 5-minute window ages out
- [ ] Restarting the board mid-lockout comes back still locked out, not reset — five wrong codes, then a restart, then a sixth still shows `TOO MANY ATTEMPTS`
- [ ] The correct code clears a partial lockout outright — a couple of wrong codes, then the right one, then five more wrong codes are needed again, not just three
- [ ] DOWN backs out of the keypad without submitting anything, still locked
- [ ] Restarting the board while unlocked comes back locked
- [ ] Changing the code and the auto-relock timeout on the setup page survives a restart
- [ ] ALERT band's word is bold and full-strength colour at every level; the band's own background matches the rest of the screen at rest and only goes solid during the whole-screen flash
- [ ] A short `alert.message` sits still beside the level word; a long one (near 20 chars) scrolls slowly, one character at a time, and never overlaps the word or runs past the screen edge
- [ ] A soft rule is visible between the ALERT band and the BODY at all times, not just during a flash
- [ ] `crit` plays its 500 ms sound with every flash, in step with it; `warn` and no connection stay silent
- [ ] The battery icon shows an explicit bolt while charging, not just a colour change; no percentage number is drawn beside it, and the fill only moves between 10 % steps
- [ ] Sending a metric with no `gateway`, no `aggregates`, or a null field logs the problem on Serial and still renders — never a crash
- [ ] A one-metric payload shows one full-width position segment and polls every 30 s
- [ ] A sixth `metrics` entry is ignored rather than crashing, and Serial says so
- [ ] Backlight brightness matches the setup page's two figures — dim on battery, bright on the charger — and switches within moments of plugging in or unplugging, no restart needed
- [ ] Factory reset on the setup page does nothing until the checkbox is ticked and **YES** is typed; once confirmed, the board restarts with no endpoint, no saved networks, and the lock code back to `123456`
- [ ] At 10 % battery and not charging, a `LOW BATTERY` toast and notice sound appear, and the board powers off on its own 2 minutes later unless it's plugged in first
- [ ] The STATUS band's clock still reads the configured zone (not UTC) a few seconds after joining Wi-Fi, not just right at boot
- [ ] Critical alert sound on battery with no brown-out reboot, and no rattle or distortion from the speaker
