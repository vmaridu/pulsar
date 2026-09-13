# ⚡ Pulsar

A desk display that answers one question: **is your API gateway healthy?**

- 🔌 Polls one backend you control — **one device, one endpoint**
- 🔁 Shows each metric for **5 s** and refetches when the cycle wraps, never faster than **30 s**
- 🔔 A **critical** sounds a short alert with every flash — **even with the screen off**
- 🔕 Double-tap the right key to silence every sound; a restart always brings it back
- 🚦 Counts **2xx / 4xx / 5xx** apart, so a caller's bug never looks like yours
- 🚨 An alert flashes the **whole screen**, not a stripe of it
- 👁️ Reads only — no acknowledge, no writes

## 📡 The contract

```
GET {base_url}/v1/gateway_health
Authorization: Bearer <token>
```

- 📦 One `GET`, one JSON object under **4 KB**, bearer auth
- 🩺 An `alert` verdict — `info` · `warning` · `critical` + one 20-char line
- 📊 Up to **5 rows**, each with **1–5 stat tiles** plus a bucketed series named by `buckets_value_type` — today always `total_count`
- 🏷️ Each row carries a **`gateway`** attribute, so a backend that merges several platforms still says where a number came from
- 🚫 Nothing derived is ever sent — a tile carries `name` + `value` + optional `unit`/`level`, nothing computed from anything else on the wire

📖 **Every field, limit and guideline → [docs/api.md](docs/api.md)**

## 🎛️ What it does

- 🗂️ **5 metrics = 5 screens** — an attention limit, not a rendering one
- 🔄 Screens turn on their own every 5 s; tap the glass to steer, hold it 2 s to force a poll
- 🖥️ Narrow displays walk screens one at a time; wide ones show every metric side by side
- 🚨 Alerts pulse: `warning`, `critical` and a lost connection flash for **500 ms every 5 s**, and only `critical` makes a sound
- 🌑 The screen going dark changes nothing — polling and alerts carry on

## 🎛️ Controls

| Input             | Tap                    | Double-tap        | Hold 2 s               |
| ----------------- | ---------------------- | ----------------- | ---------------------- |
| **Glass** (touch) | Next metric screen     | _future use_      | **Force refresh**      |
| **LEFT** (PLUS)   | **Settings** on/off    | _future use_      | **Hotspot** (UI later) |
| **POWER** (PWR)   | _future use_           | _future use_      | **Power off / on**     |
| **RIGHT** (BOOT)  | **Display on/off**     | **Mute / unmute** | _future use_           |

🔧 **What each one means → [docs/device.md](docs/device.md#2--input--the-standard-map)**

## 🧭 Design rules

- 🧮 **Nothing derived is sent.** Four counters and a clock produce everything on screen
- 🚦 **4xx and 5xx never blend.** One is the caller's fault, one is yours
- 📍 **The verdict never moves.** Same place, same size, at every level
- ⚠️ **A fetch failure is a warning, never an outage** — and never silent either
- 🪵 **Everything is on the serial port at 115200** — every press, every poll, every failure

## 📚 Docs

| Path                                    | Owns                                                        |
| --------------------------------------- | ----------------------------------------------------------- |
| 🤖 **[AGENTS.md](AGENTS.md)**            | Working agreement for AI assistants — Claude and Cursor both |
| 📡 **[docs/api.md](docs/api.md)**        | The contract — endpoint, payload, every field, limits        |
| 🔧 **[docs/device.md](docs/device.md)**  | Shared behaviour — cycle, input, bands, logging, speaker     |
| 🖥️ `ws_lcd_154/` · `ws_lcd_349/`         | One folder per supported display — firmware, docs and mockup together |
| ↳ `<display>/device.md`                 | Hardware, screen layout, on-device checklist                 |
| ↳ `<display>/mockup.html`               | Interactive mockup running a real payload                    |
| ↳ `<display>/README.md`                 | Flashing and troubleshooting                                 |
| 🧪 **[simulator/README.md](simulator/README.md)** | A fake backend to point either build (or the mockups) at — zero dependencies |

## 🚀 Getting started

1. 🔧 Serve `GET /v1/gateway_health` against **[docs/api.md](docs/api.md)**
2. 📶 Point a device at it over the Wi-Fi hotspot → **[docs/device.md](docs/device.md#6--configuration)**
3. 🖼️ Open a display's `mockup.html` to see it rendered before any hardware exists

> ⚠️ **ws_lcd_154 firmware runs today**, offline — the payload is a JSON literal in the sketch and there is no radio yet → [ws_lcd_154](ws_lcd_154/README.md). ws_lcd_349 is layout and contract only.

## 🗺️ Roadmap

- 🌐 The real HTTPS poll, replacing the embedded payload
- 📶 The hotspot settings page — URL and token without a reflash
- ☁️ Cloud control panel — fleets, OTA updates, remote config
- 🔐 OAuth2 token rotation
