# ⚡ Pulsar

An ultra-low-power, pocket-sized IoT edge device featuring an e-paper display, local audio-visual alerts, and scheduled REST API synchronization designed for continuous standalone operation.

## 🔭 Overview

Pulsar is a compact, battery-powered smart display designed to sit on a desk or wall and act as a real-time peripheral for backend applications. By combining an energy-efficient e-ink screen with a duty-cycled deep-sleep architecture, Pulsar operates for weeks or months on a single charge while periodically syncing data from backend services.

## ✨ Core Capabilities

- 💤 **Low-Power Duty Cycling** — Stays in deep-sleep mode (~microamps) for 60-second intervals, waking briefly to fetch data and refresh the display before returning to sleep.
- 🌐 **Flexible REST API Integration** — Communicates with any backend server over Wi-Fi, supporting standard HTTP methods and flexible authentication schemes (Bearer tokens, API keys, HMAC signatures, or basic auth).
- 📰 **E-Ink Visual Interface** — Utilizes a high-contrast electronic paper display that draws zero power to maintain static text or graphics, updating cleanly every minute.
- 🔔 **Multi-Modal Local Alerts** — Equipped with a single programmable RGB LED indicator and a compact piezo buzzer to draw immediate attention when alerts are triggered by the backend.
- 🚨 **Extended Alert State** — Automatically overrides standard sleep cycles to remain active, pulsing the RGB LED and sounding alerts for up to 3 minutes upon receiving high-priority signals.
- 🔋 **Rechargeable Portable Power** — Powered by an integrated, rechargeable LiPo battery system with onboard charging and power management.

## 🛠️ Hardware Specifications

| Component          | Specification / Requirement                                                             |
| ------------------ | --------------------------------------------------------------------------------------- |
| 📦 Form Factor     | Ultra-compact enclosure (< 3 x 3 inches)                                                |
| 🧠 Microcontroller | ESP32-series SoC (integrated Wi-Fi & Bluetooth, ultra-low-power co-processor)           |
| 🖥️ Display         | E-Paper / E-Ink display (optimized for static data visibility and zero idle power draw) |
| ⚡ Power Supply    | Rechargeable LiPo battery with integrated charging circuit                              |
| 💡 Indicators      | Single programmable RGB LED (status and visual alerts)                                  |
| 🔊 Audio           | Compact active/passive buzzer (audible notifications)                                   |

## 🔄 Firmware Architecture & Lifecycle

1. 😴 **Deep Sleep State** — The device shuts down radio peripherals and core CPU functions, drawing minimal current. An internal RTC timer tracks the 60-second window.
2. 📡 **Wake & Connect** — The device wakes up, establishes a fast Wi-Fi connection, and initializes network stacks.
3. 🔗 **API Synchronization** — A secure HTTPS request is dispatched to the configured backend endpoint. The server responds with display payload instructions and status flags.
4. 🖼️ **Display Refresh** — If data changes, the e-paper panel updates. If an alert flag is present, the device triggers the buzzer and locks into a 3-minute active alert mode.
5. 🌙 **Sleep Return** — The device tears down connections and drops back into deep sleep until the next cycle.

## 🗺️ Future Roadmap

- ☁️ **Cloud Control Panel** — A centralized SaaS dashboard to manage device fleets, push OTA (Over-The-Air) firmware updates, configure API polling endpoints, and monitor battery health remotely.
- 🎨 **Configurable Layout Engines** — Remote definition of e-ink screen templates and UI components directly from the control panel.
- 🔐 **Expanded Auth Handlers** — Built-in support for OAuth2 token rotation and dynamic secret management.

## 🚀 Getting Started (Development Setup)

1. 📥 Clone the firmware repository.
2. ⚙️ Configure your Wi-Fi credentials and backend target URL in `config.h`.
3. 💾 Flash the code to the ESP32 development board using VS Code (PlatformIO).
4. 🔌 Power the unit via USB or battery to begin the automated sync loop.
