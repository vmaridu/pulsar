# 🤖 Pulsar — working agreement

**This file is the single source of truth for every AI assistant on this repo.**
`CLAUDE.md` and `.cursor/rules/pulsar.mdc` are one-line pointers at it — never copy
rules into them, never let them drift. Edit *this* file and both tools follow.

## 🗺️ Where things live — orient here first

| Looking for…                          | Go to                                             |
| -------------------------------------- | -------------------------------------------------- |
| The wire contract (what a backend sends) | [docs/api.md](docs/api.md)                        |
| Behaviour shared by every build         | [docs/device.md](docs/device.md)                  |
| The square board's firmware + pixels    | [ws_lcd_154/](ws_lcd_154/) — flat folder, `.ino` files + `device.md` + `README.md` + `mockup.html` |
| The landscape board (mockup only so far)| [ws_lcd_349/](ws_lcd_349/) — same flat pattern    |
| A fake backend to point either build at | [simulator/](simulator/) — `node server.js`, zero deps |
| Rules for how to work in this repo (you are here) | This file — §1 onward |

If you're not sure which file owns a fact, §7 below says who does. Never guess and
duplicate it into a second file.

---

## 1. 🧠 What Pulsar is

A desk display that answers one question: **is my API gateway healthy?**

One device, one HTTP endpoint. The backend behind it may
itself aggregate many platforms, but the device neither knows nor cares — it asks one
URL and draws what comes back.

- 🚫 **No multi-gateway on the device.** No gateway list, no gateway cycling, no fleet
  dots. That was removed on purpose; do not reintroduce it
- 🏷️ **Every metric carries a `gateway` attribute**, so a row still says where it came
  from when the backend merged several platforms into one response
- 👁️ Read-only. No acknowledge, no writes

## 2. 🪧 The four bands — say these names

The display is four fixed horizontal bands. **Refer to them by these names** in code,
comments, docs and conversation; the user does.

| Name       | Owns                                                                         |
| ---------- | ---------------------------------------------------------------------------- |
| **STATUS** | Device internals only — battery, the metric's timestamp, Wi-Fi, poll hairline |
| **ALERT**  | `info` / `warning` / `critical` + its one-line message. Nothing else          |
| **BODY**   | The stats and the graph for the current metric, combined                     |
| **FOOTER** | Gateway name + metric label, and the position rule                           |

- 📍 Bands never move, never resize, never swap content. A band's job is fixed
- 🚨 **An alert flashes the whole screen**, not one band — every band goes to the level
  colour for 500 ms right at the start of every 5 s screen (synced to the cycle, not a
  free-running clock), and every foreground turns to near-black ink
- 🏷️ In code they are `BAND_STATUS_*`, `BAND_ALERT_*`, `BAND_BODY_*`, `BAND_FOOT_*`

## 3. 🎛️ Input — the standard map

**This is the whole input contract.** Every build implements exactly this. Anything not
listed is **future use** and must be documented as "future use", never silently absent.

| Input             | Tap                        | Double-tap              | Hold 2 s                 |
| ----------------- | -------------------------- | ----------------------- | ------------------------ |
| **Glass** (touch) | Next metric screen         | *future use*            | **Force refresh**        |
| **LEFT** (PLUS)   | **Settings screen** on/off | *future use*            | **Hotspot** (UI to come) |
| **POWER** (PWR)   | *future use*               | *future use*            | **Power off / on**       |
| **RIGHT** (BOOT)  | **Display on/off**         | **Sound mute / unmute** | *future use*             |

- ⏱️ **Every hold is 2 s**, keys and glass alike, and shows a filling ring with the
  action named inside so it can be abandoned
- 🔕 **Mute never survives a restart** — RAM only. (One exception: a brown-out reset
  starts muted, so a sagging supply cannot loop the sound)
- 🌑 **Display off is the panel only.** Polling, alerts and the speaker keep running —
  a critical still sounds with the screen dark
- 🔙 Off the main screen, a glass tap returns to it
- 📝 When you change this table, change it **here first**, then in the code and in every
  readme in the same commit

## 4. 🔁 Screen rotation and polling

- ⏱️ Each metric screen is shown for **5 s**, then the next one
- 🔄 When the loop returns to the first screen, **fetch fresh data**
- 🧮 Poll interval = `max(30, metric_count × 5)` seconds — never faster than 30 s
- 👆 A 2 s hold on the glass forces a refresh regardless of where the cycle is
- 🌐 There is **no network endpoint yet**. `net.ino` holds the placeholder and the
  embedded test payload; wiring the real `GET` replaces one function

## 5. 🪵 Logging — Serial is the debugger

This board has no screen for us to debug on, so **Serial out is the whole story**.

- 🔌 **115200 baud**, always. Print it in every readme next to the Serial instructions
- 📋 Log **every** one of these, one line each, with a category prefix:
  - every button press and touch, and the **action it caused**
  - every poll: the URL, the status, the parse result — success *and* every failure,
    with the actual error text
  - power, display, mute, view changes
  - boot: reset reason, device name, hardware probe results, payload summary
- 🏷️ Format: `[  1234ms] category: message` — category is one lowercase word
  (`key`, `touch`, `net`, `power`, `view`, `sound`, `boot`, `data`)
- 🤫 Never log in a tight render loop; log state *changes* and actions, not frames

## 6. 📁 Module layout — keep it modular

`ws_lcd_154/`, `ws_lcd_349/` — one flat folder per build, holding the firmware, that build's
own docs and its mockup together. The Arduino IDE opens every `.ino` / `.cpp` / `.h`
file in the folder as a tab and concatenates them, so globals live in the main
sketch and functions may be split; `.md` / `.html` files sit alongside them,
untouched by the compiler.

| File                | Owns                                                     |
| ------------------- | -------------------------------------------------------- |
| `<device>.ino`      | Pins, model, palette, logging, `setup()` / `loop()`      |
| `intro.ino`         | The welcome screen (boot intro)                          |
| `dashboard.ino`     | The four bands                                           |
| `input.ino`         | Keys and touch — taps, double-taps, holds                |
| `power.ino`         | Power latch, off/on, display on/off                      |
| `settings.ino`      | Settings screen and hotspot screen                       |
| `sound.ino`         | The alert sound and mute                                 |
| `net.ino`           | Poll scheduling, the fetch placeholder, the test payload |
| `es8311.*`          | Vendor codec driver — do not edit                        |
| `README.md`         | Flashing and troubleshooting                             |
| `device.md`         | That build's pixels and hardware                         |
| `mockup.html`       | The interactive mockup, standalone                       |

- ➕ New concern → new file. Do not grow the main sketch back into a monolith
- 📌 Globals go in `<device>.ino` only (concatenation order), functions anywhere
- 📁 One flat folder per build — `ws_lcd_154/`, `ws_lcd_349/` — main file always named after
  the folder itself (`ws_lcd_154.ino`, `ws_lcd_349.ino`) since Arduino requires it to match
  the containing folder's name. No extra subfolder repeating the device name, and
  no separate `src/`, `docs/` or `mockups/` subfolder either — one folder, everything
  about that build

## 7. ✍️ Docs

- 📄 **Markdown docs may link to each other.** Keep links live — a broken or stale link
  is a bug
- 🌐 **HTML mockups are standalone.** No hyperlinks out of them at all, ever
- 🎯 **To the point.** No duplicated tables across files — one owner per fact:
  - `docs/api.md` owns the wire contract
  - `docs/device.md` owns behaviour shared by all builds
  - `<build>/device.md` owns that build's pixels and hardware
  - `<build>/README.md` owns flashing and troubleshooting
- 🔁 The mockups must render **the same logic as the firmware** — same bands, same
  input map, same rotation timing, same alert behaviour
- ✂️ Removing a feature means removing it from code, readmes, and mockups in one pass

## 8. 🎨 Palette

Authored in hex, quantised to RGB565.

| Use        | Colour    | Use            | Colour    |
| ---------- | --------- | -------------- | --------- |
| Background | `#05070c` | 🟢 `info`       | `#5cf22e` |
| Text       | `#e6edf7` | 🟠 `warning`    | `#ff7a00` |
| Dim        | `#7f8fa8` | 🔴 `critical`   | `#ff2626` |
| Rule       | `#1c2740` | 🩶 fetch fault  | `#8aa0c0` |
| Accent     | `#22d3ee` |                |           |

- 🖤 On a level-coloured ground every foreground goes **near-black ink** — the only
  value that reads on green, orange, red *and* grey

## 9. ⛔ Standing rules

- 🚫 **No shake gesture, no secret knock.** Removed; never suggest it back
- 🚫 No `platform` field anywhere
- 🚫 Nothing derived on the wire — no error rate, no throughput, no `window_minutes`.
  **One bounded exception**: an aggregate tile's `secondary_value` → [§10](#10--aggregate-tile-formatting)
- 🚫 Never `fillScreen()` inside `loop()`
- 🚫 Never synthesise audio continuously on-chip — render once, stream
- ✅ Clients truncate; they never wrap and never scroll
- 📝 **When a feature is fully removed, remove its shadow too.** Don't leave sentences
  that justify the current design by contrasting it with what used to be there —
  "the URL is the gateway, no gateway id is sent" only makes sense to someone who
  remembers gateway ids existing. State the current design as if it always worked
  this way. The one exception is a genuine engineering lesson worth keeping (the
  boot-intro watchdog story, the 260→180 Hz speaker note) — those explain *why*,
  not apologise for *what came before*. When you remove something, sweep every
  doc, comment and mockup for lines that only made sense in contrast to it

## 10. 🔢 Aggregate tile formatting

`metrics[i].aggregates` is an array of **1–5** stat tiles, not a fixed object — one
tile per interesting number. Position 0 is the heading; the rest fill the four BODY
slots in order → [api.md §4](docs/api.md#-aggregates--tiles). There is no `total_count`
field anywhere, on a row or in a tile — it was removed on purpose; do not reintroduce
it. If something needs a scale to generate against (a pattern, a preview), derive it
from `sum(buckets)` locally, never store or send it.

- 🚫 **No separate `label`, anywhere — on a row or in a tile.** `name` IS the text
  drawn. A row's `name` is what the FOOTER shows; a tile's `name` is what the tile
  shows. One field, one job — a second field carrying the same information was
  confusing, not helpful. Never reintroduce a `label` field
- 📏 **Tile `name` ≤ 5 chars.** Pick something recognisable at a glance: `2XX`, `4XX`,
  `5XX`, `AVG`, `P95`, `ERR`, `FAIL`. A backend that sends more is choosing what the
  client cuts — truncated, never wrapped. Row `name` gets **16** chars, one FOOTER line
- 🔢 **Numbers over 5 digits get compacted, always client-side**: `18659` draws as-is,
  `184000` draws as `184K` (then `M`, `B`). Never sent pre-compacted — a backend does
  not know how much room the client has — and the compacted suffix takes the same
  slot a unit would
- 📐 **Units are exactly `ms` / `s` / `%` / absent.** `ms` past 9999 rescales to `s`
  for display — a client concern; the wire value stays milliseconds either way
- 💯 **A `%` value never carries more than 2 decimal places** — on the wire and in
  whatever computed it. `18659 / 18783 * 100` is `99.33982856838631`; round it to
  `99.34` before it goes in `secondary_value`, don't just truncate the display.
  Everywhere a percentage is generated (a real backend, the mockups' `pct()`, the
  simulator's `pct()`) rounds at the source — `Math.round(x * 100) / 100`, not a
  display-only `toFixed()` papering over a longer stored number
- 🎯 **Two "max 5"s, not one.** `metrics` caps at 5 rows (5 **screens**); each row's
  `aggregates` caps at 5 (5 **tiles**, but as few as 1 is valid). Never conflate them
  in docs, comments or variable names — say "screens" and "tiles", never a bare "5"
- 🔁 **A tile's `secondary_value` may be derived** — the one exception to §9's
  "nothing derived on the wire." A share of some meaningful whole, a second raw
  number beside the first, whatever that tile needs. Nowhere else on the wire is
  computed

## 11. 🛡️ Render defensively — always

A row can carry 1–5 tiles, a tile can omit its unit or its secondary, a fault can
send `aggregates: null` or drop a required field entirely → [api.md §6](docs/api.md#6--errors).
Every renderer — firmware, both mockups, the simulator's control UI — must assume
any of that can happen on any given poll, not just on a fault test.

- ✅ **Check presence before drawing, every time.** `if (tile) draw(tile)`, never
  `draw(tile)` and hope. A missing tile, a missing unit, a missing secondary is not
  an error to surface — it is a slot that simply stays blank
- 🚫 **Never index or dereference without a bounds/null check first** — `r.tiles[i]`
  only after `i < r.ntiles`, `t.name` only after confirming `t` exists, array
  `.find()` results checked before use. A null pointer, an out-of-range index or a
  `.property` on `undefined` must never be reachable, in C++ or JS alike
- 🖥️ **Fewer than 5 tiles is not a degraded case, it's a normal one.** The device's
  fixed slots just show what they're given and leave the rest blank — no
  placeholder text, no reflow, no crash
- 🧪 **When you add a fault or a schema field, add the missing-data case for it too**
  — the simulator's `sparse-aggregates` fault (1 tile on a row) exists exactly to
  keep this honest, not just `null-aggregates` and `empty-metrics`
