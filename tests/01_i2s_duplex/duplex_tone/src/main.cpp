#include <Arduino.h>
#include "ESP_I2S.h"

#define I2S_BCLK 38
#define I2S_LRC  39
#define I2S_DOUT 47
#define I2S_DIN  8    // резерв был 17, ныне UART TX.

I2SClass i2s;

const uint32_t SAMPLE_RATE = 44100;
const int16_t  AMP = 4000;
const int      FREQ = 440;
const int      CHUNK = 256;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[FULLDUP] init i2s (TX=47, RX=8 на Wrover IO23)...");
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_DIN, -1);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.printf("[FULLDUP] begin FAILED, lastError=%d\n", i2s.lastError());
    while (true) delay(1000);
  }
  Serial.println("[FULLDUP] OK");
}

void loop() {
  static uint8_t rxBuf[512];          // 128 стерео-кадров
  static uint32_t total = 0, nonzero = 0;

  size_t n = i2s.readBytes((char*)rxBuf, sizeof(rxBuf));
  if (n > 0) {
    i2s.write(rxBuf, n);              // ← принятое сразу в звук
    total += n;
    for (size_t i = 0; i < n; i++)
      if (rxBuf[i] != 0) nonzero++;
  }

  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("[FULLDUP] RX->TX: %u байт, ненулевых: %u %s\n",
                  (unsigned)total, (unsigned)nonzero,
                  nonzero == 0 ? "(все нули)" : "(есть данные!)");
    total = 0; nonzero = 0;
  }
}