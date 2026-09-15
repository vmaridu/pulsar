# 🟦 The 640 × 172 wide build

Waveshare **ESP32-S3-Touch-LCD-3.49**. The panel is a 172 × 640 strip; Pulsar lays it on its long edge and draws it **640 wide, 172 tall** — the way it sits on a desk, keys at the top right. One metric screen at a time, turning on its own every 5 s.

🔧 Shared behaviour → **[functional-requirements.md](../docs/functional-requirements.md)** · 📡 Contract → **[api.md](../docs/api.md)** · 🖥️ Other build → **[ws_lcd_154](../ws_lcd_154/device.md)**
🔌 Flashing it → **[README.md](README.md)** · 🎨 Live mockup → **[mockup.html](mockup.html)**

---

## 1. 🔩 Hardware

Waveshare wide kit, SKU **32373** (Case A, 18650 cell); `-EN` variants ship without a cell, `B` variants with a LiPo in the smaller case.

| Part    | Spec                                                          |
| ------- | ------------------------------------------------------------- |
| MCU     | ESP32-S3R8, dual LX7, 240 MHz                                 |
| Memory  | 16 MB flash, 8 MB PSRAM                                       |
| Radio   | Wi-Fi 4, BLE 5, native USB                                    |
| Display | 3.49" IPS, **172 × 640**, AXS15231B over QSPI, 350 nits       |
| Touch   | Capacitive, inside the same AXS15231B, over I²C — every SKU   |
| Audio   | ES8311 codec + speaker, ES7210 + dual mics (mics unused)       |
| Expander| TCA9554 — holds the power latch, backlight enable, panel reset |
| IMU     | QMI8658 6-axis — unused                                       |
| RTC     | PCF85063 — unused so far                                      |
| Storage | TF (microSD) — unused                                         |
| Battery | 18650 (Case A) or 3.7 V LiPo (Case B), optional               |
| Keys    | **LEFT** (power circuit — a press resets the chip) · **RIGHT** (programmable) · **RESET** |

⚠️ **Things that bite**

- 🏷️ **Two revisions.** Boards shipped from June 2026 are V2 (`Rev1.1` on the silkscreen). V1 wires the backlight and the panel reset differently — `BOARD_REV` at the top of [`ws_lcd_349.ino`](ws_lcd_349.ino) picks, and a wrong pick is a dark screen
- 🔌 **The power latch is on the expander**, not a GPIO. The firmware raises it first thing at boot; on the cell a board that never did would die the moment LEFT is let go
- 🔆 **The backlight PWM is active low** — duty 0 is full brightness — and gated by an enable line on the expander. Both are handled in `power.ino`
- 🐌 A full 640 × 172 frame is 220 KB over QSPI at 40 MHz — about 11 ms a push, comfortable at ~25 fps, but never draw straight to the panel inside `loop()`

📌 **Exact pin assignments, I²C addresses and the expander's line map** live in the `#define`s at the top of [`ws_lcd_349.ino`](ws_lcd_349.ino) — the source is the one copy of this fact, taken from Waveshare's own demos, not restated here.

### 🔄 Landscape

The panel's native frame is portrait, 172 wide. The sketch draws on a 640 × 172 canvas and the canvas lays every pixel down turned a quarter — `PANEL_ROTATION` in `ws_lcd_349.ino` is `1` or `3`, the two ways a strip can lie flat. If the picture comes up upside-down on your desk, use the other one; touch reads through the same number and follows.

### 🔋 Where the live values come from

Everything except the battery comes from the payload. The battery is read off the ADC pin behind a 1/3 divider; percent is interpolated between five voltage points (< 3.52 V → 1 %, then 3.64 / 3.76 / 3.88 / 4.00 V → 20 / 40 / 60 / 80 %, ≥ 4.00 V → 100 %). **Nothing on this board tells the firmware whether the charger is plugged in**, so the STATUS corner shows the percentage alone and the bolt icon stays absent until a way turns up.

🔆 **Backlight brightness:** the setup page's two figures are stored, but with no charger detection the *while charging* figure (default 90 %) is what this board applies, always; the on-battery figure waits for a revision that can tell.

⏰ **The STATUS clock is the data's, not the wall's** — `measured_at`, formatted `MM/DD hh:mm AM`, in the zone picked on the setup page (default US Eastern), daylight saving applied automatically.

📶 **The STATUS Wi-Fi mark is a fan** — a dot and three arcs lit by signal; the whole fan dim with an orange slash through it while there is no link, and an orange dot while a sign-in page stands in the way.

---

## 2. 👆 Input

Two keys and the glass. The keys are named by where they sit on the case, **LEFT** and **RIGHT**; the third, between them, is RESET on the chip's reset line. **LEFT is on the board's power circuit, and on this hardware a press of it pulses the rail — the chip resets before any firmware sees the key.** So LEFT is power and nothing else. This board has no separate key for settings or the setup hotspot — those live on the glass instead: on the dashboard's **left part** (STATUS / ALERT — on screen whenever the dashboard is, locked or not), a tap opens settings directly and a 2 s hold raises the setup hotspot. The other two parts carry next-screen and the refresh.

No gesture on this board waits to see if a second tap follows. A double-tap was tried for settings and for sound mute, and on both the keys and the glass it read as unreliable more than deliberate — a real tap sometimes sat waiting for a second one that never came, and this hardware's own touch chatter made a stray "second tap" too easy to manufacture by accident. Every tap fires the instant the finger or the key lifts; sound's mute toggle moved off a RIGHT double-tap onto a tappable icon on the settings screen itself → [below](#-settings-screen).

| Input                        | Tap                    | Hold 2 s                                   |
| ---------------------------- | ---------------------- | ------------------------------------------ |
| **Glass** · left part        | **Settings** on / off  | **Setup hotspot** on / off                 |
| **Glass** · right two parts  | Next metric screen     | **Force refresh**                          |
| **Glass** · off the main screen | Back                 | In the hotspot: leave it                   |
| **Glass** · the keypad       | A digit, or the readout to back out | _nothing_ — a keypad only takes taps |
| **RIGHT** key                | **Display on / off**   | **Lock**, or — already locked — **open the unlock keypad** |
| **LEFT** key                 | _future use_ — a press resets the chip | **Power off** · from off: on |

- ⏱️ **Every hold is 2 s**, and shows a ring filling toward it with the action inside — `SETUP`, `EXIT`, `REFRESH`, `LOCK`, `UNLOCK`, `OFF`, `ON`
- 👆 **One narrow exception to "no gesture waits":** a second tap on the left part landing within 400 ms of the one that just opened settings, at the same spot, is a double-tap — recognized and reserved as future use, so it can't be misread as a fresh tap on whatever settings happens to be showing there (the sound icon, today) the instant it opens
- 👆 **Parts, not zones, and only on the dashboard.** The left part is `x < 213` on the 640-wide picture, exactly where part 1 draws; the right two parts are the rest. A tap that slides is not a tap; on the right two parts a tap gives the chosen screen a fresh 5 s, on the left part a tap opens settings instead; with the display off a tap does nothing — RIGHT wakes it. Off the dashboard the parts mean nothing: a tap is back, a hold in the hotspot is exit
- 🌑 **RIGHT tap sleeps the panel only.** The cycle keeps turning and a crit still sounds in the dark
- 🔕 **Sound mute/unmute lives on the settings screen now**, not a RIGHT double-tap → [below](#-settings-screen). The crossed speaker sits in STATUS while muted, same as always
- 🔒 **RIGHT held 2 s** hides parts 2 and 3 behind a padlock — no code to lock; held again, it raises the keypad over the whole screen. The lock arms once real data has shown and the glass answered at boot; before that the hold is future use and draws no ring. It boots locked, and re-locks on its own after the setup page's timeout → [functional-requirements.md §6](../docs/functional-requirements.md#6--privacy-lock)
- 🔢 **The keypad is seven equal columns** across the 640 px (`lockColX(i)` in `lock.ino`, the same split as the mockup): columns 0-1 hold the readout — `ENTER CODE`, six dots, `TAP HERE - BACK` — and columns 2-6 each split top / bottom into a digit, `1 2 3 4 5` across the top row, `6 7 8 9 0` across the bottom. Ten cells of 85 × 86 px, every one typeable. The digit just pressed shows in its dot for 550 ms, then masks. While the keypad is up the glass is a keypad and nothing else — no settings, no hotspot hold, no refresh; the ring never draws there
- 🔌 **Off on USB is deep sleep** with the panel asleep and its backlight off; LEFT wakes it. The power latch stays up through the sleep on purpose — dropping it takes the display's own rail down with it, and the panel did not come back from that cleanly
- 🔌 **Off and on are both a 2-second hold of LEFT.** The key powers the board up by itself; the firmware keeps the latch only once the hold reaches 2 s. Off drops the latch — on the cell that is truly off; on USB it is deep sleep, and LEFT wakes it into the same hold
- 🪵 Every press and touch prints on Serial with its GPIO, and every touch-down prints the raw controller pair beside the screen point — that pair is what a calibration fix needs, should one ever be

### 🔍 Settings screen

A tap on the left part opens it; a tap anywhere outside CONTROLS closes it again. Same three-part split as the dashboard itself — part 1 is the only one that answers a tap.

| Part | Owns                                                                          |
| ---- | ------------------------------------------------------------------------------ |
| 1    | **CONTROLS** — sound on/off and lock/unlock, each its own icon, stacked        |
| 2    | **DEVICE / WI-FI** — battery, name, MAC, the network joined, signal and IP     |
| 3    | **BACKEND / DATA** — the host, auth mode, every gateway named, when it last measured and polled |

- 🔈 **Tap the speaker icon to mute or unmute** — the same toggle a RIGHT double-tap used to be, moved here because double-taps on this hardware read as unreliable more than deliberate
- 🔒 **Tap the padlock icon to lock or unlock.** Locking needs no code, the same as a RIGHT hold; unlocking opens the real 6-digit keypad, never a shortcut around it. Future use until the lock has armed — real data shown, touch answered at boot, same condition as RIGHT's own hold → [functional-requirements.md §6](../docs/functional-requirements.md#6--privacy-lock)
- 📊 **Parts 2 and 3 are read-only**, packed two rows tighter than before — 17 px apart, label and value on one line — now that part 1 carries controls instead of device info
- 🔙 **A tap outside CONTROLS goes back** to the dashboard, the same as any other secondary view

### 🔄 One cycle

```
Orders · Created → Dispatched → Cancelled
  → Payments · Paid → Declined → back to Created
```

- ⏱️ **Each screen is up for 5 s** and then moves on by itself
- 👆 A tap on the right two parts steps on early and gives the chosen screen a fresh 5 s; a tap on the left part opens settings instead, and doesn't advance the cycle
- 🔄 **The wrap is the poll.** When it returns to the first screen it refetches, but never faster than `max(30, metrics × 5)` seconds → [functional-requirements.md §1](../docs/functional-requirements.md#1--screens)

---

## 3. 🖥️ Screen

**Four bands, folded into three parts** across the width — each 213 px wide, soft fades between them, nothing boxed off. Nothing moves between them. See it drawn, live → **[mockup.html](mockup.html)**.

| Part | x         | Band(s)         | Content                                                                                           |
| ---- | --------- | --------------- | ------------------------------------------------------------------------------------------------- |
| 1    | 0 – 212   | STATUS · ALERT  | A 26 px strip on top: battery + percentage left, the Wi-Fi fan right (a slash through it with no link, an orange dot while a portal is in the way), mute icon beside it. Beneath: the level word at size 3, its message bold, the reading's clock at the foot — all centred |
| 2    | 213 – 426 | BODY            | The graph, with the heading tile over it: value size 3 left with its unit, name size 2 and the span (`LAST 30M`) right |
| 3    | 427 – 639 | BODY · FOOTER   | Gateway (first five letters, dim) left and metric name right across the top; four full-width rows beneath, tile name left at size 1, value size 3 right with its unit; the position rule along the bottom |

- 🏷️ **The FOOTER's job lives in part 3** — the gateway and metric name are its header, the position rule its floor. The band never moves; it just shares a column
- 📐 **Fewer than 5 tiles leaves rows blank** — the fixed positions still exist, nothing reflows
- 🔤 **The metric name draws at size 2 while it clears the gateway beside it** (up to 10 characters), else size 1. **The alert message has its own face** — the message size below, bold, 16 characters to a line; a longer one breaks at a space onto a second line
- 📈 **The graph is part 2's ground**, one count per bucket, no headroom; it sweeps in left → right over ~900 ms on every new screen
- 🔢 The numbers count up from the previous screen's over ~520 ms
- 🎨 **A tile's colour is its own `level`** — `crit` red, `warn` orange, `info` the theme's own; the heading's span line takes the heading's colour
- 🔢 Number and unit formatting → [AGENTS.md §10](../AGENTS.md#10--aggregate-tile-formatting)

### 🚨 The ALERT part, and the whole-screen flash

- 💡 **Colour lives in the level word alone at rest** — bold, full strength, size 3. The parts share the same black ground
- 🚨 **The flash is the entire screen**: under `warn`, `crit` or a fetch fault all three parts go to the level colour for the first 500 ms of every 5 s screen, and every foreground turns to near-black ink → [functional-requirements.md §1](../docs/functional-requirements.md#-the-alert-is-the-whole-screen)
- 🔤 Levels drawn abbreviated — `INFO` / `WARN` / `CRIT` — the same short form `alert.level` carries on the wire. `CRIT` at size 3 is 64 px, comfortably inside the 213 px part

### 🔠 Text budget

One font, **ProFont** — a monospaced face drawn for terminals, crisp at small sizes on a pixel grid — in four sizes, shipped in `fonts.h` in Adafruit GFX format and picked by `txt(…, size)` the same way the square build picks `setTextSize(n)`. Every glyph is the same width, so the counts below are exact. Bold is the same glyph printed twice, one pixel apart. Never wraps, never scrolls.

| Size     | Face      | Advance × cap | Chars across one 213 px part | Used for                                         |
| -------- | --------- | ------------- | ---------------------------- | ------------------------------------------------ |
| 1        | ProFont12 | 6 × 8         | **35**                       | STATUS percentage, tile names, units, the span, every hint line |
| message  | ProFont22 | 12 × 14       | **16**                       | the alert message under the level word, one line or two |
| 2        | ProFont17 | 9 × 11        | **23**                       | the clock, the metric name, `LOCKED`, the ring's word |
| 3        | ProFont29 | 16 × 19       | **13**                       | the level word, every tile value, the keypad's digits |
| 8        | built-in  | 48 × 64       | —                            | `PULSAR` in the boot intro, 288 px across the middle — the only place the classic 6 × 8 cell font still draws |

---

## 4. 🌠 Boot intro

The shared intro → [functional-requirements.md §8](../docs/functional-requirements.md#8--boot-intro), with the wide panel's own numbers: the pulsar sits at the centre and its two beams reach 300 px — the full width — running off the short edges at the steep angles, which reads as racing past, not clipping. `PULSAR` draws at size 8, 288 px across the middle.

---

## 5. 🔔 Sound

The same three sounds as the square build — the intro's torpedo fire, the full alert on `crit`, the quieter notice on `warn` and any fault — rendered once at boot and streamed through this board's ES8311. No amplifier-enable pin here; the audio path is switched on through the expander at boot and stays on. Codec volume 74 / 100. No codec answering on I²C → silent, everything else works. Mute, and its timeout, arrive with the input map.

---

## 6. ☑️ On-device checklist

**Serial Monitor at 115200** for all of it — every step below prints what it did.

- [ ] First line of every boot: `boot: reset: power on` (or the real cause)
- [ ] `boot: expander: TCA9554 ok - power latched` — and never an `expander MISMATCH` line, which means a latch, backlight or reset line did not take
- [ ] `boot: panel ok - 172x640 as 640x172 landscape (rotation 1), QSPI 40 MHz mode 0, frame in PSRAM, one push ~11 ms, backlight 80%` — one line; a failed step prints its own `panel: … FAILED` line instead
- [ ] `touch: AXS15231B ok`, `audio: ES8311 ok`
- [ ] Boot intro plays once for 5 s — 3 s pulsar with torpedo bursts on each beam pass, 2 s the drifting-gradient `PULSAR` — then cross-fades into the dashboard
- [ ] The picture is landscape, right way up on the desk with the keys top right — else flip `PANEL_ROTATION`
- [ ] Three parts, black ground, soft fades between them, no hard lines
- [ ] No stray dots: the top line of the picture is clean end to end. The glass mangles the last pixel of every write; each strip is sent with its first two pixels repeated after it, and the last strip is sent twice — `panelFlush()` in `ws_lcd_349.ino`
- [ ] Part 1: battery percentage top left, the Wi-Fi fan top right, `INFO` in lime at size 3, `error budget healthy` bold and readable under it, the clock at the foot
- [ ] Part 2: `737k` `2XX` with `LAST 30M` over the graph on the first screen; the graph sweeps in
- [ ] Part 3: `ORDER` left and `Created` right across the top; `4XX 184k` orange, `5XX 205k` red, `AVG 42 ms`, `P95 180 ms`; five position segments, the first lit
- [ ] Screens turn on their own every 5 s through all five and wrap; `view: auto ->` prints each one
- [ ] A tap on the right two parts steps on early and the chosen screen gets a fresh 5 s; a slide prints `slide - ignored`; the touch-down line shows a screen point where you actually pressed
- [ ] A tap on the left part opens settings and closes it again; a 2 s hold there shows the `SETUP` ring and raises the hotspot; a hold on the right two parts shows `REFRESH` and polls
- [ ] A second tap on the left part landing right after settings opens (a double-tap) prints `double-tap on the left part -> future use` once and does nothing else — it doesn't fall through to the sound or lock icon underneath it
- [ ] RIGHT tap blanks the panel and brings it back
- [ ] Settings' left part (CONTROLS) shows the sound icon and the padlock icon, stacked; tapping the speaker toggles `SOUND OFF` / `SOUND ON` and the crossed speaker in STATUS follows; tapping the padlock locks instantly (no code) or, already locked, opens the real keypad — never a shortcut around it
- [ ] Once data has shown, RIGHT held 2 s shows the `LOCK` ring and parts 2 and 3 become the padlock with `HOLD RIGHT 2 S, ENTER CODE`; part 1 keeps drawing, and the left part still answers a hold
- [ ] RIGHT held 2 s again raises the keypad: `1 2 3 4 5` across the top, `6 7 8 9 0` across the bottom, six hollow dots on the left. Each digit shows in its dot for a moment, then masks; `lock: entry now 1/6 digits` prints, never the digit. `123456` unlocks (unless the setup page set another); a wrong code clears the dots and prints `lock: attempt rejected`; a tap on the readout closes the keypad; a hold on the keypad draws no ring
- [ ] A restart boots locked — the padlock is back before any number shows
- [ ] LEFT held 2 s shows the `OFF` ring, then `OFF`, then the board goes dark; from off, LEFT held 2 s brings it back through the `ON` ring — shorter does nothing
- [ ] On USB, off goes dark and LEFT held 2 s brings it back
- [ ] The intro's sound is audible and stops the instant the `PULSAR` screen starts
- [ ] No number wider than 5 characters anywhere
