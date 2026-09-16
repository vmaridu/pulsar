# 🤖 Pulsar — working agreement

**This file is the single source of truth for every AI assistant on this repo.**
`CLAUDE.md` and `.cursor/rules/pulsar.mdc` are one-line pointers at it — never copy
rules into them, never let them drift. Edit *this* file and both tools follow.

## 🗺️ Where things live — orient here first

| Looking for…                          | Go to                                             |
| -------------------------------------- | -------------------------------------------------- |
| What every build must do, device-agnostic | [docs/product-requirements.md](docs/product-requirements.md) |
| The wire contract (what a backend sends) | [docs/api.md](docs/api.md)                        |
| Behaviour shared by every build         | [docs/functional-requirements.md](docs/functional-requirements.md) |
| The square board's firmware + pixels    | [ws_lcd_154/](ws_lcd_154/) — flat folder, `.ino` files + `device.md` + `README.md` + `mockup.html` |
| The landscape board's firmware + pixels | [ws_lcd_349/](ws_lcd_349/) — same flat pattern    |
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
| **ALERT**  | `info` / `warn` / `crit` + its one-line message. Nothing else                 |
| **BODY**   | The stats and the graph for the current metric, combined                     |
| **FOOTER** | Gateway name + metric label, and the position rule                           |

- 📍 Bands never move, never resize, never swap content. A band's job is fixed
- 🚨 **An alert flashes the whole screen**, not one band — every band goes to the level
  colour for 500 ms right at the start of every 5 s screen (synced to the cycle, not a
  free-running clock), and every foreground turns to near-black ink
- 🏷️ In code they are `BAND_STATUS_*`, `BAND_ALERT_*`, `BAND_BODY_*`, `BAND_FOOT_*`

## 3. 🎛️ Input — requested actions, not gestures

**Every build must give a person a way to request each of these.** Nothing else is
prescribed here: which physical control does it — a key, the touch glass, or a mix
— and whether that's a tap, a double-tap or a hold, is each device's own fact, never
generalized across builds. The two boards don't share a button layout, and pinning
one shared "standard map" onto both used to be exactly what this section did —
don't reintroduce that mistake.

- Go to the next metric screen
- Toggle the display on/off
- Mute / unmute sound
- Toggle the settings screen
- Turn the setup hotspot on/off
- Force an immediate poll
- Lock the screen
- Unlock — enter the lock code
- Power the device off, and back on

- 📖 The full behavioural contract for each action — what it must do, never how
  it's triggered — is owned by [product-requirements.md §4](docs/product-requirements.md#4-interaction-requirements) onward
- 🖥️ Which control performs which action, and its exact gesture, is owned by that
  board's own `<device>/device.md` — never restated here, in `docs/`, or in the
  root README
- 📝 A control with nothing assigned to it must still be documented as "future
  use" on that device, never silently absent
- 📝 Adding, removing or renaming an action here means updating every device's
  own gesture map in the same commit

## 4. 🔁 Screen rotation and polling

- ⏱️ Each metric screen is shown for **5 s**, then the next one
- 🔄 When the loop returns to the first screen, **fetch fresh data**
- 🧮 Poll interval = `max(30, metric_count × 5)` seconds — never faster than 30 s
- 👆 A 2 s hold forces a refresh regardless of where the cycle is
- 🌐 **Nothing about the endpoint is compiled in.** The full URL, the API key and the
  API secret come out of NVS, set over the setup hotspot → [functional-requirements.md §5](docs/functional-requirements.md#5--configuration).
  A board with no URL shows `SETUP` and **no numbers at all** — there is no sample
  payload to fall back on, because a monitor showing invented data is worse than one
  admitting it has none
- 📶 **Failing to reach the backend is a normal state, not a crash.** No saved network in
  range, a captive portal in the way, a 401 — each gets its own banner over the last good
  numbers, and the device keeps trying → [api.md §6](docs/api.md#6--errors)

## 5. 🪵 Logging — Serial is the debugger

This board has no screen for us to debug on, so **Serial out is the whole story**.

- 🔌 **115200 baud**, always. Print it in every readme next to the Serial instructions
- 📋 Log **every** one of these, one line each, with a category prefix:
  - every button press and touch, and the **action it caused**
  - every shake gesture counted, and whether it reached three and forced a poll
  - every poll: the URL, the status, the parse result — success *and* every failure,
    with the actual error text
  - power, display, mute, view changes
  - boot: reset reason, device name, hardware probe results, payload summary
- 🏷️ Format: `[  1234ms] category: message` — category is one lowercase word
  (`key`, `touch`, `imu`, `net`, `wifi`, `cfg`, `power`, `view`, `sound`, `boot`, `data`).
  `wifi` is the radio — scanning, joining, portals; `net` is the poll on top of it;
  `cfg` is the stored configuration being read or written; `imu` is the motion sensor —
  shake detection, nothing else lives there yet
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
| `imu.ino`           | The motion sensor — shake detection                      |
| `power.ino`         | Power latch, off/on, display on/off                      |
| `config.ino`        | The stored endpoint and saved networks (NVS)             |
| `wifi.ino`          | Joining saved networks — priority, enterprise, portals   |
| `hotspot.ino`       | The setup hotspot, the page it serves, its screen        |
| `settings.ino`      | The settings screen                                      |
| `sound.ino`         | The alert sound and mute                                 |
| `lock.ino`          | The privacy lock — code, keypad, auto-relock             |
| `net.ino`           | Poll scheduling and the HTTPS GET                        |
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
  - `docs/product-requirements.md` owns device-agnostic product requirements — what, never how
  - `docs/api.md` owns the wire contract
  - `docs/functional-requirements.md` owns behaviour shared by all builds
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
| Text       | `#e6edf7` | 🟠 `warn`       | `#ff7a00` |
| Dim        | `#7f8fa8` | 🔴 `crit`       | `#ff2626` |
| Rule       | `#1c2740` | 🩶 fetch fault  | `#8aa0c0` |
| Accent     | `#22d3ee` |                |           |

- 🖤 On a level-coloured ground every foreground goes **near-black ink** — the only
  value that reads on green, orange, red *and* grey

## 9. ⛔ Standing rules

- 🚫 No `platform` field anywhere
- 🚫 Nothing derived on the wire — no error rate, no throughput, no `window_minutes`
- 🚫 Never `fillScreen()` inside `loop()`
- 🚫 Never synthesise audio continuously on-chip — render once, stream
- ✅ Clients truncate; they never wrap. (A label that scrolls instead of truncating is a rare, deliberate exception — that build's own `device.md` owns the fact of which one and why)
- 🔑 **A stored secret never leaves the device.** The setup page is told *that* a password
  exists, never what it is — it comes back as `""` with a `…Set` flag beside it, and an
  empty field posted back means "keep the one you have". Anyone within radio range of the
  hotspot can open that page; none of them may read what is already on the board
- 🛟 **Every stored setting has a compiled-in default that is in force whenever nothing is
  stored** — first boot, and straight after a factory reset. Only the endpoint and the saved
  networks may legitimately be empty, and both have an honest banner for it. A board must
  never come up holding a zero nobody chose: a backlight at 0 % is a dark glass, a timeout
  of 0 is "never", a time-zone index of 0 is the wrong clock. Set the defaults first,
  unconditionally, then overlay whatever storage actually holds
- 🧪 **No sample payload ships in the firmware.** Point a board at `simulator/` to get data
  without a backend. A fallback that renders made-up numbers when the real poll fails is
  the one bug in a monitor that costs more than a blank screen
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
  `99.34` before a tile's `value` carries it, don't just truncate the display.
  Everywhere a percentage is generated (a real backend, the mockups' `pct()`, the
  simulator's `pct()`) rounds at the source — `Math.round(x * 100) / 100`, not a
  display-only `toFixed()` papering over a longer stored number
- 🎯 **Two "max 5"s, not one.** `metrics` caps at 5 rows (5 **screens**); each row's
  `aggregates` caps at 5 (5 **tiles**, but as few as 1 is valid). Never conflate them
  in docs, comments or variable names — say "screens" and "tiles", never a bare "5"
- 🚫 **No second value on a tile, ever.** A tile is `name` + `value` + optional `unit`
  — one number, one job. A tile that needs a second number is a second tile, never
  a second field bolted onto the first
- 🚦 **`level` colours a tile: `info` (default) · `warn` orange · `crit` red**
  — the same three words as `alert.level`, but scoped to one tile, not the whole
  screen. It never sounds or flashes anything; it only changes that tile's colour.
  The device never computes it — no baked-in "`4XX` past 2% is orange" any more.
  A backend that wants a tile to read as trouble sends `"level": "warn"` (or
  `"crit"`) itself → [api.md §4](docs/api.md#-level--a-tiles-own-severity)

## 11. 🛡️ Render defensively — always

A row can carry 1–5 tiles, a tile can omit its unit or its level, a fault can
send `aggregates: null` or drop a required field entirely → [api.md §6](docs/api.md#6--errors).
Every renderer — firmware, both mockups, the simulator's control UI — must assume
any of that can happen on any given poll, not just on a fault test.

- ✅ **Check presence before drawing, every time.** `if (tile) draw(tile)`, never
  `draw(tile)` and hope. A missing tile or a missing unit is not an error to
  surface — it is a slot that simply stays blank
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
