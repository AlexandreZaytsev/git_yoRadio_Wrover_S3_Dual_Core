#include <Arduino.h>
#include "driver/i2s.h"
#include "options_slave_wrover.h"   // I2S_BCLK=26, I2S_LRC=25, I2S_DOUT=23

// Wrover как I2S SLAVE-TX (легаси-драйвер): такты от S3 (38->26, 39->25),
// данные уходят с 23 -> S3 RX(8).

#define I2S_USED I2S_NUM_0

const uint32_t SAMPLE_RATE = 44100;
const int16_t  AMP = 2500;

// --- Табличный синус ---
const int LUT_BITS = 12;
const int LUT_SIZE = 1 << LUT_BITS;
int16_t sineLUT[LUT_SIZE];
uint32_t phaseAcc = 0;

// --- Сирена (логика без изменений) ---
const float FREQ_MIN = 440.0f, FREQ_MAX = 1320.0f;
const float SWEEP_PERIOD = 4.0f, BURST_LEN = 2.0f, HILO_PERIOD = 0.5f;
const float FREQ_HI = 960.0f, FREQ_LO = 660.0f;

static int      state = 0;
static uint64_t sampleIdx = 0;

void nextSample(float& freq, float& panL, float& panR) {
  float t = (float)sampleIdx / SAMPLE_RATE;
  if (state == 0) {
    float u = t / SWEEP_PERIOD;
    freq = FREQ_MIN + (FREQ_MAX - FREQ_MIN) * u;
    float ang = u * PI / 2.0f;
    panL = cosf(ang); panR = sinf(ang);
    if (t >= SWEEP_PERIOD) { state = 1; sampleIdx = 0; }
  } else if (state == 1) {
    freq = (fmodf(t, HILO_PERIOD) < HILO_PERIOD / 2.0f) ? FREQ_HI : FREQ_LO;
    panL = 0.0f; panR = 1.0f;
    if (t >= BURST_LEN) { state = 2; sampleIdx = 0; }
  } else if (state == 2) {
    float u = t / SWEEP_PERIOD;
    freq = FREQ_MAX - (FREQ_MAX - FREQ_MIN) * u;
    float ang = u * PI / 2.0f;
    panR = cosf(ang); panL = sinf(ang);
    if (t >= SWEEP_PERIOD) { state = 3; sampleIdx = 0; }
  } else {
    freq = (fmodf(t, HILO_PERIOD) < HILO_PERIOD / 2.0f) ? FREQ_HI : FREQ_LO;
    panL = 1.0f; panR = 0.0f;
    if (t >= BURST_LEN) { state = 0; sampleIdx = 0; }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("[SLV-SIREN] slave TX (legacy): BCLK=%d<-S3(38) LRC=%d<-S3(39) DOUT=%d->S3(8)\n",
                I2S_BCLK, I2S_LRC, I2S_DOUT);

  for (int i = 0; i < LUT_SIZE; i++)
    sineLUT[i] = (int16_t)(sinf(2.0f * PI * i / LUT_SIZE) * 32767.0f);

  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_TX | I2S_MODE_SLAVE);
  cfg.sample_rate          = SAMPLE_RATE;   // номинал; реальный ритм задаёт S3
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 16;            // 16 буферов по 128 сэмплов
  cfg.dma_buf_len          = 128;           // = 8 КБ кольцо = ~46 мс запаса
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = true;          // при осечке слать тишину, а не мусор
  cfg.fixed_mclk           = 0;

  if (i2s_driver_install(I2S_USED, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[SLV-SIREN] driver_install FAILED");
    while (true) delay(1000);
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num   = I2S_PIN_NO_CHANGE;
  pins.bck_io_num   = I2S_BCLK;
  pins.ws_io_num    = I2S_LRC;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(I2S_USED, &pins) != ESP_OK) {
    Serial.println("[SLV-SIREN] set_pin FAILED");
    while (true) delay(1000);
  }
  // В слейве i2s_set_clk НЕ вызываем — ритм приходит от S3.
  Serial.println("[SLV-SIREN] OK, жду такты от S3 (кольцо ~46 мс)");
}

void loop() {
  static int16_t buf[512];   // 256 стерео-кадров

  uint32_t tFill = micros();
  for (int i = 0; i < 256; i++) {
    float freq, panL, panR;
    nextSample(freq, panL, panR);
    phaseAcc += (uint32_t)(freq * (4294967296.0f / SAMPLE_RATE));
    int16_t v = (int16_t)(((int32_t)sineLUT[(phaseAcc >> (32 - LUT_BITS)) & (LUT_SIZE - 1)] * AMP) >> 15);
    buf[2 * i]     = (int16_t)(v * panL);
    buf[2 * i + 1] = (int16_t)(v * panR);
    sampleIdx++;
  }
  uint32_t fillMs = (micros() - tFill) / 1000;

  size_t written = 0;
  uint32_t t0 = millis();
  i2s_write(I2S_USED, buf, sizeof(buf), &written, portMAX_DELAY);
  uint32_t wrMs = millis() - t0;

  static uint32_t blocks = 0, worstWr = 0, worstFill = 0;
  static uint32_t lastStat = 0;
  if (wrMs > worstWr) worstWr = wrMs;
  if (fillMs > worstFill) worstFill = fillMs;
  blocks++;

  if (millis() - lastStat > 2000) {
    lastStat = millis();
    Serial.printf("[SLV-SIREN] блоков=%u, худший fill=%u мс, худший write=%u мс (блок=5.8 мс)\n",
                  (unsigned)blocks, (unsigned)worstFill, (unsigned)worstWr);
    blocks = 0; worstWr = 0; worstFill = 0;
  }
}
