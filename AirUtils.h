// updated all functions at 0619 by LPY
#pragma once
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

/**
 * @brief 初始化吹氣感測器（ADS1115）
 *        包括 I2C 與 baseline 設定
 */
void initAirSensor();

/**
 * @brief 回傳是否偵測到吹氣
 * @return true: 偵測到吹氣  false: 未偵測到
 */
bool isBlowing();

/**
 * @brief 計算並回傳平滑後的音量增益 (5~100%)
 * @return 當前增益百分比
 */
float computeGain();
