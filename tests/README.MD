тесты

01_i2s_duplex
 - duplex_tone - s3 мастер i2s тактирует wrover
 - wrover_siren_slave - wrover гонит i2s сирену на S3 а он на dac 

02_wrover_master_dac - wrover гонит i2s сирену напрямую на цап - сирена стерео из уха в ухо на концах крякалка 

03_wrover_a2dp_sink - в паре с duplex_tone - принимает блютуз и гонит его в s3 по i2s тактируемый s3 


tests/
├── 01_i2s_duplex/
│   ├── duplex_tone/            ← S3-мастер: реле RX→TX + статистика (боевой сейчас)
│   └── wrover_siren_slave/     ← старый: Wrover-слейв + сирена (этап отладки, оставить)
├── 02_wrover_master_dac/       ← план Б (Wrover-мастер), эксперимент, не победивший
├── 03_wrover_a2dp_sink/        ← Этап 6: A2DP напрямую в слейв-TX (проверено, в репо)
└── 04_a2dp_ring_buffer/        ← НОВЫЙ: A2DP + кольцевой буфер с подстройкой
    ├── src/main.cpp
    └── platformio.ini

| Тест | Этап | Чип/роль | Что проверяет | Статус |
|------|------|----------|---------------|--------|
| 01_i2s_duplex/s3_master_duplex_tone | 1, 3, 5 | S3 master | full-duplex, реле+статистика | ✔ боевой |
| 01_i2s_duplex/wrover_slave_siren | 4 | Wrover slave | слейв-TX, сирена | исторический |
| 02_wrover_master_dac | план Б | Wrover master | Wrover-мастер → ЦАП | заархивирован |
| 03_wrover_slave_a2dp_sink | 6 | Wrover slave-TX | A2DP → слейв-TX напрямую | ✔ подтверждён |
| 04_wrover_slave_a2dp_ring | 7 | Wrover slave-TX | A2DP + кольцевой буфер с подстройкой | в работе |

