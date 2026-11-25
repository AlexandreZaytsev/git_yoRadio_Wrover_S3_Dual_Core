// a2dp/main_slave.cpp
#include <Arduino.h>
#include "AudioTools.h"
#include "options_slave_wrover.h"   // если файл в config/ — путь уже в -Iconfig

I2SStream i2s;
//SineGenerator<int16_t> tone;
SineGenerator<int16_t> sine;   // ← было tone — конфликт с tone() из Arduino.h

void setup() {
    Serial.begin(115200);
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.is_master = false;
    cfg.pin_bck  = I2S_BCLK;
    cfg.pin_ws   = I2S_LRC;
    cfg.pin_data = I2S_DOUT;
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    i2s.begin(cfg);
    sine.begin(cfg);
    // sine.setFrequency(440.0f);  // если захотите конкретную ноту


    Serial.println("Slave TX sine started, waiting for S3 clocks...");
}

void loop() {
    uint8_t buf[512];
    sine.readBytes(buf, sizeof(buf));
    i2s.write(buf, sizeof(buf));
}