# ⚡ Pulsar

A desk display that answers one question: **is your API gateway healthy?**

- 🔌 Polls a backend you control every **60 seconds**
- 🔔 Sounds a speaker when something breaks — **even with the screen off**
- 🔋 On battery the screen is **off**; polling and the speaker keep running
- 🚦 Counts **2xx / 4xx / 5xx** apart, so a caller's bug never looks like yours
- 🤝 **Shake it** to force a fresh reading
- 👁️ Reads only — no acknowledge, no writes, nothing it can touch

## 📡 The contract

```
GET {base_url}/v1/gateway_health
Authorization: Bearer <token>
```

- 📦 One `GET`, one JSON object under **4 KB**, bearer auth
- 🩺 A `health` verdict — `info` · `warning` · `critical` + one 20-char line
- 📊 Up to **5 rows**, each five aggregates plus a bucketed series of the same 2xx count
- 🚫 Nothing derived is ever sent — no error rate, no totals, no throughput

📖 **Every field, limit and guideline → [docs/api.md](docs/api.md)**

## 🎛️ What it does

- 🗂️ **3 gateways × 5 panels** — fifteen screens, an attention limit not a rendering one
- 👆 Narrow displays walk panels one at a time; wide ones show every panel side by side
- 🔄 Every gateway polls every cycle, not just the one on screen
- 🚨 Alerts fire **once on the transition**, not once per poll
- ⏱️ The poll interval never changes — power changes the screen, never the radio

## 🧭 Design rules

- 🧮 **Nothing derived is sent.** Three counters and a clock produce everything on screen
- 🚦 **4xx and 5xx never blend.** One is the caller's fault, one is yours
- 📍 **The verdict never moves.** Same place, same size, at every level
- ⚠️ **A fetch failure is a warning, never an outage** — and never silent either

## 📚 Docs

| Path                                    | Owns                                                       |
| --------------------------------------- | ---------------------------------------------------------- |
| 📡 **[docs/api.md](docs/api.md)**       | The contract — endpoint, payload, every field, limits      |
| 🔧 **[docs/device.md](docs/device.md)** | Shared behaviour — power, shake, speaker, config, palette  |
| 🖥️ `lcd154/` · `lcd349/`                | One folder per supported display                           |
| ↳ `<display>/docs/readme.md`            | Hardware, input, screen layout, on-device checklist        |
| ↳ `<display>/mockups/device-ui.html`    | Interactive mockup running a real payload                  |
| ↳ `<display>/src/`                      | Firmware for that display                                  |

## 🚀 Getting started

1. 🔧 Serve `GET /v1/gateway_health` against **[docs/api.md](docs/api.md)**
2. 📶 Point a device at it over the Wi-Fi portal → **[docs/device.md](docs/device.md)**
3. 🖼️ Open a display's `mockups/device-ui.html` to see it rendered before any hardware exists

> ⚠️ Firmware is not in this repo yet. The contract and the layouts are what is settled.

## 🗺️ Roadmap

- ☁️ Cloud control panel — fleets, OTA updates, remote config
- 🎨 Layouts pushed from the panel instead of compiled in
- 🔐 OAuth2 token rotation
