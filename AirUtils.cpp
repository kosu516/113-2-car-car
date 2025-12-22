#include "AirUtils.h"

// 單例 ADS 物件
static Adafruit_ADS1115 ads;

static unsigned long lastAdc;

// 非對稱 EMA 基線更新參數
static const float alpha_down     = 0.5f;
static const float alpha_up       = 0.01f;

// 非對稱 EMA 增益平滑參數
//   gainAlphaRise: targetGain > current -> 快速上升
//   gainAlphaFall: targetGain <= current -> 緩慢下降
static const float gainAlphaRise  = 0.8f;
static const float gainAlphaFall  = 0.4f;

// 增益百分比範圍
static const float GAIN_MIN       = 5.0f;
static const float GAIN_MAX       = 100.0f;

// 吹氣判斷閾值 (voltage - baseline)
static const float THRESHOLD      = 0.012f;

// 內部狀態
static float baseline    = 0.0f;
static float volumeGain  = GAIN_MIN;
static float lastDelta   = 0.0f;

void initAirSensor() {
  Wire.begin();
  Wire.setClock(100000);
  ads.begin(0x48);
  int16_t raw = ads.readADC_SingleEnded(0);
  baseline   = raw * 0.000125f + 0.4;
  volumeGain = GAIN_MIN;
}

bool isBlowing() {
  
  return lastDelta > THRESHOLD;
}

float computeGain() {
  if (millis() - lastAdc < 50) return volumeGain;
  lastAdc = millis();

  // 1) 讀取原始電壓並轉換
  int16_t raw = ads.readADC_SingleEnded(0);
  float rawVoltage = raw * 0.000125f;

  // 🎯 對 voltage 加入 EMA 平滑
  static float smoothedVoltage = 0.0f;
  const float voltageAlpha = 0.8f;  // 濾波程度（越小越穩）
  smoothedVoltage = voltageAlpha * rawVoltage + (1 - voltageAlpha) * smoothedVoltage;
  float voltage = smoothedVoltage;

  // 2) 非對稱 EMA 更新基線
  if (voltage < baseline) {
    baseline = alpha_down * voltage + (1 - alpha_down) * baseline;
  } else {
    baseline = alpha_up * voltage + (1 - alpha_up) * baseline;
  }

  // 3) 計算 delta 並存儲
  float delta = voltage - baseline;
  if (delta < 0.0f) delta = 0.0f;
  lastDelta = delta;

  // 4) 映射到目標增益 (5~100%)
  float targetGain = 16 * delta * (GAIN_MAX - GAIN_MIN);

  // 5) 非對稱 EMA 平滑增益
  if (targetGain > volumeGain) {
    volumeGain = gainAlphaRise * targetGain + (1 - gainAlphaRise) * volumeGain;
  } else {
    volumeGain = gainAlphaFall * targetGain + (1 - gainAlphaFall) * volumeGain;
  }

  // 6) 最後 clamp 保證範圍
  if (volumeGain < GAIN_MIN) volumeGain = GAIN_MIN;
  if (volumeGain > GAIN_MAX) volumeGain = GAIN_MAX;

  Serial.print(baseline); Serial.print('\t');
  Serial.print(10 * delta); Serial.print('\t');
  Serial.print(voltage); Serial.print('\t');
  Serial.println(lastDelta > THRESHOLD ? 1 : 0);

  return volumeGain;
}
