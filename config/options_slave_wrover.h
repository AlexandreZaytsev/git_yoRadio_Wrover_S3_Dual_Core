#ifndef OPTIONS_SLAVE_WROVER_H
#define OPTIONS_SLAVE_WROVER_H

/* ---------- I2S №1: дуплекс, S3 — мастер / WROVER -слейв ----------
   Блок I2S_NUM_0, режим SLAVE | TX:
   Такты:   BCLK ← S3(38) · LRCLK ← S3(39)
   Данные:  DOUT 23 → DIN S3(47), BT-поток в е-радио
*/
#define I2S_BCLK            26    // ← BCLK - такт от S3 (38)
#define I2S_LRC             25    // ← LRCLK - выбор канала левый/правый от S3 (39)
#define I2S_DOUT            23    // → DIN S3 (47)

/* ---------- I2S №2: приём радио-потока от S3 (маршрут «радио → BT») ----------
   Блок I2S_NUM_1, режим SLAVE | RX. Такты те же BCLK(26)/LRC(25) от S3 —
   оба блока Wrover — слейвы одного мастера.
   Джампер: DOUT S3(45) → Wrover(22), врезан параллельно DIN ЦАПа —
   S3 ничего не переключает, маршрут выбирается на стороне Wrover по UART. */
#define I2S2_DIN            22    // ← DOUT S3 (45), радио-поток для ретрансляции по BT

/* ---------- UART1: командный канал Wrover ↔ S3 ----------
   Кросс-соединение (TX одного → RX другого):
     Wrover TX(32) → S3 RX(18)
     Wrover RX(33) ← S3 TX(17)
   Скорость и протокол — те же, что на стороне S3. */
#define UART_FROM_MASTER_RX 33   // RX1: команды S3 → Wrover (его TX, GPIO17)
#define UART_TO_MASTER_TX   32   // TX1: ответы Wrover → S3 (его RX, GPIO18)

#endif
