//Shared.h
#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>

// 狀態機定義
enum SystemState {
  S_WAIT_BT, S_IDLE, S_WARN_PLANT, S_WARN_HUMAN, 
  S_CRITICAL, S_PARTY_PLANT, S_PARTY_HUMAN
};

// 個性設定
enum Personality {
  PER_KIND,    // 善良 (Angel)
  PER_MEAN,    // 惡劣 (Demon)
  PER_PLAYFUL  // 俏皮 (Kid)
};

// 系統資料結構
struct AppData {
  volatile long humanTimer;
  volatile long humanMaxSeconds;
  volatile int  soilPercent;
  volatile bool isSoilDry;
  volatile unsigned long stateStartTime;
  
  // AI 訊息相關
  String aiMessage;
  unsigned long msgStartTime;
  bool showAiMsg;
};

// 全域變數宣告 (告訴大家這些變數存在)
extern volatile SystemState currentState;
extern AppData Data;
extern volatile bool reqHumanRefill;
extern Personality currentPersonality;

#endif