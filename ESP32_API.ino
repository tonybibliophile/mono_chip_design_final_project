/**
 * Project: Smart Plant FSM v2 (AI Powered)
 * Architecture: FreeRTOS + FSM + Gemini API
 * Description: 
 * 整合 Gemini API，根據不同人格 (毒舌/教官/憂鬱/傲嬌) 生成 LCD 短句。
 * 新增 TaskAI 負責聯網處理 HTTP 請求與 JSON 解析。
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // 請務必安裝 ArduinoJson Library (v7.0+)

// **********************************************************
// [CONFIG] 使用者設定區 (請在此填寫)
// **********************************************************
const char* WIFI_SSID = "請填入WiFi名稱";    
const char* WIFI_PASS = "請填入WiFi密碼";    
const char* API_KEY   = "輸入API_KEY"; 

// 模型設定 
const char* GEMINI_MODEL = "gemini-2.0-flash"; 

// **********************************************************
// [HARDWARE] 硬體腳位配置 (維持 V1 設定)
// **********************************************************
#define PIN_OLED_CLK   18
#define PIN_OLED_MOSI  23
#define PIN_OLED_RES   4
#define PIN_OLED_DC    16
#define PIN_OLED_CS    5

#define PIN_SOIL      34
#define PIN_BUZZER    25
#define PIN_BUTTON    19

const int SOIL_DRY_THRESHOLD = 3000;
const int TIME_PARTY_PLANT   = 8000;
const int TIME_PARTY_HUMAN   = 4000;

// 音階頻率 (用於音效)
#define NOTE_G4  392
#define NOTE_C5  523
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_G6  1568
#define NOTE_C7  2093

// **********************************************************
// [FSM] 狀態機定義
// **********************************************************
enum SystemState {
  S_WAIT_BT,       
  S_IDLE,          
  S_WARN_PLANT,    
  S_WARN_HUMAN,    
  S_CRITICAL,      
  S_PARTY_PLANT,   
  S_PARTY_HUMAN    
};
volatile SystemState currentState = S_WAIT_BT;

// **********************************************************
// [AI] 人格設定 (對應 Python 字典)
// **********************************************************
enum Persona {
  P_ROASTER,    // 毒舌
  P_SERGEANT,   // 教官
  P_EMO,        // 憂鬱
  P_TSUNDERE    // 傲嬌
};

const char* PERSONA_NAMES[] = {"Roaster", "Sergeant", "Emo", "Tsundere"};

// System Instructions (提示詞)
const char* SYSTEM_INSTRUCTIONS[] = {
  "You are a mean plant. Insult the user in 3-5 words. Be punchy.",           // Roaster
  "You are a Drill Sergeant. Yell short commands (CAPS). Max 4 words.",       // Sergeant
  "You are sad. Speak in 3-word tragic phrases. e.g. 'Life is pain.'",        // Emo
  "You act like you don't care. Short & cute. Max 5 words."                   // Tsundere
};

// **********************************************************
// [DATA] 全域資料結構
// **********************************************************
struct AppData {
  volatile long humanTimer;       // 人類倒數秒數
  volatile long humanMaxSeconds;  // 設定的最大秒數
  volatile int  soilPercent;      // 土壤濕度 %
  volatile bool isSoilDry;        // 土壤狀態
  volatile unsigned long stateStartTime;
  
  // AI 相關變數
  Persona currentPersona;
  char aiMessageLine1[17];    // LCD 第1行緩衝區
  char aiMessageLine2[17];    // LCD 第2行緩衝區
  volatile bool isAiUpdating; // AI 是否正在思考中
};

// 初始化資料
AppData Data = {180, 180, 0, false, 0, P_ROASTER, "System Init...", "Waiting AI...", false};

// 硬體物件
U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, PIN_OLED_CLK, PIN_OLED_MOSI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BluetoothSerial SerialBT;

// 事件旗標 (Task 間溝通)
volatile bool reqHumanRefill = false;
volatile bool reqAiUpdate = false; // 通知 TaskAI 去呼叫 API

// **********************************************************
// [VIEW] 繪圖與動畫模組 (精簡版，保留核心動畫)
// **********************************************************
void drawThickLine(int x1, int y1, int x2, int y2) {
  u8g2.drawLine(x1, y1, x2, y2); u8g2.drawLine(x1+1, y1, x2+1, y2);
  u8g2.drawLine(x1, y1+1, x2, y2+1); u8g2.drawLine(x1+1, y1+1, x2+1, y2+1);
}

void view_WaitBT(unsigned long t) {
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  if ((t/500)%2==0) u8g2.drawGlyph(54, 40, 0x231B); else u8g2.drawGlyph(54, 40, 0x23F3); 
  u8g2.setFont(u8g2_font_ncenB08_tr); u8g2.drawStr(25, 60, "Waiting BT...");
}

void view_Idle(unsigned long t) {
  int eyeH = (t % 3000 < 150) ? 1 : 12;
  u8g2.drawFilledEllipse(44, 30, 10, eyeH); u8g2.drawFilledEllipse(84, 30, 10, eyeH);
  u8g2.drawLine(60, 50, 68, 50);
}

void view_PlantSad(unsigned long t) {
  u8g2.drawLine(34, 30, 54, 30); u8g2.drawLine(74, 30, 94, 30);
  u8g2.drawCircle(64, 52, 6, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT); 
  int tearY = 40 + ((t / 50) % 20); 
  u8g2.drawDisc(40, tearY, 3); u8g2.drawDisc(88, tearY, 3);
}

void view_HumanPanic(unsigned long t) {
  u8g2.drawCircle(44, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(44, 30, 3);
  u8g2.drawCircle(84, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(84, 30, 3);
  int y = 50;
  if ((t / 100) % 2 == 0) { u8g2.drawLine(55, y, 65, y+3); u8g2.drawLine(65, y+3, 75, y); }
  else { u8g2.drawLine(55, y+3, 65, y); u8g2.drawLine(65, y, 75, y+3); }
}

void view_Demon(unsigned long t) {
  drawThickLine(30, 35, 55, 45); drawThickLine(98, 35, 73, 45); 
  u8g2.drawDisc(50, 50, 4); u8g2.drawDisc(78, 50, 4); 
  u8g2.drawBox(44, 60, 40, 4); 
}

void view_PartyPlant(unsigned long now, unsigned long startT) {
  unsigned long elapsed = now - startT;
  int cx = 64; int cy = 32;
  if (elapsed < 2500) { // Spin
    float angle = (now / 100.0);
    cx = 64 + (int)(cos(angle) * 10); cy = 32 + (int)(sin(angle) * 10);
    u8g2.drawCircle(cx-15, cy, 10, U8G2_DRAW_ALL); u8g2.drawCircle(cx+15, cy, 10, U8G2_DRAW_ALL);
    u8g2.drawCircle(cx, cy+15, 12, U8G2_DRAW_LOWER_RIGHT|U8G2_DRAW_LOWER_LEFT);
  } else { // Heart
    u8g2.setFont(u8g2_font_unifont_t_symbols); u8g2.drawGlyph(60, 40, 0x2665);
  }
}

void view_PartyHuman(unsigned long now, unsigned long startT) {
   u8g2.setFont(u8g2_font_ncenB10_tr); u8g2.drawStr(25, 62, "ENERGY UP!");
   u8g2.drawCircle(64, 30, 20, U8G2_DRAW_ALL);
}

// **********************************************************
// [TASK 1] TaskAI - 負責 WiFi 連線與 Gemini API 呼叫
// **********************************************************
void TaskAI(void *pvParameters) {
  (void) pvParameters;

  // 1. 初始化 WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  
  // 2. 設定 HTTPS Client (不驗證憑證以簡化流程)
  WiFiClientSecure client;
  client.setInsecure(); 

  for (;;) {
    // 等待觸發旗標
    if (reqAiUpdate && WiFi.status() == WL_CONNECTED) {
      Data.isAiUpdating = true; // 標記為正在更新(顯示 Loading)
      
      // A. 準備狀態描述字串
      String stateStr = "";
      switch(currentState) {
        case S_IDLE: stateStr = "S_IDLE (Peaceful)"; break;
        case S_WARN_PLANT: stateStr = "S_WARN_PLANT (Plant Thirsty)"; break;
        case S_WARN_HUMAN: stateStr = "S_WARN_HUMAN (Human Thirsty)"; break;
        case S_CRITICAL: stateStr = "S_CRITICAL (Both Dying)"; break;
        default: stateStr = "NORMAL"; break;
      }

      // B. 構建 JSON Payload (使用 ArduinoJson)
      // 對應 Python: payload = { "contents": [{ "parts": [{"text": prompt_text}] }] }
      JsonDocument doc;
      JsonObject content = doc["contents"].add();
      JsonObject parts = content["parts"].add();
      
      // 構建 Prompt (包含人格與限制)
      String prompt = "[ROLE]: " + String(SYSTEM_INSTRUCTIONS[Data.currentPersona]) + "\n";
      prompt += "[DATA]: State=" + stateStr + ", Soil=" + String(Data.soilPercent) + "%, Timer=" + String(Data.humanTimer/60) + "m.\n";
      prompt += "[TASK]: Write for 16x2 LCD.\n";
      prompt += "[STRICT CONSTRAINTS]:\n";
      prompt += "1. MAX 35 CHARS TOTAL.\n";
      prompt += "2. MAX 6 WORDS.\n";
      prompt += "3. NO Emojis.\n";
      prompt += "4. If 2 sentences, separate with space.";
      
      parts["text"] = prompt;

      String jsonString;
      serializeJson(doc, jsonString);

      // C. 發送 HTTP POST
      HTTPClient http;
      String url = "https://generativelanguage.googleapis.com/v1beta/models/" + String(GEMINI_MODEL) + ":generateContent?key=" + String(API_KEY);
      
      http.begin(client, url);
      http.addHeader("Content-Type", "application/json");
      
      int httpCode = http.POST(jsonString);
      
      // D. 處理回應
      if (httpCode == 200) {
        String response = http.getString();
        
        JsonDocument resDoc;
        DeserializationError error = deserializeJson(resDoc, response);

        if (!error) {
          const char* aiText = resDoc["candidates"][0]["content"]["parts"][0]["text"];
          
          if (aiText) {
            String textStr = String(aiText);
            textStr.trim();
            textStr.replace("\n", " "); // 移除換行
            
            // 將回應文字分割到 LCD 兩行
            memset(Data.aiMessageLine1, 0, 17);
            memset(Data.aiMessageLine2, 0, 17);

            // 簡單分割邏輯：如果大於16字，找中間的空格切開
            int len = textStr.length();
            if (len <= 16) {
               textStr.toCharArray(Data.aiMessageLine1, 17);
            } else {
               int splitIdx = textStr.lastIndexOf(' ', 16); // 在前16字中找最後一個空格
               if (splitIdx == -1) splitIdx = 16; // 沒空格就硬切
               
               textStr.substring(0, splitIdx).toCharArray(Data.aiMessageLine1, 17);
               textStr.substring(splitIdx + 1).toCharArray(Data.aiMessageLine2, 17);
            }
          }
        }
      } else {
        String err = "API Err: " + String(httpCode);
        err.toCharArray(Data.aiMessageLine1, 17);
      }
      
      http.end();
      reqAiUpdate = false;      // 清除觸發旗標
      Data.isAiUpdating = false; // 結束更新狀態
    }
    
    // 避免過度佔用 CPU，每 500ms 檢查一次是否需要更新
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// **********************************************************
// [TASK 2] TaskLogic - 系統大腦與狀態判斷
// **********************************************************
void TaskLogic(void *pvParameters) {
  (void) pvParameters;
  bool lastSoilDry = true;
  SystemState lastState = S_WAIT_BT;
  unsigned long lastAiUpdateTime = 0; // 用於定時更新 AI

  for (;;) {
    // 1. 藍牙連線檢查
    if (!SerialBT.hasClient()) {
      currentState = S_WAIT_BT;
      vTaskDelay(200 / portTICK_PERIOD_MS);
      continue;
    }

    // 2. 讀取感測器
    int val = analogRead(PIN_SOIL);
    Data.soilPercent = map(constrain(val, 0, 4095), 4095, 0, 0, 100);
    Data.isSoilDry = (val > SOIL_DRY_THRESHOLD);

    // 3. 事件邊緣偵測
    bool eventPlantWatered = (lastSoilDry && !Data.isSoilDry); // 乾 -> 濕
    bool eventHumanRefill  = reqHumanRefill;
    if (reqHumanRefill) reqHumanRefill = false;
    lastSoilDry = Data.isSoilDry;

    unsigned long now = millis();

    // 4. FSM 狀態切換
    switch (currentState) {
      case S_WAIT_BT: 
        currentState = S_IDLE; 
        break;

      case S_PARTY_PLANT: // 慶祝中不切換，直到時間到
        if (now - Data.stateStartTime > TIME_PARTY_PLANT) currentState = S_IDLE;
        break;

      case S_PARTY_HUMAN: // 慶祝中不切換
        if (now - Data.stateStartTime > TIME_PARTY_HUMAN) currentState = S_IDLE;
        break;

      default: // 一般狀態 (IDLE, WARN, CRITICAL)
        if (eventHumanRefill) {
          currentState = S_PARTY_HUMAN;
          Data.stateStartTime = now;
        } else if (eventPlantWatered) {
          currentState = S_PARTY_PLANT;
          Data.stateStartTime = now;
        } else {
          // 倒數計時邏輯
          static int tick = 0;
          if (++tick >= 10) { 
            tick = 0; 
            if (Data.humanTimer > 0) Data.humanTimer--;
          }
          
          bool humanDry = (Data.humanTimer == 0);
          bool plantDry = Data.isSoilDry;

          if (humanDry && plantDry) currentState = S_CRITICAL;
          else if (humanDry)        currentState = S_WARN_HUMAN;
          else if (plantDry)        currentState = S_WARN_PLANT;
          else                      currentState = S_IDLE;
        }
        break;
    }

    // 5. AI 觸發邏輯 (關鍵修改)
    // 只有在穩定狀態 (非慶祝、非等待) 才呼叫 AI
    bool isStable = (currentState != S_PARTY_PLANT && currentState != S_PARTY_HUMAN && currentState != S_WAIT_BT);
    
    if (isStable) {
        // 條件A: 狀態改變了
        if (currentState != lastState) {
           reqAiUpdate = true;
        } 
        // 條件B: 同一狀態太久 (每 5 分鐘刷新一次閒聊)
        else if (now - lastAiUpdateTime > 300000) { 
           reqAiUpdate = true; 
           lastAiUpdateTime = now;
        }
    }
    
    lastState = currentState;
    vTaskDelay(100 / portTICK_PERIOD_MS); // Logic Loop 10Hz
  }
}

// **********************************************************
// [TASK 3] TaskDisplay - 負責 OLED 與 LCD
// **********************************************************
void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  int lcdUpdateCnt = 0;

  for (;;) {
    unsigned long now = millis();
    u8g2.clearBuffer();
    
    // A. OLED 動畫 (根據狀態繪製)
    switch (currentState) {
      case S_WAIT_BT:    view_WaitBT(now); break;
      case S_IDLE:       view_Idle(now); break;
      case S_WARN_PLANT: view_PlantSad(now); break;
      case S_WARN_HUMAN: view_HumanPanic(now); break;
      case S_CRITICAL:   view_Demon(now); break;
      case S_PARTY_PLANT: view_PartyPlant(now, Data.stateStartTime); break;
      case S_PARTY_HUMAN: view_PartyHuman(now, Data.stateStartTime); break;
    }
    u8g2.sendBuffer();

    // B. LCD 文字 (顯示 AI 內容)
    if (++lcdUpdateCnt > 5) { // 降頻更新 LCD
      lcdUpdateCnt = 0;
      
      // 右上角顯示 AI 思考中的小點
      lcd.setCursor(15, 0); 
      lcd.print(Data.isAiUpdating ? "." : " ");

      // 特殊狀態優先顯示固定文字 (反應較快)
      if (currentState == S_PARTY_PLANT) {
        lcd.setCursor(0, 0); lcd.print("WOOO! SPINNING! ");
        lcd.setCursor(0, 1); lcd.print("Yummy Water!!!  ");
      } 
      else if (currentState == S_PARTY_HUMAN) {
        lcd.setCursor(0, 0); lcd.print("HUMAN REFILLING!");
        lcd.setCursor(0, 1); lcd.print("ENERGY RESTORED ");
      }
      else if (currentState == S_WAIT_BT) {
        lcd.setCursor(0, 0); lcd.print("Pair Bluetooth..");
        lcd.setCursor(0, 1); lcd.print("WiFi Connecting.");
      }
      else {
        // 一般狀態：顯示 AI 生成的文字
        if (strlen(Data.aiMessageLine1) == 0 && !Data.isAiUpdating) {
           // 剛開機還沒連到網
           lcd.setCursor(0, 0); lcd.print("AI System Init..");
           lcd.setCursor(0, 1); lcd.print("Connecting...   ");
        } else {
           // 顯示 AI 回應 (補空白清除舊字)
           lcd.setCursor(0, 0); lcd.print(Data.aiMessageLine1); 
           for(int i=strlen(Data.aiMessageLine1); i<16; i++) lcd.print(" ");
           
           lcd.setCursor(0, 1); lcd.print(Data.aiMessageLine2);
           for(int i=strlen(Data.aiMessageLine2); i<16; i++) lcd.print(" ");
        }
      }
    }
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}

// **********************************************************
// [TASK 4] TaskSound - 負責音效
// **********************************************************
void TaskSound(void *pvParameters) {
  (void) pvParameters;
  SystemState lastState = S_WAIT_BT;
  
  for (;;) {
    // One-shot 音效
    if (currentState != lastState) {
      if (currentState == S_PARTY_HUMAN) {
         tone(PIN_BUZZER, NOTE_G4); vTaskDelay(80); tone(PIN_BUZZER, NOTE_C5); vTaskDelay(80);
         tone(PIN_BUZZER, NOTE_E5); vTaskDelay(80); tone(PIN_BUZZER, NOTE_G5); vTaskDelay(80);
         noTone(PIN_BUZZER);
      }
      else if (currentState == S_PARTY_PLANT) {
         tone(PIN_BUZZER, NOTE_C6); vTaskDelay(100); tone(PIN_BUZZER, NOTE_C6); vTaskDelay(100);
         noTone(PIN_BUZZER);
      }
      lastState = currentState;
    }

    // Loop 音效 (背景音)
    switch (currentState) {
      case S_WARN_HUMAN: 
        tone(PIN_BUZZER, 800); vTaskDelay(200); tone(PIN_BUZZER, 1200); vTaskDelay(200); 
        break;
      case S_CRITICAL: 
        tone(PIN_BUZZER, 1100); vTaskDelay(70); tone(PIN_BUZZER, 1400); vTaskDelay(70); 
        break;
      case S_WARN_PLANT: 
        tone(PIN_BUZZER, NOTE_G5); vTaskDelay(400); noTone(PIN_BUZZER); vTaskDelay(2500); 
        break;
      default: 
        noTone(PIN_BUZZER); vTaskDelay(100); 
        break;
    }
  }
}

// **********************************************************
// [TASK 5] TaskInput - 處理藍牙指令
// **********************************************************
void TaskInput(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (SerialBT.hasClient()) {
      // 1. 藍牙指令解析
      if (SerialBT.available()) {
        String s = SerialBT.readStringUntil('\n');
        s.trim();
        if (s.length() > 0) {
          
          // 指令 A: 喝水
          if (s.equalsIgnoreCase("Water")) {
            Data.humanTimer = Data.humanMaxSeconds;
            reqHumanRefill = true;
          } 
          // 指令 B: 切換人格 (新增功能)
          else if (s.equalsIgnoreCase("Persona")) {
            int nextP = (int)Data.currentPersona + 1;
            if (nextP > 3) nextP = 0;
            Data.currentPersona = (Persona)nextP;
            
            // 強制 AI 更新以顯示新人格
            reqAiUpdate = true;
            
            // 回傳給手機
            SerialBT.print("Persona switched to: ");
            SerialBT.println(PERSONA_NAMES[Data.currentPersona]);
          }
          // 指令 C: 設定時間 (數字)
          else {
            float m = s.toFloat();
            if (m > 0) { 
              Data.humanMaxSeconds = (long)(m*60); 
              Data.humanTimer = Data.humanMaxSeconds; 
              reqHumanRefill = true;
            }
          }
        }
      }
      // 2. 實體按鈕
      if (digitalRead(PIN_BUTTON) == LOW) {
        Data.humanTimer = Data.humanMaxSeconds;
        reqHumanRefill = true;
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// **********************************************************
// [SETUP] 系統初始化
// **********************************************************
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  // LCD 初始化
  lcd.init(); lcd.backlight();
  lcd.clear(); lcd.print("System Booting..");
  
  // OLED 初始化
  u8g2.begin(); u8g2.setDrawColor(1);
  
  // IO 設定
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // 藍牙初始化
  SerialBT.begin("Smart_Plant_AI_v2");

  delay(1000);

  // 創建 RTOS 任務 (調整堆疊大小以適應 AI 需求)
  // Stack Size: AI 需要較大 (8K)，其他維持預設
  xTaskCreate(TaskDisplay, "Disp", 4096, NULL, 3, NULL);
  xTaskCreate(TaskLogic,   "Logic",4096, NULL, 2, NULL); // 邏輯稍微加大
  xTaskCreate(TaskInput,   "Input",4096, NULL, 1, NULL);
  xTaskCreate(TaskSound,   "Sound",2048, NULL, 1, NULL);
  xTaskCreate(TaskAI,      "AI",   8192, NULL, 1, NULL); // AI 專用任務
}

void loop() { 
  // FreeRTOS 使用 Task 運作，Loop 留空即可
  vTaskDelay(1000); 
}