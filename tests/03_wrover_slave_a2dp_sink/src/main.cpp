// Тест 03: A2DP sink (Wrover) -> легаси I2S slave TX -> S3
#include <Arduino.h>
#include "driver/i2s.h"
#include "BluetoothA2DPSink.h"
#include "options_slave_wrover.h"
// BCLK=26<-S3(38), LRC=25<-S3(39), DOUT=23->S3(47)

#define I2S_PORT I2S_NUM_0
const uint32_t SAMPLE_RATE = 44100;

BluetoothA2DPSink a2dp_sink;

// PCM от BT-стека пишем в легаси-I2S сами: библиотеке запрещаем
// поднимать свой I2S (второй аргумент false)
void pcm_reader(const uint8_t* data, uint32_t len) {
  size_t w = 0;
  i2s_write(I2S_PORT, data, len, &w, portMAX_DELAY);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("[A2DP] init legacy slave: BCLK=%d<-S3, LRC=%d<-S3, DOUT=%d->S3\n",
                I2S_BCLK, I2S_LRC, I2S_DOUT);

  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_TX);
  cfg.sample_rate          = SAMPLE_RATE;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 16;
  cfg.dma_buf_len          = 128;
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = true;
  cfg.fixed_mclk           = 0;
  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[A2DP] driver_install FAILED"); while (true) delay(1000);
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num   = I2S_PIN_NO_CHANGE;
  pins.bck_io_num   = I2S_BCLK;
  pins.ws_io_num    = I2S_LRC;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("[A2DP] set_pin FAILED"); while (true) delay(1000);
  }
  // НЕТ i2s_set_clk — такты приносит S3, мы их не назначаем

  a2dp_sink.set_stream_reader(pcm_reader, false);
  a2dp_sink.set_auto_reconnect(true, 20);
  a2dp_sink.start("yoRadio_Wrover_Sink");
  Serial.println("[A2DP] started, ждём телефон");
}

void loop() {}
