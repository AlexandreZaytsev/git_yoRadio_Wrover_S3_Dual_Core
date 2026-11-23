// A2DP sink тест для ESP32-Wrover-Kit
#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

constexpr int I2S_BCK_PIN  = 26;   // BCK
constexpr int I2S_WS_PIN   = 25;   // WS/LRC
constexpr int I2S_DATA_PIN = 27;   // DOUT (провод уже на GPIO27 с синус-теста)

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== yoRadio A2DP sink test ===");

    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_bck  = I2S_BCK_PIN;
    cfg.pin_ws   = I2S_WS_PIN;
    cfg.pin_data = I2S_DATA_PIN;
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    i2s.begin(cfg);
    Serial.println("I2S TX started");

    // Библиотека сама пишет принятый PCM в I2S — колбэки не нужны
    a2dp_sink.start("yoRadio_Wrover_Sink");
    Serial.println("A2DP sink started, waiting for phone...");
}

void loop() {}
