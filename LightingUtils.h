#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "FingeringsUtils.h"

// NeoPixel 定義（硬體固定）
#define LED_PIN    12
#define NUM_LEDS   10

// bit-to-LED 映射（硬體固定）
static const int bitToLed[9] = {4,4,3,2,1,6,7,8,9};

// 內部 NeoPixel 物件
static Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

/**
 * @brief 初始化 LED 燈效
 *
 * - 呼叫 strip.begin(), strip.show()
 * - 將時間軸歸零 (startMillis)
 * - 重設事件索引
 *
 */
void initLighting();

/**
 * @brief 根據當前時間更新燈效
 *
 * - 計算目前事件索引
 * - 計算當前與下一事件的漸變比
 * - 設定對應 LED 顏色
 * - 呼叫 strip.show()
 *
 * @param now   當前時間 (ms)，通常為 millis() - startMillis
 */
void updateLighting(uint32_t now);
