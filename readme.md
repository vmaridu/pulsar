# ⚡ Pulsar

Pulsar is a portable, ultra-low-power IoT device that acts as a persistent smart display for real-time data monitoring. It uses a long-lifetime color TFT, and saves battery by powering the screen off completely whenever it is unplugged. Background polling continues while the display is dark; if the backend reports something important, the speaker sounds so you never miss an alert. A button press wakes the screen for 60 seconds so you can glance at what is happening. When a power supply is connected, the display stays fully on and the device polls the server every 60 seconds for alerts and display data. Beyond the visual interface, the onboard speaker signals high-priority events from a backend REST API. The project is a hardware and firmware framework for developers who want to bridge cloud information and physical, glanceable peripherals.

- 🌈 Long-lifetime **colour TFT**, portrait, five fixed bands
- 🔋 On battery the screen is **off**; polling and the speaker keep running
- 👆 One blue key walks every screen — **shake the stick** to refresh now
- 🔌 Polls every **60 seconds**, plugged in or not
- 🚦 Counts by **2xx / 4xx / 5xx**, so a caller's bug never looks like yours
- 🔔 Alarms sound **once on the transition**, not once per poll

## 🔭 Overview

Pulsar is a compact, battery-powered smart display designed to sit on a desk or wall and act as a real-time peripheral for backend applications. It uses a color TFT panel for a long-lived, full-color interface, and saves energy by turning the screen completely off whenever it is running on battery. Background polling continues while the display is dark; important events wake the speaker so you never miss an alert. Plug it in and the screen stays on. The poll interval is 60 seconds either way — power changes what the screen does, never what the radio does.

## ✨ Core Capabilities

- 🔌 **Power-Aware Display** — On mains/USB power the color TFT stays fully on. On battery the display is powered off entirely so the device can run for weeks or months on a single charge.
- 📡 **Background Polling on Battery** — While unplugged, Pulsar keeps polling the backend in the background with the screen off. If anything important is found, the speaker sounds immediately.
- 👆 **Button Wake Peek** — On battery, a single press of the blue key wakes the screen so you can see what is happening. The display stays on for 60 seconds, then turns off again.
- 🖥️ **Always-On Docked Mode** — When a power supply is connected, the display remains fully on and the device polls the server every 60 seconds for alerts and display data.
- 🤝 **Shake to Refresh** — The 6-axis IMU catches a deliberate shake and forces a poll, rate-limited to one every 5 seconds. No button to hold, nothing to label.
- 🌐 **One Simple Contract** — Any backend that can serve one `GET` returning a small JSON object can be watched. Standard `Authorization: Bearer` auth, with optional HMAC.
- 🔔 **Speaker Alerts** — The speaker fires on the **transition** into an alarm, not on the state, so a problem that stays broken sounds once rather than every minute. The device never writes back, so there is no acknowledge — the backend clears it.
- 🔋 **Rechargeable Portable Power** — Powered by an integrated, rechargeable LiPo battery with onboard charging and power-source detection.

## 🛠️ Hardware

Three boards, one contract. Every one polls the same endpoint and shows the same five numbers.

| Board | Screen | Input | Notes |
| --- | --- | --- | --- |
| **M5StickS3** | 1.14" 135 × 240 | blue key + shake | Speaker and 250 mAh cell in the case, magnetic back |
| **ESP32-S3-Touch-LCD-3.49** | 3.49" **640 × 172** landscape | touch + shake | Real RTC, dual mics, speaker on a header |
| **ESP32-S3-LCD-1.54** | 1.54" 240 × 240 | touch + PLUS key | Speaker in the case, TF slot, square |

Pixel count drives the design, not just the layout. Laid out **landscape at 640 × 172** the 3.49 becomes a comparison board: every panel gets its own column, side by side at the same scale, so you read across to find the one dragging the service down instead of stepping through them. See **[docs/stick.md](docs/stick.md)**.

## 📚 Docs

| File | What is in it |
| --- | --- |
| **[docs/api.md](docs/api.md)** | The backend contract. One `GET`, one JSON object, bearer auth, field limits and data guidelines. Device-independent. |
| **[docs/stick.md](docs/stick.md)** | The device. Hardware specs, the five screen bands, the keys, power behaviour, animation budget. |
| **[docs/code.md](docs/code.md)** | Proposed firmware — model, HTTP client, drawing, main loop, storage. |
| **[docs/compare.md](docs/compare.md)** | Hardware comparison — candidate boards, ESP32 chips, DevKits, what to check on anything else. |
| **[docs/ui-sticks3.html](docs/ui-sticks3.html)** | Interactive mockup — M5StickS3, 135 × 240. |
| **[docs/ui-lcd349.html](docs/ui-lcd349.html)** | Interactive mockup — ESP32-S3-Touch-LCD-3.49, 640 × 172 landscape. |
| **[docs/ui-lcd154.html](docs/ui-lcd154.html)** | Interactive mockup — ESP32-S3-LCD-1.54, 240 × 240. |

Earlier drafts live in `docs/archive/`.

## 🎛️ How you use it

Three services at most, three panels each — nine screens, and **one key walks all of them**.

```
Payouts · Overview → Payouts · Payout → Payouts · Account
   → Fraud VAS · Overview → Fraud VAS · Eval → back to the start
```

| Input | Tap | Hold |
| --- | --- | --- |
| **Blue front** | Next screen (wakes the panel first if dark) | — |
| **Right side** | Setup screen | Wi-Fi portal |
| **Power** (left) | On | Off |
| **Shake it** | Force a poll now | — |

The blue key does one thing and there is nothing to hold. To force a refresh you pick the stick up and shake it — the IMU catches a deliberate shake, ignores desk bumps, and rate-limits to one request every 5 s.

Every service is polled every cycle, not just the one on screen — so a service you are not looking at can still sound the speaker.

## 🖥️ What it shows

```
┌──────────────────────────┐
│ ▮82%                 ᯤ   │  battery · charge · wi-fi
├──────────────────────────┤
│ ⚠ CRITICAL               │  the alarm band never moves
│ 5xx 2.14% max 0.50%      │  one line, 20 characters
├──────────────────────────┤
│ P95              210ms   │
│ AVG               48ms   │  four numbers, size 2
│ 4XX        169    0.92%  │
│ 5XX        389    2.14%  │
├──────────────────────────┤
│ 595                      │  2xx/min, drawn ON the graph
│ 2XX / MIN                │
│    ▁▂▃▅▆▇█▇▆▅▃▂▁         │  the same metric over time
│              LAST 30 MIN │
├──────────────────────────┤
│ Payouts             ● ○  │  service, panels, fleet dots
│ Overview             1/3 │
└──────────────────────────┘
```

The headline and the graph are **one metric at two time scales** — 2xx per minute now, and how it got there. Details in **[docs/stick.md](docs/stick.md)**.

## 📊 What a service sends

```json
{
  "service": "Payouts",
  "window_minutes": 30,
  "updated_at": 1770000100,
  "alarm":    { "state": "active", "level": "critical", "message": "5xx 2.14% max 0.50%" },
  "overview": { "name": "Overview", "avg_latency_ms": 48, "p95_latency_ms": 210,
                "count_2xx": 17862, "count_4xx": 169, "count_5xx": 389,
                "trend": [601, 618, 604, 341, 288, 240] },
  "metrics":  [ { "name": "Payout", "avg_latency_ms": 61, "p95_latency_ms": 340,
                  "count_2xx": 8709, "count_4xx": 114, "count_5xx": 377,
                  "trend": [301, 308, 297, 170, 141, 118] } ]
}
```

`overview` and every `metrics` entry are the same shape, so one struct renders every panel.

Requests are counted by **status class**, not lumped into one "errors" figure — 4xx is the caller's fault, 5xx is yours, and only one of them should wake you. Nothing derived is sent: the headline is `count_2xx / window_minutes`, the rates come from the three counts, and `trend` is that same 2xx rate per minute — so the big number and the graph under it are one metric at two time scales. Counts go raw; the device abbreviates them (`18.4k`, `2.5M`). Full contract in **[docs/api.md](docs/api.md)**.

## 🔄 Lifecycle

The stick polls **every 60 seconds, always**. Power does not change the polling, only what the screen does.

### 🔋 Unplugged

1. 🌑 **Display Off** — The TFT is powered down completely. No backlight, no panel refresh.
2. 📡 **Background Poll** — The device periodically wakes, connects to Wi-Fi, and checks the backend for new data and alert flags.
3. 🚨 **Important Event** — If the server reports something important, the speaker sounds. The screen stays off unless the user asks for it.
4. 👆 **Button Wake** — Press the button to turn the display on and review the latest payload. After **60 seconds** of inactivity the screen turns off again.
5. 🌙 **Return to Dark Idle** — Connections are torn down and the device resumes low-power background polling.

### 🔌 Plugged in

1. 💡 **Display Always On** — The color TFT stays fully powered and shows the latest dashboard / alert data.
2. ⏱️ **60-Second Poll** — Every **60 seconds** the device fetches alerts and display data — the same interval it uses on battery.
3. 🖼️ **Live Refresh** — The screen updates when the payload changes. Alerts also drive the speaker.

The firmware detects USB vs. battery at runtime. It is the same loop either way — only the backlight and the wake timer differ.

## 🧭 Design rules

A few decisions that the rest follows from:

- **Nothing derived is ever sent.** No error rate, no request total, no throughput field — five counters and a window produce everything on screen. A second source of truth is a second thing that can disagree.
- **Three services, three panels each.** Nine screens is the whole product. It is an attention limit, not a rendering one.
- **The alarm band never moves.** Same pixels, same height, in every state. Only the colour and the message change.
- **A fetch failure is a warning, never an outage.** Every non-200 blinks at the warning rate — noticeable, never dressed up as `CRITICAL`, and never silent.
- **The device only reads.** No acknowledge, no writes, no controls it cannot honour.

## 🗺️ Future Roadmap

- ☁️ **Cloud Control Panel** — A centralized SaaS dashboard to manage device fleets, push OTA (Over-The-Air) firmware updates, configure API polling endpoints, and monitor battery health remotely.
- 🎨 **Configurable Layout Engines** — Remote definition of color TFT screen templates and UI components directly from the control panel.
- 🔐 **Expanded Auth Handlers** — Built-in support for OAuth2 token rotation and dynamic secret management.

## 🚀 Getting Started

1. 📥 Clone the repository.
2. 🖥️ Open one of the mockups — **[stick](docs/ui-sticks3.html)** · **[3.49](docs/ui-lcd349.html)** · **[1.54](docs/ui-lcd154.html)** — to see what you are building.
3. 🔧 Point a backend at **[docs/api.md](docs/api.md)** and serve `GET /v1/status`.
4. 📟 Build the firmware from **[docs/code.md](docs/code.md)**; add service URLs and tokens over the Wi-Fi portal — hold the right key.
5. 🔌 Polls every 60 s either way. USB = screen on. Battery = screen off, blue key wakes it for 60 s, speaker on alerts.
