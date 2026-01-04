//Task.cpp
#include "Config.h"
#include "Shared.h"
#include "Peripherals.h" // 包含 playTone 宣告

// 引用外部函式
void connectWiFi();
void askGemini(String event); 

// --- 1. 邏輯任務 ---
void TaskLogic(void *pvParameters) {
  (void) pvParameters;
  bool lastSoilDry = true;
  SystemState lastState = S_WAIT_BT;

  for (;;) {
    // A. 讀取感測器
    int val = analogRead(PIN_SOIL);
    // 更新全域變數
    Data.isSoilDry = (val > SOIL_DRY_THRESHOLD);
    
    // B. 偵測邊緣事件
    bool eventPlantWatered = (lastSoilDry && !Data.isSoilDry);
    bool eventHumanRefill = reqHumanRefill;
    if (reqHumanRefill) reqHumanRefill = false;
    lastSoilDry = Data.isSoilDry;

    unsigned long now = millis();

    // C. 狀態機切換
    switch (currentState) {
      case S_WAIT_BT: currentState = S_IDLE; break;
      default:
        if (eventHumanRefill) { 
           currentState = S_PARTY_HUMAN; Data.stateStartTime = now; 
        }
        else if (eventPlantWatered) { 
           currentState = S_PARTY_PLANT; Data.stateStartTime = now; 
        }
        else if (Data.isSoilDry) {
           currentState = S_WARN_PLANT;
        }
        else if (now - Data.stateStartTime > TIME_PARTY_HUMAN && (currentState == S_PARTY_HUMAN || currentState == S_PARTY_PLANT)) {
           currentState = S_IDLE;
        }
        
        // 倒數計時
        static int tick = 0;
        if(currentState == S_IDLE && ++tick >= 10) { 
           tick = 0; if(Data.humanTimer > 0) Data.humanTimer--; 
           if(Data.humanTimer == 0) currentState = S_WARN_HUMAN;
        }
        break;
    }

    // D. ★★★ AI 觸發點 ★★★
    if (currentState != lastState) {
      String prompt = "";
      switch(currentState) {
        case S_WARN_PLANT:  prompt = "My soil is dry, I am thirsty!"; break;
        case S_WARN_HUMAN:  prompt = "Human forgot to drink water!"; break;
        case S_PARTY_PLANT: prompt = "I just got watered! So happy!"; break;
        case S_PARTY_HUMAN: prompt = "Human finally drank water!"; break;
      }
      // 只有在有話要說時才呼叫 AI
      if (prompt != "") {
        askGemini(prompt);
      }
      lastState = currentState;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// --- 2. 輸入任務 (藍牙) ---
void TaskInput(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (SerialBT.hasClient()) {
      bool shouldTriggerParty = false;
      if (SerialBT.available()) {
        String s = SerialBT.readStringUntil('\n'); s.trim();
        if (s.length() > 0) {
          
          if (s.equalsIgnoreCase("Water")) {
            Data.humanTimer = Data.humanMaxSeconds; shouldTriggerParty = true;
          } 
          // 切換個性
          else if (s.equalsIgnoreCase("Kind")) { currentPersonality = PER_KIND; askGemini("I decided to be kind."); }
          else if (s.equalsIgnoreCase("Mean")) { currentPersonality = PER_MEAN; askGemini("I decided to be mean."); }
          else if (s.equalsIgnoreCase("Playful")) { currentPersonality = PER_PLAYFUL; askGemini("I decided to be playful."); }
          // 設定時間
          else {
            float m = s.toFloat();
            if (m > 0) { Data.humanMaxSeconds = (long)(m*60); Data.humanTimer = Data.humanMaxSeconds; }
          }
        }
      }
      if (shouldTriggerParty) reqHumanRefill = true;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// --- 3. 顯示任務 ---
void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    unsigned long now = millis();
    drawCurrentStateView(now);
    updateLCDText(now);
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}

// --- 4. 聲音任務 (關鍵修復：使用 playTone) ---
void TaskSound(void *pvParameters) {
  (void) pvParameters;
  SystemState lastState = S_WAIT_BT;

  for (;;) {
    // One-shot 音效
    if (currentState != lastState) {
       if (currentState == S_PARTY_HUMAN) {
         playTone(NOTE_G4, 80); playTone(NOTE_C5, 80); playTone(NOTE_E5, 80); playTone(0,0);
       }
       else if (currentState == S_PARTY_PLANT) {
         playTone(NOTE_C6, 100); playTone(NOTE_C6, 100); playTone(0,0);
       }
       lastState = currentState;
    }
    
    // 背景音 (Loop)
    switch (currentState) {
      case S_WARN_HUMAN: 
        playTone(800, 200); playTone(1200, 200); 
        break;
      case S_WARN_PLANT: 
        playTone(NOTE_G5, 400); playTone(0, 2000); // 悲傷間歇音
        break;
      default: 
        playTone(0, 100); // 靜音
        break;
    }
  }
}