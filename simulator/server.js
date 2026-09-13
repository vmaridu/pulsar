#!/usr/bin/env node
/* ===========================================================================
   Pulsar simulator — a fake backend for docs/api.md.

   Zero dependencies, plain Node core (http/fs/path/url). Nothing to
   `npm install`; `node server.js` just runs it. See README.md for how to
   point a real Pulsar device (or the mockups) at it.

   Two things live behind this one process:
     GET  /v1/gateway_health   the actual contract — what a device polls
     GET  /                    the control UI — a page for a person, not a device

   The control UI talks to a small JSON API (POST /api/...) that mutates the
   same in-memory STATE the device endpoint reads from, so a change made in
   the browser is what the next poll sees.
   =========================================================================== */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const { URL } = require('url');

const PORT = process.env.PORT ? Number(process.env.PORT) : 4180;

/* --------------------------------------------------------------- tiles
   One stat tile — see the aggregate-tile guideline in AGENTS.md. `name`
   IS the text drawn (<= 5 chars) — no separate label. `value` + optional
   `unit` is the whole number; `level` (optional, default info) is this
   backend's own judgement call — the client never computes it.          */
function tile(name, value, unit, level) {
  const t = { name, value };
  if (unit) t.unit = unit;
  if (level) t.level = level;
  return t;
}
function pct(n, total) { return total ? Math.round((n * 100 / total) * 100) / 100 : 0; }
/* Same thresholds the client used to bake in before `level` existed —
   now it's this backend's call, sent on the wire instead of assumed.    */
function levelFor(name, share) {
  if (name === '5XX') return share >= 1 ? 'critical' : share >= 0.5 ? 'warning' : undefined;
  if (name === '4XX') return share >= 2 ? 'warning' : undefined;
  return undefined;
}

/* ------------------------------------------------------------- the model
   Same shape, same numbers, as docs/api.md §2's example object — Orders
   and Payments, the same buckets and aggregates.
   Position 0 of `aggregates` is the heading (2XX); the rest fill the four
   BODY slots.                                                            */
function row(gateway, name, buckets, c4, c5, avg, p95) {
  const total = buckets.reduce((a, b) => a + b, 0);
  const c2 = total - c4 - c5;
  return {
    gateway, name,
    bucket_unit: 'm', bucket_size: 1, bucket_count: 30,
    buckets_value_type: 'total_count',
    buckets,
    aggregates: [
      tile('2XX', c2),
      tile('4XX', c4, null, levelFor('4XX', pct(c4, total))),
      tile('5XX', c5, null, levelFor('5XX', pct(c5, total))),
      tile('AVG', avg, 'ms'),
      tile('P95', p95, 'ms'),
    ],
  };
}
/* buckets are the only source of truth for scale after a pattern
   regenerates them: 2XX follows whatever the series did, 4XX/5XX counts
   hold steady, only their shares — and so their level — move.           */
function retotal(r, p4pct, p5pct) {
  const total = r.buckets.reduce((a, b) => a + b, 0);
  const byName = (n) => r.aggregates.find((t) => t.name === n);
  const t2 = byName('2XX'), t4 = byName('4XX'), t5 = byName('5XX');
  t4.value = Math.round(total * p4pct / 100);
  t5.value = Math.round(total * p5pct / 100);
  t2.value = Math.max(0, total - t4.value - t5.value);
  const l4 = levelFor('4XX', pct(t4.value, total));
  const l5 = levelFor('5XX', pct(t5.value, total));
  if (l4) t4.level = l4; else delete t4.level;
  if (l5) t5.level = l5; else delete t5.level;
}

function defaultMetrics() {
  return [
    row('Orders', 'Created',
      [649,625,606,646,647,632,636,610,640,616,607,641,621,633,633,
       613,614,611,607,627,648,609,630,627,632,610,631,618,630,634], 109, 15, 42, 180),
    row('Orders', 'Dispatched',
      [304,302,302,291,302,296,313,291,309,302,302,300,285,298,289,
       287,285,306,302,303,314,302,291,313,288,293,303,299,290,292], 40, 5, 55, 230),
    row('Orders', 'Cancelled',
      [40,39,41,37,40,38,38,40,41,39,37,39,39,40,39,
       38,41,38,39,40,37,38,40,39,40,37,38,40,39,38], 11, 2, 61, 240),
    row('Payments', 'Paid',
      [488,490,468,496,484,469,502,489,480,470,484,500,490,481,480,
       495,481,480,476,500,483,470,488,468,494,483,486,487,480,483], 51, 7, 71, 310),
    row('Payments', 'Declined',
      [27,26,28,27,28,26,27,27,27,26,28,27,27,26,27,
       28,27,26,27,28,27,26,27,27,26,28,27,27,28,26], 9, 2, 68, 260),
  ];
}

const DEFAULT_MESSAGE = {
  info: 'error budget healthy',
  warning: '5xx 0.68% max 0.50%',
  critical: '5xx 2.14% max 0.50%',
};

function defaultState() {
  return {
    alert: { level: 'info', message: DEFAULT_MESSAGE.info },
    metrics: defaultMetrics(),
    fault: null,          // null | fault-type key
  };
}

let STATE = defaultState();
const LOG = [];          // recent /v1/gateway_health hits, newest first
function logHit(line) {
  const t = new Date().toTimeString().slice(0, 8);
  LOG.unshift(`[${t}] ${line}`);
  if (LOG.length > 60) LOG.length = 60;
  console.log(`[${t}] ${line}`);
}

/* ------------------------------------------------------------- devices
   In memory only — no file, nothing survives a restart, on purpose. Keyed
   by X-Device-Mac (falling back to X-Device-Id, then source IP, so curl
   and the mockups still show up as *something* while testing). At most
   MAX_DEVICES tracked at once, least-recently-seen evicted first; each
   device keeps at most MAX_HISTORY of its own polls, oldest trimmed.     */
const MAX_DEVICES = 10;
const MAX_HISTORY = 500;
const ACTIVE_MS = 2 * 60 * 1000;      // "connected" dot: seen in the last 2 minutes
const DEVICES = new Map();

function touchDevice(req) {
  const mac   = req.headers['x-device-mac']   || null;
  const id    = req.headers['x-device-id']    || null;
  const board = req.headers['x-device-board'] || null;
  const ip    = (req.socket.remoteAddress || '').replace(/^::ffff:/, '');
  const key   = mac || id || ip || 'unknown';
  const now   = Date.now();

  let d = DEVICES.get(key);
  if (!d) {
    if (DEVICES.size >= MAX_DEVICES) {
      let oldestKey = null, oldestSeen = Infinity;
      for (const [k, v] of DEVICES) if (v.lastSeen < oldestSeen) { oldestSeen = v.lastSeen; oldestKey = k; }
      if (oldestKey !== null) DEVICES.delete(oldestKey);
    }
    d = { key, mac, id, board, ip, firstSeen: now, lastSeen: now, pollCount: 0, history: [] };
    DEVICES.set(key, d);
  }
  d.mac = mac || d.mac;
  d.id = id || d.id;
  d.board = board || d.board;
  d.ip = ip || d.ip;
  d.lastSeen = now;
  return d;
}
function recordPoll(dev, status, label) {
  if (!dev) return;
  dev.pollCount++;
  dev.history.unshift({ t: Date.now(), status, label });
  if (dev.history.length > MAX_HISTORY) dev.history.length = MAX_HISTORY;
}
function devicesJSON() {
  const now = Date.now();
  return [...DEVICES.values()]
    .sort((a, b) => b.lastSeen - a.lastSeen)
    .map((d) => ({
      mac: d.mac, id: d.id, board: d.board, ip: d.ip,
      firstSeen: d.firstSeen, lastSeen: d.lastSeen, pollCount: d.pollCount,
      active: now - d.lastSeen < ACTIVE_MS,
      recent: d.history.slice(0, 20),
    }));
}

/* ------------------------------------------------------- bucket patterns
   Each one is a shape for a 30-point series, base-scaled. A "label" ships
   alongside for the UI so a person sees what each button does without
   guessing.                                                              */
const PATTERNS = {
  steady:  { label: 'Steady — flat with light noise, nothing happening',
             gen: (b, t) => b * (0.95 + Math.random() * 0.1) },
  rising:  { label: 'Rising — climbs across the window',
             gen: (b, t) => b * (0.55 + 0.9 * t) * (0.95 + Math.random() * 0.1) },
  falling: { label: 'Falling — tails off across the window',
             gen: (b, t) => b * (1.45 - 0.9 * t) * (0.95 + Math.random() * 0.1) },
  dip:     { label: 'Dip — a sag in the middle, recovers by the end',
             gen: (b, t) => b * (1 - 0.6 * Math.exp(-((t - 0.5) ** 2) / 0.015)) * (0.95 + Math.random() * 0.1) },
  spike:   { label: 'Spike — a short burst near the end',
             gen: (b, t) => b * (1 + 2.2 * Math.exp(-((t - 0.75) ** 2) / 0.003)) * (0.95 + Math.random() * 0.1) },
  cliff:   { label: 'Cliff — falls off a ledge at the newest end (pairs well with critical)',
             gen: (b, t) => t < 0.72 ? b * (0.95 + Math.random() * 0.1)
                                      : b * (0.95 + Math.random() * 0.1) * Math.max(0.04, 1 - (t - 0.72) / 0.28) },
  noisy:   { label: 'Noisy — wide random scatter, no shape',
             gen: (b) => b * (0.35 + Math.random() * 1.3) },
};
/* a pattern that visibly degrades traffic gets to bring its own error rate up too */
const PATTERN_ERROR_PCT = {
  cliff:   { p4: 2.4, p5: 4.8 },
  dip:     { p4: 1.1, p5: 0.9 },
  spike:   { p4: 0.6, p5: 0.3 },
  default: { p4: 0.5, p5: 0.1 },
};

function applyPattern(m, patternName) {
  const pat = PATTERNS[patternName];
  if (!pat) return false;
  const base = m.buckets.reduce((a, b) => a + b, 0) / 30 || 20;
  const buckets = [];
  for (let i = 0; i < 30; i++) {
    const t = i / 29;
    buckets.push(Math.max(0, Math.round(pat.gen(base, t))));
  }
  m.buckets = buckets;
  const rates = PATTERN_ERROR_PCT[patternName] || PATTERN_ERROR_PCT.default;
  retotal(m, rates.p4, rates.p5);
  return true;
}

/* --------------------------------------------------------------- faults
   Each one names what it does to the NEXT /v1/gateway_health hit — stays
   armed until cleared, so a device can retry against the same fault a
   few times if you want to watch that.                                  */
const FAULTS = {
  timeout:            { label: 'Long timeout (8 s)', kind: 'delay', ms: 8000 },
  'http-500':         { label: 'HTTP 500 Backend', kind: 'status', code: 500 },
  'http-401':         { label: 'HTTP 401 No Access', kind: 'status', code: 401 },
  'http-404':         { label: 'HTTP 404 Not Found', kind: 'status', code: 404 },
  'http-429':         { label: 'HTTP 429 Throttled', kind: 'status', code: 429, headers: { 'Retry-After': '30' } },
  'http-3xx':         { label: 'HTTP 302 Redirect', kind: 'redirect' },
  'malformed-json':   { label: 'Malformed JSON (truncated body)', kind: 'malformed' },
  'missing-alert':    { label: 'Missing `alert` entirely', kind: 'missing-alert' },
  'missing-gateway':  { label: 'A row missing `gateway`', kind: 'missing-gateway' },
  'null-aggregates':  { label: 'A row with `aggregates: null`', kind: 'null-aggregates' },
  'sparse-aggregates':{ label: 'Row 0 down to 1 tile — heading only, no body slots', kind: 'sparse-aggregates' },
  'empty-metrics':    { label: 'Empty `metrics: []`', kind: 'empty-metrics' },
  'oversized':        { label: 'Oversized body (padded past 4 KB)', kind: 'oversized' },
};

/* -------------------------------------------------------------- payload */
function buildPayload() {
  return {
    measured_at: Math.floor(Date.now() / 1000),
    alert: { level: STATE.alert.level, message: STATE.alert.message },
    metrics: STATE.metrics.map((m) => ({ ...m, aggregates: m.aggregates.map((t) => ({ ...t })) })),
  };
}

/* ------------------------------------------------------------------ http */
function send(res, code, body, headers) {
  const buf = Buffer.from(body);
  res.writeHead(code, Object.assign({ 'Content-Type': 'application/json' }, headers || {}));
  res.end(buf);
}
function sendJSON(res, code, obj) {
  send(res, code, JSON.stringify(obj), {});
}
function readBody(req) {
  return new Promise((resolve, reject) => {
    let data = '';
    req.on('data', (c) => { data += c; if (data.length > 1e6) req.destroy(); });
    req.on('end', () => resolve(data));
    req.on('error', reject);
  });
}

function handleGatewayHealth(req, res) {
  const dev = touchDevice(req);
  const fault = STATE.fault ? FAULTS[STATE.fault] : null;

  if (fault && fault.kind === 'delay') {
    logHit(`GET /v1/gateway_health -> delaying ${fault.ms}ms (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    setTimeout(() => {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(buildPayload()));
    }, fault.ms);
    return;
  }
  if (fault && fault.kind === 'status') {
    logHit(`GET /v1/gateway_health -> ${fault.code} (${fault.label})`);
    recordPoll(dev, fault.code, fault.label);
    res.writeHead(fault.code, Object.assign({ 'Content-Type': 'text/plain' }, fault.headers || {}));
    res.end('');
    return;
  }
  if (fault && fault.kind === 'redirect') {
    logHit(`GET /v1/gateway_health -> 302 (${fault.label})`);
    recordPoll(dev, 302, fault.label);
    res.writeHead(302, { Location: '/v1/gateway_health' });
    res.end('');
    return;
  }
  if (fault && fault.kind === 'malformed') {
    logHit(`GET /v1/gateway_health -> 200 malformed body (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const body = JSON.stringify(buildPayload());
    send(res, 200, body.slice(0, Math.floor(body.length * 0.6)), {}); // truncated, invalid JSON
    return;
  }
  if (fault && fault.kind === 'missing-alert') {
    logHit(`GET /v1/gateway_health -> 200, no "alert" (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload(); delete p.alert;
    sendJSON(res, 200, p);
    return;
  }
  if (fault && fault.kind === 'missing-gateway') {
    logHit(`GET /v1/gateway_health -> 200, row 0 missing "gateway" (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload(); delete p.metrics[0].gateway;
    sendJSON(res, 200, p);
    return;
  }
  if (fault && fault.kind === 'null-aggregates') {
    logHit(`GET /v1/gateway_health -> 200, row 0 aggregates:null (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload(); p.metrics[0].aggregates = null;
    sendJSON(res, 200, p);
    return;
  }
  if (fault && fault.kind === 'sparse-aggregates') {
    logHit(`GET /v1/gateway_health -> 200, row 0 aggregates trimmed to 1 (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload(); p.metrics[0].aggregates = p.metrics[0].aggregates.slice(0, 1);
    sendJSON(res, 200, p);
    return;
  }
  if (fault && fault.kind === 'empty-metrics') {
    logHit(`GET /v1/gateway_health -> 200, metrics:[] (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload(); p.metrics = [];
    sendJSON(res, 200, p);
    return;
  }
  if (fault && fault.kind === 'oversized') {
    logHit(`GET /v1/gateway_health -> 200, padded past 4 KB (${fault.label})`);
    recordPoll(dev, 200, fault.label);
    const p = buildPayload();
    p._padding = 'x'.repeat(5000);
    sendJSON(res, 200, p);
    return;
  }

  logHit('GET /v1/gateway_health -> 200');
  recordPoll(dev, 200, 'ok');
  sendJSON(res, 200, buildPayload());
}

async function handleApi(req, res, pathname) {
  if (pathname === '/api/devices' && req.method === 'GET') {
    return sendJSON(res, 200, { devices: devicesJSON(), activeMs: ACTIVE_MS, maxDevices: MAX_DEVICES });
  }
  if (pathname === '/api/state' && req.method === 'GET') {
    return sendJSON(res, 200, {
      alert: STATE.alert,
      fault: STATE.fault,
      metrics: STATE.metrics,
      patterns: Object.fromEntries(Object.entries(PATTERNS).map(([k, v]) => [k, v.label])),
      faults: Object.fromEntries(Object.entries(FAULTS).map(([k, v]) => [k, v.label])),
      log: LOG.slice(0, 20),
    });
  }
  if (pathname === '/api/alert' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)) || '{}');
    if (body.level) STATE.alert.level = String(body.level).slice(0, 10);
    if (typeof body.message === 'string') STATE.alert.message = body.message.slice(0, 20);
    logHit(`UI -> alert set to ${STATE.alert.level} "${STATE.alert.message}"`);
    return sendJSON(res, 200, { ok: true, alert: STATE.alert });
  }
  if (pathname === '/api/pattern' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)) || '{}');
    const idx = STATE.metrics.findIndex((m) => m.gateway === body.gateway && m.name === body.name);
    if (idx < 0) return sendJSON(res, 404, { ok: false, error: 'no such metric' });
    const ok = applyPattern(STATE.metrics[idx], body.pattern);
    if (!ok) return sendJSON(res, 400, { ok: false, error: 'no such pattern' });
    logHit(`UI -> ${body.gateway}/${body.name} pattern = ${body.pattern}`);
    return sendJSON(res, 200, { ok: true, metric: STATE.metrics[idx] });
  }
  if (pathname === '/api/fault' && req.method === 'POST') {
    const body = JSON.parse((await readBody(req)) || '{}');
    const type = body.type || null;
    if (type && !FAULTS[type]) return sendJSON(res, 400, { ok: false, error: 'no such fault' });
    STATE.fault = type;
    logHit(`UI -> fault ${type ? 'armed: ' + FAULTS[type].label : 'cleared'}`);
    return sendJSON(res, 200, { ok: true, fault: STATE.fault });
  }
  if (pathname === '/api/reset' && req.method === 'POST') {
    STATE = defaultState();
    logHit('UI -> reset to defaults');
    return sendJSON(res, 200, { ok: true });
  }
  sendJSON(res, 404, { ok: false, error: 'no such route' });
}

const PUBLIC_DIR = path.join(__dirname, 'public');
function serveStatic(req, res, pathname) {
  const file = pathname === '/' ? 'index.html' : pathname.replace(/^\/+/, '');
  const full = path.join(PUBLIC_DIR, file);
  if (!full.startsWith(PUBLIC_DIR)) return sendJSON(res, 403, { ok: false });
  fs.readFile(full, (err, data) => {
    if (err) return sendJSON(res, 404, { ok: false, error: 'not found' });
    const ext = path.extname(full);
    const type = ext === '.html' ? 'text/html' : ext === '.js' ? 'application/javascript'
               : ext === '.css' ? 'text/css' : 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': type + '; charset=utf-8' });
    res.end(data);
  });
}

function handleOpenApiSpec(req, res) {
  fs.readFile(path.join(__dirname, 'openapi.yaml'), (err, data) => {
    if (err) return sendJSON(res, 404, { ok: false, error: 'not found' });
    res.writeHead(200, { 'Content-Type': 'application/yaml; charset=utf-8' });
    res.end(data);
  });
}

const server = http.createServer((req, res) => {
  const u = new URL(req.url, `http://${req.headers.host}`);
  const pathname = u.pathname;
  if (pathname === '/v1/gateway_health' && req.method === 'GET') return handleGatewayHealth(req, res);
  if (pathname === '/openapi.yaml' && req.method === 'GET') return handleOpenApiSpec(req, res);
  if (pathname.startsWith('/api/')) return void handleApi(req, res, pathname).catch((e) => sendJSON(res, 500, { ok: false, error: String(e) }));
  return serveStatic(req, res, pathname);
});

server.listen(PORT, () => {
  console.log(`Pulsar simulator on http://localhost:${PORT}`);
  console.log(`  UI       -> http://localhost:${PORT}/`);
  console.log(`  Endpoint -> http://localhost:${PORT}/v1/gateway_health`);
});
