// A2DP sink тест для ESP32-Wrover-Kit
#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

constexpr int I2S_BCK_PIN  = 26;
constexpr int I2S_WS_PIN   = 25;
constexpr int I2S_DATA_PIN = 27;

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);   // ← объект объявлен ДО колбэков

char md_title[64]   = "";
char md_artist[64]  = "";
char md_album[64]   = "";

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

// Теперь a2dp_sink уже объявлен — ошибки не будет
void connection_state_changed(esp_a2d_connection_state_t state, void*) {
    Serial.printf("[STATE ] %s\r\n", a2dp_sink.to_str(state));
}


// Колбэк громкости: телефон меняет уровень (0-127) по AVRCP
void volume_changed(int vol) {
    Serial.printf("[VOLUME] %d/127\r\n", vol);
    // здесь позже будет дергаться энкодер/дисплей yoRadio
}

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

    a2dp_sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE |
        ESP_AVRC_MD_ATTR_ARTIST |
        ESP_AVRC_MD_ATTR_ALBUM);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);

//    a2dp_sink.set_avrc_rn_volumechanged_callback(volume_changed);
    a2dp_sink.set_avrc_rn_volumechange(volume_changed);

    a2dp_sink.set_auto_reconnect(true, 20);   // до 20 попыток дозвона
    a2dp_sink.start("yoRadio_Wrover_Sink");
    Serial.println("A2DP sink started, waiting for phone...");
}

void loop() {}

/*

pio run -e a2dp_sink -t upload

*/