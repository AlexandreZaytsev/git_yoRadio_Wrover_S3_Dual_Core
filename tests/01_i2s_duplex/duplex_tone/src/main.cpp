#include <Arduino.h>
#include "driver/i2s.h"

// S3 мастер: TX=47->DAC, RX=8<-Wrover(23), такты 38/39 всем
#define BCLK 38
#define LRC  39
#define DOUT 47
#define DIN  8
#define I2S_PORT I2S_NUM_0

const uint32_t SAMPLE_RATE = 44100;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[FULLDUP] init legacy: TX=47->DAC, RX=8<-Wrover, MCLK 38/39");

  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate          = SAMPLE_RATE;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 16;      // как на слейве: ~46 мс запаса
  cfg.dma_buf_len          = 128;
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = true;
  cfg.fixed_mclk           = 0;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[FULLDUP] driver_install FAILED");
    while (true) delay(1000);
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num   = I2S_PIN_NO_CHANGE;
  pins.bck_io_num   = BCLK;
  pins.ws_io_num    = LRC;
  pins.data_out_num = DOUT;
  pins.data_in_num  = DIN;
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("[FULLDUP] set_pin FAILED");
    while (true) delay(1000);
  }
  i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  Serial.println("[FULLDUP] OK, реле RX->TX на большом кольце");
}

void loop() {
  static int16_t rxBuf[512];                 // 256 стерео-кадров
  static uint32_t frames = 0, nzL = 0, nzR = 0;

  size_t n = 0;
  i2s_read(I2S_PORT, rxBuf, sizeof(rxBuf), &n, portMAX_DELAY);
  if (n > 0) {
    size_t w = 0;
    i2s_write(I2S_PORT, rxBuf, n, &w, portMAX_DELAY);
    int fr = n / 4;
    for (int i = 0; i < fr; i++) {
      frames++;
      if (rxBuf[2 * i]     != 0) nzL++;
      if (rxBuf[2 * i + 1] != 0) nzR++;
    }
  }

  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("[FULLDUP] кадров=%u, L-ненулевых=%u, R-ненулевых=%u\n",
                  (unsigned)frames, (unsigned)nzL, (unsigned)nzR);
    frames = 0; nzL = 0; nzR = 0;
  }
}
