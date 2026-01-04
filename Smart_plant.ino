//Smart_plant.ino
#include "Config.h"
#include "Shared.h"
#include "Peripherals.h"


#include "soc/soc.h"             // 必須引用
#include "soc/rtc_cntl_reg.h"    // 必須引用
void connectWiFi(); 
void TaskLogic(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskSound(void *pvParameters);
void TaskInput(void *pvParameters);

volatile SystemState currentState = S_WAIT_BT;
AppData Data = {180, 180, 0, false, 0, "", 0, false};
volatile bool reqHumanRefill = false;
Personality currentPersonality = PER_KIND; 

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);//test
  Serial.begin(115200);
  
  initPeripherals();
  connectWiFi();

  xTaskCreate(TaskDisplay, "Disp", 8192, NULL, 3, NULL);
  
  // ★ 關鍵修正：加大 Logic 任務的記憶體 (8192 -> 12288)
  // 這是為了應付 WiFiClientSecure 的龐大開銷
  xTaskCreate(TaskLogic,   "Logic", 8192, NULL, 2, NULL); 
  
  xTaskCreate(TaskInput,   "Input",4096, NULL, 1, NULL);
  xTaskCreate(TaskSound,   "Sound",4096, NULL, 1, NULL);
}

void loop() { vTaskDelete(NULL); }