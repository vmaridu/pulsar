/* ===========================================================================
   Power — the latch, off and on, and the display on / off toggle.

   Keys and touch live in input.ino; this file is only what they drive.

   How off and on work — from Waveshare's own bsp_power_manager.c and factory
   firmware for this board:
     • GPIO2 latches the cell's power. Drive it LOW and the board goes dark;
       on battery that is truly off.
     • Pressing the power key powers the board up by itself. The firmware
       latches GPIO2 only once the key has been held 2 s — let go sooner and
       the power simply drops again, so a brush against the key does nothing.
     • On USB the rail stays up whatever GPIO2 says, so "off" is the panel
       dark plus deep sleep, and the power key wakes it into the same 2 s hold.

   DISPLAY OFF IS NOT POWER OFF. A tap on RIGHT sleeps the panel and kills the
   backlight, and nothing else stops: the cycle keeps turning, the poll keeps
   polling, and a critical still sounds in the dark. That is the whole point —
   the speaker is there for when you are not looking at it.
   =========================================================================== */
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#define BOOTLOADER_MS    400     /* the power key was already down this long before setup() could count */

/* ---------------------------------------------------------------- display */

/* RIGHT tap. The panel only — see the header. */
void displayToggle(){
  displayAwake = !displayAwake;
  if (displayAwake){
    panel->displayOn();
    digitalWrite(PIN_LCD_BL, HIGH);
    LOG("key", "RIGHT tap -> display on (polling and alerts never stopped)");
  } else {
    digitalWrite(PIN_LCD_BL, LOW);
    panel->displayOff();
    LOG("key", "RIGHT tap -> display off (still polling, a critical still sounds)");
  }
}

/* ------------------------------------------------------------------ power */

/* Cut power. On the cell the board is gone after the latch drops; on USB it
   carries on into deep sleep, woken by the power key.                     */
static void powerDown(){
  digitalWrite(PIN_LCD_BL, LOW);
  panel->displayOff();
  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, LOW);
  while (digitalRead(KEY_POWER) == LOW) delay(10);   /* let go first, or the key wakes it straight back */
  delay(60);
  LOG("power", "off");
  Serial.flush();
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, LOW);                   /* on battery, this is the end */
  delay(400);

  LOG("power", "USB holds the rail up - deep sleep until the power key");
  Serial.flush();
  rtc_gpio_pullup_en((gpio_num_t)KEY_POWER);
  rtc_gpio_pulldown_dis((gpio_num_t)KEY_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)KEY_POWER, 0);
  esp_deep_sleep_start();
}

void powerOff(){
  cv->fillScreen(RGB565_BLACK);
  txt("OFF", 120, 112, 2, C_DIM, 'c', true);
  cv->flush();
  delay(400);
  powerDown();
}

/* First thing after the panel is up. Started by the power key — from off, or
   woken from sleep — it has to be held 2 s before power is latched.        */
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
    cv->flush();
    delay(16);
  }
  LOG("power", "let go early - back off");
  cv->fillScreen(RGB565_BLACK);
  cv->flush();
  powerDown();
}
