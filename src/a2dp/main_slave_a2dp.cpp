// a2dp/main_slave_a2dp.cpp
// BT-этап: телефон → A2DP (Wrover) → ring buffer → I2S TX slave → S3 → колонки
//
// Архитектура сознательно отличается от теста a2dp_sink (main.cpp):
//   там A2DP сам писал в I2SStream-мастер — работало.
//   Здесь I2S — слейв (такты от S3), пишет в него loop(), как в этапе 4.
//   A2DP только наполняет буфер. В BT-задаче I2S не трогаем —
//   это лечит краш I2SDriverESP32V1::onSentCb.

#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include <freertos/ringbuf.h>
#include "options_slave_wrover.h"

I2SStream i2s;
BluetoothA2DPSink* a2dp_sink = nullptr;   // ← только через new в setup()

RingbufHandle_t ring = nullptr;           // декодер → буфер → loop()
constexpr size_t RING_SIZE = 64 * 1024;   // ~1,8 с музыки при 176 400 Б/с

char md_title[64]  = "";
char md_artist[64] = "";
char md_album[64]  = "";

// --- статистика (пишется из BT-задачи, печатается из loop) ---
volatile uint32_t bt_bytes = 0;
volatile uint32_t bt_nonzero = 0;
volatile uint32_t ring_overflow = 0;
uint32_t last_bt = 0, last_nz = 0, last_tx = 0, last_ms = 0;
volatile uint32_t tx_bytes = 0;

void avrc_metadata_callback(uint8_t attribute_id, const uint8_t* value) {
    const char* text = reinterpret_cast<const char*>(value);
    switch (attribute_id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            strncpy(md_title, text, sizeof(md_title) - 1);
            Serial.printf("[TITLE ] %s\r\n", md_title);
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            strncpy(md_artist, text, sizeof(md_artist) - 1);
            Serial.printf("[ARTIST] %s\r\n", md_artist);
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            strncpy(md_album, text, sizeof(md_album) - 1);
            Serial.printf("[ALBUM ] %s\r\n", md_album);
            break;
        default:
            Serial.printf("[META  ] attr 0x%x: %s\r\n", attribute_id, text);
            break;
    }
}

void connection_state_changed(esp_a2d_connection_state_t state, void*) {
    Serial.printf("[STATE ] %s\r\n", a2dp_sink->to_str(state));
}

void volume_changed(int vol) {
    Serial.printf("[VOLUME] %d/127\r\n", vol);
}

// Колбэк декодера A2DP — вызывается из BT-задачи.
// ТВЁРДОЕ ПРАВИЛО: здесь НЕ трогаем I2S, только наполняем буфер.
void reader_cb(const uint8_t* data, uint32_t len) {
    bt_bytes += len;
    for (uint32_t i = 0; i < len; i++) {
        if (data[i]) { bt_nonzero++; }   // ненулевые байты = «есть музыка»
    }
    if (xRingbufferSend(ring, data, len, 0) != pdTRUE) {
        ring_overflow++;   // буфер полон — выбрасываем кусок
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== yoRadio BT-этап: A2DP -> буфер -> I2S slave TX ===");

    ring = xRingbufferCreate(RING_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (ring == nullptr) {
        Serial.println("[FATAL] ring buffer не создан");
        while (true) delay(1000);
    }

    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.is_master = false;            // такты от S3: BCLK 38→26, LRC 39→25
    cfg.pin_bck  = I2S_BCLK;
    cfg.pin_ws   = I2S_LRC;
    cfg.pin_data = I2S_DOUT;
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    i2s.begin(cfg);
    Serial.println("I2S TX slave started, waiting for S3 clocks...");

    a2dp_sink = new BluetoothA2DPSink();          // БЕЗ I2SStream: выводом заняты сами
    a2dp_sink->set_stream_reader(reader_cb, false); // false = не писать в I2S из BT-задачи

    a2dp_sink->set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
    a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink->set_on_connection_state_changed(connection_state_changed);
    // a2dp_sink->set_avrc_rn_volumechange(volume_changed);  // включим после первого звука
    a2dp_sink->set_auto_reconnect(true, 20);
    a2dp_sink->start("yoRadio_Wrover_Sink");
    Serial.println("A2DP sink started, waiting for phone...");
}

void loop() {
    static uint8_t buf[512];
    size_t item_size = 0;
    const uint8_t* item =
        (const uint8_t*)xRingbufferReceiveUpTo(ring, &item_size, 0, sizeof(buf));
    size_t n = 0;
    if (item) {
        memcpy(buf, item, item_size);
//        vRingbufferReturnItem(ring, item);
        vRingbufferReturnItem(ring, (void*)item);
        n = item_size;
        tx_bytes += n;
    }
    if (n < sizeof(buf)) {
        memset(buf + n, 0, sizeof(buf) - n);   // недобор — тишина в хвост
    }
    i2s.write(buf, sizeof(buf));

    uint32_t now = millis();
    if (now - last_ms >= 2000) {
        uint32_t dt = now - last_ms;
        Serial.printf("[BT] %u б/с, ненулевых %u б/2с | [TX] %u б/с | ovr %u\r\n",
                      (unsigned)((bt_bytes - last_bt) * 1000 / dt),
                      (unsigned)(bt_nonzero - last_nz),
                      (unsigned)((tx_bytes - last_tx) * 1000 / dt),
                      (unsigned)ring_overflow);
        last_bt = bt_bytes; last_nz = bt_nonzero; last_tx = tx_bytes; last_ms = now;
    }
}
