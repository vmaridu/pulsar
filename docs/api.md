# 📡 Pulsar gateway API

One `GET`, one JSON object, no device knowledge required.

🖥️ Builds that consume it → **[ws_lcd_154](../ws_lcd_154/device.md)** · **[ws_lcd_349](../ws_lcd_349/device.md)**

## 1. 🔌 Endpoint

```
GET {url}
Authorization: Bearer <token>
Accept: application/json
```

- 🔗 **One URL per device, exactly as configured.** Nothing is appended — whatever path you serve this from is what the device polls, verbatim
- 🧩 **Serve every screen from this one endpoint.** If your backend fronts several platforms, merge them here and tag each row with `gateway` → [§4](#4--metrics)
- 🔒 HTTPS only — plain HTTP hands the token to the network
- 🔑 `Authorization` required unless HMAC is configured → [appendix](#-appendix--hmac)

| Response | Must be                                      |
| -------- | -------------------------------------------- |
| Status   | `200` + `Content-Type: application/json`     |
| Size     | **≤ 4 KB** (a full 5-row payload is ~2.6 KB) |
| Time     | **≤ 5 s** — clients time out                 |

---

## 2. 📦 The object

```json
{
  "measured_at": 1770000100,
  "alert": {
    "level": "info",
    "message": "error budget healthy"
  },
  "metrics": [
    {
      "gateway": "Orders",
      "name": "Created",
      "bucket_unit": "m",
      "bucket_size": 1,
      "bucket_count": 30,
      "buckets_value_type": "total_count",
      "buckets": [
        649, 625, 606, 646, 647, 632, 636, 610, 640, 616, 607, 641, 621, 633,
        633, 613, 614, 611, 607, 627, 648, 609, 630, 627, 632, 610, 631, 618,
        630, 634
      ],
      "aggregates": [
        { "name": "2XX", "value": 18659 },
        { "name": "4XX", "value": 109, "level": "warning" },
        { "name": "5XX", "value": 15, "level": "critical" },
        { "name": "AVG", "value": 42, "unit": "ms" },
        { "name": "P95", "value": 180, "unit": "ms" }
      ]
    },
    {
      "gateway": "Payments",
      "name": "Paid",
      "bucket_unit": "m",
      "bucket_size": 1,
      "bucket_count": 30,
      "buckets_value_type": "total_count",
      "buckets": [
        488, 490, 468, 496, 484, 469, 502, 489, 480, 470, 484, 500, 490, 481,
        480, 495, 481, 480, 476, 500, 483, 470, 488, 468, 494, 483, 486, 487,
        480, 483
      ],
      "aggregates": [
        { "name": "2XX", "value": 14467 },
        { "name": "4XX", "value": 51, "level": "warning" },
        { "name": "5XX", "value": 7 },
        { "name": "AVG", "value": 71, "unit": "ms" },
        { "name": "P95", "value": 310, "unit": "ms" }
      ]
    }
  ]
}
```

Two rows, two `gateway` names — a backend that fronts both **Orders** and
**Payments** merged them into the one response this device shows.

| Field             | Type   | Req | What it is                                                                 |
| ----------------- | ------ | --- | -------------------------------------------------------------------------- |
| `measured_at`     | number | ✅  | Unix seconds you **measured** — not when you answered                      |
| `alert`           | object | ✅  | Your verdict right now → [§3](#3--alert)                                   |
| `metrics`         | array  | ✅  | **1–5 rows** ("screens"), each names its own `gateway` → [§4](#4--metrics) |
| `refresh_seconds` | number | ➖  | Suggested poll rate, advice only → [§7](#7--polling)                       |

- 🚫 **No root `gateway`.** The word only ever appears per row → [§4](#4--metrics) — a device doesn't have "a" gateway any more than it has "a" metric, it shows whatever the backend sends
- 🚫 No top-level window — **each row carries its own clock**
- 🚫 No separate `overview` — a row happens to be the summary by **position** (`metrics[0]`), never by name
- 🚫 Never send `null` — omit the field instead. Unknown fields are ignored

### 🪶 Minimum valid response

```json
{
  "measured_at": 1770000100,
  "alert": { "level": "info", "message": "all systems nominal" },
  "metrics": [
    {
      "gateway": "Payments",
      "name": "Paid",
      "aggregates": [{ "name": "2XX", "value": 95865 }]
    }
  ]
}
```

- `measured_at` + `alert` + one row (with its own `gateway` and at least one tile) is the floor
- No `buckets` → no graph · No clock → thirty 1-minute buckets

---

## 3. 🚨 `alert`

Exactly one, never an array. **Always sent** — including when everything is fine.

| Field     | Type   | Req | Rule                                  |
| --------- | ------ | --- | ------------------------------------- |
| `level`   | string | ✅  | `info` · `warning` · `critical`       |
| `message` | string | ✅  | **≤ 20** chars, one line, every level |

| `level`       | Means                                 | Client                   |
| ------------- | ------------------------------------- | ------------------------ |
| 🟢 `info`     | Nothing to act on — the resting state | Green, steady, silent    |
| 🟠 `warning`  | Degraded                              | Orange, flashes, silent  |
| 🔴 `critical` | Broken                                | Red, flashes, **sounds** |

- 🖥️ **The verdict belongs to the response, not to a row.** Clients flash the **whole screen** on `warning` and `critical`, whichever metric happens to be showing

---

## 4. 📊 `metrics`

Every entry is the same object. `metrics[0]` is the summary; the rest are parts of it. Each is one **screen** on the device — up to 5 screens, the whole product.

| Field                | Type     | Req | What it is                                                                               |
| -------------------- | -------- | --- | ---------------------------------------------------------------------------------------- |
| `gateway`            | string   | ✅  | Which platform this row came from, **≤ 14** chars — required, no top-level fallback      |
| `name`               | string   | ✅  | **≤ 16** chars — IS the FOOTER text, no separate label. Plain ASCII, stable across polls |
| `bucket_unit`        | string   | ➖  | `s`·`m`·`h` — default `m`                                                                |
| `bucket_size`        | number   | ➖  | Units per bucket — default **1**, whole only                                             |
| `bucket_count`       | number   | ➖  | Buckets in the row — default **30**, max **30**                                          |
| `buckets_value_type` | string   | ➖  | What one bucket counts — only `total_count`. **Required whenever `buckets` is sent**     |
| `buckets`            | number[] | ➖  | The span spread back over time                                                           |
| `aggregates`         | array    | ✅  | **1–5 stat tiles** for this screen's BODY band → [below](#-aggregates--tiles)            |

### 📐 The rest

- 📐 **Cap is 5 rows** — the metric **screens**, not to be confused with the up-to-5 **tiles** inside each row's `aggregates` ([below](#-aggregates--tiles)). Anything past the fifth row is dropped — an attention limit, not a rendering one
- 📋 Order: summary first, then **most important first**
- 🔤 `name` **is** the FOOTER text — no separate label. Clients hold position by it, so keep it stable across polls even though it's user-facing
- ⚖️ Rows after the first should roughly reconcile with it
- 👁️ **Everything you send is displayed.** There is no field a client accepts and quietly ignores — if you want to add one, there is nowhere to put it

### ⏱️ The clock — `bucket_*`

```
span = bucket_size × bucket_count (in bucket_unit)
```

| `unit` | `size` | `count` | Row covers | Caption drawn beside the heading      |
| ------ | ------ | ------- | ---------- | ------------------------------------- |
| `m`    | 1      | 30      | 30 minutes | `LAST 30M`                            |
| `m`    | 5      | 12      | 1 hour     | `LAST 1H` — 60 m compacts to hours    |
| `s`    | 10     | 30      | 5 minutes  | `LAST 5M` — 300 s compacts to minutes |
| `h`    | 1      | 24      | 1 day      | `LAST 24H`                            |

- 🎚️ Three fields, not one `window_minutes` — one number says _how long_, never _how finely_
- 🌍 **Governs the whole row**, not just the graph. A row with no `buckets` still needs it: the span is the divisor under the biggest number on screen
- 🔤 **`bucket_unit` is a single lowercase letter** — `s` · `m` · `h`. Anything else reads as `m`
- 🖥️ **The client computes and draws the span itself** — `LAST 30M`, `LAST 5M`, `LAST 24H` — beside the heading tile, in the second line only the heading tile draws. Not sent on the wire; nothing to keep in sync
- 🤝 Send the **same clock in every row** — screens are compared one after another, and two resolutions is a comparison that lies
- 🏁 The newest bucket **ends at `measured_at`**; buckets are contiguous and equal; the oldest starts one span earlier
- ⛔ **Never send a bucket still filling.** A third-full bucket draws as a cliff and fakes an incident every poll — end at the last complete one
- 🕐 Clock-aligned boundaries (`:00`, `:01`) are nice, not required

### 🧮 `aggregates` — tiles

Each row's `aggregates` is an array of **1–5 tiles**, not a fixed object — one tile per interesting number, not one field per status code. **Position 0 is the heading** (the big number at the top of the screen); tiles 1–4 fill the four BODY slots, in order. A row with fewer than 5 tiles just leaves the remaining slots blank.

| Field   | Type   | Req | What it is                                                                              |
| ------- | ------ | --- | --------------------------------------------------------------------------------------- |
| `name`  | string | ✅  | **≤ 5 chars.** IS the text actually drawn — no separate label                           |
| `value` | number | ✅  | The tile's number                                                                       |
| `unit`  | string | ➖  | `ms` · `s` · `%` · omitted for a plain count                                            |
| `level` | string | ➖  | `info` · `warning` · `critical` — default `info` → [below](#-level--a-tiles-own-colour) |

```json
"aggregates": [
  { "name": "2XX", "value": 18659 },
  { "name": "4XX", "value": 109,   "level": "warning" },
  { "name": "5XX", "value": 15,    "level": "critical" },
  { "name": "AVG", "value": 42,  "unit": "ms" },
  { "name": "P95", "value": 180, "unit": "ms" }
]
```

- 🎯 **One number, one unit, one job.** There is no second value bolted on beside it any more — a tile that needs a second number is a second tile
- 🧩 **A client that gets fewer than 5 tiles just leaves the remaining slots blank** — never an error, never a crash. 1–5 is the whole valid range, not just 5
- 0️⃣ No traffic = tiles with `0`, not a missing array. Zero is a fact
- ⏲️ Latency values are whole milliseconds; the client rescales to `s` for display past 9999 — a display concern, not this doc's

### 🚦 `level` — a tile's own colour

Any tile may carry `level`, the same three words as [`alert.level`](#3--alert): `info` · `warning` · `critical`. Omit it and the tile is `info` — a normal-looking number, nothing to flag.

| `level`       | Colour on the tile's value    |
| ------------- | ----------------------------- |
| 🟢 `info`     | Normal theme colour (default) |
| 🟠 `warning`  | Orange                        |
| 🔴 `critical` | Red                           |

- 🎯 **Per-tile, not per-row.** `4XX` can be `warning` while `5XX` is `critical` on the same screen — each tile speaks for itself
- 🚫 **The device never computes this.** No threshold baked into the client — a backend that wants `4XX` red past some rate sends `"level": "critical"` itself
- 🔕 A tile's `level` never sounds or flashes anything — that's `alert`'s job ([§3](#3--alert)). This only changes one number's colour

### 📈 `buckets` + `buckets_value_type`

`buckets_value_type` names what one bucket counts. **`total_count` is the only type defined** — each bucket is the `total_count` of its slice of the span, **counts, not rates**.

```
buckets_value_type = "total_count"
bucket_size × bucket_count = 1 × 30 = 30 minutes
buckets[i] / bucket_size = requests per minute in bucket i
the "2XX" tile's value / (1 × 30) = 622 = 2XX per minute
```

- ⬅️ **Oldest first**, newest last. No timestamps — the clock places them
- 📏 Exactly `bucket_count` long. Short is zero-padded at the old end, long is cut to the newest
- 0️⃣ **A gap is a zero.** No traffic, no data, exporter restarted, row younger than the span — all `0`. Never `null`, never a hole, never a short array
- 🏷️ **Send `buckets_value_type` whenever you send `buckets`.** A client that meets a type it does not know draws no graph rather than guess — the numbers still show
- 1️⃣ One series per row. No 2xx / 4xx / 5xx series yet — a new series is a new `buckets_value_type`, never a second array
- 🚫 Omit both for a row with no graph. A row with three points looks broken

### 🚫 Nothing is ever derived

Nothing on the wire is computed from something else already on the wire — `level` is the backend's own judgement call, not a number crunched from other fields.

| Shown               | Client computes                               |
| ------------------- | --------------------------------------------- |
| a tile's throughput | `value / (bucket_size × bucket_count)`        |
| the span label      | `bucket_size × bucket_count` in `bucket_unit` |

- 🚫 No `error_rate`, `request_count`, `throughput`, `rps`, `window_minutes`
- 🔢 Numbers go **raw** — `18783`, not `"18.8k"`. Compaction and unit rescaling are the client's job
- ⚠️ A wrong clock silently scales the biggest number on screen. It is not a label

---

## 5. 📏 Limits

| Thing                | Limit            | Why                                                        |
| -------------------- | ---------------- | ---------------------------------------------------------- |
| `gateway`            | **14** chars     | Rendered in the FOOTER                                     |
| row `name`           | **16** chars     | One FOOTER line — IS the text drawn                        |
| `alert.message`      | **20** chars     | One line, never wrapped, every level                       |
| `metrics`            | **5** entries    | 5 screens — summary + four parts                           |
| `aggregates`         | **1–5** entries  | Up to 5 tiles per screen — heading + up to four body slots |
| tile `name`          | **5** chars      | One BODY tile line — IS the text drawn                     |
| `bucket_count`       | **30**           | The graph is 228 px wide                                   |
| `bucket_size`        | ≥ **1**          | Whole units only                                           |
| `buckets`            | = `bucket_count` | Short padded, long cut                                     |
| `buckets_value_type` | `total_count`    | The only series type defined so far                        |
| body                 | **4 KB**         | Parsed on small hardware                                   |

- ✂️ Clients **truncate, never wrap and never scroll**. Anything longer is cut without warning

---

## 6. ⚠️ Errors

Clients keep the **last good payload** on screen and mark it held. A failed poll must never look like a healthy zero.

| Status        | Shown as    | Client does                               |
| ------------- | ----------- | ----------------------------------------- |
| `200`         | `alert`     | Parse, cache, render                      |
| `3xx`         | `REDIRECT`  | **Not followed** — fix the configured URL |
| `401` / `403` | `NO ACCESS` | Keep the last payload                     |
| `404`         | `NOT FOUND` | Keep the last payload                     |
| `429`         | `THROTTLED` | Back off a cycle, honours `Retry-After`   |
| `5xx`         | `BACKEND`   | Retry next cycle                          |
| timeout / DNS | `OFFLINE`   | Keep the last payload                     |

- 🟠 **Every fault gets alert treatment** — it flashes on the same 5 s pattern as `warning` and `critical`, but never sounds. A Wi-Fi roam must not sound like an outage
- 🔇 **Never silent either.** Stale numbers shown calmly read as good news
- 🩶 The fault banner is visually distinct from a real `warning` — "I cannot reach you" and "you say you are degraded" have different owners
- 🪵 **Every outcome is logged on the device's serial port**, status and error text alike → [device.md §2](device.md#2--logging)
- 🚫 Do not put error detail in a non-200 body. Clients read only the status

---

## 7. ⏱️ Polling

The device shows each metric for **5 s** and refetches when the cycle wraps, so the poll rate follows the number of rows you send:

```
poll interval = max(30, metric_count × 5) seconds
```

| Rows | Cycle | Polls every |
| ---- | ----- | ----------- |
| 1    | 5 s   | **30 s**    |
| 3    | 15 s  | **30 s**    |
| 5    | 25 s  | **30 s**    |
| 8    | 40 s  | **40 s**    |

- 🛑 **Never faster than 30 s**, whatever the row count — the monitor must never become the incident
- 📅 The clock is how much history an answer covers, **not** how often you are asked. A 30-minute span polled every 30 s is intended
- 🤝 Tolerate an off-cadence request: a person can **hold the touch glass 2 s** to force a poll. Cache; do not treat it as an error
- ⏳ Never hold a request open for fresh data — answer in 5 s with what you have
- 🕰️ `measured_at` is when the sample was taken. Serving a cached row? Send the cached row's time — clients dim numbers older than **twice the span**
- 🎛️ `refresh_seconds` is clamped to **10–900 s** and treated as advice
- 🌑 **The screen going dark changes nothing.** Polling and alerts carry on

---

## 🔐 Appendix — HMAC

Only when a static bearer token is unacceptable. Same endpoint, different headers.

| Header        | Value                       |
| ------------- | --------------------------- |
| `X-Api-Key`   | public key for this gateway |
| `X-Timestamp` | unix seconds                |
| `X-Signature` | hex HMAC-SHA256             |

```
string_to_sign = "GET" + "\n" + path + "\n" + timestamp + "\n" + body
signature = hex( HMAC-SHA256(secret, string_to_sign) )
```

- `GET` has an empty body
- Reject timestamps older than **60 s**
- The client needs a real clock — plan for one that just booted with no RTC
