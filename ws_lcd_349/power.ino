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
   tell the two apart. Asleep is dark regardless.                        */
void applyBacklight(){
  if (!displayAwake){
    ledcWrite(PIN_LCD_BL, 255);
    exioWrite(EXIO_BL_EN, false);
    return;
  }
  const uint8_t pct = cfg.brightnessCharging;
  ledcWrite(PIN_LCD_BL, 255 - (uint32_t)pct * 255 / 100);
  exioWrite(EXIO_BL_EN, true);
}

/* -------------------------------------------------------------- low battery
   A safety net, not a warning system: at 10% and not on the charger, the
   board shuts itself down after a 2-minute grace period rather than run
   the cell down to where it can't safely restart. Checked once a second.
   With no charger detection on this board the cancel can only come from
   the voltage climbing back over the line — which a charger does cause. */
#define LOW_BATT_PCT       10
#define LOW_BATT_GRACE_MS  (2UL * 60000UL)
static uint32_t lowBattT0 = 0;     /* 0 = the shutdown timer isn't running */
void batteryTick(){
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  const int pc = batteryPercent();
  if (charging() || pc > LOW_BATT_PCT){
    if (lowBattT0) LOG("power", "battery recovered - shutdown cancelled");
    lowBattT0 = 0;
    return;
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

/* RIGHT tap. DISPLAY OFF IS NOT POWER OFF: the panel sleeps and the backlight
   goes dark, and nothing else stops — the cycle keeps turning, and a
   critical still sounds in the dark.                                     */
void displayToggle(){
  displayAwake = !displayAwake;
  if (displayAwake){
    panel->displayOn();
    applyBacklight();
    LOG("key", "RIGHT tap -> display on (the cycle never stopped)");
  } else {
    applyBacklight();          /* dark, now that displayAwake is false */
    panel->displayOff();
    LOG("key", "RIGHT tap -> display off (still cycling, a critical still sounds)");
  }
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
