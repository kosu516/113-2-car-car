// ---------- FingeringsUtils.h ----------
#pragma once
#include <Arduino.h>

// 音名對應 9-bit 指法 bitmask
struct Fingering {
  uint16_t mask;
  const char* note;
};

// 原始譜表結構：音名與開始時間 (ms)
struct RawEvent {
  const char* note;
  const uint32_t startMs;
  uint16_t mask;
};

/**
 * @brief 初始化所有指法按鈕腳位
 *
 * 將按鈕腳位設為 INPUT_PULLDOWN
 */
void initButtons();

/**
 * @brief 探測當前按鍵狀態，並回傳 9-bit bitmask
 *
 * @param doPrint 若為 true，函式內會印出 bitmask
 * @return uint16_t 指法按鍵 bitmask
 */
uint16_t detectButtonMask(bool doPrint);

/**
 * @brief 根據時間計算預期的指法 bitmask
 *
 * @param now 當前時間 (ms)
 * @return uint16_t 預期指法 bitmask
 */
uint16_t getExpectedMask(uint32_t now);

/**
 * @brief 檢查當前時間的指法是否正確
 *
 * @param now     當前時間 (ms)
 * @param doPrint 是否列印預期與實際指法資訊
 * @return true   指法正確
 * @return false  指法錯誤或尚未到第一音
 */
bool checkFingering(uint32_t now, bool doPrint);

/**
 * @brief 由音名取得對應的指法 bitmask
 *
 * @param note 音名字串，例如 "c5"
 * @return uint16_t 該音的 bitmask，找不到則回傳 0
 */
uint16_t getMaskFromNote(const char* note);

/**
 * @brief 初始化音名對應的指法
*/
void initFingeringMasks();

// 以下變數暨映射表由 FingeringsUtils.cpp 定義

// 按鈕腳位
extern const int buttonPins[9];

// 譜表與指法映射
extern RawEvent rawScore[];
extern const size_t scoreLen;
extern const Fingering fingerings[];
extern const size_t fingeringCount;
// ---------- end FingeringsUtils.h ----------
