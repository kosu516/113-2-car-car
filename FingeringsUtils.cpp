
// ---------- FingeringsUtils.cpp ----------
#include "FingeringsUtils.h"
#include <Arduino.h>

// 按鈕腳位定義
const int buttonPins[9] = {14, 17, 27, 33, 2, 16, 15, 4, 32};

// 譜表：音名與開始時間，已扣除偏移
RawEvent rawScore[] = {
    {"a4",0}, {"b4",422}, {"c5",845}, {"e5",1690}, {"d5",2535}, {"c5",3380},
    {"d5",3803}, {"g4",4225}, {"d5",5070}, {"c5",5915}, {"a4",6760}, {"b4",7183},
    {"c5",7605}, {"e5",8450}, {"d5",9295}, {"c5",10141}, {"e5",10986}, {"a4",13521},
    {"b4",13943}, {"c5",14366}, {"e5",15211}, {"d5",16056}, {"g5",16901}, {"d5",17324},
    {"f5",17746}, {"e5",18169}, {"d5",18380}, {"e5",18803}, {"d5",19014},
    {"c5",19648}, {"a4",20281}, {"b4",20704}, {"c5",21126}, {"e5",21972}, {"d5",22817},
    {"c5",23662}, {"c5",24507}
};
const size_t scoreLen = sizeof(rawScore)/sizeof(rawScore[0]);

// 指法映射表
Fingering fingerings[] = {
  {0b00011111111, "c4"}, {0b00011111111, "#c4"}, {0b00011111110, "d4"}, {0b00011111110, "#d4"}, {0b00011111100, "e4"},
  {0b00011111011, "f4"}, {0b00011110110, "#f4"}, {0b00011110000, "g4"}, {0b00011100111, "#g4"}, {0b00011100000, "a4"},
  {0b00011011000, "#a4"},{0b00011000000, "b4"},  {0b00010100000, "c5"}, {0b00001100000, "#c5"}, {0b00000100000, "d5"},
  {0b00000111110, "#d5"},{0b00101111100, "e5"},  {0b00101111010, "f5"}, {0b00101110100, "#f5"}, {0b00101110000, "g5"},
  {0b00101101000, "#g5"},{0b00101100000, "a5"},  {0b00101101110, "#a5"},{0b00101101100, "b5"},  {0b00101001100, "c6"}, 
  {0b00101011101, "#c6"},{0b00101011010, "d6"},
};
const size_t fingeringCount = sizeof(fingerings)/sizeof(fingerings[0]);

// 當前事件索引
extern volatile size_t currentIdx;

/**
 * @brief 初始化所有指法按鈕腳位
 */
void initButtons() {
  for (int i = 0; i < 9; i++) {
    pinMode(buttonPins[i], INPUT_PULLDOWN);
  }
}

/**
 * @brief 探測當前按鍵狀態，並回傳 9-bit bitmask
 */
uint16_t detectButtonMask(bool doPrint) {
  uint16_t mask = 0;
  for (int i = 0; i < 9; i++) {
    if (digitalRead(buttonPins[i]) == HIGH) {
      mask |= (1 << (8 - i));
    }
  }
  if (doPrint) {
    Serial.print("[FINGERINGS] 0b");
    for (int b = 8; b >= 0; b--) Serial.print((mask >> b) & 1);
    Serial.println();
  }
  delay(1);
  return mask;
}

/**
 * @brief 根據時間計算預期的指法 bitmask
 */
uint16_t getExpectedMask(uint32_t now) {
  if (now < rawScore[0].startMs) return 0;
  while (currentIdx + 1 < scoreLen && now >= rawScore[currentIdx + 1].startMs) {
    currentIdx++;
  }
  const char* note = rawScore[currentIdx].note;
  for (size_t i = 0; i < fingeringCount; i++) {
    if (strcmp(fingerings[i].note, note) == 0) {
      return fingerings[i].mask;
    }
  }
  return 0;
}

uint16_t getMaskFromNote(const char* note) {
    for (size_t i = 0; i < fingeringCount; i++) {
        if (strcmp(fingerings[i].note, note) == 0) {
            return fingerings[i].mask;
        }
    }
    return 0;
}

/**
 * @brief 檢查當前時間的指法是否正確
 */
bool checkFingering(uint32_t now, bool doPrint) {
    // 1. 取得當前事件的 expected mask，並更新 currentIdx（若 now >= 下一事件 start 時）
    uint16_t expected = getExpectedMask(now);
    // 取得實際按鍵
    uint16_t actual = detectButtonMask(doPrint);

    // 2. 取得當前事件 start 時間（rawScore[currentIdx].startMs）
    uint32_t currStart = rawScore[currentIdx].startMs;

    // 3. 如有下一事件，取得其 start 時間
    bool hasNext = (currentIdx + 1 < scoreLen);
    uint32_t nextStart = hasNext ? rawScore[currentIdx + 1].startMs : 0;

    // 4. 如果 doPrint，輸出 debug 訊息
    if (doPrint) {
        Serial.printf("[DEBUG] now=%lu, currentIdx=%u, currStart=%u", now, (unsigned)currentIdx, (unsigned)currStart);
        if (hasNext) {
            Serial.printf(", nextIdx=%u, nextStart=%u", (unsigned)(currentIdx+1), (unsigned)nextStart);
        }
        Serial.println();
        Serial.print("[EXPECTED] 0b");
        for (int b = 8; b >= 0; b--) Serial.print((expected >> b) & 1);
        Serial.print(" (" + String(rawScore[currentIdx].note) + ")");
        Serial.println();
    }

    // 5. 定義一個函式或 lambda 計算兩個 uint32_t 時間差的絕對值，並判 <= 50ms
    auto isWithin50 = [&](uint32_t t) -> bool {
        // 比較 now 與 t
        if (now >= t) {
            return (now - t) <= 50;
        } else {
            return (t - now) <= 50;
        }
    };

    // 6. 如果 expected==0，但下一事件臨近（within50(nextStart)），則嘗試下一事件按法
    if (expected == 0) {
        if (hasNext && isWithin50(nextStart)) {
            uint16_t nextMask = rawScore[currentIdx + 1].mask;
            if (actual == nextMask) {
                return true;
            }
        }
        // 若希望在 expected==0 而又不在下一事件 ±50ms 時直接 false：
        return false;
    }

    // 7. 判斷是否在當前事件開始 ±50ms
    bool withinCurr = isWithin50(currStart);
    // 8. 判斷是否在下一事件開始 ±50ms（即使 currentIdx 尚未更新）
    bool withinNext = hasNext && isWithin50(nextStart);

    // 9. 如果在當前事件 ±50ms 之內
    if (withinCurr) {
        // 容錯：接受前一、當前、或下一事件按法
        // 9.1 前一事件
        if (currentIdx > 0) {
            uint16_t prevMask = rawScore[currentIdx - 1].mask;
            if (actual == prevMask) {
                return true;
            }
        }
        // 9.2 當前事件
        if (actual == expected) {
            return true;
        }
        // 9.3 下一事件（若也在 ±50ms 內）
        if (withinNext) {
            uint16_t nextMask = rawScore[currentIdx + 1].mask;
            if (actual == nextMask) {
                return true;
            }
        }
        // 都不符合則 false
        return false;
    }

    // 10. 如果不在當前事件 ±50ms，但如果在下一事件 ±50ms，也接受下一事件按法
    if (withinNext) {
        uint16_t nextMask = rawScore[currentIdx + 1].mask;
        if (actual == nextMask) {
            return true;
        }
        // 若希望在下一事件 ±50ms 內只要任何按鍵都接受，可改為直接 return true;
    }

    // 11. 非容錯區段：嚴格比對當前事件
    return (actual == expected);
}

// 批次初始化：將每個 RawEvent.note 對應的 mask 填入
void initRawEvents() {
    for (size_t i = 0; i < 37; ++i) {
        rawScore[i].mask = getMaskFromNote(rawScore[i].note);
        if (rawScore[i].mask == 0) {
            // Optional: handle unknown note, e.g. log warning
            Serial.printf("Warning: note %s not found in fingering table\n", rawScore[i].note);
        }
    }
    // Serial.println("nope");
}
// ---------- end FingeringsUtils.cpp ----------


