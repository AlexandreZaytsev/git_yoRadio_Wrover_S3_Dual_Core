#include <Arduino.h>
#include "ESP_I2S.h"
#include "options_slave_wrover.h"   // пины 1:1 из основного проекта

/* 
Wrover как мастер 3 провода: 
 DAC pin BCLK <> Wrover pin 26
 DAC pin LCK/LCR <> Wrover pin 25
 DAC pin DIN <> Wrover pin 23
*/

I2SClass i2s;

const uint32_t SAMPLE_RATE = 44100;
const int16_t  AMP = 2500;          // тише: было 8000

// --- Полёт сирены (пока панорамируется) ---
const float FREQ_MIN = 440.0f;
const float FREQ_MAX = 1320.0f;
const float SWEEP_PERIOD = 4.0f;    // сек на пролёт L->R или R->L

// --- Крякалка в крайних точках ---
const float BURST_LEN     = 2.0f;   // длительность кряканья, сек
const float HILO_PERIOD   = 0.5f;   // период чередования "и-а", сек
const float FREQ_HI       = 960.0f;
const float FREQ_LO       = 660.0f;

// Состояние: 0 = полёт влево->вправо, 1 = крякалка справа,
// 2 = полёт вправо->влево, 3 = крякалка слева
static int      state = 0;
static uint64_t sampleIdx = 0;      // сэмплы в текущем состоянии
static uint64_t totalIdx  = 0;

void nextSample(float& freq, float& panL, float& panR, bool& newEvent) {
  float t = (float)sampleIdx / SAMPLE_RATE;
  newEvent = false;

  if (state == 0) {                    // пролёт: 440->1320, пан L->R
    float u = t / SWEEP_PERIOD;                       // 0..1
    freq = FREQ_MIN + (FREQ_MAX - FREQ_MIN) * u;
    float ang = u * PI / 2.0f;                        // 0=левый, PI/2=правый
    panL = cosf(ang); panR = sinf(ang);
    if (t >= SWEEP_PERIOD) { state = 1; sampleIdx = 0; newEvent = true; }
  }
  else if (state == 1) {               // крякалка справа
    freq = (fmodf(t, HILO_PERIOD) < HILO_PERIOD / 2.0f) ? FREQ_HI : FREQ_LO;
    panL = 0.0f; panR = 1.0f;
    if (t >= BURST_LEN) { state = 2; sampleIdx = 0; newEvent = true; }
  }
  else if (state == 2) {               // пролёт: 1320->440, пан R->L
    float u = t / SWEEP_PERIOD;
    freq = FREQ_MAX - (FREQ_MAX - FREQ_MIN) * u;
    float ang = u * PI / 2.0f;
    panR = cosf(ang); panL = sinf(ang);
    if (t >= SWEEP_PERIOD) { state = 3; sampleIdx = 0; newEvent = true; }
  }
  else {                               // крякалка слева
    freq = (fmodf(t, HILO_PERIOD) < HILO_PERIOD / 2.0f) ? FREQ_HI : FREQ_LO;
    panL = 1.0f; panR = 0.0f;
    if (t >= BURST_LEN) { state = 0; sampleIdx = 0; newEvent = true; }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("[WRV-SIREN] пины из options_slave_wrover.h: BCLK=%d LRC=%d DOUT=%d\n",
                I2S_BCLK, I2S_LRC, I2S_DOUT);
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT, -1, -1);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[WRV-SIREN] begin FAILED");
    while (true) delay(1000);
  }
  Serial.println("[WRV-SIREN] OK: пролёты с панорамой + крякалки на краях");
}

void loop() {
  static int16_t buf[512];             // 256 стерео-кадров
  static float phase = 0.0f;
  static uint32_t lastPrint = 0;

  for (int i = 0; i < 256; i++) {
    float freq, panL, panR; bool newEvent;
    nextSample(freq, panL, panR, newEvent);
    if (newEvent) {
      const char* names[] = {"достигнут RIGHT -> крякалка",
                             "достигнут LEFT -> крякалка",
                             "пролёт L->R", "пролёт R->L"};
      Serial.printf("[WRV-SIREN] %s\n", names[state]);
    }
    phase += 2.0f * PI * freq / SAMPLE_RATE;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
    int16_t v = (int16_t)(sinf(phase) * AMP);
    buf[2 * i]     = (int16_t)(v * panL);   // левый
    buf[2 * i + 1] = (int16_t)(v * panR);   // правый
    sampleIdx++; totalIdx++;
  }
  i2s.write((uint8_t*)buf, sizeof(buf));

  if (millis() - lastPrint > 2000) {
    lastPrint = millis();
    Serial.printf("[WRV-SIREN] состояние=%d, t=%.1f сек\n", state,
                  (float)sampleIdx / SAMPLE_RATE);
  }
}
