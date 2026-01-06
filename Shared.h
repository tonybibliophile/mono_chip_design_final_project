#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>

enum SystemState {
    S_WAIT_BT, S_IDLE, S_WARN_PLANT, S_WARN_HUMAN, 
    S_PARTY_PLANT, S_PARTY_HUMAN, S_CRITICAL
};

enum Personality { PER_KIND, PER_MEAN, PER_PLAYFUL };

struct AppDataStruct {
    bool wifiConnected = false;
    volatile bool bleConnected = false;
    
    int soilRaw = 0;
    bool isSoilDry = false;
    
    int humanTimer = 1800;          
    int humanWaterInterval = 1800;  
    volatile bool flagHumanDrank = false; 

    unsigned long stateStartTime = 0;
    
    // --- AI 相關變數 ---
    volatile bool requestAI = false;
    
    // 【新增】這個是用來顯示 "AI Thinking..." 的旗標
    volatile bool isAiLoading = false; 
    
    String aiPromptType = "";
    String aiMessage = "";
    volatile bool showAiMsg = false;
    unsigned long msgStartTime = 0;
};

extern AppDataStruct AppData;
extern SystemState currentState;
extern Personality currentPersonality;

#endif