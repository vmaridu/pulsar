/* ===========================================================================
   Sound — the boot-intro torpedo fire, the crit alert, the subtler
   notice, and mute.

   The boot intro (intro.ino) fires a torpedo-launch burst every time a beam
   sweeps past top — twice a turn, same phase drawIntroFrame() pulses the
   core on — then stops with the animation: silent for the "PULSAR" screen
   and the cross-fade after it. Audible but soft-edged on purpose — well
   under the crit alert's own volume, and a different character
   entirely — a launch, not a siren — so it never reads as trouble. Once
   running, one of two sounds rides every alert flash — started in the same
   frame the flash is drawn, ending as it ends: `crit` gets the full
   500 ms alert below; `warn` and any connection fault (OFFLINE, NOT FOUND, NO ACCESS, every
   banner api.md §6 names) get a short, quiet notice instead — enough to
   turn a head, not enough to sound like an outage. `info` stays silent.

   The alert is a stressed dip, pitched where this speaker can play it: a
   tone falling fast from 920 to 560 Hz with two harmonics for body, a 14 Hz
   shiver on top, a short thump on the attack and a soft 1.76 kHz glint. It
   fades to nothing at 500 ms. The notice is one soft tone, well under the
   alert's volume, a fifth of its length — a nudge, not an alarm.

   All three are rendered once at boot, in float maths only — the S3's FPU
   is single precision, and double maths (anything touching PI or a plain
   sin()) runs in software, slow enough to starve a core. Then a 250 Hz
   high-pass: the speaker cannot move lower, and trying only rattles and
   pulls current.

   Double-tap the UP key (BOOT) to silence every sound this build makes —
   double-tap again to bring it back. Mute clears itself three ways: the
   double-tap, a restart (RAM only, so it's always back after one — except
   straight after a brown-out reset, which starts muted so a weak supply
   cannot loop, and so the intro's own torpedo fire can't be what tips it
   over again on the very next boot), or the configured timeout elapsing on
   its own (set on the setup page, default 30 minutes, `0` = never times out).

   Past the intro, sound does not care what is on screen, or whether the
   panel is even awake — UP tap sleeps the display and a crit still
   sounds in the dark. That is what the speaker is for.

   Mirrors ws_lcd_154/mockup.html — same synth, same numbers.
   =========================================================================== */
#include <esp_system.h>

static I2SClass i2s;

#define AUDIO_RATE    16000
/* ES8311 0..100, a log scale: 60 is -19.5 dB, 74 is -1.5 dB, 75 is 0 dB. Past
   ~75 a small cell can sag and brown out.                                 */
#define SOUND_VOLUME     74
#define SOUND_PEAK    0.85f    /* of full scale, after the high-pass */
#define INTRO_PEAK    0.32f    /* subtle — closer to the notice's 0.35 than the alert's 0.85.
                                  Present enough to notice, quiet enough to ignore; the
                                  waveform below (a slow eased attack, a gentle pitch drift
                                  instead of a sharp glide, and an explicit fade at the tail)
                                  is what keeps it from reading as a beep even this quiet   */
#define INTRO_HIT_MS    260    /* one torpedo-fire burst, launch to tail-off */
#define NOTICE_MS       220    /* a fifth of the alert's length — a nudge, not an alarm */
#define NOTICE_PEAK    0.35f   /* well under the alert's 0.85 — quiet on purpose */
#define NOTICE_HZ      660.0f  /* one plain tone, no sweep, no harmonics past a touch of body */
#define HIGHPASS_HZ     250
#define PA_IDLE_MS     6000    /* the amp stays on this long after a sound, so the next
                                  one is not clipped by its start-up; then it sleeps */

static const float TAU_F = 6.2831853f;
static TaskHandle_t soundTask = NULL;
static int16_t*     alertBuf  = NULL;
static int           alertLen = 0;
static int16_t*     introBuf  = NULL;
static int           introLen = 0;
static int16_t*     noticeBuf = NULL;
static int           noticeLen = 0;
static int16_t*     playBuf   = NULL;   /* what the task is asked to play next */
static int           playLen  = 0;
static uint32_t      muteT0   = 0;      /* millis() mute was last engaged — soundTick() below */

/* a drum kick: a sine sweeping down from fEnd+fSweep to fEnd, decaying */
static float kick(float tau, float fEnd, float fSweep, float k, float decay){
  if (tau < 0 || tau > decay * 7) return 0;
  return sinf(TAU_F * (fEnd * tau + fSweep * k * (1 - expf(-tau / k)))) * expf(-tau / decay);
}

/* one sample of the alert, t in seconds from its start */
static float alertSample(float t){
  const float T = ALERT_FLASH_MS / 1000.0f;
  if (t < 0 || t >= T) return 0;
  const float k = 0.12f;                                            /* how fast it falls */
  const float ph = TAU_F * (560 * t + (920 - 560) * k * (1 - expf(-t / k)));  /* 920 → 560 Hz */
  const float body  = sinf(ph) + 0.35f * sinf(2 * ph) + 0.2f * sinf(3 * ph);
  const float shiver = 1 - 0.2f * (0.5f + 0.5f * sinf(TAU_F * 14 * t));
  const float knock = kick(t, 180, 220, 0.02f, 0.04f);
  const float glint = sinf(TAU_F * 1760 * t) * expf(-t / 0.08f);
  const float attack  = t < 0.006f ? t / 0.006f : 1;
  const float release = t > 0.38f ? 0.5f + 0.5f * cosf(0.5f * TAU_F * (t - 0.38f) / (T - 0.38f)) : 1;
  const float s = attack * release * (0.42f * body * shiver + 0.35f * knock + 0.08f * glint);
  return tanhf(1.6f * s) / tanhf(1.6f);
}

/* One sample of the notice, t in seconds from its start. A single soft tone
   with a touch of its own octave for body, quick in and quicker out — the
   opposite of the alert's urgency on purpose. Rides `warn` and any
   connection fault; `crit` keeps the stronger alert above instead, and
   the two never play together — alertTick() (ws_lcd_154.ino) picks one. */
static float noticeSample(float t){
  const float T = NOTICE_MS / 1000.0f;
  if (t < 0 || t >= T) return 0;
  const float ph = TAU_F * NOTICE_HZ * t;
  const float tone = sinf(ph) + 0.15f * sinf(2 * ph);
  const float attack  = t < 0.02f ? t / 0.02f : 1;
  const float release = t > T - 0.08f ? (T - t) / 0.08f : 1;
  return attack * release * tone;
}

/* One torpedo-fire burst, tau in seconds since THIS hit's own launch instant:
   a soft downward drift (kick(), barely sweeping) plus a rounder low thump
   — a launch heard through a wall, not a beep. Three things remove the
   hardness a sharper version had: a 25 ms eased-in attack (smoothstep, not
   a straight ramp — no corner where the ramp suddenly stops accelerating),
   a much smaller pitch sweep (a drift, not a glide — a big frequency slide
   is what reads as a "beep" or a siren), and an explicit fade over the
   final 40 ms so the sound always reaches exactly zero on its own terms,
   never truncated mid-decay by INTRO_HIT_MS (which is what a residual
   amplitude cut off abruptly would otherwise sound like: a click).       */
static float torpedoHit(float tau){
  const float T = INTRO_HIT_MS / 1000.0f;
  if (tau < 0 || tau > T) return 0;
  const float whoosh = kick(tau, 420, 260, 0.045f, 0.13f);   /* a smaller drift still, ~680 -> 420 Hz */
  const float thump  = kick(tau, 130, 60,  0.020f, 0.10f);   /* barely there — texture, not a punch */
  const float A = 0.03f;                                     /* a touch slower to ease in */
  const float p = tau < A ? tau / A : 1.0f;
  const float attack  = p * p * (3.0f - 2.0f * p);           /* smoothstep — no elbow */
  const float R = 0.04f;
  const float release = tau > T - R ? (T - tau) / R : 1.0f;
  return attack * release * (0.38f * whoosh + 0.20f * thump);
}

/* The boot-intro sound, one sample at a time, t in seconds from the intro's
   start: a torpedo-fire burst timed to every beam pass. Two beams a turn, so
   two launches a turn — `fmodf(t, period)` is time since the nearest one,
   and a beam passes top exactly when that phase hits zero, the same instant
   drawIntroFrame() (intro.ino) pulses its core on, so the sound and the glow
   land together. Fades the whole run in over 150 ms and out over the last
   300 ms so it never clicks against silence at either end. */
static float introSample(float t){
  const float T = I_ANIM_END;                        /* intro.ino */
  if (t < 0 || t >= T) return 0;
  const float period  = 1.0f / (2.0f * SPIN_HZ);
  const float tau      = fmodf(t, period);
  const float attack  = t < 0.15f ? t / 0.15f : 1;
  const float release = t > T - 0.3f ? (T - t) / 0.3f : 1;
  return attack * release * torpedoHit(tau);
}

/* Render one waveform to a fresh 16-bit buffer: float synthesis, then two
   one-pole high-passes (12 dB/octave) so the speaker is never asked to move
   air it cannot, then scaled to `peak` of full scale. `*outLen` gets the
   sample count. Returns NULL (and leaves *outLen at 0) if either malloc
   fails — the caller just gets a sound that never plays, not a crash.     */
static int16_t* renderSound(float (*sample)(float), int lenMs, float peak, int* outLen){
  *outLen = 0;
  const int n = AUDIO_RATE * lenMs / 1000;
  int16_t* buf = (int16_t*)malloc(n * sizeof(int16_t));
  float*   raw = (float*)malloc(n * sizeof(float));
  if (!buf || !raw){ free(raw); free(buf); return NULL; }

  const float rc = 1.0f / (TAU_F * HIGHPASS_HZ), dt = 1.0f / AUDIO_RATE, a = rc / (rc + dt);
  float x1 = 0, y1 = 0, y2 = 0, pk = 1e-6f;
  for (int i = 0; i < n; i++){
    const float x  = sample((float)i / AUDIO_RATE);
    const float h1 = a * (y1 + x - x1);
    const float h2 = a * (y2 + h1 - y1);
    x1 = x; y1 = h1; y2 = h2;
    raw[i] = h2;
    if (fabsf(h2) > pk) pk = fabsf(h2);
  }
  const float gain = peak * 32767 / pk;
  for (int i = 0; i < n; i++) buf[i] = (int16_t)lroundf(raw[i] * gain);
  free(raw);
  *outLen = n;
  return buf;
}

/* ---------------------------------------------------------------- the codec */
static bool codecBegin(){
  Wire.beginTransmission(ES8311_ADDRRES_0);
  if (Wire.endTransmission() != 0) return false;     /* no codec answering on this unit */

  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, LOW);                    /* amp off — the sound task owns it */
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, -1, PIN_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) return false;

  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  if (!es) return false;
  const es8311_clock_config_t clk = {
    .mclk_inverted = false, .sclk_inverted = false, .mclk_from_mclk_pin = true,
    .mclk_frequency = AUDIO_RATE * 256, .sample_frequency = AUDIO_RATE,
  };
  if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK){ i2s.end(); return false; }
  es8311_voice_volume_set(es, SOUND_VOLUME, NULL);
  es8311_microphone_config(es, false);
  return true;
}

/* Waits on core 0 for a nudge, then plays whatever `playBuf`/`playLen` held
   at that moment. The write blocks on the DMA buffers, so the task always
   yields. The amp is switched on for the first sound and off after
   PA_IDLE_MS of quiet.                                                    */
static void soundTaskFn(void*){
  int16_t quiet[240] = { 0 };
  bool amp = false;
  for (;;){
    if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PA_IDLE_MS))){
      if (amp){ digitalWrite(PIN_PA_CTRL, LOW); amp = false; }
      continue;
    }
    if (!amp){ digitalWrite(PIN_PA_CTRL, HIGH); amp = true; vTaskDelay(pdMS_TO_TICKS(15)); }
    i2s.write((uint8_t*)playBuf, playLen * sizeof(int16_t));
    i2s.write((uint8_t*)quiet, sizeof quiet);
  }
}

static void requestSound(int16_t* buf, int len){
  if (!audioOK || soundMuted || !buf || !soundTask) return;
  playBuf = buf; playLen = len;
  xTaskNotifyGive(soundTask);
}

/* Before the boot intro, so its torpedo fire has something to play through:
   find the codec, render both sounds once, start the task that streams them. */
void soundBegin(){
  audioOK = codecBegin();
  LOGF("boot", "audio: %s", audioOK ? "ES8311 ok" : "NO CODEC - alerts are silent");
  if (!audioOK) return;

  alertBuf = renderSound(alertSample, ALERT_FLASH_MS, SOUND_PEAK, &alertLen);
  if (!alertBuf){
    LOG("boot", "audio: buffer alloc FAILED - sound disabled");
    audioOK = false;
    return;
  }

  xTaskCreatePinnedToCore(soundTaskFn, "sound", 3072, NULL, 5, &soundTask, 0);

  /* a brown-out means the supply sagged — do not let this sound loop it.
     The toast itself waits until after bootIntro() — nothing is on screen
     yet for it to appear over (setup(), ws_lcd_154.ino). Counts as the mute
     being "engaged now" too, so it still auto-expires on the same timer. */
  if (esp_reset_reason() == ESP_RST_BROWNOUT){
    soundMuted = true;
    muteT0 = millis();
    LOG("sound", "muted - the last reset was a brown-out (weak supply). Double-tap UP to unmute");
  }

  introBuf = renderSound(introSample, (int)(I_ANIM_END * 1000), INTRO_PEAK, &introLen);
  if (!introBuf) LOG("boot", "audio: intro sound alloc FAILED - boot intro will be silent");

  noticeBuf = renderSound(noticeSample, NOTICE_MS, NOTICE_PEAK, &noticeLen);
  if (!noticeBuf) LOG("boot", "audio: notice buffer alloc FAILED - warn/fault sound disabled");
}

/* called by render() in the frame a crit flash first shows */
void alertSound(){ requestSound(alertBuf, alertLen); }

/* called once, right as bootIntro() (intro.ino) starts drawing screen 1 */
void introSound(){ requestSound(introBuf, introLen); }

/* called by render() in the frame a warn or fault flash first shows —
   never in the same frame as alertSound(), alertTick() picks one or the other */
void noticeSound(){ requestSound(noticeBuf, noticeLen); }

/* UP double-tap. Also cleared by a restart, or by cfg.muteTimeoutMin
   elapsing on its own — soundTick() below. */
void toggleMute(){
  soundMuted = !soundMuted;
  if (soundMuted) muteT0 = millis();
  LOGF("sound", "%s", soundMuted ? "muted" : "on");
  showToast(soundMuted ? "SOUND OFF" : "SOUND ON");
}

/* Checked every loop(). cfg.muteTimeoutMin is minutes, 0 = never — set on
   the setup page, config.ino owns the store. A mute that started at boot
   (the brown-out case above) is timed exactly the same way.              */
void soundTick(){
  if (!soundMuted || !cfg.muteTimeoutMin) return;
  if (millis() - muteT0 < (uint32_t)cfg.muteTimeoutMin * 60000UL) return;
  soundMuted = false;
  LOGF("sound", "mute auto-expired after %u minute%s", (unsigned)cfg.muteTimeoutMin,
       cfg.muteTimeoutMin == 1 ? "" : "s");
  showToast("SOUND ON");
}
