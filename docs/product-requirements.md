# Pulsar — Product Requirements

This document states *what* Pulsar must do. It is deliberately silent on *how*:
no chip names, no pin numbers, no file names, no pixel coordinates. Any device
that satisfies every requirement here is a valid Pulsar client, regardless of
its screen size, input hardware, or implementation language. Today's two
reference builds (a small square touch display and a wide landscape display)
are two different answers to these same requirements, not the requirements
themselves.

---

## 1. Product concept

Pulsar is a desk display that answers one question at a glance: **is my API
gateway healthy?**

- One device, one HTTP endpoint. The device polls that single endpoint and
  renders whatever comes back — it does not know or care whether the backend
  behind that endpoint aggregates one platform or many.
- The device is **read-only**. It never acknowledges, dismisses, or writes
  anything back to the backend beyond the request needed to fetch data.
- The device must remain honest under failure: it never invents data, and it
  never presents stale data as current without saying so.

### Non-goals

- No multi-endpoint / multi-gateway management on the device (no gateway
  list, no gateway switching, no fleet view). A backend that merges several
  platforms into one response is the mechanism for showing more than one
  thing — not a device-side feature.
- No derived analytics computed by the device (no error rates, throughput,
  or windowed math the device invents from raw numbers). Everything shown is
  either sent explicitly or a straightforward display-only transform of what
  was sent (formatting, compaction, elapsed-time text).
- No local persistence of historical data beyond what is needed to render
  the current response and animate a transition from the previous one.

---

## 2. The data contract

### 2.1 Request

- The device issues a single HTTP `GET` to one fully-configured URL. Nothing
  is appended or assumed about the URL's shape.
- Authentication is exactly one of:
  - No credential at all.
  - A bearer token.
  - A signed request: a public identifier plus a per-request signature
    computed from a shared secret, a timestamp, and the request itself. The
    secret must never be transmitted, only used locally to compute the
    signature.
- A request that must be signed but cannot yet produce a trustworthy
  timestamp must not be sent — the device must wait rather than send a
  request guaranteed to be rejected.
- Any signing timestamp must be an absolute, timezone-independent instant
  (UTC/epoch). A device's own display timezone must never influence what it
  signs, even though it may influence what it *shows*.
- The device identifies itself to the backend (name/identifier, hardware
  address, and build/model) via request metadata, so a backend serving many
  devices can tell them apart. This identification is a courtesy to the
  backend, never a contract the backend depends on to answer correctly.

### 2.2 Response

- One JSON object per response, under a defined maximum size.
- The object carries:
  - A single **alert verdict**: one of three severities, plus one short
    human-readable message.
  - One to five **rows**, each describing one metric stream. Each row names
    the upstream platform it came from (so a backend merging platforms can
    still say which is which), a display name, and a time-series of raw
    counts over a defined bucket size/count/unit.
  - Each row carries one to five **summary values** ("tiles"): a name, a
    numeric value, an optional unit, and an optional severity used only to
    colour that one value. The first tile is the row's headline number.
- Nothing on the wire is pre-formatted for a screen: no pre-abbreviated
  numbers, no client-specific truncation, no computed percentages beyond two
  decimal places. Formatting for display is entirely the client's job.
- There is no secondary field duplicating a name (no separate "label"
  anywhere) and no field carrying information already derivable from another
  field on the wire.

### 2.3 Polling and freshness

- The device shows one metric row at a time, each for a fixed dwell time,
  cycling through all rows in the order the response listed them.
- The device refetches data when the cycle wraps back to the first row, but
  never faster than a floor interval computed from how many rows there are
  (more rows to show means more time has necessarily passed before the wrap,
  but the floor guarantees a minimum regardless).
- The person in front of the device can force an immediate refetch on
  demand, overriding the schedule.
- A rate-limited response must be honoured: the device must not poll again
  before the backend's stated retry delay, within a sane upper bound.

### 2.4 Errors

- A failed request or a malformed response **never clears the screen**. The
  last successfully parsed data stays visible, visibly marked as held, while
  a banner explains what is currently wrong.
- A device that has never once received usable data shows **no fabricated
  numbers at all** — an honest empty/setup state, never sample or placeholder
  data standing in for the real thing.
- Every distinct failure reason (no route configured, no network reachable,
  a network reachable but gated behind a sign-in step, a rejected credential,
  a wrong address, a redirect, a backend error or rate limit, a response the
  device cannot parse, a local clock not yet trustworthy enough to sign
  with) must produce a *specific*, human-readable banner — never a generic
  "error" and never silence.
- Recovery is automatic: the moment a request succeeds, the banner clears
  and normal display resumes. Nothing requires manual dismissal.
- Any value the response omits or sends with the wrong shape must degrade to
  a safe default and be noted in the device's own diagnostic log — never
  crash, never partially render, never guess a replacement value.

---

## 3. Display requirements

The display is divided into four functional regions, always present,
never overlapping, never trading jobs with each other:

1. **Device status** — battery/power state, the age or timestamp of the
   currently shown data, connectivity state, and any transient device-level
   indicator (e.g. a muted-sound indicator). Nothing about the metric itself
   lives here.
2. **Alert** — the current severity and its one-line message. Nothing else.
   When a fetch/connection problem is standing, this region shows *that*
   problem instead of the payload's own verdict — a live connection problem
   always outranks a stale "everything is fine" reading.
3. **Body** — the current row's headline number, its remaining summary
   values, and a visualization of its time series.
4. **Position/identity** — which upstream platform and which named metric is
   currently shown, and where the device is in its cycle through the
   available rows.

Requirements on these regions:

- A region's screen position and size are fixed for the life of a build —
  they never move, resize, or grow to accommodate more or less content.
  Fewer summary values than the maximum is a normal, expected case, not a
  degraded one — unused slots stay blank, nothing reflows to fill the gap.
- Every visible number is bounded to a small, fixed number of characters
  regardless of magnitude (compacted with a thousand/million/billion-style
  suffix once it would otherwise overflow that budget) and to a small,
  fixed number of decimal places for any percentage. Text never wraps; it
  truncates, except for the rare label a build chose to scroll instead —
  that build's own device.md says which.
- **A severity change is communicated with the whole display, not a corner
  of it.** Under a non-nominal severity (including a live connection
  problem), every region flashes to that severity's colour together, on a
  regular, predictable cadence tied to the display's own dwell cycle — never
  a separate free-running timer that can drift out of sync with everything
  else. During that flash, every region's foreground must remain legible
  against whatever colour it is flashing to.
- The three severities (plus a distinct "fetch/connection problem" state)
  must each read as visually distinct at a glance, including to someone who
  cannot rely on colour alone as much as most people can (sufficient
  contrast between a region's background and its foreground at every
  severity, not just the neutral state).
- A boot/startup sequence, if a build has one, must never block on a
  synthesized effect indefinitely — any accompanying animation or sound is
  bounded in duration and must not stall the device from reaching its normal
  display within a small, fixed time budget.

---

## 4. Interaction requirements

Every build implements the same functional set of gestures, however the
underlying controls are distributed across whatever physical controls
(buttons, touch surface, or a mix) that build actually has:

| Function | Gesture pattern |
| --- | --- |
| Move to the next metric row | short press |
| Force an immediate refetch | long press (a fixed, consistent hold duration across every gesture and every build) |
| Toggle a read-only device-diagnostics view | short press |
| Enter/exit a local setup mode for (re)configuring the device | long press |
| Toggle the display on/off (without stopping polling, alerting, or sound) | short press |
| Toggle sound muted/unmuted | a distinct short-press pattern (e.g. double press) |
| Power the device off, and back on | long press |
| Lock the display's data away, instantly, with no credential required | one gesture, when unlocked |
| Present a way to re-enter the credential and unlock | the *same* gesture, when locked |

Requirements:

- Any gesture not assigned a function must be explicitly inert and must say
  so in the device's own diagnostic log — never silently do nothing without
  a record of the fact, and never be repurposed later without updating this
  contract everywhere it is documented.
- Every long-press gesture uses the same fixed hold duration, and gives
  live, visible feedback while held (so the action can be seen coming and
  abandoned by releasing early) unless the gesture is genuinely unassigned.
- From any secondary view, there is always a way back to the primary
  display.
- A gesture that would move between metric rows must not fire from a
  passing touch that traveled — only a stationary press counts.

---

## 5. Alerting and sound requirements

- The two non-nominal severities and a live connection problem each get a
  sound, timed to start and end together with that same screen-wide flash —
  never lagging or leading it.
- The most severe level's sound must be clearly, audibly distinct from the
  sound used for the lesser non-nominal severity and for a connection
  problem — a person must be able to tell "something is actively broken"
  apart from "something is worth a glance" without looking at the screen.
- The nominal severity is silent.
- Sound continues to function when the display itself has been toggled off
  and while any secondary view is open — muting the screen must never mute
  the alert.
- A person can mute all sound with one gesture, and unmute it with the same
  gesture repeated.
- A mute must not be capable of surviving indefinitely by accident: it
  clears itself on a device restart (a device must never stay silently
  broken-and-muted forever because of one stale button press), and it must
  also clear itself automatically after a configurable duration — the
  device owner chooses "never," a short duration, or anything reasonable in
  between.
- A device that has just recovered from an under-voltage/brownout condition
  must start muted, so that a struggling power supply cannot be driven into
  a repeating restart loop by its own alert sound.
- Any synthesized sound must be pre-rendered rather than generated on the
  fly during time-critical processing, so that audio generation can never
  starve whatever process keeps the display responsive and free of restart
  loops.

---

## 6. Privacy lock requirements

A device sitting on a shared desk must be able to hide the actual metric
values from a passing glance while still visibly indicating overall health.

- The device supports a lock, guarded by a short numeric code, that hides
  the **Body** and **Position/identity** regions (§3) behind a neutral
  "locked" indicator. **Device status** and **Alert** are never hidden by
  the lock — severity and connectivity remain visible at all times, locked
  or not.
- **Locking requires no code.** Only *unlocking* does. A single gesture
  (§4) locks the device instantly with no credential prompt.
- The same gesture, while already locked, presents a way to enter the code.
  A correct code unlocks immediately; an incorrect code is rejected and
  cleared, and counts as a failed attempt.
- Entry may show the single most-recently-entered character briefly before
  masking it, the way a phone's PIN entry does, so a mis-entry is visible
  to whoever is looking at the device right now. This is the one allowed
  exception to "never shown" below, and only ever on the device's own
  display, never logged, and never more than that one most recent
  character at a time.
- The code is configurable through the device's normal local setup flow,
  ships with a reasonable non-blank default, and is never re-displayed once
  set — the setup flow may confirm that a code exists, never what it is.
  The code's character set must never exceed what the device's own entry
  method can actually produce — a device must never accept a code it could
  not later type back in.
- Repeated failed attempts must be rate-limited: after a small fixed number
  of failures within a rolling time window, further attempts are refused
  until enough of that window has elapsed. This must survive a restart —
  a device restart must never be usable to reset the count — which means
  the count, and when it was last reached, must be written to storage that
  survives power loss, not held only in memory. A correct code clears the
  count outright.
- The device's own diagnostic log must record that a lock/unlock/attempt
  happened and whether an attempt succeeded — and must **never** record the
  code itself, stored or entered, correct or not.
- **Every device restart re-locks the display**, regardless of whether it
  was unlocked immediately before the restart. The safe state is always the
  default; it is never assumed to persist from a prior session.
- Once unlocked, the device may automatically re-lock itself after a
  configurable idle duration (including "never"), independent of, and using
  the same style of option set as, the sound-mute auto-clear duration
  (§5) — the two are separate settings, not the same clock.
- The lock must never engage before the device has ever displayed real
  data, and must never engage on hardware that has no way to accept a code
  back from a person — a device that cannot ever be unlocked again must
  never lock itself in the first place.
- Nothing about the lock may alter sound behaviour, any other gesture's
  function, the device's diagnostics view, or its local setup flow. The
  lock's entire effect is limited to hiding and revealing the two regions
  named above.

---

## 7. Configuration requirements

- Nothing about the backend, credentials, or network is fixed at build
  time. A device with none of this configured must say so plainly and show
  no numbers rather than fall back to any built-in sample.
- All configuration is set through a local, on-device setup flow that does
  not depend on the backend being reachable, and does not depend on any
  other device or internet access to complete.
- The setup flow must let the owner configure, at minimum:
  - The full endpoint address.
  - A credential (and, if the signed-request mode in §2.1 is used, its
    secret).
  - However many networks or connection paths the device needs to reach
    that endpoint, tried in a stated priority order.
  - An optional trust anchor for verifying the backend's identity over an
    encrypted connection, with the device stating plainly whenever a
    connection is encrypted but *not* verified this way.
  - The mute auto-clear duration (§5) and the lock auto-relock duration (§6).
  - The lock's own code (§6).
  - A display time zone, chosen from a list of real zones rather than
    entered as a fixed offset, so daylight saving (where the zone observes
    it) is applied automatically for the life of the device — never a
    number an owner has to remember to change twice a year. This only ever
    affects what is shown, never what is signed (§2.1).
  - Where the display's brightness can be adjusted, separate levels for
    on-battery and on-charger operation, so battery life and readability
    can each be tuned on their own terms, switching the moment the device
    notices the power source change.
- The setup flow must offer a way to erase every configured value and
  saved connection path and return the device to its unconfigured,
  fresh-from-the-factory state — behind a confirmation deliberate enough
  that a stray tap or click cannot trigger it.
- **Every credential and the lock code are write-only from the setup flow's
  point of view.** The flow may state that a value has been set; it must
  never return the value itself. Submitting a blank field for one of these
  must mean "leave the existing value alone," never "clear it" — clearing
  must be an explicit, separate action.
- Saving configuration must clearly indicate whether the save succeeded,
  and the device's own display must be able to confirm — within a short
  time — whether the new configuration actually works.
- Anyone in physical range of the device during setup mode may open the
  setup flow; the requirement above (credentials never read back) is what
  keeps that safe, not restricting who can reach the page.

---

## 8. Power requirements

- Powering off and back on both use the same long-press gesture (§4); a
  press released before the hold completes must do nothing at all.
- A battery-powered build must protect its cell. Below a low-charge
  threshold, and not on external power, it announces the fact on screen
  and by sound, waits a short grace period, then shuts itself down —
  unless external power arrives during that period, which cancels it.
- Turning the display off (§4) never stops polling, alerting or sound.
- Where the display's brightness can be adjusted, it follows the power
  source as configured in §7.

---

## 9. Observability requirements

- A device with no other debugging interface must expose a live, human-
  readable diagnostic log covering, at minimum: every input gesture and the
  action it caused (or the fact that it was intentionally inert); every
  network request attempt, its outcome, and the real underlying error text
  on failure; every contract violation found in a response, logged *before*
  anything is rendered from it; power, display, mute, and view-state
  changes; and the reason the device most recently started up (so a device
  stuck in a restart loop can name its own cause).
- The log must never include a secret, credential, or the lock code, stored
  or entered, in any form.
- The log must record state changes and discrete actions, not continuous
  frame-by-frame activity — it must remain useful at a glance, not scroll
  past what matters.

---

## 10. Defensive rendering requirements

A response can legally omit any optional field, undersize any list, or
otherwise arrive in a minimal-but-valid shape. Every renderer — the device
firmware and any simulator or preview tool built to exercise it — must:

- Check that a value exists before drawing it; a missing optional value is
  a blank slot, never an error to surface to the person looking at the
  screen.
- Never index into a list or dereference a value without first confirming
  it is actually present and in range.
- Treat "fewer than the maximum" as the normal case for any list-shaped
  field, not a degraded one — no placeholder content, no layout shift, no
  crash.
- Have an explicit, testable case for every kind of missing/partial data a
  backend is allowed to send, not just the fully-populated example.

---

## 11. Extensibility requirements

- The set of requirements in this document must hold for any future
  display form factor. A new build is a new set of answers to these same
  requirements (its own screen layout, its own mapping of gestures onto
  whatever physical controls it has), not a reason to relax any requirement
  above.
- A capability that is removed from one build must be removed everywhere it
  was documented or simulated in the same change — no build, document, or
  preview tool may describe or exercise a capability that no longer exists.
- A capability that does not yet exist on a given build (e.g. because that
  build has no physical control capable of it, or has not been built yet at
  all) must be documented as absent on that build specifically, never
  silently omitted.
