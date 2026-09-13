/* ===========================================================================
   The setup hotspot — the board's own Wi-Fi, the page it serves, and the
   screen that tells you how to get in.

   HOLD LEFT 2 s. The board raises an access point named after itself, serves
   one page at 192.168.4.1, and everything on that page is everything the
   firmware does not bake in: the base URL, the API key, the API secret, and
   the networks to reach them over, in priority order.

   Three decisions worth knowing about:

   · THE AP IS WPA2, NOT OPEN, and its password is made fresh every time the
     hotspot is raised and shown on the device's screen. An open AP means
     anybody within range can rewrite where this device points and read what
     it posts; the screen is the out-of-band channel that fixes that, and it
     costs one glance to use
   · A DNS RESPONDER ANSWERS EVERY NAME with 192.168.4.1, and anything not
     served is redirected to the page, so joining the network pops the page
     up on its own the way a hotel's does. That is the whole reason the
     phone's captive-portal probe exists, and it is much kinder than telling
     someone to type an address
   · NOTHING IS TESTED FROM HERE, on purpose. One radio cannot hold this AP
     up and be associated to your office network at the same time without
     dropping the phone that is mid-edit, so the page scans (which needs no
     association, and tells you whether the SSID you typed is even in the
     room) and then hands off: Save and restart, and the device's own screen
     says OFFLINE, NO ACCESS or real numbers within a few seconds. The
     screen is the test, and it is the screen you will be reading anyway

   The page itself is PAGE[] at the bottom — standalone, no fonts or scripts
   fetched from anywhere, because a device serving its own setup page has no
   internet to fetch them over.
   =========================================================================== */

#define AP_IP_STR   "192.168.4.1"
#define AP_PASS_LEN 8            /* WPA2's floor is 8 */
#define AP_JSON_MAX  3072        /* every response this serves fits inside this */
#define AP_SCAN_MAX    24        /* scan rows sent to the page — keeps it inside AP_JSON_MAX */

/* --------------------------------------------------------------- the page
   Standalone: every byte of style and script is right here, because a device
   serving its own setup page has no internet to fetch a font or a framework
   over. Kept in flash and sent straight from it, never copied into RAM.

   It shows no stored password back — the firmware masks them (config.ino) and
   the page says "saved" with an empty box beside it. An empty box posted back
   means "keep the one you have", matched up by network name, so reordering
   the list or renaming one network never silently loses another's password.
                                                                            */
static const char PAGE[] PROGMEM = R"PULSARPAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>pulsar setup</title>
<style>
:root{--bg:#05070c;--panel:#0c1422;--tx:#e6edf7;--dim:#7f8fa8;--dim2:#4d5c76;
--rule:#1c2740;--cy:#22d3ee;--gr:#5cf22e;--or:#ff7a00;--rd:#ff2626}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);
font:15px/1.5 ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
padding-bottom:110px}
header{padding:18px 16px 12px;border-bottom:1px solid var(--rule);position:sticky;top:0;
background:var(--bg);z-index:5}
h1{margin:0;font-size:19px;letter-spacing:.14em;text-transform:uppercase;color:var(--cy)}
header span{color:var(--dim);font-size:12px;letter-spacing:.1em}
main{padding:0 16px}
section{margin:22px 0 0;background:var(--panel);border:1px solid var(--rule);border-radius:10px;
padding:14px}
h2{margin:0 0 4px;font-size:12px;letter-spacing:.16em;text-transform:uppercase;color:var(--dim)}
label{display:block;margin:12px 0 0;font-size:12px;letter-spacing:.06em;
text-transform:uppercase;color:var(--dim)}
input,select,textarea{width:100%;margin-top:5px;padding:10px;background:#070b14;
color:var(--tx);border:1px solid var(--rule);border-radius:7px;font-size:16px;
font-family:inherit}
textarea{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12px}
input:focus,select:focus,textarea:focus{outline:none;border-color:var(--cy)}
.hint{margin:8px 0 0;font-size:12px;color:var(--dim2);line-height:1.45}
.row{display:flex;align-items:center;gap:9px;text-transform:none;letter-spacing:0;
font-size:13px;color:var(--tx)}
.row input{width:auto;margin:0}
button{font:inherit;font-size:14px;padding:10px 14px;border-radius:7px;cursor:pointer;
background:#101b2d;color:var(--tx);border:1px solid var(--rule)}
button:active{transform:translateY(1px)}
.primary{background:var(--cy);color:#04060a;border-color:var(--cy);font-weight:700}
.net{border:1px solid var(--rule);border-radius:9px;padding:11px;margin:11px 0 0;
background:#070b14}
.netHead{display:flex;align-items:center;gap:7px}
.netHead b{flex:1;font-size:12px;letter-spacing:.12em;color:var(--cy)}
.netHead button{padding:5px 10px;font-size:13px;line-height:1}
.tools{display:flex;gap:9px;margin-top:13px;flex-wrap:wrap}
.seen{margin-top:11px;font-size:13px}
.seen div{display:flex;justify-content:space-between;gap:10px;padding:7px 0;
border-top:1px solid var(--rule);cursor:pointer}
.seen small{color:var(--dim2)}
footer{position:fixed;bottom:0;left:0;right:0;background:var(--bg);
border-top:1px solid var(--rule);padding:12px 16px;display:flex;gap:10px;
align-items:center;flex-wrap:wrap}
footer button{flex:1;min-width:120px}
#msg{margin:0;flex-basis:100%;font-size:13px;color:var(--dim)}
#msg.bad{color:var(--or)}
#msg.good{color:var(--gr)}
.hide{display:none}
details{margin-top:14px}
summary{font-size:12px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);
cursor:pointer}
</style></head><body>

<header><h1>Pulsar setup</h1><span id="dev">&nbsp;</span></header>

<main>
<section>
  <h2>Endpoint</h2>
  <label>Base URL
    <input id="url" inputmode="url" autocapitalize="off" autocomplete="off"
           spellcheck="false" placeholder="https://api.example.com"></label>
  <p class="hint">One URL. <code>/v1/gateway_health</code> is added when it polls, so
     leave it off — paste the whole thing and it gets trimmed back for you.</p>
  <label>API key
    <input id="key" type="password" autocomplete="off" spellcheck="false"></label>
  <label>API secret
    <input id="secret" type="password" autocomplete="off" spellcheck="false"></label>
  <label class="row"><input type="checkbox" id="secretClear"> forget the saved secret</label>
  <p class="hint">Leave the secret empty and the key is sent as
     <code>Authorization: Bearer</code>. Fill it in and the key becomes the public
     <code>X-Api-Key</code> and every request is signed with the secret instead.</p>
  <details>
    <summary>TLS root certificate</summary>
    <textarea id="ca" rows="4" spellcheck="false"
      placeholder="-----BEGIN CERTIFICATE-----"></textarea>
    <label class="row"><input type="checkbox" id="caClear"> forget the saved certificate</label>
    <p class="hint">Optional, and worth doing. Without a root to check against, an
       <code>https://</code> poll is encrypted but not authenticated — anything in the
       middle can read the key. Paste your CA's PEM here and it gets verified.</p>
  </details>
</section>

<section>
  <h2>Wi-Fi networks</h2>
  <p class="hint">Top of the list wins. The device joins the highest one it can
     actually see, and if none of them are in range it simply keeps trying — that
     is not an error.</p>
  <div id="nets"></div>
  <div class="tools">
    <button id="add">+ add a network</button>
    <button id="scan">scan for networks</button>
  </div>
  <div class="seen" id="seenList"></div>
  <datalist id="seen"></datalist>
</section>
</main>

<footer>
  <button id="save">Save</button>
  <button id="saveboot" class="primary">Save &amp; restart</button>
  <p id="msg">Loading&hellip;</p>
</footer>

<script>
var MAXN = 6, st = {nets: []}, flags = {};
var $ = function(id){ return document.getElementById(id); };

function say(text, cls){
  var m = $('msg');
  m.textContent = text;
  m.className = cls || '';
}

/* ---- one network row. `f` carries which passwords are already stored, so the
   box can say "saved" and stay empty rather than showing what is in there. */
function row(n, i, total){
  var d = document.createElement('div');
  d.className = 'net';
  var entSaved = n.passSet ? 'saved - leave blank to keep' : '';
  var pskSaved = n.pskSet ? 'saved - leave blank to keep' : '';
  var ppSaved  = n.portalPassSet ? 'saved - leave blank to keep' : '';
  d.innerHTML =
    '<div class="netHead"><b>' + (i + 1) + (i === 0 ? ' &middot; tried first' : '') + '</b>' +
      '<button data-a="up" ' + (i === 0 ? 'disabled' : '') + ' title="higher priority">&uarr;</button>' +
      '<button data-a="down" ' + (i === total - 1 ? 'disabled' : '') + ' title="lower priority">&darr;</button>' +
      '<button data-a="del" title="remove">&#10005;</button></div>' +
    '<label>Network name (SSID)<input class="ssid" list="seen" autocapitalize="off" ' +
      'autocomplete="off" spellcheck="false"></label>' +
    '<label>Security<select class="sec">' +
      '<option value="psk">Password (WPA/WPA2)</option>' +
      '<option value="open">Open - no password</option>' +
      '<option value="enterprise">Enterprise (802.1X / PEAP)</option>' +
      '</select></label>' +
    '<label class="f-psk">Wi-Fi password<input class="psk" type="password" ' +
      'autocomplete="off" placeholder="' + pskSaved + '"></label>' +
    '<div class="f-ent">' +
      '<label>Username<input class="user" autocapitalize="off" autocomplete="off" ' +
        'spellcheck="false"></label>' +
      '<label>Password<input class="pass" type="password" autocomplete="off" ' +
        'placeholder="' + entSaved + '"></label>' +
      '<label>Outer identity<input class="identity" autocapitalize="off" ' +
        'autocomplete="off" spellcheck="false" placeholder="optional - defaults to the username">' +
        '</label>' +
      '<p class="hint">802.1X asks for these during the join itself. Without them the ' +
        'device never gets an address at all.</p>' +
    '</div>' +
    '<label class="row"><input type="checkbox" class="portal"> a sign-in page stands ' +
      'between this network and the internet</label>' +
    '<div class="f-portal">' +
      '<label>Sign-in name<input class="puser" autocapitalize="off" autocomplete="off" ' +
        'spellcheck="false"></label>' +
      '<label>Sign-in password<input class="ppass" type="password" autocomplete="off" ' +
        'placeholder="' + ppSaved + '"></label>' +
      '<p class="hint">For the ordinary hotel or guest portal: the device finds the form, ' +
        'fills these in and posts it. A portal that builds its form in JavaScript, or ' +
        'sends you through a separate identity provider, cannot be driven this way - ' +
        'the device will say PORTAL on screen instead of pretending.</p>' +
    '</div>';

  d.querySelector('.ssid').value     = n.ssid || '';
  d.querySelector('.sec').value      = n.security || 'psk';
  d.querySelector('.user').value     = n.user || '';
  d.querySelector('.identity').value = n.identity || '';
  d.querySelector('.portal').checked = !!n.portal;
  d.querySelector('.puser').value    = n.portalUser || '';
  /* Passwords typed this session are put back too. The device never sends one,
     so anything here is something just typed — and adding, removing or
     reordering a row redraws every row, which would otherwise throw away a
     password that was typed a moment ago without saying a word.            */
  d.querySelector('.psk').value      = n.psk || '';
  d.querySelector('.pass').value     = n.pass || '';
  d.querySelector('.ppass').value    = n.portalPass || '';

  function sync(){
    var sec = d.querySelector('.sec').value;
    d.querySelector('.f-psk').classList.toggle('hide', sec !== 'psk');
    d.querySelector('.f-ent').classList.toggle('hide', sec !== 'enterprise');
    d.querySelector('.f-portal').classList.toggle('hide', !d.querySelector('.portal').checked);
  }
  d.querySelector('.sec').addEventListener('change', sync);
  d.querySelector('.portal').addEventListener('change', sync);
  sync();

  d.addEventListener('click', function(e){
    var a = e.target.getAttribute && e.target.getAttribute('data-a');
    if (!a) return;
    e.preventDefault();
    collect();
    if (a === 'del') st.nets.splice(i, 1);
    if (a === 'up' && i > 0) st.nets.splice(i - 1, 0, st.nets.splice(i, 1)[0]);
    if (a === 'down' && i < st.nets.length - 1) st.nets.splice(i + 1, 0, st.nets.splice(i, 1)[0]);
    draw();
  });
  return d;
}

function draw(){
  var box = $('nets');
  box.textContent = '';
  if (!st.nets.length){
    var p = document.createElement('p');
    p.className = 'hint';
    p.textContent = 'No networks yet. Add one, or scan to see what is in range.';
    box.appendChild(p);
  }
  for (var i = 0; i < st.nets.length; i++) box.appendChild(row(st.nets[i], i, st.nets.length));
  $('add').disabled = st.nets.length >= MAXN;
}

/* Read every row back into st.nets, keeping the "already stored" flags so a
   redraw does not forget that a password exists.                           */
function collect(){
  var rows = $('nets').querySelectorAll('.net');
  var out = [];
  for (var i = 0; i < rows.length; i++){
    var d = rows[i], old = st.nets[i] || {};
    out.push({
      ssid:       d.querySelector('.ssid').value.trim(),
      security:   d.querySelector('.sec').value,
      psk:        d.querySelector('.psk').value,
      user:       d.querySelector('.user').value.trim(),
      pass:       d.querySelector('.pass').value,
      identity:   d.querySelector('.identity').value.trim(),
      portal:     d.querySelector('.portal').checked,
      portalUser: d.querySelector('.puser').value.trim(),
      portalPass: d.querySelector('.ppass').value,
      pskSet:        old.pskSet || !!d.querySelector('.psk').value,
      passSet:       old.passSet || !!d.querySelector('.pass').value,
      portalPassSet: old.portalPassSet || !!d.querySelector('.ppass').value
    });
  }
  st.nets = out;
  return out;
}

/* `keep` is set after a save: the fields are refreshed from the device (so the
   "saved" hints on the password boxes become true again) without throwing away
   the message that says the save worked.                                    */
function load(keep){
  fetch('/api/config').then(function(r){ return r.json(); }).then(function(c){
    MAXN = c.maxNets || 6;
    $('dev').textContent = c.device || '';
    $('url').value = c.url || '';
    $('key').placeholder    = c.keySet ? 'saved - leave blank to keep' : '';
    $('secret').placeholder = c.secretSet ? 'saved - leave blank to keep' : 'none - bearer token mode';
    $('ca').placeholder     = c.caSet ? 'saved - leave blank to keep'
                                      : '-----BEGIN CERTIFICATE-----';
    st.nets = c.nets || [];
    draw();
    if (!keep) say(c.url ? 'Pointing at ' + c.url : 'No endpoint set yet.', c.url ? '' : 'bad');
  }).catch(function(e){ say('Could not read the current settings: ' + e, 'bad'); });
}

function save(reboot){
  var nets = collect();
  for (var i = 0; i < nets.length; i++){
    if (!nets[i].ssid){ say('Network #' + (i + 1) + ' has no name.', 'bad'); return; }
    if (nets[i].security === 'psk' && !nets[i].psk && !nets[i].pskSet){
      say('"' + nets[i].ssid + '" needs a password.', 'bad'); return;
    }
    if (nets[i].security === 'enterprise' && !nets[i].user){
      say('"' + nets[i].ssid + '" needs a username.', 'bad'); return;
    }
    if (nets[i].security === 'enterprise' && !nets[i].pass && !nets[i].passSet){
      say('"' + nets[i].ssid + '" needs a password.', 'bad'); return;
    }
  }
  var body = {
    url: $('url').value.trim(),
    key: $('key').value,
    secret: $('secret').value,
    secretClear: $('secretClear').checked,
    ca: $('ca').value.trim(),
    caClear: $('caClear').checked,
    nets: nets
  };
  say('Saving&hellip;');
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body)
  }).then(function(r){
    return r.json().then(function(j){ return {ok: r.ok, j: j}; });
  }).then(function(res){
    if (!res.ok || !res.j.ok){ say('Not saved: ' + (res.j.error || 'unknown'), 'bad'); return; }
    if (!reboot){
      say('Saved. Restart to use it.', 'good');
      load(true);
      return;
    }
    say('Saved. Restarting&hellip; watch the device screen.', 'good');
    fetch('/api/reboot', {method: 'POST'}).catch(function(){});
  }).catch(function(e){ say('Save failed: ' + e, 'bad'); });
}

/* ---- scan. The device answers "still scanning" rather than holding the
   request open, so poll it until the list arrives.                        */
var scanTries = 0;
function scan(){
  fetch('/api/scan').then(function(r){ return r.json(); }).then(function(s){
    if (s.error){ say(s.error, 'bad'); return; }
    if (s.scanning){
      if (++scanTries > 14){ say('The scan is taking too long.', 'bad'); return; }
      say('Scanning&hellip;');
      setTimeout(scan, 1200);
      return;
    }
    scanTries = 0;
    var list = s.nets || [];
    list.sort(function(a, b){ return b.rssi - a.rssi; });
    var dl = $('seen'), out = $('seenList');
    dl.textContent = '';
    out.textContent = '';
    for (var i = 0; i < list.length; i++){
      var o = document.createElement('option');
      o.value = list[i].ssid;
      dl.appendChild(o);

      var d = document.createElement('div');
      var bars = list[i].rssi > -60 ? 'strong' : list[i].rssi > -75 ? 'ok' : 'weak';
      d.innerHTML = '<span></span><small>' + list[i].security + ' &middot; ' + bars +
                    ' ' + list[i].rssi + ' dBm</small>';
      d.firstChild.textContent = list[i].ssid || '(hidden)';
      d.addEventListener('click', (function(entry){
        return function(){
          collect();
          var at = -1;
          for (var k = 0; k < st.nets.length; k++) if (st.nets[k].ssid === entry.ssid) at = k;
          if (at < 0 && st.nets.length < MAXN){
            st.nets.push({ssid: entry.ssid, security: entry.security, portal: false});
            draw();
            say('Added "' + entry.ssid + '".', 'good');
          } else if (at < 0){
            say('That is already ' + MAXN + ' networks.', 'bad');
          } else {
            say('"' + entry.ssid + '" is already in the list.', '');
          }
        };
      })(list[i]));
      out.appendChild(d);
    }
    say(list.length + ' network' + (list.length === 1 ? '' : 's') +
        ' in range. Tap one to add it.', '');
  }).catch(function(e){ say('Scan failed: ' + e, 'bad'); });
}

$('add').addEventListener('click', function(){
  collect();
  if (st.nets.length < MAXN) st.nets.push({security: 'psk', portal: false});
  draw();
});
$('scan').addEventListener('click', function(){ scanTries = 0; scan(); });
$('save').addEventListener('click', function(){ save(false); });
$('saveboot').addEventListener('click', function(){ save(true); });
load();
</script>
</body></html>
)PULSARPAGE";

/* ------------------------------------------------------------------ the AP */

/* A fresh password per session, from an alphabet with no character anyone has
   to squint at — no 0/O, no 1/l/I. It is read off a 240 px screen and typed
   into a phone once.                                                       */
static void apMakePassword(){
  static const char alpha[] = "23456789abcdefghjkmnpqrstuvwxyz";
  const size_t n = sizeof alpha - 1;
  for (int i = 0; i < AP_PASS_LEN; i++) apPass[i] = alpha[esp_random() % n];
  apPass[AP_PASS_LEN] = 0;
}

static int apClients(){ return WiFi.softAPgetStationNum(); }

/* --------------------------------------------------------------- responses */

/* Serialised into a fixed buffer rather than a String: this runs on a device
   with 4 KB of config and no business growing the heap a character at a time
   while a phone is connected. Too big to fit is a 500 that says so, not a
   truncated document the page would fail to parse with no explanation.    */
static void apSendJson(int code, JsonDocument& doc){
  static char out[AP_JSON_MAX];
  const size_t need = measureJson(doc);
  if (need + 1 > sizeof out){
    LOGF("view", "hotspot: a %u byte response will not fit in %u - sending 500",
         (unsigned)need, (unsigned)sizeof out);
    apServer->send(500, "application/json", "{\"ok\":false,\"error\":\"response too large\"}");
    return;
  }
  serializeJson(doc, out, sizeof out);
  apServer->send(code, "application/json", out);
}
static void apSendError(int code, const char* msg){
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = msg;
  apSendJson(code, doc);
}

/* ---------------------------------------------------------------- handlers */

static void apHandleRoot(){
  apLastHit = millis();
  LOG("view", "hotspot: serving the setup page");
  apServer->sendHeader("Cache-Control", "no-store");
  apServer->send_P(200, "text/html", PAGE);
}

/* Everything else on every name — this is what makes a phone pop the page. */
static void apHandleElsewhere(){
  apLastHit = millis();
  LOGF("view", "hotspot: %s -> redirected to the setup page", apServer->uri().c_str());
  apServer->sendHeader("Location", "http://" AP_IP_STR "/", true);
  apServer->send(302, "text/plain", "");
}

static void apHandleConfigGet(){
  apLastHit = millis();
  JsonDocument doc;
  configJson(doc);
  doc["device"] = deviceName;
  LOG("view", "hotspot: the page asked for the current config (no secrets sent)");
  apSendJson(200, doc);
}

static void apHandleConfigPost(){
  apLastHit = millis();
  const String body = apServer->arg("plain");
  if (!body.length()){ apSendError(400, "empty body"); return; }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body.c_str());
  if (err){
    LOGF("cfg", "the page posted something unparseable: %s", err.c_str());
    apSendError(400, err.c_str());
    return;
  }
  char problem[96];
  if (!configApplyJson(doc.as<JsonObjectConst>(), problem, sizeof problem)){
    LOGF("cfg", "rejected what the page posted: %s", problem);
    apSendError(422, problem);
    return;
  }
  const bool saved = configSave();
  if (saved) apSaves++;

  JsonDocument out;
  out["ok"] = saved;
  if (!saved) out["error"] = "could not write to storage";
  out["nets"] = cfg.nnets;
  out["url"]  = cfg.url;
  apSendJson(saved ? 200 : 500, out);
}

/* What is actually in the air. Asked for repeatedly by the page while a scan
   runs, so it answers "still scanning" rather than blocking the server.    */
static void apHandleScan(){
  apLastHit = millis();
  JsonDocument doc;
  const int found = WiFi.scanComplete();

  if (found == -1){ doc["scanning"] = true; apSendJson(200, doc); return; }

  if (found < 0){
    const int16_t started = WiFi.scanNetworks(true /* async */);
    doc["scanning"] = started != WIFI_SCAN_FAILED;
    if (started == WIFI_SCAN_FAILED){
      doc["error"] = "the radio would not start a scan";
      LOG("view", "hotspot: scan would not start");
    } else {
      LOG("view", "hotspot: scanning for the page");
    }
    apSendJson(200, doc);
    return;
  }

  doc["scanning"] = false;
  JsonArray arr = doc["nets"].to<JsonArray>();
  for (int i = 0; i < found && i < AP_SCAN_MAX; i++){
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    /* What the beacon claims, as a hint for the security picker. An
       enterprise network advertises WPA2_ENTERPRISE; everything else with a
       key looks like a PSK from out here.                                  */
    const wifi_auth_mode_t a = WiFi.encryptionType(i);
    o["security"] = a == WIFI_AUTH_OPEN ? "open"
                  : a == WIFI_AUTH_WPA2_ENTERPRISE ? "enterprise" : "psk";
  }
  LOGF("view", "hotspot: sent %d network%s to the page", found, found == 1 ? "" : "s");
  WiFi.scanDelete();
  apSendJson(200, doc);
}

static void apHandleReboot(){
  apLastHit = millis();
  JsonDocument doc;
  doc["ok"] = true;
  apSendJson(200, doc);
  LOG("view", "hotspot: the page asked for a restart - going now");
  apServer->client().flush();
  delay(300);
  Serial.flush();
  ESP.restart();
}

/* -------------------------------------------------------------- lifecycle */

static void hotspotStart(){
  apMakePassword();
  apSaves   = 0;
  apLastHit = 0;

  /* The station side is stood down but kept enabled: a scan needs it, and a
     scan is how the page tells you whether the SSID you just typed is
     actually in the room. It never associates while the AP is up — one
     radio cannot follow your office network's channel and hold this AP
     still at the same time.                                               */
  wifiSuspend();                                   /* wifi.ino */
  WiFi.mode(WIFI_AP_STA);
  const bool up = WiFi.softAP(deviceName, apPass);
  if (!up){
    LOG("view", "hotspot: the access point would not come up");
    return;
  }
  LOGF("view", "hotspot: \"%s\" up at %s, password %s", deviceName,
       WiFi.softAPIP().toString().c_str(), apPass);

  apDns = new DNSServer();
  if (apDns){
    apDns->setErrorReplyCode(DNSReplyCode::NoError);
    apDns->start(53, "*", WiFi.softAPIP());         /* every name points here */
    LOG("view", "hotspot: dns is answering every name - joining should pop the page");
  } else LOG("view", "hotspot: no memory for dns - the page needs typing in by hand");

  apServer = new WebServer(80);
  if (!apServer){
    LOG("view", "hotspot: no memory for the web server");
    return;
  }
  apServer->on("/",            HTTP_GET,  apHandleRoot);
  apServer->on("/api/config",  HTTP_GET,  apHandleConfigGet);
  apServer->on("/api/config",  HTTP_POST, apHandleConfigPost);
  apServer->on("/api/scan",    HTTP_GET,  apHandleScan);
  apServer->on("/api/reboot",  HTTP_POST, apHandleReboot);
  apServer->onNotFound(apHandleElsewhere);
  apServer->begin();
  LOG("view", "hotspot: serving http://" AP_IP_STR "/");
}

static void hotspotStop(){
  /* Leaving by a LEFT tap calls this twice — once from onTap, once from inside
     showMain() — so it has to be safe to call when nothing is up. apPass is
     the session marker: hotspotStart() sets it first and this clears it last. */
  if (!apPass[0] && !apServer && !apDns) return;
  if (apServer){ apServer->stop(); delete apServer; apServer = nullptr; }
  if (apDns){ apDns->stop(); delete apDns; apDns = nullptr; }
  WiFi.softAPdisconnect(true);
  apPass[0] = 0;                                   /* the session is over — see the guard above */
  LOGF("view", "hotspot: closed%s", apSaves ? " - config was saved" : "");
  if (apSaves) netBegin();                         /* the banner says what we are waiting for now */
  wifiResume();                                    /* wifi.ino — start over with the saved networks */
}

/* Called every frame from loop(); does nothing at all unless the AP is up. */
static void hotspotTick(){
  if (!apServer) return;
  if (apDns) apDns->processNextRequest();
  apServer->handleClient();
}

/* ------------------------------------------------------------------ screen
   Everything needed to get in, in the order it is needed: the network, its
   password, then the address. Nothing here is a placeholder — if the AP did
   not come up, this screen says that instead of lying about a password.   */
static void drawHotspot(){
  cv->fillScreen(C_BG);
  txt("SETUP", X_L, 8, 2, C_CY, 'l', true);
  cv->fillRect(X_L, 28, X_R - X_L, 1, C_LINE);

  if (!apServer || !apPass[0]){
    txt("HOTSPOT FAILED", 120, 100, 2, C_OR, 'c', true);
    txt("the radio would not raise it", 120, 126, 1, C_DIM2, 'c');
    txt("TAP LEFT - BACK", 120, 196, 1, C_DIM, 'c');
    return;
  }

  txt("JOIN THIS WI-FI", X_L, 34, 1, C_DIM2);
  txt(deviceName, X_L, 45, 2, C_TX, 'l', true);
  txt("PASSWORD", X_L, 68, 1, C_DIM2);
  txt(apPass, X_L, 79, 2, C_CY, 'l', true);
  txt("THEN OPEN", X_L, 102, 1, C_DIM2);
  txt(AP_IP_STR, X_L, 113, 2, C_TX);
  cv->fillRect(X_L, 136, X_R - X_L, 1, C_LINE);

  /* live state, so you can see the phone arrive and the save land */
  const int n = apClients();
  char v[40];
  if (!n) txt("WAITING FOR A PHONE", X_L, 144, 1, C_DIM, 'l');
  else {
    snprintf(v, sizeof v, "%d CONNECTED", n);
    txt(v, X_L, 144, 1, C_GR, 'l', true);
  }
  if (apLastHit) txt("PAGE OPENED", X_R, 144, 1, C_DIM2, 'r');

  if (apSaves){
    snprintf(v, sizeof v, "SAVED %u TIME%s", (unsigned)apSaves, apSaves == 1 ? "" : "S");
    txt(v, X_L, 158, 1, C_GR, 'l', true);
    txt("RESTART FROM THE PAGE TO USE IT", X_L, 170, 1, C_DIM2);
  } else {
    snprintf(v, sizeof v, "%u network%s saved, url %s", (unsigned)cfg.nnets,
             cfg.nnets == 1 ? "" : "s", cfg.url[0] ? "set" : "NOT SET");
    txt(v, X_L, 158, 1, C_DIM2);
  }

  cv->fillRect(X_L, 184, X_R - X_L, 1, C_LINE);
  txt("TAP LEFT - BACK", 120, 192, 1, C_DIM, 'c');
  txt("HOLD LEFT 2 S - BACK", 120, 204, 1, C_DIM2, 'c');
  txt("THE AP CLOSES WHEN YOU LEAVE", 120, 220, 1, C_DIM2, 'c');
}
