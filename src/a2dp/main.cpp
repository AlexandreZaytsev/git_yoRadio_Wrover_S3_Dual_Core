// A2DP sink тест для ESP32-Wrover-Kit
// Bluetooth-приёмник + вывод на I2S + метаданные AVRCP (в монитор)

#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

constexpr int I2S_BCK_PIN  = 26;   // BCK
constexpr int I2S_WS_PIN   = 25;   // WS/LRC
constexpr int I2S_DATA_PIN = 27;   // DOUT

// --- Хранилище метаданных ---
// Сюда пишем последние принятые значения. Позже здесь же
// будет забирать их дисплей yoRadio.
char md_title[64]   = "";
char md_artist[64]  = "";
char md_album[64]   = "";

// --- Колбэк метаданных AVRCP ---
// attribute_id: ESP_AVRC_MD_ATTR_TITLE / _ARTIST / _ALBUM / _PLAYING_TIME ...
// value: строка UTF-8 (для PLAYING_TIME — длительность в мс строкой!)
void avrc_metadata_callback(uint8_t attribute_id, const uint8_t* value) {
    const char* text = reinterpret_cast<const char*>(value);
    switch (attribute_id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            strncpy(md_title, text, sizeof(md_title) - 1);
            md_title[sizeof(md_title) - 1] = '\0';
            Serial.printf("[TITLE ] %s\r\n", md_title);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            strncpy(md_artist, text, sizeof(md_artist) - 1);
            md_artist[sizeof(md_artist) - 1] = '\0';
            Serial.printf("[ARTIST] %s\r\n", md_artist);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            strncpy(md_album, text, sizeof(md_album) - 1);
            md_album[sizeof(md_album) - 1] = '\0';
            Serial.printf("[ALBUM ] %s\r\n", md_album);
            break;
        default:
            Serial.printf("[META  ] attr 0x%x: %s\r\n", attribute_id, text);
            break;
    }
}

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

    // Маска атрибутов: что просим у телефона.
    // По умолчанию библиотека и так берёт самое важное,
    // но зададим явно — чтобы точно получать исполнителя и альбом.
    a2dp_sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE |
        ESP_AVRC_MD_ATTR_ARTIST |
        ESP_AVRC_MD_ATTR_ALBUM);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);

    a2dp_sink.start("yoRadio_Wrover_Sink");
    Serial.println("A2DP sink started, waiting for phone...");
}

void loop() {}
