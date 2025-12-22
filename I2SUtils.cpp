
// ---------- I2SUtils.cpp ----------
#pragma once
#include "I2SUtils.h"
#include <Arduino.h>

// MAX98357A I2S 腳位定義
static const int I2S_BCK_PIN = 26;             // BCLK
static const int I2S_WS_PIN = 25;              // LRCK/WS
static const int I2S_DATA_PIN = 13;            // DIN
static const i2s_port_t I2S_PORT = I2S_NUM_0;  // 固定使用 I2S_NUM_0
extern bool playbackDone;

// 音訊緩衝大小
#define BUF_SAMPLES 256

bool readWavHeader(File& file) {
  file.seek(0);
  uint8_t header[44];
  if (file.read(header, sizeof(header)) != sizeof(header)) {
    Serial.println("[ERROR] Cannot read WAV header");
    return false;
  }
  uint16_t fmt = header[20] | (header[21] << 8);
  uint32_t rate = (uint32_t)header[24]
                  | ((uint32_t)header[25] << 8)
                  | ((uint32_t)header[26] << 16)
                  | ((uint32_t)header[27] << 24);
  uint16_t ch = header[22] | (header[23] << 8);
  uint16_t bps = header[34] | (header[35] << 8);
  file.seek(44);

  Serial.printf("[WAV] Format=%u, Rate=%u Hz, Channels=%u, Bits=%u\n",
                fmt, rate, ch, bps);
  return (fmt == 1 && rate == 48000 && bps == 32);
}

bool initI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 48000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_DATA_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[ERROR] I2S driver install failed");
    return false;
  }
  i2s_set_pin(I2S_PORT, &pins);
  return true;
}

// returns false when EOF reached
bool playNextChunk(bool mute, uint8_t volume) {

  volume = constrain(volume, 5, 100);
  static int32_t zeroBuf[BUF_SAMPLES] = {0};
  int32_t buf[BUF_SAMPLES];

  size_t bytesRead = songFile.read((uint8_t*)buf,
                                   BUF_SAMPLES * sizeof(int32_t));
  if (bytesRead == 0) return false;  // no more data

  int frames = bytesRead / sizeof(int32_t);
  int32_t* out = mute ? zeroBuf : buf;

  if (!mute && volume != 100) {
    for (int i = 0; i < frames; i++) {
      int64_t v = int64_t(buf[i]) * volume;
      out[i] = int32_t(v / 100);
    }
  }

  size_t bytesWritten;
  i2s_write(I2S_NUM_0,
            out,
            frames * sizeof(int32_t),
            &bytesWritten,
            portMAX_DELAY);
  return true;
}
