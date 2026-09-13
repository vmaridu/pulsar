# 🔧 How every build behaves

Behaviour every build shares — the pixels are each build's own.

📡 Contract → **[api.md](api.md)**

---

## 1. 🖥️ Screens

One metric per screen, shown for **5 s**, then the next. Wrapping back to the
first screen fetches fresh data, never faster than `max(30, metric_count × 5)`
seconds. A 2 s hold on the glass forces a poll now, wherever the cycle is.

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

Only `critical` has a sound, and it rides the same clock as the flash — they
start and end together.

| Level          | Screen  | Sound                                                       |
| -------------- | ------- | -------------------------------------------------------------- |
| 🟢 `info`      | steady  | none                                                            |
| 🟠 `warning`   | flashes | none                                                            |
| 🔴 `critical`  | flashes | a short alert, every flash                                      |
| 🩶 fetch fault | flashes | none — a dropped connection must not sound like an outage       |

A double-tap on the mute control silences every sound and brings it back with
the same gesture. Mute lives in RAM only — a restart always comes back with
sound, except straight after a brown-out reset, which starts muted so a
sagging supply can't loop the sound that caused it.

---

## 4. ⚠️ Errors

A failed poll never clears the screen — the last good payload stays on,
marked held, with a banner naming what went wrong. A board that has never
polled successfully shows no numbers at all: a monitor that renders invented
data while it is broken is worse than one that admits it has none.

Every failure gets its own banner rather than a blank or silently stale
screen — no endpoint configured, no network in range, a sign-in page in the
way, a rejected key, a host that never answers. The device keeps retrying and
clears the banner the moment a poll succeeds.

---

## 5. ⚙️ Configuration

Nothing about the endpoint is compiled in. Hold **LEFT** 2 s to raise the
device's own setup hotspot — a WPA2 network named after the board, with a
password shown only on its screen — and open the page it serves at
`192.168.4.1`:

- The **full URL** to poll, exactly as your backend serves it — nothing is
  appended — an **API key**, and an optional **API secret**: set the secret
  and every request is signed instead of sent as a bearer token
- Up to **six Wi-Fi networks**, in priority order, including
  WPA2-Enterprise and captive-portal sign-in
- An optional pasted **CA root**, for a backend behind a private certificate

A stored secret never comes back to the page — it reports that one exists,
never what it is, and leaving the field empty on save means "keep it." Save
and restart, and the device's own screen says whether it worked.
