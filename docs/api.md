# Pulsar service API

A backend contract. One `GET`, one JSON object, no device knowledge required.

Anything that speaks this contract can be watched by Pulsar. Nothing in this document assumes a particular screen, chip, or client — the limits below are part of the contract, so any client can render a response without measuring it first.

The client that consumes it → **[stick.md](stick.md)** · Live payloads → **[ui.html](ui.html)**

---

## 1. Endpoint

```
GET {base_url}/v1/status
```

One URL per service. The URL **is** the service — no service id is ever sent by the client, and no query parameters are used. Two services means two URLs.

**Request headers**

| Header          | Value              | Required |
| --------------- | ------------------ | -------- |
| `Authorization` | `Bearer <token>`   | yes      |
| `Accept`        | `application/json` | yes      |

**Response**

| | |
| ------------- | ------------------------------------------------ |
| Status        | `200` with `Content-Type: application/json`       |
| Size          | **≤ 4 KB**                                        |
| Time          | **≤ 5 s** — clients time out                      |
| Transport     | HTTPS. Plain HTTP hands the token to the network. |

Bearer is the only scheme you need. If a static token is unacceptable, see [Appendix — HMAC](#appendix--hmac).

---

## 2. The object

Six fields. Nothing derivable is sent twice.

```json
{
  "service": "Payouts",
  "window_minutes": 30,
  "updated_at": 1770000100,
  "alarm":    { "state": "active", "level": "critical", "message": "5xx 2.14% max 0.50%" },
  "overview": { "name": "Overview", "avg_latency_ms": 48, "p95_latency_ms": 210,
                "count_2xx": 17862, "count_4xx": 169, "count_5xx": 389,
                "trend": [601, 618, 604, 588, 612, 341, 288, 240] },
  "metrics": [
    { "name": "Payout",  "avg_latency_ms": 61, "p95_latency_ms": 340,
      "count_2xx": 8709, "count_4xx": 114, "count_5xx": 377,
      "trend": [301, 308, 297, 302, 170, 141, 118] },
    { "name": "Account", "avg_latency_ms": 22, "p95_latency_ms": 65,
      "count_2xx": 9153, "count_4xx": 56, "count_5xx": 11,
      "trend": [307, 310, 305, 300, 171, 147, 122] }
  ]
}
```

| Field            | Type   | Required | What it means                                                     |
| ---------------- | ------ | -------- | ----------------------------------------------------------------- |
| `service`        | string | yes      | The name a person reads. Not an id — the URL is the id.           |
| `window_minutes` | number | no       | How much time every number covers. **Default 30.**                |
| `updated_at`     | number | yes      | Unix seconds when you measured. Not when you answered.            |
| `alarm`          | object | no       | Whether something is wrong right now. Absent means nothing is.    |
| `overview`       | object | yes      | The whole service as one row.                                     |
| `metrics`        | array  | no       | The same row, broken down. Max **2** — see [§5](#5-limits).        |

Unknown fields are ignored, so you may add your own. Never send `null` — omit the field instead.

---

## 3. `alarm`

Exactly one, never an array. If five things are wrong, decide which one a person should act on and send that.

| Field     | Type   | Required | Values                          | What it means                          |
| --------- | ------ | -------- | ------------------------------- | -------------------------------------- |
| `state`   | string | yes      | `ok` · `active`                 | Is it firing.                          |
| `level`   | string | when firing | `info` · `warning` · `critical` | How bad. Ignored when `state` is `ok`. |
| `message` | string | yes      | ≤ **20** chars                  | One short line. Shown in every state.  |

| `state`  | Meaning                             |
| -------- | ----------------------------------- |
| `ok`     | Nothing wrong. `level` is not read. |
| `active` | Firing.                             |

Two states, and there is deliberately no `acknowledged`. Pulsar is a **notification-only** device: it reads your endpoint and never writes back, so nothing a person does at the device could ever reach you. An acknowledgement state would be a control the device cannot honour.

If a human takes ownership of an incident in your own tooling and you want the stick to stop shouting, express that the way you express everything else — set `state` back to `ok`, or drop the `level` to `info`. The decision stays on your side, where the acknowledgement actually happened.

`message` is shown in **both** states, including `ok`, on one line of at most twenty characters.

The healthy message is the one people read most, because most of the time nothing is wrong. Do not waste it restating the word next to it. Say what you checked, or what the headroom looks like:

| Instead of        | Write                                          |
| ----------------- | ---------------------------------------------- |
| `OK`              | `all systems nominal`                          |
| `healthy`         | `error budget healthy`                         |
| `no errors`       | `quiet, models fresh`                          |
| `service is up`   | `all clear, watching`                          |
| `200`             | `well inside SLO`                              |
| `no alerts firing`| `nothing to report`                            |

The failing message has a different job: lead with the number and the line it crossed, because that is what someone acts on. `5xx 2.14% max 0.50%` beats `elevated error rate`, and `p95 690ms max 500ms` beats `latency degraded`. Twenty characters is enough for a value and a threshold if you drop the articles.

Clients render `warning` and `critical` differently from each other beyond colour, so the distinction is load-bearing. Do not use `critical` for something that can wait until Monday.

`active` is the state that means *interrupt someone*. Clients with an alerting channel act on the transition into `active` at `warning` or `critical`, and only on the transition — so an alarm that stays `active` across many polls alerts once, not every poll. Setting it back to `ok` when the problem clears is part of the contract; a client cannot tell the difference between "still broken" and "you forgot", and it has no way to ask.

---

## 4. `overview` and `metrics` — one shape

`overview` and every entry in `metrics` are **the same object**. `overview` covers the whole service; each `metrics` entry covers one part of it. A client stores one structure and renders one view for both.

| Field            | Type     | Required | What it means                                            |
| ---------------- | -------- | -------- | -------------------------------------------------------- |
| `name`           | string   | yes      | What this row covers. ≤ **16** chars.                     |
| `avg_latency_ms` | number   | yes      | Mean response time, whole milliseconds.                    |
| `p95_latency_ms` | number   | yes      | 95 % of requests were faster than this, ms.                |
| `count_2xx`      | number   | yes      | Requests that succeeded during the window.                 |
| `count_4xx`      | number   | yes      | Requests rejected as the caller's fault.                   |
| `count_5xx`      | number   | yes      | Requests that failed as **your** fault.                    |
| `trend`          | number[] | no       | `count_2xx` per minute across the window. ≤ **30** values.  |

### Count by status class, not by "error"

There is no `error_count` and no `request_count`. Requests are counted in the three classes that mean different things to whoever is holding the device:

| Field       | Means                        | What you do about it                        |
| ----------- | ---------------------------- | -------------------------------------------- |
| `count_2xx` | It worked                    | Nothing. This is the number worth watching.  |
| `count_4xx` | The caller got it wrong      | Usually someone else's bug, or an integration drifting. |
| `count_5xx` | **You** got it wrong         | This is the one that should wake people.     |

Lumping 4xx and 5xx into one "errors" figure hides the only distinction that matters at 3 a.m. A spike of 4xx after a client ships a bad release is not the same event as a spike of 5xx, and a single error rate cannot tell you which one you are looking at. Alarm on `count_5xx`.

Requests that are neither (1xx, 3xx) are not counted. If you need them, they belong in a service of their own.

### Nothing derived is ever sent

Five counters and a window produce everything a client shows:

| Shown              | Derived from                                     |
| ------------------ | ------------------------------------------------ |
| 2xx throughput     | `count_2xx / window_minutes`                      |
| total requests     | `count_2xx + count_4xx + count_5xx`               |
| 4xx rate           | `count_4xx / total`                               |
| 5xx rate           | `count_5xx / total`                               |

So there is **no** `error_rate`, **no** `request_count`, **no** `throughput` and **no** `rps` field. A second source of truth is a second thing that can be wrong, and the first symptom is a screen that disagrees with itself.

Counts are sent **raw** and abbreviated by the client — `18420` becomes `18.4k`, `2456789` becomes `2.5M`. Do not pre-format them, and do not round them for display; the client knows how much room it has and you do not.

The corollary: `window_minutes` is load-bearing. It is not a label — it is the divisor under the biggest number on the screen. If your numbers cover 5 minutes, say `5`.

### `trend`

`count_2xx` per minute — the same metric as the headline number, one value per minute, **oldest first**, newest last, evenly spaced across `window_minutes`.

```json
"trend": [601, 618, 604, 588, 612, 341, 288, 240]
```

2xx only, not total. The headline shows success throughput *now*; the graph shows how it got there. They are one metric at two time scales, so a client can put the number directly above the graph with nothing between them and no second label. Sending totals here breaks that — during an incident the graph would stay flat while the headline collapsed.

No timestamps — the right-hand end is *now* and the span is the window. At most 30 values; longer arrays are cut to the newest 30. Omit it entirely for a row that does not need a graph.

The values should reconcile with the counts: the mean of `trend` is roughly `count_2xx / window_minutes`. They will not match exactly and are not expected to, but a graph averaging 600 under a headline of 40 is a bug in your aggregation.

### Three panels, no more

`overview` plus `metrics` is what a person steps through one screen at a time, so the whole response is capped at **three panels**: the overview and **at most two** `metrics` entries.

```
Payouts     →  Overview · Payout · Account        3 panels
Fraud VAS   →  Overview · Eval                    2 panels
```

Anything past the second `metrics` entry is dropped. This is not a rendering limit — it is an attention limit. A glanceable device that needs six taps to get back where it started is not glanceable, and a service that genuinely has six interesting parts is really several services.

---

## 5. Limits

Hard parts of the contract. A client may truncate anything longer without warning, so treat these as maximums you design for, not as suggestions.

| Field                     | Limit           | Why                                              |
| ------------------------- | --------------- | ------------------------------------------------ |
| `service`                 | **14** chars    | Rendered as the headline.                        |
| `name` (overview/metrics) | **16** chars    | Rendered as a single label line.                 |
| `alarm.message`           | **20** chars    | Rendered as a **single** line, in every state.   |
| `metrics`                 | **2** entries   | 3 panels total with the overview.                |
| `trend`                   | **30** values   | One per minute over the default window.          |
| whole body                | **4 KB**        | Parsed on small hardware.                        |

And one limit that is not in the payload at all, because it is the operator's choice:

| Configuration        | Limit | Why                                                     |
| -------------------- | ----- | ------------------------------------------------------- |
| services per device  | **3** | 3 services × 3 panels = 9 screens, the most one key can walk without getting lost. |

Nothing enforces the three-service cap over the wire — it is a deployment rule. Design your endpoints so a device never has to be pointed at a fourth.

`alarm.message` gets **one line and no wrapping**. Twenty characters is a headline, not a sentence — `err 2.14% max 0.50%`, not `An automated check has determined that…`. Drop articles, use the shortest form of the number, and put the value before the threshold.

---

## 6. Data guidelines

Following these is what makes a response renderable without the client guessing.

**Types**
- Numbers are JSON numbers, never strings. `48`, not `"48"`.
- Latency is whole milliseconds. Never seconds, never `"210ms"`.
- Counts are integers. Send raw values — `18420`, not `"18.4k"` — clients compact them.
- No `null`. Omit optional fields.

**Consistency**
- `p95_latency_ms` ≥ `avg_latency_ms`.
- The three counts are disjoint. Every request lands in exactly one of them; none is a subset of another.
- A window with no traffic sends three zeros, not a missing object. Zero is a fact.
- Every row covers the same `window_minutes`. Do not mix a 5-minute row into a 30-minute response.
- `metrics` should roughly reconcile with `overview`. They need not sum exactly, but an overview quieter than every row it contains reads as a bug.

**Naming**
- Plain ASCII, Title Case, no emoji, no vendor prefixes: `Payout`, not `svc.payouts.v2.create`.
- `name` values are **stable across polls**. Clients hold your position in the list by index; renaming or reordering rows between polls moves the ground under the reader.
- `metrics` is ordered **most important first**, because that is the order a person walks through it.

**Freshness**
- `updated_at` is when the sample was taken. If you serve a cached row, send the cached row's time — clients dim numbers that are older than twice the window.
- Never hold a request open waiting for fresh data. Answer within 5 s with what you have.

**Alarms**
- One alarm, most severe wins.
- The message states the number and the limit it crossed.
- Return to `ok` when it clears.
- Do not encode alarms in the counts. A client cannot tell 400 expected 404s from an outage — that judgement is yours, and `alarm` is where it goes.

**Shape and scope**
- **Three panels per service, three services per device.** Nine screens is the whole product. Plan the split before you plan the fields.
- One service = one thing a person owns. If `metrics` wants a third entry, that is usually a second service, not a third row.
- Put the aggregate in `overview` and the two parts that most often explain it in `metrics`. Not the two with the most traffic — the two you would look at first when the alarm fires.
- 3 panels × 30 trend points is roughly 1 KB. That is the intended shape; 4 KB is headroom, not a target.
- If the story needs a paragraph, send a 40-character summary and keep the detail behind your own console.

**What gets prominence**

A client has one big number, a graph, and a few small figures. The order is fixed, so send data that survives it:

| Rank | Shown as            | From                                            |
| ---- | ------------------- | ----------------------------------------------- |
| 1    | The headline number | `count_2xx / window_minutes`, drawn **on** the graph |
| 2    | The graph itself    | `trend` — the **same** metric over the window       |
| 3    | Row                 | `p95_latency_ms`                                    |
| 4    | Row                 | `avg_latency_ms`                                    |
| 5    | Row                 | `count_4xx` with its rate                           |
| 6    | Row                 | `count_5xx` with its rate                           |

`p95` sits above `avg` because the tail is what people feel. The 4xx and 5xx rates are coloured by the client — 5xx goes amber past 0.5 % and red past 1 % — so they raise their own hand without you firing an alarm.

Every field you send is displayed. There is no field the client accepts and quietly ignores, which is the point: if you are tempted to add one, the answer is that there is nowhere to put it.

**Rendering assumptions you can rely on**
- Clients render a fixed layout with a fixed place for the alarm. Sending an `ok` alarm every poll is correct and expected — silence is not "no news", it is a blank.
- The message is never wrapped and never scrolled. Twenty characters, then it is cut.
- Clients truncate, they never wrap and never scroll. A string that does not fit is cut, not shrunk.
- **2xx throughput is the headline, and `trend` is the same metric.** Clients draw `count_2xx / window_minutes` directly on top of the `trend` graph, with no label between them. Send 2xx per minute in `trend`, not totals.
- **Alarm on 5xx, not on "errors".** A 4xx spike and a 5xx spike are different incidents with different owners.
- Nothing derived is sent, so `count_2xx`, `count_4xx`, `count_5xx` and `window_minutes` have to be right — a wrong `window_minutes` silently scales the biggest number on the screen.
- A row without `trend` renders with no graph and looks deliberate. A row with a two-point `trend` looks broken. Send 30 or none.

---

## 7. Minimum valid response

Every optional field removed. This renders:

```json
{
  "service": "Fraud VAS",
  "updated_at": 1770000100,
  "overview": {
    "name": "Overview",
    "avg_latency_ms": 18,
    "p95_latency_ms": 42,
    "count_2xx": 95865,
    "count_4xx": 298,
    "count_5xx": 77
  }
}
```

No `alarm` → the client shows a neutral OK. No `metrics` → there is nothing to step through. No `trend` → no graph. No `window_minutes` → 30.

---

## 8. Errors

Clients keep the **last good payload** on screen and mark it as held. A failed poll must never look like a healthy zero.

| Status          | Shown as    | Client behaviour                                 |
| --------------- | ----------- | ------------------------------------------------ |
| `200`           | the alarm   | Parse, cache, render.                            |
| `3xx`           | `REDIRECT`  | **Not followed.** Configure the final URL.       |
| `401` / `403`   | `NO ACCESS` | Bad or expired token. Keep the last payload.     |
| `404`           | `NOT FOUND` | The URL is wrong. Keep the last payload.         |
| `429`           | `THROTTLED` | Back off one cycle. `Retry-After` is honoured.   |
| `5xx`           | `BACKEND`   | Retry next cycle.                                |
| timeout / DNS   | `OFFLINE`   | Keep the last payload.                           |

### Every fault is a warning, never an outage

Anything that is not `200` gets **warning treatment**: a distinct fault banner that blinks at the warning rate — noticeable, but never at the critical rate and never with a beep.

That single rule is deliberate, and it cuts both ways:

- **Never `critical`.** A dropped packet, a Wi-Fi roam or a redeploy would otherwise scream like a real outage, and a device that cries wolf gets unplugged.
- **Never silent either.** A stick that quietly shows stale numbers is worse than one showing an error, because it looks like good news.

The fault banner is visually distinct from `warning` — a different colour and its own glyph — because "I cannot reach you" and "you told me you are degraded" are different problems with different owners. A client must not collapse them into one.

Redirects are **not followed**. A `301` means the URL you configured is not the URL that answers; fix the configuration rather than paying an extra round trip on every poll forever.

Do not put error detail in the body of a non-200. Clients only read the status.

---

## 9. Polling

The client calls **every 60 seconds**, plugged in or on battery. `window_minutes` is how much history each answer covers, **not** how often you are asked. Polling a 30-minute window once a minute is normal and intended: the alarm has to be fresh even though the numbers move slowly.

One extra call you must tolerate: a person can **shake the device** to demand a fresh reading, which fires a poll off-schedule. The client rate-limits this to one every 5 seconds, so the worst case is a burst of a few requests while someone is staring at the stick waiting for a number to move — not a sustained load. Cache accordingly; do not treat an off-cadence request as an error.

You may suggest a rate:

```json
"refresh_seconds": 60
```

Clamped to 10–900 s and treated as advice. Your endpoint must be cheap enough to answer at this rate — cache it.

---

## 10. Backend checklist

- [ ] `GET /v1/status` returns JSON under 4 KB in under 5 s
- [ ] Rejects a missing or wrong `Authorization` with `401`
- [ ] `service`, `updated_at` and `overview` always present
- [ ] `overview` carries all five required numbers, as numbers
- [ ] ≤ 2 `metrics` (3 panels total), same shape as `overview`, most important first
- [ ] The device this feeds is pointed at no more than 3 services
- [ ] `name` values stable and ordered stably across polls
- [ ] `trend` oldest-first, ≤ 30 values
- [ ] Exactly one `alarm`, and it returns to `ok` when the problem clears
- [ ] Lengths within [§5](#5-limits): 14 / 16 / 20
- [ ] No `error_rate`, no `request_count`, no `null`, no stringified numbers
- [ ] Counts sent raw, not pre-formatted as `18.4k`

---

## Appendix — HMAC

Only when a static bearer token is not acceptable. Same endpoint, different headers.

| Header        | Value                       |
| ------------- | --------------------------- |
| `X-Api-Key`   | public key for this service |
| `X-Timestamp` | unix seconds                |
| `X-Signature` | hex HMAC-SHA256             |

```
string_to_sign = "GET" + "\n" + path + "\n" + timestamp + "\n" + body
signature      = hex( HMAC-SHA256(secret, string_to_sign) )
```

`GET` has an empty body. Reject timestamps older than **60 s**. The client needs a real clock for this — plan for one that has just booted with no RTC.
