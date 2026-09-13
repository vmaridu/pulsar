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
- 📭 **A board that has never polled successfully shows no numbers at all** — a banner naming the reason and, when there is no endpoint set, how to set one → [§6](#6--configuration)
- 🌑 **The screen going dark changes nothing.** The cycle turns, the poll polls, the speaker sounds

---

## 2. 🎛️ Input — the standard map

Every build implements exactly this. **Anything not in this table is future use**, and future use is something a build says out loud on its serial port — never a key that silently does nothing.

| Input             | Tap                        | Double-tap              | Hold 2 s                 |
| ----------------- | -------------------------- | ----------------------- | ------------------------ |
| **Glass** (touch) | Next metric screen         | _future use_            | **Force refresh**        |
| **LEFT** (PLUS)   | **Settings screen** on/off | _future use_            | **Setup hotspot**        |
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

Categories: `boot` · `key` · `touch` · `view` · `net` · `wifi` · `cfg` · `data` · `power` · `sound`

- 📶 `wifi` is the radio itself — the scan, which saved network was tried and why the last one failed, the captive-portal probe and what it found
- ⚙️ `cfg` is the stored configuration being read at boot or written from the setup page. **Never the values** — a URL and a username, yes; a password, never

**Everything below is logged, one line each:**

- 🔘 **Every button press and every touch** — and the action it caused, including the ones that are future use
- 🌐 **Every poll** — the URL, which auth it used, the outcome, and the real error text on failure. Success and failure alike. A poll that never left the device (no URL set, no network, a sign-in page in the way) says which of those it was
- 📦 **Every payload** — the level, each metric row with its counts and its own `gateway`, and every contract violation found (`sum(buckets)` mismatches, a missing required field, unknown `buckets_value_type`, more than five rows) — logged BEFORE anything is drawn from it, so a bad payload is visible, never a crash
- 🔌 Power, display on/off, mute, and view changes
- 🧯 **Boot**: the reset reason first of all — if the board ever loops, that line names the cause

```
[    412ms] boot: reset: power on
[    418ms] boot: panel ok - 240x240, rotation 1 (90 right)
[    431ms] cfg: url https://api.example.com
[    432ms] cfg: auth bearer token, TLS root pinned
[    433ms] cfg: 2 saved networks
[    433ms] cfg:   1. "Office" psk
[    434ms] cfg:   2. "HotelGuest" open + sign-in page
[    902ms] wifi: scan found 9 networks in range
[    903ms] wifi:   candidate 1: "Office" psk, -52 dBm
[   3120ms] wifi: joined "Office" - 192.168.1.42, -52 dBm, took 2217ms
[   3380ms] wifi: probe: 204 - the way out is clear
[   3381ms] wifi: online
[   3382ms] net: poll -> https://api.example.com/v1/gateway_health
[   3383ms] net: auth: bearer token 7f3a...
[   3702ms] net: 200 in 320ms, 2614 bytes
[   3740ms] data: level=info "error budget healthy" - 5 metric screens
[   9120ms] key: RIGHT down (GPIO0)
[   9180ms] key: RIGHT tap -> display off (still polling, a critical still sounds)
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

**Nothing about the endpoint is compiled in.** Where a board points, what it authenticates
with, and which networks it joins all live in NVS and are set over the device's own Wi-Fi —
**hold LEFT 2 s**, join the network named after the device, and a setup page opens.

### 📶 Getting in

| Step | What                                                                                   |
| ---- | -------------------------------------------------------------------------------------- |
| 1    | **Hold LEFT 2 s.** The screen shows the network name, a password, and `192.168.4.1`     |
| 2    | Join that Wi-Fi from a phone or laptop, using the password **on the screen**            |
| 3    | The page opens by itself. If it doesn't, browse to `192.168.4.1`                        |
| 4    | Fill it in, **Save & restart**. The device's own screen says whether it worked          |

- 🔒 **The hotspot is WPA2 and its password is new every time**, shown only on the device.
  An open access point would let anyone in range repoint the board and read what it posts;
  the screen is the out-of-band channel that closes that
- 🌐 A DNS responder answers every name with `192.168.4.1`, which is what makes the page
  appear on its own — the same trick a hotel's portal uses
- 🚪 **The AP goes down the moment you leave the screen** — tap LEFT, hold LEFT, or tap the
  glass. It is never up when you are not looking at it
- 🧪 **Nothing is tested from the page.** One radio cannot hold the AP up *and* stay joined
  to your office network without dropping the phone that is mid-edit, so the page scans
  (which needs no association) and then hands off to the device's own screen

### 🎯 What it stores

`Preferences`, two namespaces — they are two concerns, and either may be replaced without
touching the other.

| Namespace | Key  | Holds                                                                |
| --------- | ---- | -------------------------------------------------------------------- |
| `gw`      | `u`  | Base URL, `https://host[/prefix]` — `/v1/gateway_health` is appended  |
| `gw`      | `k`  | API key — the bearer token, or the public `X-Api-Key`                |
| `gw`      | `s`  | API secret — set it and requests are signed instead                  |
| `gw`      | `ca` | One pasted PEM root, optional                                        |
| `wifi`    | `v`  | Layout version — a blob written by an older build is dropped, not misread |
| `wifi`    | `n`  | How many networks are stored                                         |
| `wifi`    | `b`  | The networks, one blob, **in priority order**                        |

- 1️⃣ **One URL.** There is no count and no list — the backend behind it merges whatever it
  wants to → [api.md §1](api.md#1--endpoint)
- 🔄 No reflash to change any of it
- 🏷️ The gateway **names** are never stored. They arrive in every payload

### 🔑 One secret decides how it authenticates

There is no auth mode to set, because a second field would only ever contradict the first:

| API secret | What the device sends                                                        |
| ---------- | ----------------------------------------------------------------------------- |
| empty      | `Authorization: Bearer <key>`                                                 |
| set        | `X-Api-Key` + `X-Timestamp` + `X-Signature`, signed with the secret → [api.md appendix](api.md#-appendix--hmac) |

- 🕰️ **Signing needs a clock.** These boards have no RTC, so SNTP is asked on every join;
  until it answers, a poll that would have to sign says `NO CLOCK` rather than send a
  request the backend is certain to reject as 60 s stale
- 🔐 **A stored secret is never sent back out.** The page is told a key exists, never what
  it is, and an empty box posted back means "keep it" — matched up by network name, so
  reordering the list never loses one. Clearing a secret is a separate, explicit tick
- 🩶 **Without a pasted CA root, TLS is encrypted but not verified** — anything in the
  middle can read the key. It is allowed, because plenty of backends sit behind a private
  CA, but the settings screen says `TLS UNVERIFIED` and every poll says it on serial

### 📶 Several networks, in priority order

Up to **six**, and the order of the list *is* the priority. There is no signal preference
and no score: a person who put the office network first meant it.

- 🔍 The device scans first, then joins the **highest-priority network it can actually
  see** — so a list is not twelve seconds of nothing per network that is out of range
- 🫥 A saved network the scan did *not* see is still tried, after the ones it did. A hidden
  SSID never appears in a scan and joins perfectly well
- 🛑 **Failing every network is a normal outcome.** The banner says `OFFLINE`, the last good
  numbers stay, and it scans again in 30 s. A board carried out of range is not a board
  with a problem

### 🎓 The two networks that need more than a password

They fail in completely different places, so they are configured separately:

| Kind                       | What it needs                             | Where it fails without it        |
| -------------------------- | ----------------------------------------- | -------------------------------- |
| **WPA2-Enterprise** (802.1X) | Username, password, and an outer identity | The join — there is never an IP  |
| **Captive portal**         | A sign-in name and password               | After the join — an IP, no route |

- 🎓 **Enterprise** authenticates during the association itself, over PEAP. Leave the outer
  identity blank and the username is used; set it to something like `anonymous@realm` where
  the network wants the real one hidden
- 🏨 **A captive portal** joins perfectly and then holds every request until a form is
  posted, which is why every join ends with a probe rather than a celebration. Tick the box
  and the device finds the form, fills the field that looks like a username and the one
  that looks like a password, carries the hidden fields through, and posts it
- 🚧 **It cannot drive every portal, and says so instead of pretending.** A portal that
  builds its form in JavaScript, or bounces through a separate identity provider, needs a
  browser this device does not have. When it cannot get through, the banner reads `PORTAL`
  and the settings screen shows the network with `SIGN-IN` beside it — so you look at the
  ceiling, not at the backend
- 🔁 A portal signed into from a phone afterwards is noticed: the device re-probes every
  30 s and lets itself out without being asked

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
