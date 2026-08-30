# ⚡ Pulsar

Pulsar is a portable, ultra-low-power IoT device that acts as a persistent smart display for real-time data monitoring. It uses a long-lifetime color TFT, and saves battery by powering the screen off completely whenever it is unplugged. Background polling continues while the display is dark; if the backend reports something important, the speaker sounds so you never miss an alert. A button press wakes the screen for 60 seconds so you can glance at what is happening. When a power supply is connected, the display stays fully on and the device polls the server every 20 seconds for alerts and display data. Beyond the visual interface, the onboard speaker signals high-priority events from a backend REST API. The project is a hardware and firmware framework for developers who want to bridge cloud information and physical, glanceable peripherals.

- 🌈 Long-lifetime **color TFT** display
- 🔋 On battery: screen **off**, background polling, **speaker** on important events
- 👆 On battery: button wakes the screen for **60 seconds**
- 🔌 On power: screen **always on**, polls every **20 seconds**
- 🌐 Flexible REST API sync plus speaker alerts

## 🔭 Overview

Pulsar is a compact, battery-powered smart display designed to sit on a desk or wall and act as a real-time peripheral for backend applications. It uses a color TFT panel for a long-lived, full-color interface, and saves energy by turning the screen completely off whenever it is running on battery. Background polling continues while the display is dark; important events wake the speaker so you never miss an alert. Plug it in and the screen stays on, refreshing from the server every 20 seconds.

## ✨ Core Capabilities

- 🔌 **Power-Aware Display** — On mains/USB power the color TFT stays fully on. On battery the display is powered off entirely so the device can run for weeks or months on a single charge.
- 📡 **Background Polling on Battery** — While unplugged, Pulsar keeps polling the backend in the background with the screen off. If anything important is found, the speaker sounds immediately.
- 👆 **Button Wake Peek** — On battery, a single button press wakes the screen so you can see what is happening. The display stays on for 60 seconds, then turns off again.
- 🖥️ **Always-On Docked Mode** — When a power supply is connected, the display remains fully on and the device polls the server every 20 seconds for alerts and display data.
- 🌈 **Color TFT Interface** — A long-lifetime color TFT shows rich status, alerts, and graphics.
- 🌐 **Flexible REST API Integration** — Communicates with any backend server over Wi-Fi, supporting standard HTTP methods and flexible authentication schemes (Bearer tokens, API keys, HMAC signatures, or basic auth).
- 🔔 **Speaker Alerts** — The onboard speaker draws attention when the backend raises an alert — even if the screen is off.
- 🔋 **Rechargeable Portable Power** — Powered by an integrated, rechargeable LiPo battery with onboard charging and power-source detection.

## 🛠️ Hardware

**[M5StickS3](docs/m5sticks3.md)** — ESP32-S3, 1.14" color TFT, speaker, 250 mAh battery, two user buttons.

How to program it → **[docs/tutorial.md](docs/tutorial.md)**  
Need vs other boards → **[docs/compare.md](docs/compare.md)**

## 🔄 Power Modes & Firmware Lifecycle

### 🔋 Battery Mode (unplugged)

1. 🌑 **Display Off** — The TFT is powered down completely. No backlight, no panel refresh.
2. 📡 **Background Poll** — The device periodically wakes, connects to Wi-Fi, and checks the backend for new data and alert flags.
3. 🚨 **Important Event** — If the server reports something important, the speaker sounds. The screen stays off unless the user asks for it.
4. 👆 **Button Wake** — Press the button to turn the display on and review the latest payload. After **60 seconds** of inactivity the screen turns off again.
5. 🌙 **Return to Dark Idle** — Connections are torn down and the device resumes low-power background polling.

### 🔌 Plugged-In Mode (power supply connected)

1. 💡 **Display Always On** — The color TFT stays fully powered and shows the latest dashboard / alert data.
2. ⏱️ **20-Second Poll** — Every **20 seconds** the device fetches alerts and display data from the configured backend.
3. 🖼️ **Live Refresh** — The screen updates when the payload changes. Alerts also drive the speaker.

The firmware detects USB/mains vs. battery at runtime and switches between these two modes automatically.

## 🗺️ Future Roadmap

- ☁️ **Cloud Control Panel** — A centralized SaaS dashboard to manage device fleets, push OTA (Over-The-Air) firmware updates, configure API polling endpoints, and monitor battery health remotely.
- 🎨 **Configurable Layout Engines** — Remote definition of color TFT screen templates and UI components directly from the control panel.
- 🔐 **Expanded Auth Handlers** — Built-in support for OAuth2 token rotation and dynamic secret management.

## 🚀 Getting Started

1. 📥 Clone the firmware repository.
2. 📖 Follow **[docs/tutorial.md](docs/tutorial.md)** — flash the M5StickS3, build the UI, HMAC REST, saved Wi-Fi, power modes, alarm.
3. ⚙️ Put API URL, key, and HMAC secret in `include/config.h`.
4. 🔌 USB = screen on, poll every 20 s. Battery = screen off, Btn A peek 60 s, speaker on alerts.
