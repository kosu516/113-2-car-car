// mode 切換參數
volatile bool stop0 = true;
volatile bool stop1 = false;
volatile bool stop2 = true;
int MODE = 1;
unsigned long startMillis = 0;
bool startMode2 = false;

#include <SDUtils.h>          // SDcardSetup(), measureOpenDelay()
#include <FingeringsUtils.h>  // detectButtonMask(), fingerings[], numFingerings
#include <I2SUtils.h>         // initI2S()
#include <LightingUtils.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ======= 常數設定 =======
#define SAMPLE_RATE 48000
#define NOTE_DURATION_SEC 3
#define AUDIO_BUF_SIZE 512
#define WAV_HEADER_SIZE 44
#define I2S_PORT I2S_NUM_0
#define volumePercent 100

// Queue 大小，可存 4 段 audio buffer
#define QUEUE_LENGTH 4
volatile size_t currentIdx = 0;

// 一個緩衝單元
typedef struct {
  size_t len;  // 有效 bytes 數
  uint8_t data[AUDIO_BUF_SIZE];
} AudioBuffer;

// 全域變數
static QueueHandle_t audioQueue;
File songFile;

// 用於追蹤目前欲播放的 note
static volatile int8_t lastNoteIndex = -1;
static uint32_t samplesRemaining = 0;
#define samplesPerNote 144000
// static const uint32_t samplesPerNote = SAMPLE_RATE * NOTE_DURATION_SEC;

// MODE 1 新東
int currentIndex = 0;

// mode 2 RTOS 共享變數：指法是否正確
volatile bool fingerOK = false;
TaskHandle_t audioTaskHandle = NULL;

// ———— Mask 轉 NoteIndex ————
static int8_t maskToNoteIndex(uint16_t mask) {
  for (size_t i = 0; i < 27; ++i) {
    if (fingerings[i].mask == mask) return i;
  }
  return -1;
}

// 放在檔案頂端
#define QUEUE_PREFILL_CHUNKS 4  // 預先填滿多少 chunk


// MODE 0 //

void createTasks0() {
  stop0 = false;
  initI2S();
  xTaskCreatePinnedToCore(producerTask0, "Producer", 4096, NULL, 1, NULL, 1);
  Serial.println("Producer created");
  xTaskCreatePinnedToCore(consumerTask0, "Consumer", 4096, NULL, 2, NULL, 1);
  Serial.println("Consumer created");
}

void deleteTasks0() {
  stop0 = true;
  vTaskDelay(20 / portTICK_PERIOD_MS);  // 確保任務真的結束了
  // 清空 LED
  strip.clear();
  strip.show();
  // 重設播放狀態
  lastNoteIndex = -1;
  samplesRemaining = 0;
  i2s_zero_dma_buffer(I2S_NUM_0);
  xQueueReset(audioQueue);
  // ⚠️ 不要直接 vTaskDelete 任務（你是讓任務自己刪自己）
  // 若你有 task handle，可改加等待 flag 結束的機制再建立新任務
  Serial.println("Tasks0 marked for deletion and resources cleared.");
}

// ———— Producer Task ————

static void producerTask0(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if (!stop0) {

      // 1. 读当前指法
      uint16_t mask = detectButtonMask(true);
      int8_t noteIndex = maskToNoteIndex(mask);

      // 2. 切换音符时先预填充几个 chunk，再 continue
      if (noteIndex != lastNoteIndex) {
        lastNoteIndex = noteIndex;
        if (noteIndex >= 0) {
          uint32_t byteOffset = WAV_HEADER_SIZE
                                + uint32_t(noteIndex) * samplesPerNote * sizeof(int32_t);
          songFile.seek(byteOffset);
          samplesRemaining = samplesPerNote;
        } else {
          samplesRemaining = 0;
        }
        // 预先填满 queue
        for (int i = 0; i < QUEUE_PREFILL_CHUNKS; ++i) {
          if (lastNoteIndex >= 0 && samplesRemaining != 0) {
            buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
            if (buf.len == 0) {
              memset(buf.data, 0, AUDIO_BUF_SIZE);
              buf.len = AUDIO_BUF_SIZE;
            } else {
              int frames = buf.len / sizeof(int32_t);
              int32_t* samples = (int32_t*)buf.data;
              for (int j = 0; j < frames; ++j) {
                samples[j] = int32_t((int64_t)samples[j] * volumePercent / 100);
              }
              samplesRemaining -= frames;
              // 按下的手指亮燈
              strip.clear();
              for (int i = 0; i < 9; ++i) {
                int ledIndex = bitToLed[i];
                if (mask & (1 << (8 - i)))
                  strip.setPixelColor(ledIndex, strip.Color(10, 10, 0));
              }
              strip.show();  // 更新燈光
            }
          } else {
            buf.len = AUDIO_BUF_SIZE;
            memset(buf.data, 0, buf.len);
            strip.clear();
            strip.show();
          }
          xQueueSend(audioQueue, &buf, portMAX_DELAY);
        }
        continue;  // 跳过下面的“单次填充”逻辑
      }

      // 3. 普通每轮只填一个 chunk
      if (lastNoteIndex >= 0 && samplesRemaining != 0) {
        buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
        if (buf.len == 0) {
          memset(buf.data, 0, AUDIO_BUF_SIZE);
          buf.len = AUDIO_BUF_SIZE;
          samplesRemaining = 0;
        } else {
          int frames = buf.len / sizeof(int32_t);
          int32_t* samples = (int32_t*)buf.data;
          for (int j = 0; j < frames; ++j) {
            samples[j] = int32_t((int64_t)samples[j] * volumePercent / 100);
          }
          samplesRemaining -= frames;
          // 按下的手指亮燈
          strip.clear();
          for (int i = 0; i < 9; ++i) {
            int ledIndex = bitToLed[i];
            if (mask & (1 << (8 - i))) {
              strip.setPixelColor(ledIndex, strip.Color(10, 10, 0));
            }
          }
          strip.show();  // 更新燈光
        }
      } else {
        buf.len = AUDIO_BUF_SIZE;
        memset(buf.data, 0, buf.len);
        strip.clear();
        strip.show();
      }
      // 4. 推到 queue
      xQueueSend(audioQueue, &buf, portMAX_DELAY);
    } else {
      Serial.println("ProducerTask0 exiting...");
      vTaskDelete(NULL);
    }
  }
}

// ———— Consumer Task ————
// 不斷取 queue，把 data 丟給 I²S DMA
static void consumerTask0(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if (!stop0) {
      if (xQueueReceive(audioQueue, &buf, portMAX_DELAY) == pdTRUE) {
        size_t written = 0;
        i2s_write(I2S_PORT,
                  buf.data,
                  buf.len,
                  &written,
                  portMAX_DELAY);
      }
    } else {
      vTaskDelete(NULL);
    }
  }
}


// MODE 1 //

void createTasks1() {
  stop1 = false;
  initI2S();
  xTaskCreatePinnedToCore(producerTask1, "Producer", 4096, NULL, 2, NULL, 1);
  Serial.println("Producer created");
  xTaskCreatePinnedToCore(consumerTask1, "Consumer", 4096, NULL, 3, NULL, 1);
  Serial.println("Consumer created");
  xTaskCreatePinnedToCore(lightTask1, "LightHint", 2048, NULL, 1, NULL, 0);
  Serial.println("LightHint created");
}

void deleteTasks1() {
  stop1 = true;
  strip.clear();
  strip.show();
  lastNoteIndex = -1;
  samplesRemaining = 0;
  i2s_zero_dma_buffer(I2S_NUM_0);
  xQueueReset(audioQueue);
  Serial.println("Tasks1 marked for deletion and resources cleared.");
}


// ———— Light Task ————
void lightTask1(void* pv) {
  const float  BRIGHTNESS = 0.2f;     // 全部顏色 1/5 亮度
  const uint8_t RED   = uint8_t(255 * BRIGHTNESS);
  const uint8_t PURP  = uint8_t(128 * BRIGHTNESS);  // 暗紫色
  const uint32_t FADE_STEPS = 5;      // 暗紫淡出步數
  const uint32_t FADE_DELAY = 10;     // 每步 10ms → 總淡出約 50ms

  bool    isHolding  = false;
  uint32_t holdStart = 0;

  while (true) {
    if (stop1) vTaskDelete(NULL);
    if (currentIndex >= scoreLen) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // 計算 duration
    uint32_t thisStart = rawScore[currentIndex].startMs;
    uint32_t nextStart = (currentIndex+1 < scoreLen)
                         ? rawScore[currentIndex+1].startMs
                         : thisStart + NOTE_DURATION_SEC*1000;
    uint32_t durationMs = nextStart - thisStart;

    // 目標與當前指法
    uint16_t targetMask = getMaskFromNote(rawScore[currentIndex].note);
    uint16_t nowMask    = detectButtonMask(true);

    // 開始按下
    if (!isHolding && nowMask == targetMask) {
      isHolding = true;
      holdStart = millis();
    }

    if (isHolding) {
      uint32_t elapsed = millis() - holdStart;

      // 中途放開且未完成 → 暗紫瞬爆並淡出到紅
      if (nowMask != targetMask && elapsed < durationMs) {
        isHolding = false;
        // 暗紫瞬爆
        strip.clear();
        for (int i = 0; i < 9; ++i) {
          if (targetMask & (1 << (8 - i))) {
            strip.setPixelColor(bitToLed[i], strip.Color(PURP, 0, PURP));
          }
        }
        strip.show();
        // 快速淡出
        for (uint32_t step = 0; step < FADE_STEPS; ++step) {
          uint8_t b = uint8_t(PURP * (1.0f - float(step+1)/FADE_STEPS));
          strip.clear();
          for (int i = 0; i < 9; ++i) {
            if (targetMask & (1 << (8 - i))) {
              strip.setPixelColor(bitToLed[i], strip.Color(b, 0, b));
            }
          }
          strip.show();
          vTaskDelay(pdMS_TO_TICKS(FADE_DELAY));
        }
        // 回到純紅
        strip.clear();
        for (int i = 0; i < 9; ++i) {
          if (targetMask & (1 << (8 - i))) {
            strip.setPixelColor(bitToLed[i], strip.Color(RED, 0, 0));
          }
        }
        strip.show();
      }
      // 正在按且未完成 → 紅→黃→綠 漸變
      else if (nowMask == targetMask && elapsed < durationMs) {
        float ratio = float(elapsed) / float(durationMs);
        uint8_t r, g;
        if (ratio < 0.5f) {
          r = RED;
          g = uint8_t((ratio*2.0f) * 255 * BRIGHTNESS);
        } else {
          r = uint8_t((1.0f - (ratio-0.5f)*2.0f) * 255 * BRIGHTNESS);
          g = uint8_t(255 * BRIGHTNESS);
        }
        strip.clear();
        for (int i = 0; i < 9; ++i) {
          if (targetMask & (1 << (8 - i))) {
            strip.setPixelColor(bitToLed[i], strip.Color(r, g, 0));
          }
        }
        strip.show();
      }
      // 按滿時長 → 自動進下一個
      else if (elapsed >= durationMs) {
        isHolding = false;
        currentIndex++;
      }
    }
    else {
      // 未按 → 保持純紅
      strip.clear();
      for (int i = 0; i < 9; ++i) {
        if (targetMask & (1 << (8 - i))) {
          strip.setPixelColor(bitToLed[i], strip.Color(RED, 0, 0));
        }
      }
      strip.show();
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ———— Producer Task ————
// 常數

static void producerTask1(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if (!stop1) {

      // 1. 读当前指法
      uint16_t mask = detectButtonMask(true);
      int8_t noteIndex = maskToNoteIndex(mask);

      // 2. 切换音符时先预填充几个 chunk，再 continue
      if (noteIndex != lastNoteIndex) {
        lastNoteIndex = noteIndex;
        if (noteIndex >= 0) {
          uint32_t byteOffset = WAV_HEADER_SIZE
                                + uint32_t(noteIndex) * samplesPerNote * sizeof(int32_t);
          songFile.seek(byteOffset);
          samplesRemaining = samplesPerNote;
        } else {
          samplesRemaining = 0;
        }
        // 预先填满 queue
        for (int i = 0; i < QUEUE_PREFILL_CHUNKS; ++i) {
          if (lastNoteIndex >= 0 && samplesRemaining != 0) {
            buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
            if (buf.len == 0) {
              memset(buf.data, 0, AUDIO_BUF_SIZE);
              buf.len = AUDIO_BUF_SIZE;
            } else {
              int frames = buf.len / sizeof(int32_t);
              int32_t* samples = (int32_t*)buf.data;
              for (int j = 0; j < frames; ++j) {
                samples[j] = int32_t((int64_t)samples[j] * volumePercent / 100);
              }
              samplesRemaining -= frames;
            }
          } else {
            buf.len = AUDIO_BUF_SIZE;
            memset(buf.data, 0, buf.len);
          }
          xQueueSend(audioQueue, &buf, portMAX_DELAY);
        }
        continue;  // 跳过下面的“单次填充”逻辑
      }

      // 3. 普通每轮只填一个 chunk
      if (lastNoteIndex >= 0 && samplesRemaining != 0) {
        buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
        if (buf.len == 0) {
          memset(buf.data, 0, AUDIO_BUF_SIZE);
          buf.len = AUDIO_BUF_SIZE;
          samplesRemaining = 0;
        } else {
          int frames = buf.len / sizeof(int32_t);
          int32_t* samples = (int32_t*)buf.data;
          for (int j = 0; j < frames; ++j) {
            samples[j] = int32_t((int64_t)samples[j] * volumePercent / 100);
          }
          samplesRemaining -= frames;
        }
      } else {
        buf.len = AUDIO_BUF_SIZE;
        memset(buf.data, 0, buf.len);
      }
      // 4. 推到 queue
      xQueueSend(audioQueue, &buf, portMAX_DELAY);
    } else {
      Serial.println("ProducerTask0 exiting...");
      vTaskDelete(NULL);
    }
  }
}

// ———— Consumer Task ————
// 不斷取 queue，把 data 丟給 I²S DMA
static void consumerTask1(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if (!stop1) {
      if (xQueueReceive(audioQueue, &buf, portMAX_DELAY) == pdTRUE) {
        size_t written = 0;
        i2s_write(I2S_PORT,
                  buf.data,
                  buf.len,
                  &written,
                  portMAX_DELAY);
      }
    } else {
      Serial.println("ConsumerTask1 exiting...");
      vTaskDelete(NULL);
    }
  }
}


// MODE 2 //

void createTasks2() {
  stop2 = false;
  currentIdx = 0;
  prepareLighting();  // 預備燈三下
  startMode2 = true;
  startMillis = millis();
  xTaskCreate(audioTask, "AudioTask", 4096, NULL, 2, &audioTaskHandle);
  Serial.println("AudioTask created");
  xTaskCreate(fingerTask, "FingerTask", 2048, NULL, 1, NULL);
  Serial.println("FingerTask created");
  xTaskCreate(lightingTask, "LightingTask", 2048, NULL, 1, NULL);
  Serial.println("LightingTask created");
}

void deleteTasks2() {
  stop2 = true;
  while (eTaskGetState(audioTaskHandle) != eDeleted) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  audioTaskHandle = NULL;

  vTaskDelay(50 / portTICK_PERIOD_MS);  // 多等一點，確保任務真的結束

  // 重設播放狀態與 DMA 緩衝
  i2s_zero_dma_buffer(I2S_PORT);
  i2s_stop(I2S_PORT);  // ⛔ 停止 I2S 傳輸
  vTaskDelay(10);
  i2s_start(I2S_PORT);  // ✅ 重新啟動
  xQueueReset(audioQueue);

  i2s_driver_uninstall(I2S_PORT);  // 卸載 I2S 驅動
  Serial.println("I2S uninstalled");

  strip.clear();
  strip.show();

  Serial.println("Tasks2 marked for deletion and I2S cleared.");
}


void audioTask(void* pv) {
  // 直接呼叫 playAudio，由它自行處理讀／寫／靜音／縮放
  while (1) {
    if (startMode2) {
      songFile.seek(44);
      Serial.println("audio start");
      playAudio();
      break;
    }
  }
  Serial.println("AudioTask exiting...");
  vTaskDelete(NULL);
}

void fingerTask(void* pvParameters) {
  while (true) {
    while (startMode2) {
      if (!stop2) {
        fingerOK = checkFingering(millis() - startMillis, false);
        vTaskDelay(pdMS_TO_TICKS(10));
      } else {
        Serial.println("fingerTask exiting...");
        vTaskDelete(NULL);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void lightingTask(void* pvParameters) {
  // startMillis = millis();
  Serial.println("light start");
  // 等待 startMillis 在 initLighting() 中已設置
  while (true) {
    if (!stop2) {
      updateLighting(millis() - startMillis);
      vTaskDelay(pdMS_TO_TICKS(10));  // 每 10 ms 更新一次
    } else {
      Serial.println("lightingTask exiting...");
      vTaskDelete(NULL);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // SD 卡初始化
  if (!SDcardSetup()) {
    Serial.println("SDcardSetup failed!");
    while (1)
      ;
  }
  songFile = measureOpenDelay("/notes.wav");
  if (!songFile) {
    Serial.println("Failed to open /notes.wav");
    while (1)
      ;
  }

  // I²S & 按鍵初始化
  initI2S();
  initButtons();

  // lights初始化
  initLighting();
  initRawEvents();
  // 建 Queue
  audioQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AudioBuffer));
  Serial.println("Queue Create succeeded");

  // 建立兩個任務：Producer 優先度 1、Consumer 優先度 2
  // default: mode 0
  // xTaskCreatePinnedToCore(producerTask0, "Producer", 4096, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(consumerTask0, "Consumer", 4096, NULL, 2, NULL, 1);

  xTaskCreatePinnedToCore(producerTask1, "Producer", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(consumerTask1, "Consumer", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(lightTask1, "LightHint", 2048, NULL, 1, NULL, 1);

  Serial.println("RTOS playback ready!");
}

void loop() {
  // MODE 切換與 RTOS 管理
  if (Serial.available()) {
    char input = Serial.read();
    if (input >= '0' && input <= '2') {
      int newMode = input - '0';
      if (newMode != MODE) {
        Serial.print("🔄 MODE changed to: ");
        Serial.println(newMode);
        if (MODE == 0) {
          deleteTasks0();
          if (songFile) songFile.close();
          if (newMode == 1) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");
            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            currentIndex = 0;
            createTasks1();
          } else if (newMode == 2) {
            // 重新開檔
            songFile = measureOpenDelay("/song.wav");
            while (!songFile) {
              Serial.println("Failed to open /song.wav");
            }
            songFile.seek(44);
            createTasks2();
          }
        } else if (MODE == 1) {
          deleteTasks1();
          if (songFile) songFile.close();
          if (newMode == 0) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");
            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            createTasks0();
          } else if (newMode == 2) {
            // 重新開檔
            currentIdx = 0;
            songFile = measureOpenDelay("/song.wav");
            while (!songFile) {
              Serial.println("Failed to open /song.wav");
            }
            songFile.seek(44);
            createTasks2();
          }
        } else if (MODE == 2) {
          deleteTasks2();
          fingerOK = false;
          currentIdx = 0;
          startMode2 = false;
          if (songFile) songFile.close();
          if (newMode == 0) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");

            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            createTasks0();
            // 在切到 Mode 1 之前清空 I2S
            AudioBuffer silentBuf;
            silentBuf.len = AUDIO_BUF_SIZE;
            memset(silentBuf.data, 0, AUDIO_BUF_SIZE);
            for (int i = 0; i < 3; ++i) {
              xQueueSend(audioQueue, &silentBuf, 0);
            }
          } else if (newMode == 1) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");
            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            currentIndex = 0;
            createTasks1();
            // 在切到 Mode 1 之前清空 I2S
            AudioBuffer silentBuf;
            silentBuf.len = AUDIO_BUF_SIZE;
            memset(silentBuf.data, 0, AUDIO_BUF_SIZE);
            for (int i = 0; i < 3; ++i) {
              xQueueSend(audioQueue, &silentBuf, 0);
            }
          }
        }
        MODE = newMode;
      }
    }
  }

  // 只做 power_off 偵測，其它都交由任務處理
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "power_off") {
      Serial.println("power_off received, closing SD and sleeping...");
      songFile.close();
      SD.end();
      esp_deep_sleep_start();
    }
  }
  delay(10);
}
