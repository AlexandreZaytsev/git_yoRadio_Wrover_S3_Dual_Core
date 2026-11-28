// Тест 04: A2DP sink -> кольцевой буфер с адаптивной подстройкой
// -> легаси I2S slave TX -> S3 (мастер, такты с S3)
#include <Arduino.h>
#include "driver/i2s.h"
#include "BluetoothA2DPSink.h"
#include "options_slave_wrover.h"
// BCLK=26<-S3(38), LRC=25<-S3(39), DOUT=23->S3(47)

#define I2S_PORT I2S_NUM_0
const uint32_t SAMPLE_RATE = 44100;

// ---------- кольцевой буфер (стерео-кадры 16 бит) ----------
const uint32_t RING_FRAMES = 8192;   // 32 КБ (~186 мс), степень двойки
const uint32_t PREFILL    = 2048;    // набираем ~46 мс до старта чтения
const uint32_t LOW_INSERT = 512;     // ниже — вставляем кадр (замедляем чтение)
const uint32_t HIGH_DROP  = 4096;    // выше — пропускаем кадр (ускоряем чтение)
const uint32_t CHUNK      = 256;     // кадров за один i2s_write

static int16_t ring[RING_FRAMES * 2];
static volatile uint32_t wrPos = 0, rdPos = 0;      // свободно бегущие счётчики кадров
static volatile uint32_t stDrops = 0, stIns = 0, stSkip = 0;

BluetoothA2DPSink a2dp_sink;

static inline uint32_t ringAvail() { return wrPos - rdPos; }

// BT-колбэк: единственный производитель. Не блокируем BT-стек.
void pcm_reader(const uint8_t* data, uint32_t len) {
  uint32_t frames = len / 4;
  const int16_t* p = (const int16_t*)data;
  uint32_t wr = wrPos, rd = rdPos;                  // 32-бит чтение атомарно
  uint32_t space = RING_FRAMES - (wr - rd);
  uint32_t n = (frames < space) ? frames : space;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t idx = (wr++ & (RING_FRAMES - 1)) * 2;
    ring[idx]     = p[2 * i];
    ring[idx + 1] = p[2 * i + 1];
  }
  if (n < frames) stDrops += frames - n;            // переполнение — экстренный случай
  wrPos = wr;
}

// ---------- потребитель: буфер -> I2S ----------
static int16_t out[CHUNK * 2 + 1];
static int16_t zeros[CHUNK * 2];

void readerTask(void*) {
  bool everStarted = false;
  for (;;) {
    uint32_t avail = ringAvail();

    if (avail == 0) {                               // нет данных — кормим тишиной
      size_t w;
      i2s_write(I2S_PORT, zeros, sizeof(zeros), &w, portMAX_DELAY);
      continue;
    }
    if (!everStarted) {                             // старт только после набора
      if (avail < PREFILL) {
        size_t w;
        i2s_write(I2S_PORT, zeros, sizeof(zeros), &w, portMAX_DELAY);
        continue;
      }
      everStarted = true;
    }

    uint32_t take = (avail < CHUNK) ? avail : CHUNK;
    uint32_t rd = rdPos;
    for (uint32_t i = 0; i < take; i++) {
      uint32_t idx = (rd++ & (RING_FRAMES - 1)) * 2;
      out[2 * i]     = ring[idx];
      out[2 * i + 1] = ring[idx + 1];
    }
    rdPos = rd;

    uint32_t outFrames = take;
    // адаптивная коррекция темпа по уровню буфера
    if (avail < LOW_INSERT) {                       // подсыхает: вставка (тянем)
      out[2 * take]     = out[2 * (take - 1)];
      out[2 * take + 1] = out[2 * (take - 1) + 1];
      outFrames = take + 1; stIns++;
    } else if (avail > HIGH_DROP && take > 1) {     // переполнен: пропуск (сжимаем)
      outFrames = take - 1; stSkip++;
    }

    size_t w;
    i2s_write(I2S_PORT, out, outFrames * 4, &w, portMAX_DELAY);
  }
}

void onBtState(esp_a2d_connection_state_t st, void*) {
  if (st == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    rdPos = wrPos;          // сброс буфера, чтобы старые данные не заиграли
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("[A2DP] ring+adaptive: BCLK=%d<-S3, LRC=%d<-S3, DOUT=%d->S3\n",
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

  xTaskCreatePinnedToCore(readerTask, "rdr", 6144, NULL, 5, NULL, 1);

  a2dp_sink.set_stream_reader(pcm_reader, false);
  a2dp_sink.set_on_connection_state_changed(onBtState);
  a2dp_sink.set_auto_reconnect(true, 20);
  a2dp_sink.start("yoRadio_Wrover_Sink");
  Serial.println("[A2DP] started, ждём телефон");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("[RING] fill=%u кадров, ins=%u skip=%u drop=%u\n",
                  (unsigned)ringAvail(), (unsigned)stIns,
                  (unsigned)stSkip, (unsigned)stDrops);
  }
}
