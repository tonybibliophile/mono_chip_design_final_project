#include "Shared.h"
#include "Config.h"

void triggerAI(String reason) {
    if (!AppData.requestAI) { 
        AppData.aiPromptType = reason;
        AppData.requestAI = true; 
    }
}

void Task_Logic(void *pv) {
    SystemState lastState = S_WAIT_BT;
    bool lastSoilDry = false;
    unsigned long lastTimerTick = 0;
    unsigned long lastDebugPrint = 0;

    AppData.humanWaterInterval = 1800; 
    AppData.humanTimer = AppData.humanWaterInterval; 
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    for (;;) {
        unsigned long now = millis();

        // -------------------------
        // 1. 處理：人類喝水訊號 (按鈕優先)
        // -------------------------
        if (digitalRead(PIN_BUTTON) == LOW) {
            AppData.flagHumanDrank = true;
        }

        if (AppData.flagHumanDrank) {
            AppData.flagHumanDrank = false;
            AppData.humanTimer = AppData.humanWaterInterval;
            
            if (currentState != S_WAIT_BT) {
                currentState = S_PARTY_HUMAN;
                AppData.stateStartTime = now;
            }
            Serial.println("Logic: Trigger Human Party!");
        }

        // -------------------------
        // 2. 讀取感測器 (加入軟體濾波與遲滯區)
        // -------------------------
        // A. 濾波：連續讀取 10 次取平均，減少突發雜訊
        int readingSum = 0;
        for(int i=0; i<10; i++) {
            readingSum += analogRead(PIN_SOIL);
        }
        AppData.soilRaw = readingSum / 10;

        // B. 遲滯區邏輯：防止在邊緣亂跳
        // 變乾門檻增加 200，變濕門檻減少 200 (緩衝帶共 400)
        const int DRY_LIMIT = SOIL_DRY_THRESHOLD + 200; 
        const int WET_LIMIT = SOIL_DRY_THRESHOLD - 200; 

        if (AppData.isSoilDry) {
            // 目前是乾的，讀值必須明顯「變濕」小於 WET_LIMIT 才會切換
            if (AppData.soilRaw < WET_LIMIT) {
                AppData.isSoilDry = false;
            }
        } else {
            // 目前是濕的，讀值必須明顯「變乾」大於 DRY_LIMIT 才會切換
            if (AppData.soilRaw > DRY_LIMIT) {
                AppData.isSoilDry = true;
            }
        }

        

        // 偵測澆水事件 (剛從乾變濕的那一刻)
        bool eventPlantWatered = (lastSoilDry && !AppData.isSoilDry); 
        lastSoilDry = AppData.isSoilDry;

        if (eventPlantWatered) {
             currentState = S_PARTY_PLANT;
             AppData.stateStartTime = now;
             Serial.println("Logic: Plant Watered!");
        }

        // -------------------------
        // 3. 狀態仲裁 (決定模式)
        // -------------------------
        // 在非慶祝、非等待連線時更新狀態
        if (currentState != S_WAIT_BT && 
            currentState != S_PARTY_PLANT && 
            currentState != S_PARTY_HUMAN) 
        {
            if (AppData.isSoilDry && AppData.humanTimer <= 0) {
                currentState = S_CRITICAL; // 惡魔模式
            }
            else if (AppData.isSoilDry) {
                currentState = S_WARN_PLANT;
            }
            else if (AppData.humanTimer <= 0) {
                currentState = S_WARN_HUMAN;
            }
            else {
                currentState = S_IDLE;
            }
        }
        
        // 4. 慶祝狀態自動結束回歸
        if ((currentState == S_PARTY_PLANT || currentState == S_PARTY_HUMAN) && 
            (now - AppData.stateStartTime > 5000)) {
            currentState = S_IDLE;
        }

        // -------------------------
        // 5. 計時器邏輯
        // -------------------------
        if (now - lastTimerTick >= 1000) {
            lastTimerTick = now;
            if (currentState != S_WAIT_BT && AppData.humanTimer > 0) {
                AppData.humanTimer--;
            }
        }

        // -------------------------
        // 6. Debug 輸出與 AI 觸發
        // -------------------------
        if (now - lastDebugPrint >= 1000) {
            lastDebugPrint = now;
            Serial.print("State: "); Serial.print(currentState);
            Serial.print(" | Raw: "); Serial.print(AppData.soilRaw);
            Serial.print(" | Timer: "); Serial.println(AppData.humanTimer);
        }

        if (currentState != lastState) {
            // 排除開機連線的初始轉換，其餘狀態改變才觸發 AI
            if (!(lastState == S_WAIT_BT && currentState == S_IDLE)) {
                switch (currentState) {
                    case S_CRITICAL:    triggerAI("We are both DYING! Do something!"); break;
                    case S_WARN_PLANT:  triggerAI("Soil is dry!"); break;
                    case S_WARN_HUMAN:  triggerAI("Remind user to drink water."); break;
                    case S_PARTY_PLANT: triggerAI("Plant watered! Happy!"); break;
                    case S_PARTY_HUMAN: triggerAI("User drank water. Good job."); break;
                    default: break;
                }
            }
            lastState = currentState;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}