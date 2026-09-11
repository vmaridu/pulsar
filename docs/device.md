# 🔧 How every build behaves

Everything that does not change with the display. Pixel-dependent things live with their build.

📡 Contract → **[api.md](api.md)** · 🖥️ Builds → **[lcd154](../lcd154/docs/readme.md)** · **[lcd349](../lcd349/docs/readme.md)**

---

## 1. 🔁 The loop

- ⏱️ Poll **every 60 s**, plugged in or on battery
- 🌐 **Every** configured gateway polls every cycle — one nobody is watching still has to wake the speaker
- 🧊 A failed poll never clears the screen: last good payload stays, marked held → [api.md §6](api.md#6--errors)

---

## 2. 🔋 Power

| Power         | Screen                    | Poll           | Alert            |
| ------------- | ------------------------- | -------------- | ---------------- |
| 🔌 USB         | Always on                 | every **60 s** | Speaker + screen |
| 🔋 Battery     | **Off**                   | every **60 s** | **Speaker only** |
| 👆 Battery + tap | On for **60 s**, then off | every **60 s** | You read it      |

- 🎯 **The poll interval never changes.** Power changes the screen, never the radio — so `health` is as fresh on battery as docked
- 🧮 One number in the firmware instead of two

```cpp
bool plugged() {
  if (Power.isCharging()) return true;
  return Power.vbusMv() > 4000;             // ~5 V on VBUS
}
void displayOff() { lcd.setBrightness(0); lcd.sleep(); }
void displayOn()  { lcd.wakeup(); lcd.setBrightness(128); }
```

---

## 3. 🤝 Shake to refresh

Forcing a poll is a shake, not a button — the gesture people already make at something that looks stuck.

| Rule                | Value                       | Why                                          |
| ------------------- | --------------------------- | -------------------------------------------- |
| Jolt threshold      | > 0.9 g deviation from rest | A desk knock is well under this              |
| Jolts required      | **3**                       | One bump is an accident, three is intent     |
| Inside              | 700 ms                      | Spread them out and the count resets         |
| Per-jolt refractory | 80 ms                       | The loop runs faster than a hand moves       |
| Cooldown            | **5 s**                     | One shake = one request                      |

- 👁️ A shake also wakes the panel — one gesture for *show me, and make it current*
- 🛑 The cooldown is the important one: without it a device rattling in a bag hammers every endpoint, and **the monitor must never become the incident**
- 🔧 Knocked around a lot? Raise the **jolt count**, not the threshold — a higher threshold just means shaking harder

---

## 4. 🔔 The speaker

Fires on the **transition into** `warning`/`critical`, never on the state → [api.md §3](api.md#3--health).

| `health.level`  | Tones | Volume plugged | Volume on battery |
| --------------- | ----- | -------------- | ----------------- |
| 🟢 `info`        | none  | —              | —                 |
| 🟡 `warning`     | 2     | 128            | 80                |
| 🔴 `critical`    | 4     | 128            | 80                |

- ⚡ Four tones then stop. Longer, or above ~75 % on a small cell, sags the rail and reboots the device
- 🔇 A fetch fault **never beeps** — it blinks at the warning rate. A Wi-Fi roam must not sound like an outage

---

## 5. ⚙️ Configuration

`Preferences`, namespace `gw`. Added over the Wi-Fi portal — hold the build's portal key, join `Pulsar-Setup`, open `192.168.4.1`.

| Key     | Meaning                                     |
| ------- | ------------------------------------------- |
| `count` | gateways configured, **max 3**              |
| `u{i}`  | base URL, `https://…`                       |
| `t{i}`  | bearer token                                |

- 🔄 No reflash to add a gateway
- 📶 Wi-Fi networks live in namespace `wifi`, joined with `WiFiMulti` — the device roams saved SSIDs on its own
- 🏷️ The gateway **name** is never stored. It arrives in every payload

---

## 6. 🎨 Colours

Authored in hex, quantised to RGB565 — what you pick is not quite what the panel shows.

| Use        | Colour    | Use          | Colour    |
| ---------- | --------- | ------------ | --------- |
| Background | `#05070c` | 🟢 `info`     | `#22c55e` |
| Panel      | `#0c1422` | 🟡 `warning`  | `#f5a524` |
| Text       | `#e6edf7` | 🔴 `critical` | `#ff4d5e` |
| Dim        | `#7f8fa8` | 🩶 fetch fault | `#8aa0c0` |
| Rule       | `#1c2740` | 💠 accent      | `#22d3ee` |

- 💠 The accent is not a level — it is the graph's lit surface line and the poll hairline
- 🖤 Where a build paints the level colour as a **background**, the ink on it goes near-black — the only way one region reads on green, amber, red *and* grey

---

## 7. 🎞️ Motion

One off-screen sprite pushed per frame at ~25 fps. All of it stops the instant the panel sleeps.

| Motion           | How                                                    |
| ---------------- | ------------------------------------------------------ |
| Level blink      | Health region toggles two shades — 220 ms crit, 700 ms warn |
| Gateway change   | Two sprites at an x-offset, ~300 ms                    |
| Numbers count up | Redraw only the bands that changed, ~520 ms            |
| Graph sweep-in   | Columns left → right on new data, ~300 ms              |
| Poll in flight   | A hairline in the chrome brightens                     |
| Wake / sleep     | `setBrightness()` ramp                                 |

- ⛔ Never `fillScreen()` inside `loop()`
- ⛔ Never animate with the panel dark
