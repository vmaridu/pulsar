# 🔧 How every build behaves

Behaviour every build shares — the pixels are each build's own.

📡 Contract → **[api.md](api.md)**

---

## 1. 🖥️ Screens

One metric per screen, shown for **5 s**, then the next. Wrapping back to the
first screen fetches fresh data, never faster than `max(30, metric_count × 5)`
seconds. A 2 s hold on the touch input forces a poll now, wherever the cycle is.

Every screen lays out as four fixed bands:

| Band       | Owns                                                                            |
| ---------- | -------------------------------------------------------------------------------- |
| **STATUS** | Device internals only — battery, the reading's timestamp, Wi-Fi, poll hairline   |
| **ALERT**  | `info` / `warning` / `critical` and its one-line message. Nothing else           |
| **BODY**   | The stats and the graph for the current metric, combined                        |
| **FOOTER** | Gateway name + metric name, and the position in the cycle                       |

A band never moves, never resizes, and never borrows another band's job.

### 🚨 The alert is the whole screen

`warning`, `critical` and a fetch fault flash **every band** the level colour
for **500 ms**, right at the start of every 5 s screen — the whole device,
not a stripe of it. Every foreground turns near-black ink during the flash,
since no one colour reads on black, red, orange and grey alike. The verdict
belongs to the response, not to the metric on screen — tap through if you
want, the alert follows.

### 🌑 Display off is not power off

Turning the panel off stops nothing else — the cycle keeps turning, the poll
keeps polling, and a `critical` still sounds in the dark.

---

## 2. 🪵 Logging

The board has no screen for you to debug on, so **the serial port is the
whole story**.

| Setting | Value                         |
| ------- | ------------------------------ |
| Baud    | **115200**, 8-N-1              |
| Format  | `[  1234ms] category: message` |

Categories: `boot` · `key` · `touch` · `view` · `net` · `wifi` · `cfg` ·
`data` · `power` · `sound`

Logged, one line each: every button press and touch with the action it
caused; every poll with its URL, outcome and real error text; every payload
received, with any contract violation found — logged **before** anything is
drawn from it; power, display, mute and view changes; and boot's reset reason
first of all, so a looping board names its own cause. Never logged: frames.

---

## 3. 🔔 What it speaks

Every non-`info` level has a sound, and it rides the same clock as the flash
— they start and end together. `critical` gets a stronger alert; `warning`
and any connection fault get a shorter, quieter notice instead, so the two
are never mistaken for each other by ear alone.

| Level          | Screen  | Sound                                                       |
| -------------- | ------- | -------------------------------------------------------------- |
| 🟢 `info`      | steady  | none                                                            |
| 🟠 `warning`   | flashes | a short, quiet notice, every flash                              |
| 🔴 `critical`  | flashes | a stronger alert, every flash                                   |
| 🩶 fetch fault | flashes | the same quiet notice as `warning`                               |

A double-tap on the mute control silences every sound and brings it back with
the same gesture. Mute also clears itself two other ways: a restart (RAM
only — always back after one, except straight after a brown-out reset,
which starts muted so a sagging supply can't loop the sound that caused it),
or a configured timeout elapsing on its own, set on the setup page →
[§5](#5--configuration).

---

## 4. ⚠️ Errors

A failed poll never clears the screen — the last good payload stays on,
marked held, with a banner naming what went wrong. A board that has never
polled successfully shows no numbers at all: a monitor that renders invented
data while it is broken is worse than one that admits it has none.

Every failure gets its own banner rather than a blank or silently stale
screen — no endpoint configured, no network in range, a sign-in page in the
way, a rejected key, a host that never answers. One of these, `NO CLOCK`,
reads as `warning` rather than the grey fault colour: a signed board that
hasn't yet heard back from SNTP is waiting on itself, not on the network, so
it doesn't get the same colour as a real connection failure. The device
keeps retrying and clears the banner the moment a poll succeeds.

---

## 5. ⚙️ Configuration

Nothing about the endpoint is compiled in. A 2 s hold raises the device's own
setup hotspot — a WPA2 network named after the board, with a password shown
only on its screen — and opens the page it serves at `192.168.4.1`:

- The **full URL** to poll, exactly as your backend serves it — nothing is
  appended — an **API key**, and an optional **API secret**: set the secret
  and every request is signed instead of sent as a bearer token
- Up to **six Wi-Fi networks**, in priority order, including
  WPA2-Enterprise and captive-portal sign-in
- An optional pasted **CA root**, for a backend behind a private certificate
- **How long a mute lasts** — 5 m / 10 m / 30 m / 1 h / 6 h / 12 h / 24 h, or
  never — before it clears itself on top of the usual double-tap and restart
  → [§3](#3--what-it-speaks). Defaults to 30 minutes
- **The clock's own time zone**, picked from a list of real zones rather than
  a raw offset — daylight saving is then handled automatically, for the life
  of the board, never a number someone has to remember to change twice a
  year. This only shifts what the on-screen clock reads: every signed
  request still carries raw UTC, unaffected by whatever zone is picked
  → [api.md §8](api.md#8--hmac)
- **The privacy lock's 6-digit code**, and **how long an unlocked screen stays
  that way** before it re-locks itself — same set as the mute timeout, default
  30 minutes → [§6](#6--privacy-lock)

A stored secret never comes back to the page — it reports that one exists,
never what it is, and leaving the field empty on save means "keep it." Save
and restart, and the device's own screen says whether it worked.

---

## 6. 🔒 Privacy lock

BODY and FOOTER — the actual stats and graph — can be held behind a 6-digit
code, entered on an on-screen keypad. STATUS and ALERT are unaffected either
way: the level, the message and the whole-screen flash read exactly the
same locked or not, and neither sound nor any other control changes at all.
This is strictly about who can read the numbers.

- **Locking needs no code** — only taking the screen back out of view does.
  The same gesture that locks it, held again while already locked, raises
  the keypad instead
- **Six digits**, entered on the touch surface, checked once the sixth is
  in. The right code unlocks; the wrong one just clears what was typed and
  counts as a try. The digit just pressed may show briefly before masking
  to a dot, the same way a phone's PIN entry does — but never more than
  the one most recent digit, and only ever on this device's own screen
- **The code's digits are only ever ones the keypad can actually enter** —
  a build with no way to type a given digit back in must never accept a
  code containing it, checked at the same time the code is saved
- **Five wrong codes inside a rolling 30 minutes** blocks the keypad until
  enough of that window has passed. This survives a restart — the count
  and when it last grew are stored, not just held in memory — and a
  correct code clears it outright, on the theory that proving you know it
  now outweighs a handful of old mistakes
- **The code itself is never logged, and never shown back once saved** —
  not stored, not typed, right or wrong — only that an attempt happened
  and whether it was accepted, the same rule every other secret on this
  device follows. The one exception is the brief on-screen digit above,
  which never reaches a log. Defaults to `123456` until changed on the
  setup page → [§5](#5--configuration)
- **Every restart locks it again**, whether or not it was unlocked before —
  the safe state is always the default, never something earned back
- **An unlocked screen re-locks itself on its own** after a configurable
  time, same set of options as the mute timeout → [§5](#5--configuration)
- **Only armed once real data has shown at least once**, and only on
  hardware that can actually type a code back in. A board with neither
  never locks itself at all — there is nothing yet to hide, or no way to
  ask for the code back
- **Settings and the setup hotspot are unaffected** — this only ever hides
  the stats, never the device's own diagnostics
