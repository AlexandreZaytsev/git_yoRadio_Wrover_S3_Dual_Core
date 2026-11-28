/* ============================================================
   btsource.cpp — BT-источник основной прошивки (этап 8, шаг 2)

   Перенос теста 04 (ring + adaptive, drop=0):
     - bt_writer (core 0): i2s_read(DIN <- Wrover) -> кольцевой буфер;
     - bt_reader (core 1): кольцо -> Player::playSample()
       (там же громкость/баланс/IIR/VU/мьют — как у радио и SD);
     - адаптация: fill > HIGH -> поглотить кадр (drop),
                  fill < LOW  -> повторить последний кадр (insert).
       Съедает остаточный дрейф ~80 ppm между кварцами Wrover и S3.

   Формат: 16 бит, стерео, 44100 Гц (как в тесте 04).
   ============================================================ */
#include "options.h"
#include "config.h"
#include "player.h"
#include "btsource.h"
#include "../pluginsManager/pluginsManager.h"

namespace btsource {

/* ---------- параметры (из теста 04) ---------- */
static const uint32_t RING_FRAMES = 8192;              // 32 КБ, ~186 мс @ 44.1k
static const uint32_t RING_BYTES  = RING_FRAMES * 4;   // стерео 16 бит
static const uint32_t PREFILL     = 4096 * 4;          // ~93 мс перед стартом вывода
static const uint32_t LOW_BYTES   = 2048 * 4;          // ниже — вставки
static const uint32_t HIGH_BYTES  = 6144 * 4;          // выше — поглощения
static const uint32_t BLK_BYTES   = 512;               // блок читателя: 128 кадров

/* ---------- один писатель / один читатель, индексы растут вечно ---------- */
struct Ring {
    uint8_t buf[RING_BYTES];
    volatile uint32_t rd, wr;
    inline uint32_t avail() const { return wr - rd; }
    inline uint32_t freeb() const { return RING_BYTES - (wr - rd); }
    size_t put(const uint8_t* src, size_t len) {
        if (len > freeb()) len = freeb();
        if (!len) return 0;
        uint32_t w = wr & (RING_BYTES - 1);
        size_t n1 = (RING_BYTES - w < len) ? (RING_BYTES - w) : len;
        memcpy(buf + w, src, n1);
        memcpy(buf, src + n1, len - n1);
        wr += len;
        return len;
    }
    size_t get(uint8_t* dst, size_t len) {
        if (len > avail()) len = avail();
        if (!len) return 0;
        uint32_t r = rd & (RING_BYTES - 1);
        size_t n1 = (RING_BYTES - r < len) ? (RING_BYTES - r) : len;
        memcpy(dst, buf + r, n1);
        memcpy(dst + n1, buf, len - n1);
        rd += len;
        return len;
    }
};

static Ring* s_ring = nullptr;              // выделяется при первом start()
static volatile bool s_running = false;
static volatile bool s_tasksOn = false;
static volatile uint32_t s_ins = 0, s_drops = 0, s_lost = 0, s_pauses = 0;
static uint8_t s_port = 0;
static TaskHandle_t s_wr = nullptr, s_rd = nullptr;

/* ---------- дренаж накопленной DMA-тишины ----------
   Пока BT-источник выключен, S3-мастер продолжает тактировать шину,
   Wrover-слейв шлёт нули, RX-DMA их копит (~32 КБ). Если не вычистить,
   writer первым рывком зальёт это в кольцо — отсюда lost=N при старте.
   Читаем и выбрасываем, пока DMA не опустеет: признак — чтение
   вернуло меньше полного блока. Задачи в это время спят (s_running=false). */
static void drainRx() {
    static uint8_t trash[1024];
    uint32_t deadline = millis() + 200;              // страховка от зависания
    while (millis() < deadline) {
        size_t n = 0;
        esp_err_t e = i2s_read((i2s_port_t)s_port, trash, sizeof(trash), &n, pdMS_TO_TICKS(10));
        if (e != ESP_OK) return;
        if (n < sizeof(trash)) break;                // вычерпали — дальше реальный поток
    }
}


/* ---------- задача-писатель: I2S RX -> кольцо ---------- */
static void writerTask(void*) {
    static uint8_t rb[1024];
    for (;;) {
        if (!s_running) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        size_t n = 0;
        esp_err_t e = i2s_read((i2s_port_t)s_port, rb, sizeof(rb), &n, pdMS_TO_TICKS(100));
        if (e == ESP_OK && n > 0) {
            size_t put = s_ring->put(rb, n);
            if (put < n) s_lost += (uint32_t)(n - put);   // кольцо переполнено
        }
        // n == 0: Wrover молчит/выключен — ждём дальше
    }
}

/* ---------- задача-читатель: кольцо -> playSample + адаптация ---------- */
static void readerTask(void*) {
    static uint8_t blk[BLK_BYTES];
    uint32_t lastStat = millis();
    for (;;) {
        if (!s_running) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        // префилл ~93 мс, потом стартуем вывод
        while (s_running && s_ring->avail() < PREFILL) vTaskDelay(pdMS_TO_TICKS(10));
        if (!s_running) continue;

        size_t n = s_ring->get(blk, sizeof(blk));
        int frames = n / 4;
        int16_t* p = (int16_t*)blk;
        for (int i = 0; i < frames; i++) {
            int16_t pair[2] = { p[2 * i], p[2 * i + 1] };
            player.btPlaySample(pair);
        }

        uint32_t fill = s_ring->avail();
        if (fill > HIGH_BYTES) {
            uint8_t skip[4];                    // переполнение: поглотить кадр
            if (s_ring->get(skip, 4) == 4) s_drops++;
        } else if (frames > 0 && fill < LOW_BYTES) {
            int16_t pair[2] = { p[2 * (frames - 1)], p[2 * (frames - 1) + 1] };
            player.btPlaySample(pair);           // опустошение: повтор кадра
            s_ins++;
        } else if (frames == 0) {
            s_pauses++;                         // кольцо пусто: тишина (tx_desc_auto_clear)
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        uint32_t now = millis();
        if (now - lastStat > 5000) {
            lastStat = now;
            Serial.printf("[BTSRC] fill=%u ins=%u drop=%u lost=%u pauses=%u\n",
                          (unsigned)(s_ring->avail() / 4), (unsigned)s_ins,
                          (unsigned)s_drops, (unsigned)s_lost, (unsigned)s_pauses);
        }
    }
}

/* ---------- публичный API ---------- */

void start() {
    if (s_running) return;
    if (!s_ring) {
        s_ring = (Ring*)malloc(sizeof(Ring));
        if (!s_ring) { Serial.println("[BTSRC] нет памяти под кольцевой буфер"); return; }
    }
    if (!s_tasksOn) {
        s_port = player.getI2sPort();
        xTaskCreatePinnedToCore(writerTask, "bt_wr", 4096, NULL, 2, &s_wr, 0);
        xTaskCreatePinnedToCore(readerTask, "bt_rd", 4096, NULL, 1, &s_rd, 1);
        s_tasksOn = true;
    }
    // частота потока от Wrover — 44100 (как в тесте 04); выставляем явно
    //if (player.getSampleRate() != 44100) player.setSampleRate(44100);
    if (player.getSampleRate() != 44100) player.btSetSampleRate(44100);

// Доводка: вычистить накопленную DMA-тишину ДО снятия флага.
    // Задачи ещё спят, кольцо уже пусто — writer начнёт только с живого потока.
    drainRx();


    s_ins = s_drops = s_lost = s_pauses = 0;
    s_ring->rd = s_ring->wr = 0;   // сброс ДО снятия флага — задачи спят
    s_running = true;
    player.setOutputPins(true);
    if (player_on_start_play) player_on_start_play();
    pm.on_start_play();
    config.setTitle("Bluetooth A2DP");
    Serial.println("[BTSRC] start: приём активен, ждём префилл");
}

void stop() {
    if (!s_tasksOn) return;        // BT ни разу не запускался — делать нечего
    s_running = false;
    vTaskDelay(pdMS_TO_TICKS(150)); // > таймаута i2s_read: обе задачи гарантированно спят
    player.setOutputPins(false);
    if (player_on_stop_play) player_on_stop_play();
    pm.on_stop_play();
    Serial.printf("[BTSRC] stop: ins=%u drop=%u lost=%u pauses=%u\n",
                  (unsigned)s_ins, (unsigned)s_drops, (unsigned)s_lost, (unsigned)s_pauses);
}

void toggle() {
    if (s_running) {
        s_running = false;
        vTaskDelay(pdMS_TO_TICKS(150));
        player.setOutputPins(false);
        if (player_on_stop_play) player_on_stop_play();
        pm.on_stop_play();
        Serial.println("[BTSRC] пауза");
    } else {
        start();
    }
}

bool running() { return s_running; }
bool active()  { return s_running && s_ring && s_ring->avail() > 0; }
uint32_t inserts() { return s_ins; }
uint32_t drops()   { return s_drops; }
uint32_t lost()    { return s_lost; }

} // namespace btsource
