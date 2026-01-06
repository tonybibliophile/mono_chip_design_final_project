#include "Shared.h"
#include "Config.h"

// ==========================================
// 全域變數定義
// ==========================================
AppDataStruct AppData; 
SystemState currentState = S_WAIT_BT; 
Personality currentPersonality = PER_KIND;

// ==========================================
// 外部任務函式宣告
// ==========================================
extern void Task_Logic(void *pv);
extern void Task_Display(void *pv); // 包含 Sound 與 Peripherals_Init
extern void Task_Sound(void *pv);
extern void Task_BLE(void *pv);        
extern void Task_Network(void *pv); 
extern void Peripherals_Init();

// ==========================================
// Setup
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("System Booting (ESP32-S3)...");

  // 1. 初始化硬體 (螢幕、I2C、蜂鳴器)
  Peripherals_Init();

  // 2. 建立多工任務 (FreeRTOS Tasks)
  // 參數: 函式, 名稱, 堆疊大小, 參數, 優先權, Handle

  xTaskCreate(Task_Logic,   "Logic",   4096, NULL, 1, NULL);
  xTaskCreate(Task_Display, "Display", 4096, NULL, 1, NULL);
  xTaskCreate(Task_Sound,   "Sound",   2048, NULL, 1, NULL);

  #if ENABLE_BLE
  xTaskCreate(Task_BLE,     "BLE",     4096, NULL, 1, NULL);
  #endif

  // 網路任務給 8192 bytes 避免 JSON 處理當機
  xTaskCreate(Task_Network, "Network", 8192, NULL, 1, NULL);

  Serial.println("All Tasks Created.");
}

// ==========================================
// Loop
// ==========================================
void loop() {
  vTaskDelay(10000 / portTICK_PERIOD_MS); 
}