// ---------- SDUtils.h ----------
#ifndef SDUTILS_H
#define SDUTILS_H

#pragma once
#include <SPI.h>
#include <SD.h>
#include <esp_sleep.h>

/**
 * @brief 初始化硬體 SPI 並掛載 SD 卡
 * @return true: 掛載成功；false: 掛載失敗
 */
bool SDcardSetup();

/**
 * @brief 開啟指定檔案並顯示延遲時間
 * @param path 要測試的檔案路徑（如 "/song.wav"）
 * @return 已打開的 File 物件；失敗則為空物件
 */
File measureOpenDelay(const char* path);

/**
 * @brief 執行安全下電：關閉指定檔案、卸載 SD 卡、進入深度睡眠
 * @param openFile 要關閉的 File 物件引用
 */
void safePowerOff(File& openFile);

#endif // SDUTILS_H
