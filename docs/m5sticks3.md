# M5StickS3

Hardware for Pulsar. Compact stick form factor from M5Stack (Jan 2026). Successor to StickC-Plus2.

## Specs

|               |                                                         |
| ------------- | ------------------------------------------------------- |
| MCU           | ESP32-S3-PICO-1-N8R8, dual-core Xtensa LX7, 240 MHz     |
| Memory        | 8 MB Flash, 8 MB PSRAM                                  |
| Radio         | 2.4 GHz Wi-Fi, Bluetooth 5.0 LE, native USB (OTG & CDC) |
| Display       | 1.14" color TFT, 135 × 240, ST7789P3                    |
| Audio out     | ES8311 codec + AW8737 amp, 8 Ω / 1 W cavity speaker     |
| Audio in      | MEMS microphone                                         |
| IMU           | 6-axis (accel + gyro)                                   |
| IR            | Transmitter and receiver                                |
| Ports         | Hat2-Bus 2.54-16P (top), HY2.0-4P Grove (bottom)        |
| Battery       | 250 mAh LiPo                                            |
| Size / weight | 48.0 × 24.0 × 15.0 mm, 20 g                             |
| Mount         | Magnetic back                                           |
| Buttons       | Power (PMIC), Btn A (front), Btn B (side)               |

## Notes

|                 |                                                  |
| --------------- | ------------------------------------------------ |
| No hardware RTC | Sync time over Wi-Fi (NTP)                       |
| No SD slot      | Use Hat2-Bus if needed                           |
| Battery volume  | Keep speaker under 75% to avoid brownout reboots |
| IR receive      | Turn the speaker amp off or IR RX will fail      |
