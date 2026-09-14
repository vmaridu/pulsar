# 📡 Pulsar gateway API

One `GET`, one JSON object. This is the wire contract only — what a client
does with the response is out of scope here → **[functional
requirements](functional-requirements.md)**.

## 1. 🔌 Endpoint

```
GET {url}
Authorization: Bearer <token>
Accept: application/json
```

- 🔗 **One URL per client, exactly as configured.** Nothing is appended — whatever path you serve this from is what gets polled, verbatim
- 🧩 **Serve every metric from this one endpoint.** If your backend fronts several platforms, merge them here and tag each row with `gateway` → [§4](#4--metrics)
- 🔒 HTTPS only — plain HTTP hands the token to the network
- 🔑 `Authorization` required unless HMAC is configured → [§8](#8--hmac)
- 🏷️ Every request also carries `X-Device-Id`, `X-Device-Mac` and `X-Device-Board` — not part of the contract, and a server owes them nothing back. They exist purely so the other end can tell clients apart by more than source IP, if it wants to

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
**Payments** merged them into this one response.

| Field             | Type   | Req | What it is                                                                 |
| ----------------- | ------ | --- | -------------------------------------------------------------------------- |
| `measured_at`     | number | ✅  | Unix seconds you **measured** — not when you answered                      |
| `alert`           | object | ✅  | Your verdict right now → [§3](#3--alert)                                   |
| `metrics`         | array  | ✅  | **1–5 rows**, each names its own `gateway` → [§4](#4--metrics)             |
| `refresh_seconds` | number | ➖  | Suggested poll rate, advice only → [§7](#7--polling)                       |

- 🚫 **No root `gateway`.** The word only ever appears per row → [§4](#4--metrics) — a response doesn't have "a" gateway any more than it has "a" metric, only whatever the backend sends per row
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

| `level`       | Means                                 |
| ------------- | -------------------------------------- |
| 🟢 `info`     | Nothing to act on — the resting state |
| 🟠 `warning`  | Degraded                              |
| 🔴 `critical` | Broken                                |

- 🖥️ **The verdict belongs to the response as a whole, not to any one row** — it applies regardless of which metric a client happens to be showing → **[functional requirements](functional-requirements.md)** for how a client presents each level

---

## 4. 📊 `metrics`

Every entry is the same object. `metrics[0]` is the summary; the rest are parts of it. Up to 5 rows, the whole response.

| Field                | Type     | Req | What it is                                                                               |
| -------------------- | -------- | --- | ---------------------------------------------------------------------------------------- |
| `gateway`            | string   | ✅  | Which platform this row came from, **≤ 14** chars — required, no top-level fallback      |
| `name`               | string   | ✅  | **≤ 16** chars — the row's own display text, no separate label. Plain ASCII, stable across polls |
| `bucket_unit`        | string   | ➖  | `s`·`m`·`h` — default `m`                                                                |
| `bucket_size`        | number   | ➖  | Units per bucket — default **1**, whole only                                             |
| `bucket_count`       | number   | ➖  | Buckets in the row — default **30**, max **30**                                          |
| `buckets_value_type` | string   | ➖  | What one bucket counts — only `total_count`. **Required whenever `buckets` is sent**     |
| `buckets`            | number[] | ➖  | The span spread back over time                                                           |
| `aggregates`         | array    | ✅  | **1–5 stat tiles** for this row → [below](#-aggregates--tiles)                           |

### 📐 The rest

- 📐 **Cap is 5 rows**, not to be confused with the up-to-5 **tiles** inside each row's `aggregates` ([below](#-aggregates--tiles)) — two different caps, never conflate them. Anything past the fifth row is dropped
- 📋 Order: summary first, then **most important first**
- 🔤 `name` **is** the display text — no separate label. Clients hold position by it, so keep it stable across polls even though it's user-facing
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
- 🌍 **Governs the whole row**, not just the graph. A row with no `buckets` still needs it: the span is the divisor under the biggest number shown
- 🔤 **`bucket_unit` is a single lowercase letter** — `s` · `m` · `h`. Anything else reads as `m`
- 🖥️ **The caption (`LAST 30M`, `LAST 5M`, `LAST 24H`) is computed by the client, never sent on the wire** — nothing to keep in sync
- 🤝 Send the **same clock in every row** — rows are compared one after another, and two resolutions is a comparison that lies
- 🏁 The newest bucket **ends at `measured_at`**; buckets are contiguous and equal; the oldest starts one span earlier
- ⛔ **Never send a bucket still filling.** A third-full bucket draws as a cliff and fakes an incident every poll — end at the last complete one
- 🕐 Clock-aligned boundaries (`:00`, `:01`) are nice, not required

### 🧮 `aggregates` — tiles

Each row's `aggregates` is an array of **1–5 tiles**, not a fixed object — one tile per interesting number, not one field per status code. **Position 0 is the headline figure**; positions 1–4 are supporting figures, in order. A row with fewer than 5 tiles just leaves the remaining positions unused.

| Field   | Type   | Req | What it is                                                                              |
| ------- | ------ | --- | --------------------------------------------------------------------------------------- |
| `name`  | string | ✅  | **≤ 5 chars.** IS the text actually shown — no separate label                           |
| `value` | number | ✅  | The tile's number                                                                       |
| `unit`  | string | ➖  | `ms` · `s` · `%` · omitted for a plain count                                            |
| `level` | string | ➖  | `info` · `warning` · `critical` — default `info` → [below](#-level--a-tiles-own-severity) |

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

### 🚦 `level` — a tile's own severity

Any tile may carry `level`, the same three words as [`alert.level`](#3--alert): `info` · `warning` · `critical`. Omit it and the tile is `info` — a normal-looking number, nothing to flag.

- 🎯 **Per-tile, not per-row, and independent of `alert.level`.** `4XX` can be `warning` while `5XX` is `critical` on the same row — each tile speaks for itself, and neither one changes the response's own `alert` verdict
- 🚫 **The client never computes this.** No threshold baked in — a backend that wants `4XX` flagged past some rate sends `"level": "critical"` itself
- 🔕 **Scoped to that one tile only.** `alert` (§3) is the only field that speaks for the response as a whole

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
- ⚠️ A wrong clock silently scales the biggest number shown. It is not a label

---

## 5. 📏 Limits

| Thing                | Limit            | Why                                          |
| -------------------- | ---------------- | --------------------------------------------- |
| `gateway`            | **14** chars     | A short, stable identifying label            |
| row `name`           | **16** chars     | The row's own display text                   |
| `alert.message`      | **20** chars     | One line, never wrapped, every level         |
| `metrics`            | **5** entries    | Summary plus up to four more rows            |
| `aggregates`         | **1–5** entries  | One headline figure plus up to four more     |
| tile `name`          | **5** chars      | The tile's own display text                  |
| `bucket_count`       | **30**           | Bounds how much history one row carries      |
| `bucket_size`        | ≥ **1**          | Whole units only                             |
| `buckets`            | = `bucket_count` | Short padded, long cut                       |
| `buckets_value_type` | `total_count`    | The only series type defined so far          |
| body                 | **4 KB**         | Parseable on constrained hardware            |

- ✂️ Clients **truncate, never wrap and never scroll**. Anything longer is cut without warning

---

## 6. ⚠️ Errors

A client is expected to hold its last good payload through a failed poll
rather than discard it — see **[functional requirements](functional-requirements.md)**
for how that is presented. At the protocol level:

| Status        | Meaning here                    | Retry policy                    |
| ------------- | -------------------------------- | -------------------------------- |
| `200`         | A valid response body follows   | Normal schedule                 |
| `3xx`         | The configured URL is wrong     | Not followed automatically      |
| `401` / `403` | The credential was rejected     | Normal schedule                 |
| `404`         | The URL does not resolve        | Normal schedule                 |
| `429`         | Back off — honour `Retry-After` | Delayed by `Retry-After`        |
| `5xx`         | Server-side failure             | Normal schedule                 |
| timeout / no response | Unreachable              | Normal schedule                 |

- 🚫 **Do not put error detail in a non-200 body.** Only the status is read

---

## 7. ⏱️ Polling

The expected poll rate follows the number of rows you send:

```
poll interval = max(30, metric_count × 5) seconds
```

| Rows | Polls every |
| ---- | ----------- |
| 1    | **30 s**    |
| 3    | **30 s**    |
| 5    | **30 s**    |
| 8    | **40 s**    |

- 🛑 **Never faster than 30 s**, whatever the row count — the monitor must never become the incident
- 📅 The clock is how much history an answer covers, **not** how often you are asked. A 30-minute span polled every 30 s is intended
- 🤝 **Tolerate an off-cadence request.** A client may reasonably poll early on demand — cache it, don't treat it as an error
- ⏳ Never hold a request open for fresh data — answer in 5 s with what you have
- 🕰️ `measured_at` is when the sample was taken. Serving a cached row? Send the cached row's time
- 🎛️ `refresh_seconds` is clamped to **10–900 s** and treated as advice

---

## 8. 🔐 HMAC

Only when a static bearer token is unacceptable. Same endpoint, different headers — set an API secret on the client and the key stops being a bearer token.

```
GET {url}
X-Api-Key: <key>
X-Timestamp: <unix seconds>
X-Signature: <hex HMAC-SHA256>
Accept: application/json
```

| Header         | Value                                                          |
| -------------- | -------------------------------------------------------------- |
| `X-Api-Key`    | The public key for this client                                 |
| `X-Timestamp`  | Unix seconds — must be within **60 s** of the server           |
| `X-Signature`  | Hex HMAC-SHA256 of the string below, keyed with the API secret |

```
string_to_sign = "GET\n" + path + "\n" + timestamp + "\n"
signature      = hex( HMAC-SHA256(api_secret, string_to_sign) )
```

Four lines, newline-separated: method, path, timestamp, empty body. `GET` has no body, so the last line is blank. The secret is the HMAC key, never part of the message.

- 🔑 Empty secret → `Authorization: Bearer <key>` → [§1](#1--endpoint). Secret set → these three headers, no `Authorization`
- ⏳ Reject timestamps older than **60 s** — or newer than the server will accept
- 🕰️ **The client needs a trustworthy clock before signing anything.** One that has not yet established the real time must wait rather than sign a timestamp it knows is wrong
- 🚫 Do not send `Authorization` in this mode
