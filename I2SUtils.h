// ---------- I2SUtils.h ----------
#pragma once
#include <Arduino.h>
#include "driver/i2s.h"
#include <SDUtils.h>
extern File songFile;

/**
 * @brief 讀取並解析 WAV 標頭
 *
 * 將檔案指標重設至開頭，讀取前 44 bytes 作為 WAV header，
 * 並在 Serial Monitor 中列印出 Format、SampleRate、Channels 及 BitsPerSample。
 * 若判定非 32-bit 有號整數 PCM 或取樣率非 48kHz，則回傳 false。
 *
 * @param file 已開啟的 WAV 檔案物件
 * @return true  Header 符合 32-bit PCM @48kHz
 * @return false Header 無效或格式不符
 */
bool readWavHeader(File& file);

/**
 * @brief 初始化 ESP32 I2S 硬體
 *
 * 設定 I2S 為主模式 TX，取樣率 48000Hz，位元深度 32-bit，
 * 單聲道 (Only Left)，並使用預定義的 BCLK、LRCK 與 DATA 腳位。
 *
 * @return true  驅動程式安裝並設定成功
 * @return false 驅動程式安裝失敗
 */
bool initI2S();

/**
 * @brief 播放 song.wav 音訊
 *
 * 從全域 songFile 讀取 PCM，並依 mute 與 volume 參數
 * 選擇播放正常或靜音，並做音量縮放後送入 I2S。
 *
 * @param mute   若 true，則播放靜音段
 * @param volume 5~100 百分比音量
 */
void playAudio();

/**
 * @brief 播放一小段song.wav
 * @param mute 若 true，則靜音
 * @param volume 5~100 百分比音量

*/
bool playAudioChunk();