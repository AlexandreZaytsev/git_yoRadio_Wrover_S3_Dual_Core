// A2DP sink тест для ESP32-Wrover-Kit
// Принимает Bluetooth-аудио с телефона и выводит на I2S

#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// --- Конфигурация I2S ---
/*
constexpr int PIN_I2S_BCK  = 26;   // BCK  (bit clock)
constexpr int PIN_I2S_WS   = 25;   // LRC  (word select)
constexpr int PIN_I2S_DATA = 22;   // DOUT (data out)
*/
// --- Конфигурация I2S ---
constexpr int I2S_BCK_PIN  = 26;   // BCK  (bit clock)
constexpr int I2S_WS_PIN   = 25;   // LRC  (word select)
constexpr int I2S_DATA_PIN = 22;   // DOUT (data out)


constexpr int I2S_SAMPLE_RATE = 44100;
constexpr const char* BT_DEVICE_NAME = "yoRadio_Wrover_Sink";

// --- Счётчик байтов для контроля потока ---
volatile size_t bytes_received = 0;
unsigned long last_report = 0;

// --- Потоки: только указатели, БЕЗ глобальных конструкторов! ---
I2SStream* i2s = nullptr;
BluetoothA2DPSink* audio_sink = nullptr;

// Колбэк: приходит на каждый кусочек декодированного PCM-потока
void write_data_stream(const uint8_t* data, uint32_t len) {
    bytes_received += len;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== yoRadio A2DP sink test ===");

    // 1. Сначала I2S — выставляем шину
    i2s = new I2SStream();

    /*
    auto cfg = i2s->defaultConfig(TX_MODE);
    cfg.pin_bck      = PIN_I2S_BCK;
    cfg.pin_ws       = PIN_I2S_WS;
    cfg.pin_data     = PIN_I2S_DATA;
*/
auto cfg = i2s->defaultConfig(TX_MODE);
    cfg.pin_bck      = I2S_BCK_PIN;
    cfg.pin_ws       = I2S_WS_PIN;
    cfg.pin_data     = I2S_DATA_PIN;


    cfg.sample_rate  = I2S_SAMPLE_RATE;
    cfg.channels     = 2;
    cfg.bits_per_sample = 16;
    i2s->begin(cfg);

    Serial.println("I2S TX started");

    // 2. Теперь Bluetooth — создаём В РАНТАЙМЕ, не в статической инициализации
    audio_sink = new BluetoothA2DPSink(*i2s);
    audio_sink->set_stream_reader(write_data_stream, false);

    // 3. Старт BT-стека с именем устройства
    audio_sink->start(BT_DEVICE_NAME);

    Serial.print("A2DP sink started as: ");
    Serial.println(BT_DEVICE_NAME);
    Serial.println("Waiting for phone connection...");
}

void loop() {
    // Отчёт о потоке раз в секунду
    unsigned long now = millis();
    if (now - last_report >= 1000) {
        last_report = now;
        size_t rate = bytes_received;
        bytes_received = 0;
        Serial.printf("Rate: %u B/s (target ~176400)\r\n", rate);
    }

    // Держим RSSI доступным, если подключено (для отладки сигнала)
    delay(100);
}
