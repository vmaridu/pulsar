# 📡 Pulsar gateway API

## One `GET`, one JSON object, no device knowledge required.

## 1. 🔌 Endpoint

```
GET {base_url}/v1/gateway_health
Authorization: Bearer <token>
Accept: application/json
```

- 🔗 **One URL per gateway.** The URL _is_ the id — no gateway id is sent, no query params
- 🔒 HTTPS only — plain HTTP hands the token to the network
- 🔑 `Authorization` required unless HMAC is configured → [appendix](#-appendix--hmac)

| Response | Must be                                      |
| -------- | -------------------------------------------- |
| Status   | `200` + `Content-Type: application/json`     |
| Size     | **≤ 4 KB** (a full 5-row payload is ~1.7 KB) |
| Time     | **≤ 5 s** — clients time out                 |

---

## 2. 📦 The object

````json
{
  "gateway": "Payouts",
  "measured_at": 1770000100,
  "health": {
    "level": "critical",
    "message": "5xx 2.14% max 0.50%"
  },
  "metrics": [
    {
      "name": "Overview",
      "bucket_unit": "minute",
      "bucket_size": 1,
      "bucket_count": 30,
      "aggregates": {
        "2xx_count": 17862,
        "4xx_count": 169,
        "5xx_count": 389,
        "avg_latency_ms": 48,
        "p95_latency_ms": 210
      },
      "buckets": [
        656, 662, 652, 658, 664, 654, 660, 650, 656, 660, 656, 662, 652, 658,
        664, 654, 659, 649, 655, 659, 654, 619, 569, 534, 500, 448, 414, 363,
        329, 292
      ]
    },
    {
      "name": "Init Payout",
      "bucket_unit": "minute",
      "bucket_size": 1,
      "bucket_count": 30,
      "aggregates": {
        "2xx_count": 6421,
        "4xx_count": 61,
        "5xx_count": 44,
        "avg_latency_ms": 39,
        "p95_latency_ms": 165
      },
      "buckets": [
        236, 233, 234, 231, 238, 230, 237, 228, 236, 232, 236, 232, 234, 230,
        238, 229, 237, 227, 235, 232, 237, 222, 211, 195, 189, 166, 160, 136,
        129, 111
      ]
    },
    {
      "name": "Disburse Payout",
      "bucket_unit": "minute",
      "bucket_size": 1,
      "bucket_count": 30,
      "aggregates": {
        "2xx_count": 5288,
        "4xx_count": 74,
        "5xx_count": 338,
        "avg_latency_ms": 74,
        "p95_latency_ms": 390
      },
      "buckets": [
        200, 202, 199, 201, 203, 200, 201, 198, 200, 201, 200, 202, 199, 201,
        203, 200, 201, 198, 200, 200, 194, 179, 161, 147, 134, 117, 106, 91, 80,
        70
      ]
    },
    {
      "name": "Acct Validations",
      "bucket_unit": "minute",
      "bucket_size": 1,
      "bucket_count": 30,
      "aggregates": {
        "2xx_count": 6153,
        "4xx_count": 34,
        "5xx_count": 7,
        "avg_latency_ms": 21,
        "p95_latency_ms": 62
      },
      "buckets": [
        220, 227, 219, 226, 223, 224, 222, 224, 220, 227, 220, 228, 219, 227,
        223, 225, 221, 224, 220, 227, 223, 218, 197, 192, 177, 165, 148, 136,
        120, 111
      ]
    }
  ]
}```

| Field             | Type   | Req | What it is                                              |
| ----------------- | ------ | --- | ------------------------------------------------------- |
| `gateway`         | string | ✅  | Name a person reads, **≤ 14** chars                     |
| `measured_at`     | number | ✅  | Unix seconds you **measured** — not when you answered   |
| `health`          | object | ✅  | Your verdict right now → [§3](#3--health)               |
| `metrics`         | array  | ✅  | **1–5 rows**, first is the whole gateway → [§4](#4--metrics) |
| `refresh_seconds` | number | ➖  | Suggested poll rate, advice only → [§7](#7--polling)    |

- 🚫 No top-level window — **each row carries its own clock**
- 🚫 No separate `overview` — it is `metrics[0]`, conventionally named `Overview`; **position** makes it the summary, never the name
- 🚫 Never send `null` — omit the field instead. Unknown fields are ignored

### 🪶 Minimum valid response

```json
{
  "gateway": "Fraud VAS",
  "measured_at": 1770000100,
  "health": { "level": "info", "message": "all systems nominal" },
  "metrics": [
    {
      "name": "Overview",
      "aggregates": {
        "2xx_count": 95865,
        "4xx_count": 298,
        "5xx_count": 77,
        "avg_latency_ms": 18,
        "p95_latency_ms": 42
      }
    }
  ]
}```

- `gateway` + `measured_at` + `health` + one row is the floor
- No `buckets` → no graph · No clock → thirty 1-minute buckets

---

## 3. 🩺 `health`

Exactly one, never an array. **Always sent** — including when everything is fine.

| Field     | Type   | Req | Rule                                    |
| --------- | ------ | --- | --------------------------------------- |
| `level`   | string | ✅  | `info` · `warning` · `critical`         |
| `message` | string | ✅  | **≤ 20** chars, one line, every level   |

| `level`     | Means                                   | Client                        |
| ----------- | --------------------------------------- | ----------------------------- |
| 🟢 `info`    | Nothing to act on — the resting state   | Green, steady, silent         |
| 🟡 `warning` | Degraded                                | Amber, slow blink, **sounds** |
| 🔴 `critical`| Broken                                  | Red, fast blink, **sounds**   |

---

## 4. 📊 `metrics`

Every entry is the same object. `metrics[0]` is the whole gateway; the rest are parts of it.

| Field          | Type     | Req | What it is                                     |
| -------------- | -------- | --- | ---------------------------------------------- |
| `name`         | string   | ✅  | What the row covers, **≤ 16** chars            |
| `bucket_unit`  | string   | ➖  | `second`·`minute`·`hour` — default `minute`    |
| `bucket_size`  | number   | ➖  | Units per bucket — default **1**, whole only   |
| `bucket_count` | number   | ➖  | Buckets in the row — default **30**, max **30** |
| `aggregates`   | object   | ✅  | The span reduced to numbers                    |
| `buckets`      | number[] | ➖  | The span spread back over time                 |

- 📐 **Cap is 5 rows.** Anything past the fifth is dropped — an attention limit, not a rendering one
- 🧭 3 gateways × 5 panels = **15 screens**, the whole product
- 📋 Order: summary first, then **most important first**
- 🔤 `name` values are plain ASCII Title Case and **stable across polls** — clients hold position by index
- ⚖️ Rows after the first should roughly reconcile with it
- 👁️ **Everything you send is displayed.** There is no field a client accepts and quietly ignores — if you want to add one, there is nowhere to put it

### ⏱️ The clock — `bucket_*`

````

span = bucket_size × bucket_count (in bucket_unit)

```

| `unit`   | `size` | `count` | Row covers | Caption drawn |
| -------- | ------ | ------- | ---------- | ------------- |
| `minute` | 1      | 30      | 30 minutes | `LAST 30 MIN` |
| `minute` | 5      | 12      | 1 hour     | `LAST 60 MIN` |
| `second` | 10     | 30      | 5 minutes  | `LAST 5 MIN`  |
| `hour`   | 1      | 24      | 1 day      | `LAST 24 HR`  |

- 🎚️ Three fields, not one `window_minutes` — one number says *how long*, never *how finely*
- 🌍 **Governs the whole row**, not just the graph. A row with no `buckets` still needs it: the span is the divisor under the biggest number on screen
- 🔤 Units singular and lowercase. Anything else reads as `minute`
- 🤝 Send the **same clock in every row** — panels are drawn side by side, and two resolutions is a comparison that lies
- 🏁 The newest bucket **ends at `measured_at`**; buckets are contiguous and equal; the oldest starts one span earlier
- ⛔ **Never send a bucket still filling.** A third-full bucket draws as a cliff and fakes an incident every poll — end at the last complete one
- 🕐 Clock-aligned boundaries (`:00`, `:01`) are nice, not required

### 🧮 `aggregates`

| Field            | Req | Means                                   | Do about it                            |
| ---------------- | --- | --------------------------------------- | -------------------------------------- |
| `2xx_count`      | ✅  | It worked                               | Nothing. The number worth watching     |
| `4xx_count`      | ✅  | The caller got it wrong                 | Someone else's bug, or a drifting client |
| `5xx_count`      | ✅  | **You** got it wrong                    | **This is what raises the level**      |
| `avg_latency_ms` | ✅  | Mean over the span, whole ms            | Watch against `p95`, not alone         |
| `p95_latency_ms` | ✅  | 95 % were faster than this, ms          | The tail is what people feel           |

- ➕ Counts are **sums** — two rows' counts add up and the answer is true
- 🚫 Mean and percentile are **not** — two p95s never average into a p95. Compute each row from its own requests, never from its children
- 🎯 The three counts are **disjoint**; 1xx and 3xx are not counted at all
- 🚫 No `all_count`, `total_count` or `error_count` — derived, and an "all" would be neither the sum nor the traffic you served
- 0️⃣ No traffic = three zeros, not a missing block. Zero is a fact
- ⏲️ Latency is whole milliseconds, never seconds, never `"210ms"`. `p95 ≥ avg`, always

### 📈 `buckets`

`2xx_count` per bucket — **counts, not rates**.

```

sum(buckets) = 17862 = 2xx_count ✅
bucket_size × bucket_count = 1 × 30 = 30 minutes
2xx_count / (1 × 30) = 595 = the headline, 2xx per minute
buckets[i] / bucket_size = 2xx per minute in bucket i

```

- ⬅️ **Oldest first**, newest last. No timestamps — the clock places them
- 📏 Exactly `bucket_count` long. Short is zero-padded at the old end, long is cut to the newest
- 0️⃣ **A gap is a zero.** No traffic, no data, exporter restarted, row younger than the span — all `0`. Never `null`, never a hole, never a short array
- 🟰 **`sum(buckets)` must equal `2xx_count`.** Same requests, two resolutions — derive both from one query or they drift
- 2️⃣ **2xx only, not configurable.** No `metric` field, no 4xx/5xx series. Totals here would leave the graph flat while the headline collapsed
- 🚫 Omit for a row with no graph. A row with three points looks broken

### 🚫 Nothing derived is ever sent

| Shown          | Client computes                             |
| -------------- | ------------------------------------------- |
| 2xx throughput | `2xx_count / (bucket_size × bucket_count)`  |
| total requests | `2xx_count + 4xx_count + 5xx_count`         |
| 4xx / 5xx rate | `count / total`                             |
| the span label | `bucket_size × bucket_count` in `bucket_unit` |

- 🚫 No `error_rate`, `request_count`, `throughput`, `rps`, `window_minutes`
- 🔢 Counts go **raw** — `18420`, not `"18.4k"`. Clients compact them; you do not know how much room they have
- ⚠️ A wrong clock silently scales the biggest number on screen. It is not a label

---

## 5. 📏 Limits

| Thing             | Limit            | Why                                   |
| ----------------- | ---------------- | ------------------------------------- |
| `gateway`         | **14** chars     | Rendered as the headline              |
| `name`            | **16** chars     | One label line                        |
| `health.message`  | **20** chars     | One line, never wrapped, every level  |
| `metrics`         | **5** entries    | Summary + at most four parts          |
| `bucket_count`    | **30**           | The graph is 228 px wide              |
| `bucket_size`     | ≥ **1**          | Whole units only                      |
| `buckets`         | = `bucket_count` | Short padded, long cut                |
| body              | **4 KB**         | Parsed on small hardware              |
| gateways / device | **3**            | 3 × 5 = 15 screens. A deployment rule, not enforced over the wire |

- ✂️ Clients **truncate, never wrap and never scroll**. Anything longer is cut without warning

---

## 6. ⚠️ Errors

Clients keep the **last good payload** on screen and mark it held. A failed poll must never look like a healthy zero.

| Status        | Shown as    | Client does                              |
| ------------- | ----------- | ---------------------------------------- |
| `200`         | `health`    | Parse, cache, render                     |
| `3xx`         | `REDIRECT`  | **Not followed** — fix the configured URL |
| `401` / `403` | `NO ACCESS` | Keep the last payload                    |
| `404`         | `NOT FOUND` | Keep the last payload                    |
| `429`         | `THROTTLED` | Back off a cycle, honours `Retry-After`  |
| `5xx`         | `BACKEND`   | Retry next cycle                         |
| timeout / DNS | `OFFLINE`   | Keep the last payload                    |

- 🟡 **Every fault gets warning treatment** — blinks at the warning rate, never the critical rate, never a beep. A Wi-Fi roam must not look like an outage
- 🔇 **Never silent either.** Stale numbers shown calmly read as good news
- 🩶 The fault banner is visually distinct from a real `warning` — "I cannot reach you" and "you say you are degraded" have different owners
- 🚫 Do not put error detail in a non-200 body. Clients read only the status

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
```
