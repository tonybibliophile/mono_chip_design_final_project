#include "Config.h"
#include "Shared.h"
#include "Peripherals.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Task 宣告
void connectWiFi();
void TaskLogic(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskSound(void *pvParameters);
void TaskInput(void *pvParameters);

// 全域狀態
volatile SystemState currentState = S_WAIT_BT;
AppData Data = {180, 180, 0, false, 0, "", 0, false};
volatile bool reqHumanRefill = false;
Personality currentPersonality = PER_KIND;

void setup() {
  // 防止低電壓重啟 (Brownout)，一定要開
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);

  initPeripherals();
  connectWiFi();

  // ★ 使用 xTaskCreatePinnedToCore 來指定核心 (最後一個參數是核心編號 0 或 1)

  // 1. TaskDisplay -> Core 1 (應用核)
  // 負責 OLED 繪圖，放在 Core 1 確保畫面流暢，不被無線通訊干擾
  xTaskCreatePinnedToCore(TaskDisplay, "Display", 8192, NULL, 3, NULL, 1);
  
  // 2. TaskLogic -> Core 1 (應用核)
  // 負責 AI 與 JSON 解析，運算量大，放在 Core 1 避免影響 WiFi 底層
  xTaskCreatePinnedToCore(TaskLogic,   "Logic",   16384, NULL, 2, NULL, 1);
  
  // 3. TaskInput -> Core 0 (系統核)
  // 負責 BLE 藍牙，放在 Core 0 (因為 ESP32 的藍牙底層就在 Core 0)，減少跨核心通訊
  //xTaskCreatePinnedToCore(TaskInput,   "Input",   10240, NULL, 2, NULL, 0);
  
  // 4. TaskSound -> Core 1 (應用核)
  // 簡單的周邊控制，放在 Core 1
  xTaskCreatePinnedToCore(TaskSound,   "Sound",   4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}