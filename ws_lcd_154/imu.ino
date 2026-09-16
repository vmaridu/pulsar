/* ===========================================================================
   The motion sensor — shake detection, nothing else.

   The board carries a QMI8658 6-axis IMU on the same I2C bus as touch
   (Waveshare's own demo: SDA/SCL 42/41, SensorLib's SensorQMI8658). Only the
   accelerometer is read; the gyroscope stays off — a shake gesture only
   needs to know how hard the board is moving, not which way it's turned.

   Shaking the device three times forces the same immediate poll a 2 s hold
   on UP already does (net.ino's refreshNow()) — a second way to ask for one,
   not a different action. One rule keeps the two from being confused: a
   short, quiet confirmation chirp (sound.ino's shakeSound()) plays only when
   a shake is what triggered the poll — never for the hold, and never for the
   poll that happens on its own when the cycle wraps.

   Detection is threshold-and-cooldown, not the chip's own tap/wake-on-motion
   hardware feature — those are tuned for a different gesture (a knock on the
   case, or picking the board up), and polling the raw accelerometer here
   keeps the whole gesture defined in one place, in units this file already
   understands. At rest the combined vector reads ~1 g; a shake's peaks push
   it well past that. Three such peaks, each far enough from the last to be
   a new swing and not the same one still ringing down, close enough
   together to be one deliberate gesture, and the poll fires.           */
#define SHAKE_G_THRESHOLD   1.8f    /* combined accel magnitude past this counts as one impulse */
#define SHAKE_PEAK_GAP_MS    150    /* shorter than this since the last impulse and it's the same
                                        swing's ring-down, not a second shake */
#define SHAKE_MAX_GAP_MS     700    /* longer than this since the last counted impulse and the
                                        count restarts — three shakes means close together, not
                                        three taps scattered across a lazy half-minute */
#define SHAKE_NEEDED           3

static uint32_t shakeLastT0 = 0;
static uint8_t  shakeCount  = 0;

void imuBegin(){
  imuOK = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  LOGF("boot", "imu: %s", imuOK ? "QMI8658 ok" : "NOT FOUND (shake to refresh disabled)");
  if (!imuOK) return;
  imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz,
                           SensorQMI8658::LPF_MODE_3);
  imu.enableAccelerometer();
}

void imuTick(){
  if (!imuOK) return;
  if (!imu.getDataReady()) return;
  float x, y, z;
  if (!imu.getAccelerometer(x, y, z)) return;
  const float mag = sqrtf(x * x + y * y + z * z);
  if (mag < SHAKE_G_THRESHOLD) return;

  const uint32_t now = millis();
  if (shakeCount && now - shakeLastT0 < SHAKE_PEAK_GAP_MS) return;      /* same swing, still ringing down */
  if (shakeCount && now - shakeLastT0 > SHAKE_MAX_GAP_MS) shakeCount = 0; /* too slow — start over */

  shakeCount++;
  shakeLastT0 = now;
  LOGF("imu", "shake %u/%u (%.2f g)", (unsigned)shakeCount, (unsigned)SHAKE_NEEDED, mag);

  if (shakeCount >= SHAKE_NEEDED){
    shakeCount = 0;
    LOG("imu", "shake x3 -> forced refresh");
    shakeSound();               /* sound.ino — the shake gesture's own confirmation, nothing else plays it */
    refreshNow("shake x3");     /* net.ino */
  }
}
