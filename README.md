# ⚡ Pulsar

A desk display that answers one question: **is your API gateway healthy?**

- 🔌 Polls one backend you control — **one device, one endpoint**, set over the device's own Wi-Fi, never reflashed
- 🔁 Shows each metric for **5 s** and refetches when the cycle wraps, never faster than **30 s**
- 🔔 A **critical** sounds a short alert with every flash — **even with the screen off**
- 🔕 Double-tap the right key to silence every sound; a restart always brings it back
- 🚦 Counts **2xx / 4xx / 5xx** apart, so a caller's bug never looks like yours
- 🚨 An alert flashes the **whole screen**, not a stripe of it
- 👁️ Reads only — no acknowledge, no writes

## 📡 The contract

```
GET {url}
Authorization: Bearer <token>
```

- 📦 One `GET`, one JSON object under **4 KB** — a bearer token, or a signed request when you set an API secret
- 🩺 An `alert` verdict — `info` · `warning` · `critical` + one 20-char line
- 📊 Up to **5 rows**, max **5 aggregate stats** per metric
- 🏷️ Each row carries a **`gateway`** attribute, so a backend that merges several platforms still says where a number came from
- 🚫 Nothing derived is ever sent — a tile carries `name` + `value` + optional `unit`/`level`, nothing computed from anything else on the wire

📖 **Every field, limit and guideline → [docs/api.md](docs/api.md)**

## 🎛️ Controls

| Input             | Tap                    | Double-tap        | Hold 2 s               |
| ----------------- | ---------------------- | ----------------- | ---------------------- |
| **Glass** (touch) | Next metric screen     | _future use_      | **Force refresh**      |
| **LEFT** (PLUS)   | **Settings** on/off    | _future use_      | **Setup hotspot**      |
| **POWER** (PWR)   | _future use_           | _future use_      | **Power off / on**     |
| **RIGHT** (BOOT)  | **Display on/off**     | **Mute / unmute** | _future use_           |

## 📚 Docs

| Path                                    | Owns                                                        |
| --------------------------------------- | ----------------------------------------------------------- |
| 📡 **[docs/api.md](docs/api.md)**        | The contract — endpoint, payload, every field, limits        |
| 🔧 **[docs/device.md](docs/device.md)**  | Shared behaviour — screens, logging, sound, errors, configuration |
| 🖥️ `ws_lcd_154/` · `ws_lcd_349/`         | One folder per supported display — firmware, docs and mockup together |
| ↳ `<display>/device.md`                 | Hardware, screen layout, on-device checklist                 |
| ↳ `<display>/mockup.html`               | Interactive mockup running a real payload                    |
| ↳ `<display>/README.md`                 | Flashing and troubleshooting                                 |
| 🧪 **[simulator/README.md](simulator/README.md)** | A fake backend to point either build (or the mockups) at — zero dependencies |

## 🚀 Getting started

1. 📡 Implement `GET {url}` against **[docs/api.md](docs/api.md)** — or want to test the contract first? Point at **[simulator/](simulator/)** and get real data with no backend
2. 🖥️ Choose a device and flash it — **[ws_lcd_154/](ws_lcd_154/)** firmware runs today ([flashable `.bin`](#), [flashing guide](ws_lcd_154/README.md)); **[ws_lcd_349/](ws_lcd_349/)** is layout and contract only so far
3. 📶 Hold **LEFT** for 2 s to raise the device's setup hotspot, join it, and fill in the full URL, API key/secret and your Wi-Fi networks at `192.168.4.1` → **[docs/device.md §5](docs/device.md#5--configuration)**
4. 🖼️ Or skip hardware entirely — open a display's `mockup.html` to see it rendered
