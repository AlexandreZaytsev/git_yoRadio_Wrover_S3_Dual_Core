#ifndef OPTIONS_SLAVE_WROVER_H
#define OPTIONS_SLAVE_WROVER_H

/* ---------- I2S №1: дуплекс, S3 — мастер / WROVER -слейв / DAC - слейв ----------
   Такты:   BCLK S3(38) → Wrover(26), DAC;  LRCLK S3(39) → Wrover(25), DAC
   Данные:  DOUT S3(47) → DAC PCM5102  ·  DIN S3(8) ← Wrover(23), BT-поток (резерв GPIO17)
*/
#define I2S_BCLK            26    // ← BCLK - такт от S3 (38)
#define I2S_LRC             25    // ← LRCLK - выбор канала левый/правый от S3 (39)
#define I2S_DOUT            23    // → DIN S3 (8)
#define I2S_DIN             22    // ← BT-выход радио от S3 (маршрут 3, пин пока не выбран)



/* ---------- UART1: командный канал Wrover ↔ S3 ----------
   Кросс-соединение (TX одного → RX другого):
     Wrover TX(32) → S3 RX(18)
     Wrover RX(33) ← S3 TX(17)
   Скорость и протокол — те же, что на стороне S3. */
#define UART_FROM_MASTER_RX 33   // RX1: команды S3 → Wrover (его TX, GPIO17)
#define UART_TO_MASTER_TX   32   // TX1: ответы Wrover → S3 (его RX, GPIO18)

#endif