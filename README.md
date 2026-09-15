# ⚡ Pulsar

A desk display that answers one question: **is your API gateway healthy?**

- 🔌 Polls one backend you control — **one device, one endpoint**, set over the device's own Wi-Fi, never reflashed
- 🔁 Shows each metric for **5 s** and refetches when the cycle wraps, never faster than **30 s**
- 🔔 A **critical** sounds a short alert with every flash — **even with the screen off**
- 🔕 Double-tap the UP key to silence every sound; a restart always brings it back
- 🚨 An alert flashes the **whole screen**, not a stripe of it
- 🔒 A 6-digit code locks the stats away — ALERT still shows, BODY/FOOTER don't
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

| Input             | Tap                 | Double-tap        | Hold 2 s           |
| ----------------- | ------------------- | ----------------- | ------------------ |
| **Glass** (touch) | Next metric screen  | **Settings** on/off | **Force refresh**  |
| **DOWN** (PLUS)   | _future use_        | **Settings** on/off | **Setup hotspot**  |
| **POWER** (PWR)   | _future use_        | _future use_      | **Power off / on** |
| **UP** (BOOT)     | **Display on/off**  | **Mute / unmute** | **Lock / unlock**  |

The three-key map is the square board's. The wide board has two programmable keys and the glass; how the same functions land on those is settled as each one arrives → [ws_lcd_349/device.md](ws_lcd_349/device.md).

## 📚 Docs

| Path                                              | Owns                                                                         |
| ------------------------------------------------- | ---------------------------------------------------------------------------- |
| 📋 **[docs/product-requirements.md](docs/product-requirements.md)** | Product requirements — what every build must do, device-agnostic |
| 📡 **[docs/api.md](docs/api.md)**                 | The contract — endpoint, payload, every field, limits                        |
| 🔧 **[docs/functional-requirements.md](docs/functional-requirements.md)** | Shared behaviour — screens, logging, sound, errors, configuration |
| 🖥️ `ws_lcd_154/` · `ws_lcd_349/`                  | One folder per supported display — firmware, docs and mockup together        |
| ↳ `<display>/device.md`                           | Hardware, screen layout, on-device checklist                                 |
| ↳ `<display>/mockup.html`                         | Interactive mockup running a real payload                                    |
| ↳ `<display>/README.md`                           | Flashing and troubleshooting                                                 |
| 🧪 **[simulator/README.md](simulator/README.md)** | A fake backend to point either build (or the mockups) at — zero dependencies |

## 🚀 Getting started

1. 📡 Implement `GET {url}` against **[docs/api.md](docs/api.md)** — or want to test the contract first? Point at **[simulator/](simulator/)** and get real data with no backend
2. 🖥️ Choose a device and flash it — **[ws_lcd_154/](ws_lcd_154/)** ([flashing guide](ws_lcd_154/README.md)) or **[ws_lcd_349/](ws_lcd_349/)** ([flashing guide](ws_lcd_349/README.md)); the wide board's privacy lock is still a placeholder
3. 📶 Hold **DOWN** for 2 s to raise the device's setup hotspot, join it, and fill in the full URL, API key/secret and your Wi-Fi networks at `192.168.4.1` → **[docs/functional-requirements.md §5](docs/functional-requirements.md#5--configuration)**
4. 🖼️ Or skip hardware entirely — open a display's `mockup.html` to see it rendered

## 🗺️ Next

- 🔐 Confirm HMAC signing on the device actually verifies
- 🖥️ Finish the landscape board (`ws_lcd_349/`) — the privacy lock's code entry and keypad
- 📦 Ship flashable `.bin` images for both boards
- 🔀 Merge the working branch to `main`
- ✍️ A public write-up — pick a home, then post it (LinkedIn and elsewhere)
