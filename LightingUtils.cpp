
// ---------- LightingUtils.cpp ----------
#include "LightingUtils.h"
#include <Arduino.h>

// 事件索引與開始時間從 FingeringsUtils 取得
extern size_t currentIdx;
extern RawEvent rawScore[];
extern const size_t scoreLen;
extern bool startMode2;

void initLighting() {
    strip.begin();
    strip.clear();
    strip.show();
    strip.setBrightness(50);
    // 初始化時間軸與索引
    currentIdx = 0;
}

void prepareLighting() {
  for (int i = 0; i < 3; i++) {
    // 全亮
    for (int j = 0; j < 10; j++) {
      strip.setPixelColor(j, strip.Color(10, 0, 10));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(845 / 2));

    // 全滅
    strip.clear();
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(845 / 2));
  }
}

void updateLighting(uint32_t now) {
    // 計算索引

    while (currentIdx + 1 < scoreLen && now >= rawScore[currentIdx+1].startMs) {
        currentIdx++;
    }
    uint16_t curMask  = rawScore[currentIdx].mask;
    uint16_t nextMask = (currentIdx+1 < scoreLen) ? rawScore[currentIdx+1].mask : 0;

    unsigned long start   = rawScore[currentIdx].startMs;
    unsigned long dur     = (currentIdx+1 < scoreLen)
                             ? rawScore[currentIdx+1].startMs - start
                             : 2535;  // 最後一音淡出時長
    float fraction = dur > 0
        ? float(now - start) / float(dur)
        : 1.0f;
    fraction = constrain(fraction, 0.0f, 1.0f);

    // 非線性亮度增益 (gamma 曲線)
    float gain = 1 - (1-fraction)*fraction;
    strip.clear();
    for (int b = 0; b < 9; b++) {
        int led   = bitToLed[b];
        bool isCur  = curMask  & (1 << (8 - b));
        bool isNext = nextMask & (1 << (8 - b));
        
        if (isCur) {
            // 當前要按：綠→藍 漸變
            if (isNext) {
                // 線性插值
                float g_lin  = 255.0f * (1.0f - fraction);
                float b_lin  = 255.0f * fraction;
                // 非線性亮度
                uint8_t g = uint8_t(constrain(g_lin * gain, 0.0f, 255.0f));
                uint8_t bl= uint8_t(constrain(b_lin * gain, 0.0f, 255.0f));
                strip.setPixelColor(led, strip.Color(0, g, bl));
            } else {
                // 綠→滅 漸變
                float g_lin = 255.0f * (1.0f - fraction);
                uint8_t g = uint8_t(constrain(g_lin * gain, 0.0f, 255.0f));
                strip.setPixelColor(led, strip.Color(0, g, 0));
            }
        }
        else if (isNext) {
            // 下個要按：黑→藍 漸變
            float b_lin = 255.0f * fraction;
            uint8_t bl = uint8_t(constrain(b_lin * gain, 0.0f, 255.0f));
            strip.setPixelColor(led, strip.Color(0, 0, bl));
        }
        // 否則保持熄滅
    }
    strip.show();
}

 // 使用內部定義的 bitToLed[] 來控制 LED
