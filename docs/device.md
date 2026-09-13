# 🔧 How every build behaves

Everything that does not change with the display. Pixel-dependent things live with their build.

📡 Contract → **[api.md](api.md)** · 🖥️ Builds → **[ws_lcd_154](../ws_lcd_154/device.md)** · **[ws_lcd_349](../ws_lcd_349/device.md)**

---

## 1. 🔁 The cycle

**One device, one endpoint.** The backend behind it may merge several platforms; every metric row carries the `gateway` it came from → [api.md §4](api.md#4--metrics).

| Step                      | Rule                                                 |
| ------------------------- | ---------------------------------------------------- |
| 🖥️ Each metric screen      | shown for **5 s**, then the next                      |
| 🔄 When the cycle wraps    | fetch fresh data                                     |
| 🧮 But never faster than   | `max(30, metric_count × 5)` seconds                   |
| 👆 A 2 s hold on the glass | forces a poll now, wherever the cycle is             |

- 🛑 **30 s is the floor.** One metric is a 5-second lap, and nothing gives you the right to ask a backend twelve times a minute — **the monitor must never become the incident**
- 🧊 A failed poll never clears the screen: last good payload stays, marked held → [api.md §6](api.md#6--errors)
- 🌑 **The screen going dark changes nothing.** The cycle turns, the poll polls, the speaker sounds

---

## 2. 🎛️ Input — the standard map

Every build implements exactly this. **Anything not in this table is future use**, and future use is something a build says out loud on its serial port — never a key that silently does nothing.

| Input             | Tap                        | Double-tap              | Hold 2 s                 |
| ----------------- | -------------------------- | ----------------------- | ------------------------ |
| **Glass** (touch) | Next metric screen         | _future use_            | **Force refresh**        |
| **LEFT** (PLUS)   | **Settings screen** on/off | _future use_            | **Hotspot** (UI to come) |
| **POWER** (PWR)   | _future use_               | _future use_            | **Power off / on**       |
| **RIGHT** (BOOT)  | **Display on/off**         | **Sound mute / unmute** | _future use_             |

- ⏱️ **Every hold is 2 s**, keys and glass alike, and shows a ring filling toward the mark with the action named inside — so you see it coming and can let go
- 👆 **One glass, no zones.** Anywhere counts. A tap that travels is a slide, and a slide does nothing
- 🔙 Off the main screen, a tap on the glass goes back to it
- ⌛ A key with a double-tap action waits ~450 ms before firing its single tap, so one gesture never fires both. A key without one fires immediately

### 🌑 Display off is not power off

A **RIGHT tap** sleeps the panel and kills the backlight. Nothing else stops — the cycle turns, the poll polls, and a `critical` still sounds in the dark. That is the whole point of having a speaker.

- 🔌 **Power off is a 2 s hold of POWER**, and that really is off: the latch drops and a battery-powered board is gone
- 🔋 Automatic screen-off on battery is **future use**. Today the screen is on until you tap RIGHT

### 🔕 Mute

**Double-tap RIGHT.** One gesture silences every sound; the same gesture brings it back.

- 🔄 **Mute never survives a restart.** It is held in RAM only, so a board switched off and on — or rebooted — always comes back with sound
- ⚡ One exception: a reset caused by a **brown-out** starts muted, so a sagging supply cannot be looped by the sound that sagged it
- 👁️ While muted, a small crossed-speaker icon sits in the STATUS band

---

## 3. 🪧 The four bands

Every build lays the screen out as four bands with these names. **Use the names** — in code, in docs, in conversation.

| Band       | Owns                                                                         |
| ---------- | ---------------------------------------------------------------------------- |
| **STATUS** | Device internals only — battery, the reading's timestamp, Wi-Fi, poll hairline |
| **ALERT**  | `info` / `warning` / `critical` and its one-line message. Nothing else        |
| **BODY**   | The stats and the graph for the current metric, combined                     |
| **FOOTER** | Gateway name + metric name, and the position rule                            |

- 📍 A band never moves, never resizes, and never borrows another band's job
- 🖥️ Each build says how many pixels each band gets → [ws_lcd_154](../ws_lcd_154/device.md) · [ws_lcd_349](../ws_lcd_349/device.md)

### 🚨 The alert is the whole screen

`warning`, `critical` and a fetch fault flash **every band** the level colour for **500 ms**, right at the start of every 5 s screen — the device flashes, not a stripe of it, and never on a clock of its own.

- 🖋️ **During the flash every foreground turns to near-black ink** — numbers, labels, hairlines, the graph. No one colour reads on black, red, orange and grey alike (white on orange is under 3:1), so the ink follows the ground
- 🎯 **The verdict belongs to the response, not to the metric on screen.** Tap through if you want; the alert follows you
- 🟢 `info` never flashes. 🔴 `critical` alone adds a sound

---

## 4. 🪵 Logging

The board has no screen for you to debug on, so **the serial port is the whole story**.

| Setting | Value      |
| ------- | ---------- |
| Baud    | **115200** |
| Data    | 8-N-1      |
| Format  | `[  1234ms] category: message` |

Categories: `boot` · `key` · `touch` · `view` · `net` · `data` · `power` · `sound`

**Everything below is logged, one line each:**

- 🔘 **Every button press and every touch** — and the action it caused, including the ones that are future use
- 🌐 **Every poll** — the URL, the outcome, and the real error text on failure. Success and failure alike
- 📦 **Every payload** — the level, each metric row with its counts and its own `gateway`, and every contract violation found (`sum(buckets)` mismatches, a missing required field, unknown `buckets_value_type`, more than five rows) — logged BEFORE anything is drawn from it, so a bad payload is visible, never a crash
- 🔌 Power, display on/off, mute, and view changes
- 🧯 **Boot**: the reset reason first of all — if the board ever loops, that line names the cause

```
[    412ms] boot: reset: power on
[    418ms] boot: panel ok - 240x240, rotation 1 (90 right)
[    902ms] net: poll -> https://example.invalid/v1/gateway_health
[    955ms] data: level=info "error budget healthy" - 5 metric screens
[   6120ms] key: RIGHT down (GPIO0)
[   6180ms] key: RIGHT tap -> display off (still polling, a critical still sounds)
```

- 🤫 Never logged: frames. The render loop runs at ~25 fps; state changes and actions are what matter

---

## 5. 🔔 The speaker

The alert sound rides the alert flash, and only `critical` has one → [api.md §3](api.md#3--alert).

| `alert.level`  | Screen                       | Sound                                             |
| -------------- | ---------------------------- | ------------------------------------------------- |
| 🟢 `info`       | steady                       | none                                              |
| 🟠 `warning`    | flashes **500 ms every 5 s** | **none**                                          |
| 🔴 `critical`   | flashes **500 ms every 5 s** | **500 ms alert with every flash**                 |
| 🩶 fetch fault  | flashes **500 ms every 5 s** | none — a Wi-Fi roam must not sound like an outage |

- 🔗 **The sound and the flash start in the same frame and end together** — one clock for both, so the room hears exactly what the screen shows
- 🌑 **The sound does not care what is on screen**, or whether the panel is even awake
- 🥁 **Not a beep.** A low knock for weight, a warm tone sliding down with a couple of harmonics for body, and one soft high partial so it still carries across a room. Small speakers cannot move real bass, so harmonics carry it
- ⚡ Keep the codec at or under ~75 % on a small cell. Louder sags the rail and reboots the device
- 🌠 The **boot intro** is silent — the alert is the only sound this build makes. Each build owns what the intro looks like → [ws_lcd_154](../ws_lcd_154/device.md#6--boot-intro)
- 🧮 **Never synthesise audio continuously on the chip.** Render once and stream, in float maths — live synthesis on core 0 starved its idle task past the 5 s watchdog and boot-looped the board

---

## 6. ⚙️ Configuration

`Preferences`, namespace `gw`. Added over the Wi-Fi hotspot — hold **LEFT** 2 s, join the network named after the device (`pulsar-` + the end of its MAC), open `192.168.4.1`. **The hotspot's settings page is still to be specified** — builds show a placeholder screen until then.

| Key | Meaning                |
| --- | ---------------------- |
| `u` | base URL, `https://…`  |
| `t` | bearer token           |

- 1️⃣ **One URL. One gateway.** There is no count, and no list
- 🔄 No reflash to change the endpoint
- 📶 Wi-Fi networks live in namespace `wifi`, joined with `WiFiMulti` — the device roams saved SSIDs on its own
- 🏷️ The gateway **name** is never stored. It arrives in every payload

---

## 7. 🎨 Colours

Authored in hex, quantised to RGB565 — what you pick is not quite what the panel shows.

| Use        | Colour    | Use            | Colour    |
| ---------- | --------- | -------------- | --------- |
| Background | `#05070c` | 🟢 `info`       | `#5cf22e` |
| Panel      | `#0c1422` | 🟠 `warning`    | `#ff7a00` |
| Text       | `#e6edf7` | 🔴 `critical`   | `#ff2626` |
| Dim        | `#7f8fa8` | 🩶 fetch fault  | `#8aa0c0` |
| Rule       | `#1c2740` | 💠 accent       | `#22d3ee` |

- 💠 The accent is not a level — it is the graph's lit surface line and the poll hairline
- 🖤 Where a build paints the level colour as a **background**, the ink on it goes near-black — the only way one region reads on green, orange, red _and_ grey

---

## 8. 🎞️ Motion

One off-screen sprite pushed per frame at ~25 fps. All of it stops the instant the panel sleeps.

| Motion           | How                                                                                      |
| ---------------- | ---------------------------------------------------------------------------------------- |
| Alert flash      | **The whole screen** goes the level colour for 500 ms, synced to the 5 s cycle — warning, critical, fault |
| Screen change    | Numbers count up from the previous screen's, ~520 ms                                     |
| Graph sweep-in   | Columns left → right on new data, ~900 ms                                                |
| Boot intro       | Once per power-on, a few seconds, then the dashboard                                     |
| Poll in flight   | A hairline in the STATUS band brightens                                                  |
| Hold in progress | A ring fills toward the 2 s mark with the action inside                                   |

- ⛔ Never `fillScreen()` inside `loop()`
- ⛔ Never animate with the panel dark
