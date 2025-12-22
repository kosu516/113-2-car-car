// ----------- SDUtils.cpp ----------
#include "SDUtils.h"

// SPI 與 SD 卡腳位設定（static，隱藏於此 cpp）
static const int SCK_PIN  = 18;
static const int MISO_PIN = 19;
static const int MOSI_PIN = 23;
static const int CS_PIN   = 5;

bool SDcardSetup() {
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);  // <<< 新增自定義 SPI 初始化
  Serial.print("⏳ 初始化 SD 卡...");
  if (!SD.begin(CS_PIN, SPI)) {                    // <<< 使用 SPI 物件
    Serial.println("❌ SD 卡掛載失敗");
    return false;
  }
  uint8_t type = SD.cardType();
  Serial.print("✅ SD 卡掛載成功，類型：");
  switch (type) {
    case CARD_MMC:  Serial.println("MMC");  break;
    case CARD_SD:   Serial.println("SDSC"); break;
    case CARD_SDHC: Serial.println("SDHC"); break;
    default:        Serial.println("UNKNOWN"); break;
  }
  return true;
}

File measureOpenDelay(const char* path) {
  Serial.print("⏳ 測試開啟 ");
  Serial.print(path);
  Serial.print(" 延遲...");

  unsigned long t0 = micros();
  File f = SD.open(path, FILE_READ);                // <<< 打開檔案並計時
  unsigned long t1 = micros();

  if (f) {
    Serial.print(" ✅ 延遲: ");
    Serial.print(t1 - t0);
    Serial.println(" μs");
  } else {
    Serial.print(" ❌ 開啟 ");
    Serial.print(path);
    Serial.println(" 失敗");
  }
  return f;                                         // <<< 回傳保留給主程式使用
}

void safePowerOff(File& openFile) {
  Serial.println("🛑 開始安全下電流程...");
  if (openFile) {
    openFile.close();                              // <<< 關閉傳入的 File 物件
    Serial.println("  • 關閉指定的檔案");
  }
  SD.end();                                        // <<< 卸載 SD 卡
  Serial.println("  • 卸載 SD 卡");
  Serial.println("💤 進入深度睡眠，不再執行程式");
  delay(100);
  esp_deep_sleep_start();                          // <<< 進入深度睡眠
}
