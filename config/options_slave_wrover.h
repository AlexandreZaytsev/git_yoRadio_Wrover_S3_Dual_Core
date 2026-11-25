#ifndef OPTIONS_SLAVE_WROVER_H
#define OPTIONS_SLAVE_WROVER_H

/* ----------FULL DUPLEX I2S DAC ---------- */
#define I2S_DOUT            23
#define I2S_BCLK            26
#define I2S_LRC             25
#define I2S_DIN             22    // вход потока ОТ S3 (маршрут 3: BT-выход радио)

#define RX_SLAVE_TO_MASTER_TX 33  // ← TX1 мастера (S3 GPIO17)
#define TX_SLAVE_TO_MASTER_RX 32  // → RX1 мастера (S3 GPIO18)

#endif