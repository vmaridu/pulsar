/* ===========================================================================
   Power — the latch, off and on, and the backlight.

   How off and on work — from Waveshare's own BATT_PWR demo for this board:
     • The power latch is a line on the I/O expander (EXIO_SYS_EN). Driven
       HIGH the board runs from the cell; driven LOW it goes dark — on
       battery that is truly off.
     • Pressing LEFT (the key the power circuit listens to) powers the board
       up by itself. The firmware latches the
       line only once the key has been held 2 s — let go sooner and the
       power simply drops again, so a brush against the key does nothing.
     • On USB the rail stays up whatever the latch says, so "off" is the
       panel dark plus deep sleep, and LEFT wakes it into the same 2 s hold.

   The backlight is two things: an enable line on the expander, and a PWM
   pin whose duty is ACTIVE LOW — 0 is full, 255 is dark.
   =========================================================================== */
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#define BOOTLOADER_MS    400     /* the power key was already down this long before setup() could count */

/* ---------------------------------------------------------------- display */

/* The backlight's duty, from the setup page's two brightness figures.
   Nothing on this board reports the charger, so charging() is always
   false — and a desk display that is USB-powered nine times out of ten
   must not come up dim for it. The on-charger figure applies at all
   times here; the on-battery figure is stored for a revision that can
   tell the two apart. Asleep is dark regardless. This is also where
   liveBrightnessPct — the settings screen's temporary brightness nudge —
   resyncs to the configured duty, so a config save always wins over
   whatever the +/- icons had left it at.                                */
void applyBacklight(){
  if (!displayAwake){
    ledcWrite(PIN_LCD_BL, 255);
    exioWrite(EXIO_BL_EN, false);
    return;
  }
  liveBrightnessPct = cfg.brightnessCharging;
  ledcWrite(PIN_LCD_BL, 255 - (uint32_t)liveBrightnessPct * 255 / 100);
  exioWrite(EXIO_BL_EN, true);
}

/* Settings screen's brightness +/- icons. A temporary nudge, in RAM only
   — liveBrightnessPct is never written to cfg/NVS, so it's back to
   whatever the setup page says the moment the board restarts, the same
   rule soundMuted already follows. Clamped to BRIGHTNESS_MIN..MAX, the
   same range the setup page itself allows.                              */
void brightnessNudge(int deltaPct){
  if (!displayAwake) return;
  int pct = (int)liveBrightnessPct + deltaPct;
  if (pct < BRIGHTNESS_MIN) pct = BRIGHTNESS_MIN;
  if (pct > BRIGHTNESS_MAX) pct = BRIGHTNESS_MAX;
  liveBrightnessPct = (uint8_t)pct;
  ledcWrite(PIN_LCD_BL, 255 - (uint32_t)liveBrightnessPct * 255 / 100);
  LOGF("power", "brightness %d%% (temporary, until restart)", liveBrightnessPct);
}

/* -------------------------------------------------------------- low battery
   A safety net, not a warning system: at 10% and not on the charger, the
   board shuts itself down after a 2-minute grace period rather than run
   the cell down to where it can't safely restart. Checked once a second.
   With no charger detection on this board the cancel can only come from
   the voltage climbing back over the line — which a charger does cause.

   The backlight is real load: turning it back on (RIGHT tap, off to on)
   pulls noticeably more current than a dark panel, and a cell that's
   getting weak sags under that step just long enough to read low for a
   check or two even though it recovers moments later. batteryVolts()'s
   own 8-sample burst can't catch this — the whole burst happens within
   the same brief moment, so it rides straight through a sag exactly
   like it rides through real ADC noise. So a low reading has to repeat
   for LOW_BATT_CONFIRM checks running before the grace timer even
   starts — long enough that a load-step sag has settled back out, short
   enough that a genuinely low cell (which stays low) is barely delayed. */
#define LOW_BATT_PCT       10
#define LOW_BATT_GRACE_MS  (2UL * 60000UL)
#define LOW_BATT_CONFIRM_S     3       /* consecutive low seconds before it's trusted */
static uint32_t lowBattT0 = 0;         /* 0 = the shutdown timer isn't running */
static uint8_t  lowBattStreak = 0;     /* consecutive low checks so far, confirming it first */
void batteryTick(){
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  const int pc = batteryPercent();
  if (charging() || pc > LOW_BATT_PCT){
    if (lowBattT0) LOG("power", "battery recovered - shutdown cancelled");
    lowBattT0 = 0;
    lowBattStreak = 0;
    return;
  }
  if (lowBattStreak < LOW_BATT_CONFIRM_S){
    lowBattStreak++;
    return;                       /* could still be a load-step sag settling out, not yet trusted */
  }
  if (!lowBattT0){
    lowBattT0 = millis();
    LOGF("power", "battery at %d%% - shutting down in 2 min unless it recovers", pc);
    showToast("LOW BATTERY");
    noticeSound();
    return;
  }
  if (millis() - lowBattT0 >= LOW_BATT_GRACE_MS){
    LOG("power", "battery still low after 2 min - shutting down to protect the cell");
    powerOff();
  }
}

/* RIGHT tap. DISPLAY OFF IS NOT POWER OFF: the backlight goes dark, and
   nothing else stops — the cycle keeps turning, and a crit still
   sounds in the dark.

   ===== DEVICE-SPECIFIC FIX — read before copying this pattern anywhere =====
   This does NOT call panel->displayOff()/displayOn() any more, on purpose.
   Those send SLPIN/SLPOUT, and on THIS AXS15231B panel that pair is not
   reliably reversible by a bare wake call: the first off-then-on cycle
   looks fine, but a second one can leave the panel showing a corrupted
   line and the picture degrading from there — this board's own repro.
   powerDown() below already hit the same wall once, for the power
   off/on path, and worked around it by going through a full reboot
   (which re-runs tftInit()) instead of a bare displayOn() — see its own
   comment on EXIO_SYS_EN. The display toggle has no reboot to fall back
   on, so it takes the other way out: it never puts the panel to sleep
   in the first place. Only the backlight goes dark; the panel keeps
   rendering behind it the whole time. functional-requirements.md only
   ever promised the panel goes dark, never that this is a genuine
   low-power sleep, so nothing here breaks that contract — it just
   sidesteps a sleep/wake bug specific to this one panel. Before reusing
   panel->displayOff()/displayOn() on another board (or putting them back
   here), confirm on real hardware that ITS panel actually survives a
   repeated sleep/wake cycle — this one didn't.
   ============================================================================ */
void displayToggle(){
  displayAwake = !displayAwake;
  applyBacklight();
  LOG("key", displayAwake ? "RIGHT tap -> display on (the cycle never stopped)"
                           : "RIGHT tap -> display off (still cycling, a crit still sounds)");
}

/* ------------------------------------------------------------------ power */

/* Cut power. On the cell the board is gone after the latch drops; on USB it
   carries on into deep sleep, woken by the power key.                     */
static void powerDown(){
  ledcWrite(PIN_LCD_BL, 255);
  exioWrite(EXIO_BL_EN, false);
  panel->displayOff();
  while (digitalRead(KEY_POWER) == LOW) delay(10);   /* let go first, or the key wakes it straight back */
  delay(60);
  LOG("power", "off");
  Serial.flush();
  exioWrite(EXIO_SYS_EN, false);                     /* on battery, this is the end */
  delay(400);

  /* Still here, so USB holds the chip up. Put the latch back before
     sleeping: with it dropped, the display's own rail goes with it, and
     the panel does not come back cleanly when the latch returns on wake —
     it woke to a dark glass and a touch controller that no longer answered
     on I²C. Latched, the panel just sleeps (the SLPIN above) with its
     backlight enable off, and the wake boot's init brings it straight back. */
  exioWrite(EXIO_SYS_EN, true);
  LOG("power", "USB holds the rail up - deep sleep until LEFT is pressed");
  Serial.flush();
  rtc_gpio_pullup_en((gpio_num_t)KEY_POWER);
  rtc_gpio_pulldown_dis((gpio_num_t)KEY_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)KEY_POWER, 0);
  esp_deep_sleep_start();
}

void powerOff(){
  cv->fillScreen(RGB565_BLACK);
  txt("OFF", W / 2, H / 2 - 8, 2, C_DIM, 'c', true);
  panelFlush();
  delay(400);
  powerDown();
}

/* First thing after the panel is up. Started by the power key — from off, or
   woken from sleep — it has to be held 2 s before power stays latched.    */
void powerOnHold(){
  pinMode(KEY_POWER, INPUT_PULLUP);
  const bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
  if (digitalRead(KEY_POWER) != LOW){
    if (woke){ LOG("power", "key let go before boot - back off"); powerDown(); }
    return;                                          /* USB plugged in, or reset — just start */
  }
  LOG("power", "key held - keep holding 2 s to switch on");
  for (;;){
    const uint32_t held = millis() + BOOTLOADER_MS;
    if (held >= HOLD_MS){ LOG("power", "on"); return; }
    if (digitalRead(KEY_POWER) != LOW) break;
    cv->fillScreen(RGB565_BLACK);
    holdRing(held / (float)HOLD_MS, "ON");           /* input.ino */
    panelFlush();
    delay(16);
  }
  LOG("power", "let go early - back off");
  cv->fillScreen(RGB565_BLACK);
  panelFlush();
  powerDown();
}
