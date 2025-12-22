
// ---------- LightingUtils.cpp ----------
#include "LightingUtils.h"
#include <Arduino.h>

// 事件索引與開始時間從 FingeringsUtils 取得
extern size_t currentIdx;
extern RawEvent rawScore[];
extern const size_t scoreLen;

void initLighting() {
    strip.begin();
    strip.clear();
    strip.show();
    strip.setBrightness(5);
    // 初始化時間軸與索引
    currentIdx  = 0;

}

void updateLighting(uint32_t now) {
    // 計算索引
    while (currentIdx + 1 < scoreLen && now >= rawScore[currentIdx+1].startMs) {
        currentIdx++;
    }
    uint16_t curMask  = rawScore[currentIdx].mask;
    uint16_t nextMask = (currentIdx+1 < scoreLen) ? rawScore[currentIdx+1].mask : 0;

    unsigned long noteStart = rawScore[currentIdx].startMs;
    unsigned long noteDur   = (currentIdx+1 < scoreLen)
                              ? (rawScore[currentIdx+1].startMs - noteStart)
                              : 2535;
    
    float fraction = noteDur > 0 ? float(now - noteStart) / noteDur : 1.0f;
    fraction = constrain(fraction, 0.0f, 1.0f);

    strip.clear();
    for (int b = 0; b < 9; b++) {
        int led = bitToLed[b];
        bool isCur  = curMask  & (1 << (8 - b));
        bool isNext = nextMask & (1 << (8 - b));
        const float GAMMA = 1;
        float gain = pow(1-(fraction * (1-fraction)), GAMMA);

        if (isCur) {
            // 當前要按：綠色漸變
            if (isNext) {
                // 綠→藍
                uint8_t g  = uint8_t((1.0f - fraction) * 255) * gain;
                uint8_t bl = uint8_t(fraction * 255) * gain;
                strip.setPixelColor(led, strip.Color(0, g, bl));
            } else {
                // 綠→滅
                uint8_t g = uint8_t((1.0f - fraction) * 255) * gain;
                strip.setPixelColor(led, strip.Color(0, g, 0));
            }
        }
        else if (isNext) {
            // 事前提示：從黑漸入藍
            uint8_t bl = uint8_t(fraction * 255) * gain;
            strip.setPixelColor(led, strip.Color(0, 0, bl));
        }
        // else: 不亮
    }
    strip.show();
}
 // 使用內部定義的 bitToLed[] 來控制 LED
