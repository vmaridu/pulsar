/* ===========================================================================
   Boot intro — once per power-on, 5 s total, two screens, then a cross-fade
   into the dashboard.

   Screen 1 (0.0 – 3.0 s)   The pulsar alone, full screen — a bright core and
                            two straight beams reaching almost to every edge,
                            turning steadily, humming low in step with them
                            (sound.ino). No text, nothing else.
   Screen 2 (3.0 – 5.0 s)   A plain black screen with bold PULSAR on it,
                            stencil-cut, centred, coloured with a slow
                            light-blue-to-red gradient that drifts across the
                            letters. Silent — the hum stopped with screen 1.
   Cross-fade (4.4 – 5.0 s) The dashboard is painted once into a PSRAM copy
                            and the last 0.6 s of screen 2 blends toward it
                            in RGB565 — real pixels, not a wipe.

   The hum is rendered ONCE into a buffer at boot and streamed, exactly like
   the critical alert — never synthesised live. An earlier version did
   synthesise live, on core 0, and starved that core's idle task past the
   watchdog and boot-looped the board. That is why sound.ino always renders
   first and only ever streams from here on.

   Mirrors the "boot intro" button in ws_lcd_154/mockup.html.
   =========================================================================== */

static const float I_ANIM_END = 3.0f;       /* screen 1 (the pulsar, humming) runs 0..this */
static const float I_END      = 5.0f;       /* hand over to the dashboard */
static const float I_FADE     = 0.6f;       /* the cross-fade, at the very end — overlaps screen 2 */
static const float SPIN_HZ    = 1.3f;       /* turns a second — steady */
static const int   LOGO_X = 120, LOGO_Y = 120, BEAM_LEN = 112;   /* screen centre; beams reach near every edge */
static const int   WORD_SIZE = 5;                                /* PULSAR's text size — 6px cell × this */
static const int   WORD_W  = 6 * 6 * WORD_SIZE;                   /* strlen("PULSAR") * 6 * size */
static const int   WORD_X  = (240 - WORD_W) / 2;                  /* horizontally centred */
static const int   WORD_Y  = (240 - 8 * WORD_SIZE) / 2;            /* top of the word, vertically centred */
static constexpr uint16_t C_INTRO_BLUE = rgb(0x8fd6ff);   /* light blue */
static constexpr uint16_t C_INTRO_RED  = rgb(0xff3b3b);   /* red */

#define STARS 36
static uint8_t  starX[STARS], starY[STARS];
static float    starB[STARS], starPh[STARS];
static uint32_t introSeed = 0x1234567UL;

static float introRand(){                            /* 0..1 */
  introSeed = introSeed * 1664525UL + 1013904223UL;
  return ((introSeed >> 8) & 0xFFFF) / 65535.0f;
}
static uint16_t grey(float v){
  const uint32_t c = (uint32_t)(clampf(v, 0, 1) * 255);
  return rgb((c << 16) | (c << 8) | c);
}
/* blend two RGB565 colours, m 0..1 */
static uint16_t lerp565(uint16_t c1, uint16_t c2, float m){
  m = clampf(m, 0, 1);
  const int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  const int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  const int r = (int)lroundf(r1 + (r2 - r1) * m);
  const int g = (int)lroundf(g1 + (g2 - g1) * m);
  const int b = (int)lroundf(b1 + (b2 - b1) * m);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/* -------------------------------------------------------- screen 1: pulsar
   Full screen, no text. Straight beams: each core line is walked one pixel
   at a time along its own ray — `centre + r·(cos a, sin a)` for a single,
   unchanging `a` — rather than drawn as separate segments, which is what
   keeps them perfectly straight at every angle.                          */
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
      cv->drawPixel(x, y, grey((1 - band * 0.22f) * in));
    }
  }

  /* the star: glow rings round a white core, pulsing as a beam passes up */
  float up = cos(4 * PI * SPIN_HZ * t);
  const float pulse = up > 0 ? pow(up, 6) : 0;
  cv->fillCircle(LOGO_X, LOGO_Y, 13 + 2 * pulse, grey((0.07f + 0.05f * pulse) * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 9 + pulse,      grey((0.16f + 0.08f * pulse) * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 6,              grey(0.45f * in));
  cv->fillCircle(LOGO_X, LOGO_Y, 3,              grey(in));
}

/* --------------------------------------------------------- screen 2: label
   Plain black, bold PULSAR, no plate — a stencil-cut label, not an effect.
   Printed twice one row apart (on top of the built-in font's own sideways
   bold pass) for a thick, army-stencil weight, then two thin horizontal
   bands are cut back to black straight across the whole word — the
   "bridges" a real stencil template needs to hold its letters together,
   same height on every letter regardless of shape.

   Drawn white first so the stencil cut has a clean mask to work from, then
   every pixel the mask left lit is recoloured from the framebuffer directly
   — a travelling light-blue → red gradient, plus a small fast ripple for a
   shimmer, both a function of x and time so the whole thing visibly drifts
   for as long as this screen is up. `tt` is seconds since THIS screen
   started (0 at the cut to screen 2), not since bootIntro() began.        */
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
  const int x0 = WORD_X - 1, x1 = WORD_X + WORD_W + 2;   /* +1 col from the bold pass, +1 margin */
  for (int y = y0; y <= y1; y++){
    if (y < 0 || y > 239) continue;
    for (int x = x0; x <= x1; x++){
      if (x < 0 || x > 239) continue;
      const int i = y * 240 + x;
      if (!fb[i]) continue;                                    /* black — the cut, or off the glyph */
      const float u = (x - WORD_X) / (float)WORD_W;             /* 0..1 across the word */
      float mix = 0.5f + 0.5f * sinf(2 * PI * (u - 0.35f * tt));  /* the travelling wave */
      mix += 0.06f * sinf(tt * 23.0f + x * 0.25f);                /* the shimmer */
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

/* Blocks setup() for I_END seconds: screen 1 the pulsar (humming), then
   screen 2 the label. For the fade the dashboard is painted once into a
   PSRAM copy, and each frame from I_END - I_FADE onward is blended toward
   it.                                                                    */
static void bootIntro(){
  for (int i = 0; i < STARS; i++){
    starX[i] = introRand() * 239; starY[i] = introRand() * 239;
    starB[i] = 0.10f + introRand() * 0.25f; starPh[i] = introRand() * 2 * PI;
  }
  const uint32_t n = 240UL * 240UL;
  uint16_t* dash = NULL;
  bool tried = false;

  introSound();             /* sound.ino — starts now, runs I_ANIM_END seconds, then silence */
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
    cv->flush();
  }
  free(dash);
}
