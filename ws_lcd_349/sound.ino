/* ===========================================================================
   Sound — the boot-intro torpedo fire, the crit alert, the subtler notice,
   and the shake-accepted chirp. The same four waveforms as the square
   build, on this board's ES8311 codec and speaker. The chirp is the
   quietest and shortest of the four, sweeps UP instead of down like the
   alert's own dip, and plays only from imu.ino once a shake gesture is
   confirmed — never for the 2 s hold or the automatic poll, both of which
   also force a refresh but stay silent about it.

   All four are rendered once at boot, in float maths only, then high-
   passed at 250 Hz — the speaker cannot move lower — and streamed from a
   buffer by a task on core 0. Never synthesised live.

   There is no separate amplifier-enable pin on this board; the audio path
   is switched on through the expander's NS_MODE line at boot (main sketch)
   and left on. The sound icon on the settings screen mutes; a restart or
   the timeout set on the setup page clears it again.
   =========================================================================== */
#include <esp_system.h>

static I2SClass i2s;

#define AUDIO_RATE    16000
#define SOUND_VOLUME     74     /* ES8311 0..100, a log scale — 74 is -1.5 dB */
#define SOUND_PEAK    0.85f
#define INTRO_PEAK    0.32f
#define INTRO_HIT_MS    260
#define NOTICE_MS       220
#define NOTICE_PEAK    0.35f
#define NOTICE_HZ      660.0f
#define SHAKE_MS        120     /* the shortest of the four — a UI tick, not a banner */
#define SHAKE_PEAK     0.30f    /* the quietest too — this one just says "got it" */
#define SHAKE_HZ_LO    650.0f   /* sweeps UP, the opposite direction of the alert's dip, so it */
#define SHAKE_HZ_HI   1100.0f   /* never reads as trouble even heard on its own */
#define HIGHPASS_HZ     250

static const float TAU_F = 6.2831853f;
static TaskHandle_t soundTask = NULL;
static int16_t*     alertBuf  = NULL;
static int           alertLen = 0;
static int16_t*     introBuf  = NULL;
static int           introLen = 0;
static int16_t*     noticeBuf = NULL;
static int           noticeLen = 0;
static int16_t*     shakeBuf  = NULL;
static int           shakeLen  = 0;
static int16_t*     playBuf   = NULL;
static int           playLen  = 0;
static uint32_t      muteT0   = 0;      /* millis() mute was last engaged — soundTick() below */

/* a drum kick: a sine sweeping down from fEnd+fSweep to fEnd, decaying */
static float kick(float tau, float fEnd, float fSweep, float k, float decay){
  if (tau < 0 || tau > decay * 7) return 0;
  return sinf(TAU_F * (fEnd * tau + fSweep * k * (1 - expf(-tau / k)))) * expf(-tau / decay);
}

/* the alert: a stressed dip from 920 to 560 Hz, two harmonics, a shiver,
   a thump on the attack and a soft glint, fading out by 500 ms */
static float alertSample(float t){
  const float T = ALERT_FLASH_MS / 1000.0f;
  if (t < 0 || t >= T) return 0;
  const float k = 0.12f;
  const float ph = TAU_F * (560 * t + (920 - 560) * k * (1 - expf(-t / k)));
  const float body  = sinf(ph) + 0.35f * sinf(2 * ph) + 0.2f * sinf(3 * ph);
  const float shiver = 1 - 0.2f * (0.5f + 0.5f * sinf(TAU_F * 14 * t));
  const float knock = kick(t, 180, 220, 0.02f, 0.04f);
  const float glint = sinf(TAU_F * 1760 * t) * expf(-t / 0.08f);
  const float attack  = t < 0.006f ? t / 0.006f : 1;
  const float release = t > 0.38f ? 0.5f + 0.5f * cosf(0.5f * TAU_F * (t - 0.38f) / (T - 0.38f)) : 1;
  const float s = attack * release * (0.42f * body * shiver + 0.35f * knock + 0.08f * glint);
  return tanhf(1.6f * s) / tanhf(1.6f);
}

/* the notice: one soft tone with a touch of its octave — a nudge, not an alarm */
static float noticeSample(float t){
  const float T = NOTICE_MS / 1000.0f;
  if (t < 0 || t >= T) return 0;
  const float ph = TAU_F * NOTICE_HZ * t;
  const float tone = sinf(ph) + 0.15f * sinf(2 * ph);
  const float attack  = t < 0.02f ? t / 0.02f : 1;
  const float release = t > T - 0.08f ? (T - t) / 0.08f : 1;
  return attack * release * tone;
}

/* the shake-accepted chirp: a quick upward sweep, the opposite direction
   of the alert's downward dip, so it reads as "got it" even by itself */
static float shakeSample(float t){
  const float T = SHAKE_MS / 1000.0f;
  if (t < 0 || t >= T) return 0;
  const float hz = SHAKE_HZ_LO + (SHAKE_HZ_HI - SHAKE_HZ_LO) * (t / T);
  const float tone = sinf(TAU_F * hz * t);
  const float attack  = t < 0.01f ? t / 0.01f : 1;
  const float release = t > T - 0.03f ? (T - t) / 0.03f : 1;
  return attack * release * tone;
}

/* one torpedo-fire burst: a soft downward drift plus a rounder low thump,
   eased in over 30 ms and faded out over the last 40 ms so it never clicks */
static float torpedoHit(float tau){
  const float T = INTRO_HIT_MS / 1000.0f;
  if (tau < 0 || tau > T) return 0;
  const float whoosh = kick(tau, 420, 260, 0.045f, 0.13f);
  const float thump  = kick(tau, 130, 60,  0.020f, 0.10f);
  const float A = 0.03f;
  const float p = tau < A ? tau / A : 1.0f;
  const float attack  = p * p * (3.0f - 2.0f * p);
  const float R = 0.04f;
  const float release = tau > T - R ? (T - tau) / R : 1.0f;
  return attack * release * (0.38f * whoosh + 0.20f * thump);
}

/* the intro: a burst on every beam pass — two a turn — fading in over
   150 ms and out over the last 300 ms */
static float introSample(float t){
  const float T = I_ANIM_END;                        /* intro.ino */
  if (t < 0 || t >= T) return 0;
  const float period  = 1.0f / (2.0f * SPIN_HZ);
  const float tau      = fmodf(t, period);
  const float attack  = t < 0.15f ? t / 0.15f : 1;
  const float release = t > T - 0.3f ? (T - t) / 0.3f : 1;
  return attack * release * torpedoHit(tau);
}

/* Render one waveform to a fresh 16-bit buffer. Returns NULL (and leaves
   *outLen at 0) if a malloc fails — a sound that never plays, not a crash. */
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
  if (Wire.endTransmission() != 0) return false;     /* no codec answering */

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
   at that moment. The write blocks on the DMA buffers, so the task yields. */
static void soundTaskFn(void*){
  int16_t quiet[240] = { 0 };
  for (;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    i2s.write((uint8_t*)playBuf, playLen * sizeof(int16_t));
    i2s.write((uint8_t*)quiet, sizeof quiet);
  }
}

static void requestSound(int16_t* buf, int len){
  if (!audioOK || soundMuted || !buf || !soundTask) return;
  playBuf = buf; playLen = len;
  xTaskNotifyGive(soundTask);
}

/* Before the boot intro, so its torpedo fire has something to play through. */
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

  /* a brown-out means the supply sagged — do not let a sound loop it */
  if (esp_reset_reason() == ESP_RST_BROWNOUT){
    soundMuted = true;
    muteT0 = millis();                 /* counts as engaged now, so it still auto-expires */
    LOG("sound", "muted - the last reset was a brown-out (weak supply). Tap the sound icon on the settings screen to unmute");
  }

  introBuf = renderSound(introSample, (int)(I_ANIM_END * 1000), INTRO_PEAK, &introLen);
  if (!introBuf) LOG("boot", "audio: intro sound alloc FAILED - boot intro will be silent");
  noticeBuf = renderSound(noticeSample, NOTICE_MS, NOTICE_PEAK, &noticeLen);
  if (!noticeBuf) LOG("boot", "audio: notice buffer alloc FAILED - warn sound disabled");
  shakeBuf = renderSound(shakeSample, SHAKE_MS, SHAKE_PEAK, &shakeLen);
  if (!shakeBuf) LOG("boot", "audio: shake-chirp buffer alloc FAILED - shake still refreshes, silently");
}

void alertSound(){ requestSound(alertBuf, alertLen); }
void introSound(){ requestSound(introBuf, introLen); }
void noticeSound(){ requestSound(noticeBuf, noticeLen); }
/* called only by imu.ino, only once a shake gesture is confirmed — see
   the file header above for why nothing else may call this one */
void shakeSound(){ requestSound(shakeBuf, shakeLen); }

/* The settings screen's sound icon — see settings.ino's own CONTROLS
   comment. Also cleared by a restart, or by cfg.muteTimeoutMin elapsing
   on its own — soundTick() below. */
void toggleMute(){
  soundMuted = !soundMuted;
  if (soundMuted) muteT0 = millis();
  LOGF("sound", "%s", soundMuted ? "muted" : "on");
  showToast(soundMuted ? "SOUND OFF" : "SOUND ON");
}

/* Checked every loop(). cfg.muteTimeoutMin is minutes, 0 = never — set on
   the setup page, config.ino owns the store.                             */
void soundTick(){
  if (!soundMuted || !cfg.muteTimeoutMin) return;
  if (millis() - muteT0 < (uint32_t)cfg.muteTimeoutMin * 60000UL) return;
  soundMuted = false;
  LOGF("sound", "mute auto-expired after %u minute%s", (unsigned)cfg.muteTimeoutMin,
       cfg.muteTimeoutMin == 1 ? "" : "s");
  showToast("SOUND ON");
}
