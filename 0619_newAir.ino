/*
使用建議：把沒有要用的函式都關起來比較短
mode 2 整合進來應該只需要加create跟delete tasks的函式吧
*/

/*
單獨mode改的東
mode0
1. include: <AirUtils.h> <LightingUtils.h>
2. setup: initAirSensor(); initLighting();
3. "LightingUtils.cpp"裡的initLighting()加strip.clear(); strip.setBrightness(30);
4. static void producerTask(void* pv)
5. "AirUtils.cpp"裡的isBlowingStopped()註解掉三行

mode1跟mode0的差異
1. 參數在 // Mode 1 新東 // 區塊
2. setup: xTaskCreatePinnedToCore(lightTask, "LightHint", 2048, NULL, 1, NULL, 0);
3. void lightTask(void* pv)
*/

/*
跟單獨mode的檔案差在（此整合檔才有的）
1. // mode 切換參數 // 區塊
2. setup: xTaskCreatePinnedToCore(producerTask0, "Producer", 4096, NULL, 1, NULL, 1); //Task後面多一個0！
          xTaskCreatePinnedToCore(consumerTask0, "Consumer", 4096, NULL, 2, NULL, 1);
3. void createTasks0(); void deleteTasks0(); 
4. static void producerTask0(void* pv); static void consumerTask0(void* pv);
5. void createTasks1(); void deleteTasks1();
6. void lightTask1(void* pv); static void producerTask1(void* pv); static void consumerTask1(void* pv);
7. loop()的Serial部分

備註：
1. .cpp都沒變
2. 有些結尾是task有些是tasks
*/

// mode 切換參數
volatile bool stop0 = false;
volatile bool stop1 = true;
volatile bool stop2 = true;
int MODE = 0;

#include <SDUtils.h>          // SDcardSetup(), measureOpenDelay()
#include <FingeringsUtils.h>  // detectButtonMask(), fingerings[], numFingerings
#include <I2SUtils.h>         // initI2S()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <AirUtils.h>
#include <LightingUtils.h>

// ======= 常數設定 =======
#define SAMPLE_RATE 48000
#define NOTE_DURATION_SEC 3
#define AUDIO_BUF_SIZE 512
#define WAV_HEADER_SIZE 44
#define I2S_PORT I2S_NUM_0

// Queue 大小，可存 4 段 audio buffer
#define QUEUE_LENGTH 4

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
static const uint32_t samplesPerNote = SAMPLE_RATE * NOTE_DURATION_SEC;

// MODE 1 新東
int currentIndex = 0; 

// ———— Mask 轉 NoteIndex ————
static int8_t maskToNoteIndex(uint16_t mask) {
  for (size_t i = 0; i < 27; ++i) {
    if (fingerings[i].mask == mask) return i;
  }
  return -1;
}

// 放在檔案頂端
#define VOLUME_PERCENT 20
#define QUEUE_PREFILL_CHUNKS 4  // 預先填滿多少 chunk


// MODE 0 //

void createTasks0() {
  stop0 = false;
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
    if(!stop0) {

      // 1. 读当前指法
      uint16_t mask      = detectButtonMask(false);
      int8_t   noteIndex = maskToNoteIndex(mask);

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
          float gain = computeGain();
          bool blowing = isBlowing();
          if(blowing && lastNoteIndex >= 0 && samplesRemaining != 0) {
            buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
            if (buf.len == 0) {
              memset(buf.data, 0, AUDIO_BUF_SIZE);
              buf.len = AUDIO_BUF_SIZE;
            } else {
              int frames = buf.len / sizeof(int32_t);
              int32_t* samples = (int32_t*)buf.data;
              for (int j = 0; j < frames; ++j) {
                samples[j] = int32_t((int64_t)samples[j] * gain / 100);
              }
              samplesRemaining -= frames;
              // 按下的手指亮燈
              for (int i = 0; i < 9; ++i) {  
                int ledIndex = bitToLed[i];
                if (mask & (1 << (8-i))) {
                  strip.setPixelColor(ledIndex, strip.Color(10, 10, 0));
                } else {
                  strip.setPixelColor(ledIndex, strip.Color(0, 0, 0));
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
          xQueueSend(audioQueue, &buf, portMAX_DELAY);
        }
        continue;  // 跳过下面的“单次填充”逻辑
      }

      // 3. 普通每轮只填一个 chunk
      float gain = computeGain();
      bool blowing = isBlowing();
      if(blowing && lastNoteIndex >= 0 && samplesRemaining != 0) {
        buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
        if (buf.len == 0) {
          memset(buf.data, 0, AUDIO_BUF_SIZE);
          buf.len = AUDIO_BUF_SIZE;
          samplesRemaining = 0;
        } else {
          int frames = buf.len / sizeof(int32_t);
          int32_t* samples = (int32_t*)buf.data;
          for (int j = 0; j < frames; ++j) {
            samples[j] = int32_t((int64_t)samples[j] * gain / 100);
          }
          samplesRemaining -= frames;
          // 按下的手指亮燈
          for (int i = 0; i < 9; ++i) { 
            int ledIndex = bitToLed[i];
            if (mask & (1 << (8-i))) {
              strip.setPixelColor(ledIndex, strip.Color(10, 10, 0));
            } else {
              strip.setPixelColor(ledIndex, strip.Color(0, 0, 0));
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
    }
    else {
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
    if(!stop0) {
      if (xQueueReceive(audioQueue, &buf, portMAX_DELAY) == pdTRUE) {
        size_t written = 0;
        i2s_write(I2S_PORT,
                  buf.data,
                  buf.len,
                  &written,
                  portMAX_DELAY);
      }
    }
    else {
      vTaskDelete(NULL);
    }
  }
}




// MODE 1 //

void createTasks1() {
  stop1 = false;
  xTaskCreatePinnedToCore(producerTask1, "Producer", 4096, NULL, 1, NULL, 1);
  Serial.println("Producer created");
  xTaskCreatePinnedToCore(consumerTask1, "Consumer", 4096, NULL, 2, NULL, 1);
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
  while (true) {
    if(!stop1) {
      if (currentIndex >= scoreLen) {
        vTaskDelay(10 / portTICK_PERIOD_MS);  // 不做事時短暫延遲
        continue;
      }

      uint16_t targetMask = getMaskFromNote(rawScore[currentIndex].note);

      // 持續亮紅燈：所有 targetMask 對應的 bitToLed 都設紅色
      for (int i = 0; i < 9; ++i) {
        if (targetMask & (1 << (8-i))) {
          strip.setPixelColor(bitToLed[i], strip.Color(255, 0, 0));
        } else {
          strip.setPixelColor(bitToLed[i], 0); // 關掉其他燈
        }
      }
      strip.show();

      // 檢查是否達成條件：正確指法 + 吹氣
      uint16_t nowMask = detectButtonMask(false);
      if ((nowMask == targetMask) && isBlowing()) {
        // 閃綠燈一次
        for (int i = 0; i < 9; ++i) {
          if (targetMask & (1 << (8-i))) {
            strip.setPixelColor(bitToLed[i], strip.Color(0, 255, 0));
          }
        }
        strip.show();
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 綠燈閃一下
        currentIndex++;  // 前進到下一音符
      }

      vTaskDelay(20 / portTICK_PERIOD_MS);  // 每 20ms 檢查一次狀態
    }
    else vTaskDelete(NULL);
  }
}


// ———— Producer Task ————
// 常數

static void producerTask1(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if(!stop1) {
      // 1. 读当前指法
      uint16_t mask      = detectButtonMask(false);
      int8_t   noteIndex = maskToNoteIndex(mask);

      float gain = computeGain();
      bool blowing = isBlowing();
      Serial.print("gain: "); Serial.print(gain);
      Serial.println(blowing ? 1 : 0);

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
          if(blowing && lastNoteIndex >= 0 && samplesRemaining != 0) {
            buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
            if (buf.len == 0) {
              memset(buf.data, 0, AUDIO_BUF_SIZE);
              buf.len = AUDIO_BUF_SIZE;
            } else {
              int frames = buf.len / sizeof(int32_t);
              int32_t* samples = (int32_t*)buf.data;
              for (int j = 0; j < frames; ++j) {
                samples[j] = int32_t((int64_t)samples[j] * gain / 100);
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
      if(blowing && lastNoteIndex >= 0 && samplesRemaining != 0) {
        buf.len = songFile.read(buf.data, AUDIO_BUF_SIZE);
        if (buf.len == 0) {
          memset(buf.data, 0, AUDIO_BUF_SIZE);
          buf.len = AUDIO_BUF_SIZE;
          samplesRemaining = 0;
        } else {
          int frames = buf.len / sizeof(int32_t);
          int32_t* samples = (int32_t*)buf.data;
          for (int j = 0; j < frames; ++j) {
            samples[j] = int32_t((int64_t)samples[j] * gain / 100);
          }
          samplesRemaining -= frames;
        }
      } else {
        buf.len = AUDIO_BUF_SIZE;
        memset(buf.data, 0, buf.len);
      }
      // 4. 推到 queue
      xQueueSend(audioQueue, &buf, portMAX_DELAY);
    }
    else {
      Serial.println("ProducerTask1 exiting...");
      vTaskDelete(NULL);
    }
  }
}

// ———— Consumer Task ————
// 不斷取 queue，把 data 丟給 I²S DMA
static void consumerTask1(void* pv) {
  AudioBuffer buf;
  for (;;) {
    if(!stop1) {
      if (xQueueReceive(audioQueue, &buf, portMAX_DELAY) == pdTRUE) {
        size_t written = 0;
        i2s_write(I2S_PORT,
                  buf.data,
                  buf.len,
                  &written,
                  portMAX_DELAY);
      }
    }
    else vTaskDelete(NULL);
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

  // ads初始化
  initAirSensor();

  // lights初始化
  initLighting();

  // 建 Queue
  audioQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AudioBuffer));

  // 建立兩個任務：Producer 優先度 1、Consumer 優先度 2
  // default: mode 0
  xTaskCreatePinnedToCore(producerTask0, "Producer", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(consumerTask0, "Consumer", 4096, NULL, 2, NULL, 1);

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
        if(MODE == 0) {
          deleteTasks0();
          if (songFile) songFile.close();
          if(newMode == 1) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");
            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            createTasks1();
          }
        }
        if(MODE == 1) {
          deleteTasks1();
          if (songFile) songFile.close();
          if(newMode == 0) {
            // 重新開檔
            songFile = measureOpenDelay("/notes.wav");
            while (!songFile) {
              Serial.println("Failed to open /notes.wav");
            }
            createTasks0();
          }
        }
        
        
        MODE = newMode;
      }
    }
  }
  // Serial.print(isBlowing() ? 1 : 0); Serial.print('\t');
  // Serial.println(computeGain() / 100);

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
