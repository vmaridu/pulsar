/* ===========================================================================
   Boot intro — once per power-on, 5 s total, two screens, then a cross-fade
   into the dashboard. The same timeline as the square build; the wide
   panel just gives the beams room to run.

   Screen 1 (0.0 – 3.0 s)   The pulsar alone, full screen — a bright core and
                            two straight beams reaching the full width,
                            turning steadily, firing a torpedo-launch burst
                            every time a beam sweeps past top (sound.ino).
   Screen 2 (3.0 – 5.0 s)   A plain black screen with bold PULSAR on it,
                            stencil-cut, centred, a slow light-blue-to-red
                            gradient drifting across the letters. Silent.
   Cross-fade (4.4 – 5.0 s) The dashboard is painted once into a PSRAM copy
                            and the last 0.6 s of screen 2 blends toward it.

   The sound is rendered ONCE into a buffer at boot and streamed — never
   synthesised live, which once starved a core past its watchdog.

   Mirrors the "replay boot" button in ws_lcd_349/mockup.html.
   =========================================================================== */

static const float I_ANIM_END = 3.0f;
static const float I_END      = 5.0f;
static const float I_FADE     = 0.6f;
static const float SPIN_HZ    = 1.3f;
static const int   LOGO_X = W / 2, LOGO_Y = H / 2, BEAM_LEN = 300;   /* the beams run off the short edges — that reads as racing past, not clipping */
static const int   WORD_SIZE = 8;                                    /* PULSAR — 6 px cell × this */
static const int   WORD_W  = 6 * 6 * WORD_SIZE;
static const int   WORD_X  = (W - WORD_W) / 2;
static const int   WORD_Y  = (H - 8 * WORD_SIZE) / 2;
static constexpr uint16_t C_INTRO_BLUE = rgb(0x8fd6ff);
static constexpr uint16_t C_INTRO_RED  = rgb(0xff3b3b);

#define STARS 36
static uint16_t starX[STARS];
static uint8_t  starY[STARS];
static float    starB[STARS], starPh[STARS];
static uint32_t introSeed = 0x1234567UL;

static float introRand(){
  introSeed = introSeed * 1664525UL + 1013904223UL;
  return ((introSeed >> 8) & 0xFFFF) / 65535.0f;
}
static uint16_t grey(float v){
  const uint32_t c = (uint32_t)(clampf(v, 0, 1) * 255);
  return rgb((c << 16) | (c << 8) | c);
}
static uint16_t lerp565(uint16_t c1, uint16_t c2, float m){
  m = clampf(m, 0, 1);
  const int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  const int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  const int r = (int)lroundf(r1 + (r2 - r1) * m);
  const int g = (int)lroundf(g1 + (g2 - g1) * m);
  const int b = (int)lroundf(b1 + (b2 - b1) * m);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/* -------------------------------------------------------- screen 1: pulsar */
static void drawIntroFrame(float t){
  cv->fillScreen(RGB565_BLACK);
  const float in = easeOut(clampf(t / 0.6f, 0, 1));

  for (int i = 0; i < STARS; i++)
    cv->drawPixel(starX[i], starY[i], grey(starB[i] * in * (0.7f + 0.3f * sin(t * 1.7f + starPh[i]))));

  const float theta = 2 * PI * SPIN_HZ * t - PI / 2, len = BEAM_LEN * in;
  for (int b = 0; b < 2; b++){
    const float a = theta + b * PI, ca = cos(a), sa = sin(a);
    cv->fillTriangle(LOGO_X, LOGO_Y,
                     LOGO_X + len * cos(a - 0.035f), LOGO_Y + len * sin(a - 0.035f),
                     LOGO_X + len * cos(a + 0.035f), LOGO_Y + len * sin(a + 0.035f), grey(0.12f * in));
    const int steps = (int)len;
    for (int s = 1; s <= steps; s++){
      int band = (s * 4) / (steps > 0 ? steps : 1);
      if (band > 3) band = 3;
      const int x = LOGO_X + (int)lround(s * ca), y = LOGO_Y + (int)lround(s * sa);
      if (x < 0 || x >= W || y < 0 || y >= H) continue;
      cv->drawPixel(x, y, grey((1 - band * 0.22f) * in));
    }
  }

  float up = cos(4 * PI * SPIN_HZ * t);
  const float pulse = up > 0 ? pow(up, 6) : 0;
  cv->fillCircle(LOGO_X, LOGO_Y, 13 + 2 * pulse, grey((0.07f + 0.05f * pulse) * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 9 + pulse,      grey((0.16f + 0.08f * pulse) * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 6,              grey(0.45f * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 3,              grey(in));
}

/* --------------------------------------------------------- screen 2: label
   Bold PULSAR, printed twice one row apart for stencil weight, two thin
   bridges cut back to black, then every lit pixel recoloured straight in
   the framebuffer with a travelling blue → red gradient. `tt` is seconds
   since THIS screen started.                                             */
static void drawIntroText(float tt){
  cv->fillScreen(RGB565_BLACK);
  const uint16_t white = grey(1.0f), black = grey(0.0f);
  txt("PULSAR", WORD_X, WORD_Y,     WORD_SIZE, white, 'l', true);
  txt("PULSAR", WORD_X, WORD_Y + 1, WORD_SIZE, white, 'l', true);
  const int bandH = WORD_SIZE > 4 ? 3 : 2;
  cv->fillRect(WORD_X, WORD_Y + (int)(0.34375f * 8 * WORD_SIZE), WORD_W, bandH, black);
  cv->fillRect(WORD_X, WORD_Y + (int)(0.65625f * 8 * WORD_SIZE), WORD_W, bandH, black);

  uint16_t* fb = cv->getFramebuffer();
  const int y0 = WORD_Y - 1, y1 = WORD_Y + 8 * WORD_SIZE + 1;
  const int x0 = WORD_X - 1, x1 = WORD_X + WORD_W + 2;
  for (int y = y0; y <= y1; y++){
    if (y < 0 || y >= H) continue;
    for (int x = x0; x <= x1; x++){
      if (x < 0 || x >= W) continue;
      const uint32_t i = fbIndex(x, y);       /* the canvas is rotated — never y*W+x here */
      if (!fb[i]) continue;
      const float u = (x - WORD_X) / (float)WORD_W;
      float mix = 0.5f + 0.5f * sinf(2 * PI * (u - 0.35f * tt));
      mix += 0.06f * sinf(tt * 23.0f + x * 0.25f);
      fb[i] = lerp565(C_INTRO_BLUE, C_INTRO_RED, mix);
    }
  }
}

/* dst = dst · (32 − a) + src · a, per RGB565 channel, a in 0 … 32 */
static void blendFrame(uint16_t* dst, const uint16_t* src, uint32_t n, uint32_t a){
  const uint32_t ia = 32 - a;
  for (uint32_t i = 0; i < n; i++){
    const uint32_t d = dst[i], s = src[i];
    const uint32_t r = (((d >> 11) & 0x1F) * ia + ((s >> 11) & 0x1F) * a) >> 5;
    const uint32_t g = (((d >> 5)  & 0x3F) * ia + ((s >> 5)  & 0x3F) * a) >> 5;
    const uint32_t b = (( d        & 0x1F) * ia + ( s        & 0x1F) * a) >> 5;
    dst[i] = (uint16_t)((r << 11) | (g << 5) | b);
  }
}

/* Blocks setup() for I_END seconds. For the fade the dashboard is painted
   once into a PSRAM copy and each frame from I_END - I_FADE onward is
   blended toward it.                                                     */
void bootIntro(){
  for (int i = 0; i < STARS; i++){
    starX[i] = (uint16_t)(introRand() * (W - 1)); starY[i] = (uint8_t)(introRand() * (H - 1));
    starB[i] = 0.10f + introRand() * 0.25f; starPh[i] = introRand() * 2 * PI;
  }
  const uint32_t n = (uint32_t)PANEL_W * PANEL_H;
  uint16_t* dash = NULL;
  bool tried = false;

  introSound();
  const uint32_t t0 = millis();
  for (;;){
    const float t = (millis() - t0) / 1000.0f;
    if (t >= I_END) break;

    const float f = clampf((t - (I_END - I_FADE)) / I_FADE, 0, 1);
    if (f > 0 && !tried){
      tried = true;
      dash = (uint16_t*)ps_malloc(n * sizeof(uint16_t));
      if (dash){ drawDashboard(); memcpy(dash, cv->getFramebuffer(), n * sizeof(uint16_t)); }
    }
    if (t < I_ANIM_END) drawIntroFrame(t); else drawIntroText(t - I_ANIM_END);
    if (dash && f > 0) blendFrame(cv->getFramebuffer(), dash, n, (uint32_t)lround(f * f * (3 - 2 * f) * 32));
    panelFlush();
  }
  free(dash);
}
