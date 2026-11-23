#include <Arduino.h>
#include "AudioTools.h"

I2SStream* i2s = nullptr;
SineWaveGenerator<int16_t>* sine = nullptr;
GeneratedSoundStream<int16_t>* sineStream = nullptr;  // было "tone" — конфликт с Arduino tone()
StreamCopy* copier = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);

    i2s = new I2SStream();
    auto cfg = i2s->defaultConfig(TX_MODE);
    cfg.pin_bck = 26;
    cfg.pin_ws  = 25;
    cfg.pin_data = 27;      // GPIO27, а не 22!
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    i2s->begin(cfg);

    sine = new SineWaveGenerator<int16_t>();
    sine->setFrequency(440);
    sine->setAmplitude(8000);

    sineStream = new GeneratedSoundStream<int16_t>(*sine);
    sineStream->begin(cfg);   // тот же формат: 44.1 кГц, 2 канала, 16 бит
    copier = new StreamCopy(*i2s, *sineStream);

    Serial.println("Tone test started: 440 Hz on I2S");
}

void loop() {
    copier->copy();
}
