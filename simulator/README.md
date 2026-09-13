# ⚡ Pulsar simulator

A fake backend for [`docs/api.md`](../docs/api.md) — point a real device, or either
build's mockup, at this instead of a real server while you don't have one yet.

- 📡 Serves `GET /v1/gateway_health` — the real contract, full payload, two platforms
  merged into one response: **Orders** (Created, Dispatched, Cancelled) and
  **Payments** (Paid, Declined) — five metric screens
- 🖥️ Serves a control page at `/` — change the alert level, reshape any metric's
  graph, or fault the endpoint (timeouts, bad statuses, broken JSON), and the very
  next poll sees it
- 📦 **Zero dependencies.** Node core only (`http`, `fs`, `path`, `url`) — nothing to
  `npm install`, one file to run
- 📄 Ships an [OpenAPI spec](openapi.yaml) for the real contract — copy it into
  Swagger Editor, Postman, or a codegen tool as-is

---

## Requirements

**Node.js 20 LTS or newer** (built and tested on the current Node 22 LTS — run
`node --version` to check; download from <https://nodejs.org> if you need it).
Nothing else. No `npm install` step — the server has no dependencies to fetch.

## Run it

### Windows

1. Open **PowerShell** or **Command Prompt**
2. `cd` into this folder, e.g.:
   ```
   cd path\to\pulsar\simulator
   ```
3. Start it:
   ```
   node server.js
   ```
4. Open **http://localhost:4180/** in a browser

### macOS

1. Open **Terminal**
2. `cd` into this folder:
   ```
   cd path/to/pulsar/simulator
   ```
3. Start it:
   ```
   node server.js
   ```
4. Open **http://localhost:4180/**

### Linux

Same as macOS — a terminal, `cd` into `simulator/`, `node server.js`, then open
`http://localhost:4180/`.

### Changing the port

The default port is **4180**. Override it with the `PORT` environment variable:

```
# Windows (PowerShell)
$env:PORT=8080; node server.js

# Windows (cmd.exe)
set PORT=8080 && node server.js

# macOS / Linux
PORT=8080 node server.js
```

## Point a device at it

The device needs to reach this machine's LAN IP, not `localhost` — find it with
`ipconfig` (Windows) or `ifconfig` / `ip addr` (macOS/Linux). Then hold **LEFT** on the
device for 2 s, join the hotspot it raises, and put this in as the **URL** — the device
polls exactly what you paste, nothing appended:

```
http://<this-machine's-IP>:4180/v1/gateway_health
```

Leave the API key and secret empty too; this server does not check them.

- 🔓 **Plain `http://` is fine here and nowhere else.** The device allows it, and says
  `[PLAIN HTTP - the key is readable on the wire]` on every poll — which is exactly right
  for a laptop on your own bench and exactly wrong for anything else
- 🧯 **This is the cheapest way to see the error screens.** Arm a fault and watch
  `NO ACCESS`, `THROTTLED`, `BACKEND` or `OFFLINE` land on real hardware, over a real
  network, without breaking anything you care about → [api.md §6](../docs/api.md#6--errors)

---

## Using the control page

| Section     | What it does                                                                           |
| ----------- | --------------------------------------------------------------------------------------- |
| **Alert**   | Sets `alert.level` and `alert.message` for the next poll. `warning`/`critical` are what make a device's whole screen flash; `critical` also sounds. The message box is pre-filled with a sensible default per level — edit it if you want something else, 20 characters max |
| **Metrics** | One card per screen. Each has a live sparkline of its current `buckets` and a pattern dropdown — pick one and hit **Apply** to reshape that row's graph. the aggregate tiles' shares recompute to match automatically |
| **Faults**  | Makes the **next** poll misbehave instead of succeeding — a slow response, a non-200 status, broken JSON, or a payload missing something the contract requires. Stays armed until you clear it, so you can watch a device retry against the same problem more than once |
| **Log**     | Every hit on `/v1/gateway_health`, and every change made on this page, newest first |

Everything above talks to this server's own `/api/*` routes, which are separate
from `/v1/gateway_health` — the device (or mockup) never sees the control page or
its API, only the one real endpoint.

### Bucket patterns

| Pattern     | Shape                                                          |
| ----------- | --------------------------------------------------------------- |
| **steady**  | Flat, light noise — the resting state                           |
| **rising**  | Climbs across the 30-minute window                               |
| **falling** | Tails off across the window                                      |
| **dip**     | Sags in the middle, recovers by the end                          |
| **spike**   | A short burst near the newest end                                |
| **cliff**   | Falls off a ledge at the newest end, with elevated 4xx/5xx — pairs well with `critical` |
| **noisy**   | Wide random scatter, no shape                                    |

### Faults

| Fault                          | What the device sees                                              |
| ------------------------------- | --------------------------------------------------------------- |
| Long timeout (8 s)              | The response is delayed 8 seconds — past api.md's 5 s budget, so a well-behaved client should already have given up |
| HTTP 500 / 401 / 404 / 429 / 302 | The status api.md §6 defines a client behaviour for, with no body |
| Malformed JSON                  | A 200 with a truncated, invalid JSON body                        |
| Missing `alert`                 | A 200 with the whole `alert` object omitted                      |
| A row missing `gateway`         | The first metric row has no `gateway` — required per api.md §4   |
| A row with `aggregates: null`   | The first metric row's `aggregates` is `null`                    |
| Empty `metrics: []`             | A structurally valid response with nothing to show               |
| Oversized body                  | Padded past the 4 KB limit                                       |

Every one of these is exactly the kind of thing [`net.ino`'s `parseSnapshot()`](../ws_lcd_154/net.ino)
is written to survive — logging the problem to Serial and keeping the last good
screen, never crashing.

---

## API spec

[`openapi.yaml`](openapi.yaml) is a copy-pastable OpenAPI 3.0 mirror of
[`docs/api.md`](../docs/api.md) — one path (`GET /v1/gateway_health`), the full
`GatewayHealth` schema (`alert`, up to 5 `metrics[]` **screens**, each with up
to 5 `aggregates[]` **tiles**, plus `buckets`), and the same limits the doc
states. Tile formatting (label length, number compaction, units) is
[AGENTS.md](../AGENTS.md#10--aggregate-tile-formatting)'s, not repeated here.

Use it to:

- **Paste into [Swagger Editor](https://editor.swagger.io)** for an interactive,
  browseable version of the contract
- **Import into Postman or Insomnia** to get a ready-made request against
  either this simulator or a real backend
- **Feed a codegen tool** (`openapi-generator`, `orval`, etc.) to generate a
  typed client or server stub in any language
- **Validate a real backend's response** against the schema before pointing a
  device at it

It is served by this simulator too, so you can grab it straight from a running
instance instead of the repo:

```
curl http://localhost:4180/openapi.yaml
```

It is hand-maintained, not generated — if `docs/api.md` changes, update this
file to match in the same commit.

---

## What it doesn't do

- No persistence — restart the process and it's back to the Orders/Payments defaults
- No auth check — `Authorization` is ignored, on purpose, to keep this simple
- No HTTPS — plug this in behind a reverse proxy if you need TLS for a real test
