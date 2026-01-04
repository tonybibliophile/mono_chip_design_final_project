//Peripherals.cpp
#include "Peripherals.h"
#include "Config.h"
#include "Shared.h"

U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, PIN_OLED_CLK, PIN_OLED_MOSI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2); 
BluetoothSerial SerialBT;

void initPeripherals() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.print("System Init...");
  
  u8g2.begin(); u8g2.setDrawColor(1);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  ledcSetup(BUZZER_CHANNEL, 5000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
  
  //SerialBT.begin("Smart_Plant_AI");
}

void playTone(int freq, int durationMs) {
  if (freq > 0) ledcWriteTone(BUZZER_CHANNEL, freq); 
  else ledcWriteTone(BUZZER_CHANNEL, 0);
  if (durationMs > 0) vTaskDelay(durationMs / portTICK_PERIOD_MS);
}

// --- 動畫繪圖區 (把 V4 的靈魂裝回來) ---

// 畫粗線工具
void drawThickLine(int x1, int y1, int x2, int y2) {
  u8g2.drawLine(x1, y1, x2, y2); u8g2.drawLine(x1+1, y1, x2+1, y2);
  u8g2.drawLine(x1, y1+1, x2, y2+1); u8g2.drawLine(x1+1, y1+1, x2+1, y2+1);
}

// 1. 等待藍牙 (沙漏動畫)
void view_WaitBT(unsigned long t) {
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  // 每 0.5 秒切換一次圖案
  if ((t/500)%2==0) u8g2.drawGlyph(54, 40, 0x231B); // 沙漏
  else u8g2.drawGlyph(54, 40, 0x23F3); 
  u8g2.setFont(u8g2_font_ncenB08_tr); u8g2.drawStr(25, 60, "Connect BT...");
}

// 2. 正常待機 (會眨眼)
void view_Idle(unsigned long t) {
  // 每 3 秒眨眼一次 (高度變 1)
  int eyeH = (t % 3000 < 150) ? 1 : 12; 
  u8g2.drawFilledEllipse(44, 30, 10, eyeH); 
  u8g2.drawFilledEllipse(84, 30, 10, eyeH);
  u8g2.drawLine(60, 50, 68, 50); // 微笑
}

// 3. 植物渴了 (會流淚)
void view_PlantSad(unsigned long t) {
  u8g2.drawLine(34, 30, 54, 30); // 左眼閉
  u8g2.drawLine(74, 30, 94, 30); // 右眼閉
  u8g2.drawCircle(64, 52, 6, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT); // 苦瓜嘴
  // 眼淚落下動畫
  int tearY = 40 + ((t / 50) % 20); 
  u8g2.drawDisc(40, tearY, 3); u8g2.drawDisc(88, tearY, 3);
}

// 4. 人類渴了 (驚恐搖晃)
void view_HumanPanic(unsigned long t) {
  u8g2.drawCircle(44, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(44, 30, 3);
  u8g2.drawCircle(84, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(84, 30, 3);
  int y = 50; 
  // 嘴巴抖動
  if ((t / 100) % 2 == 0) { u8g2.drawLine(55, y, 65, y+3); u8g2.drawLine(65, y+3, 75, y); }
  else { u8g2.drawLine(55, y+3, 65, y); u8g2.drawLine(65, y, 75, y+3); }
  
  // 警示符號閃爍
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t); 
  if ((t / 200) % 2 == 0) u8g2.drawGlyph(100, 20, 64);
}

// 5. 派對模式 (旋轉跳躍)
void view_Party(unsigned long now, unsigned long startT) {
  unsigned long elapsed = now - startT;
  int cx = 64; int cy = 32;
  
  // 階段 1: 旋轉 (0~2.5秒)
  if (elapsed < 2500) {
    float angle = (now / 100.0);
    cx = 64 + (int)(cos(angle) * 10); cy = 32 + (int)(sin(angle) * 10);
    u8g2.drawCircle(cx-15, cy, 10, U8G2_DRAW_ALL); u8g2.drawCircle(cx+15, cy, 10, U8G2_DRAW_ALL);
    u8g2.drawCircle(cx, cy+15, 12, U8G2_DRAW_LOWER_RIGHT|U8G2_DRAW_LOWER_LEFT);
  } 
  // 階段 2: 跳躍 (2.5秒後)
  else {
    float jump = abs(sin(now / 80.0)) * 15;
    cy = 45 - (int)jump;
    u8g2.drawEllipse(cx-20, cy, 8, 10); u8g2.drawEllipse(cx+20, cy, 8, 10);
    u8g2.drawCircle(cx, cy+15, 5, U8G2_DRAW_ALL);
    // 星星閃爍
    u8g2.setFont(u8g2_font_unifont_t_symbols);
    if ((now/100)%2==0) u8g2.drawGlyph(5, 20, 0x2605); else u8g2.drawGlyph(110, 20, 0x2605);
  }
}

// 6. 惡魔模式 (CRITICAL)
void view_Demon(unsigned long t) {
  drawThickLine(30, 35, 55, 45); drawThickLine(98, 35, 73, 45); // 怒眉
  u8g2.drawDisc(50, 50, 4); u8g2.drawDisc(78, 50, 4); 
  u8g2.drawBox(44, 60, 40, 4); // 扁嘴
  
  // 火焰閃爍
  if ((t / 100) % 2 == 0) { 
     u8g2.setFont(u8g2_font_open_iconic_thing_2x_t);
     u8g2.drawGlyph(5, 25, 78); u8g2.drawGlyph(105, 25, 78);
  }
}

// --- 繪圖總管 (根據狀態切換動畫) ---
void drawCurrentStateView(unsigned long now) {
  u8g2.clearBuffer();
  
  switch(currentState) {
    case S_WAIT_BT:    view_WaitBT(now); break;
    case S_IDLE:       view_Idle(now); break;
    case S_WARN_PLANT: view_PlantSad(now); break;
    case S_WARN_HUMAN: view_HumanPanic(now); break;
    case S_PARTY_PLANT: 
    case S_PARTY_HUMAN:
       view_Party(now, Data.stateStartTime); break; // 兩種派對共用動畫
    case S_CRITICAL:   view_Demon(now); break;
  }
  
  // 顯示當前個性 (右上角小字)
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(90, 8);
  switch(currentPersonality) {
    case PER_KIND:    u8g2.print("KIND"); break;
    case PER_MEAN:    u8g2.print("MEAN"); break;
    case PER_PLAYFUL: u8g2.print("PLAY"); break;
  }
  
  u8g2.sendBuffer();
}

// --- LCD 顯示總管 (維持不變) ---
void updateLCDText(unsigned long now) {
  static int cnt = 0;
  if (++cnt > 5) { cnt = 0;
    
    // A. 優先顯示 AI
    if (Data.showAiMsg) {
      if (now - Data.msgStartTime < SHOW_AI_DURATION) {
        lcd.setCursor(0, 0); lcd.print("AI:             ");
        lcd.setCursor(4, 0); lcd.print(Data.aiMessage.substring(0, 12));
        lcd.setCursor(0, 1); lcd.print(Data.aiMessage.substring(12, 28));
        return; 
      } else {
        Data.showAiMsg = false;
        lcd.clear();
      }
    }

    // B. 標準顯示
    lcd.setCursor(0, 0);
    switch (currentState) {
      case S_WAIT_BT: lcd.print("Step 2: App BT  "); break; // 提示連藍牙
      case S_IDLE:    lcd.print("Plant: Happy :) "); break;
      case S_WARN_PLANT: lcd.print("Plant: THIRSTY! "); break;
      case S_WARN_HUMAN: lcd.print("Human: DRINK!   "); break;
      case S_PARTY_PLANT:lcd.print("Watering...     "); break;
      case S_PARTY_HUMAN:lcd.print("Refilling...    "); break;
      case S_CRITICAL:   lcd.print("CRITICAL ERROR! "); break;
    }
    
    lcd.setCursor(0, 1);
    int m = Data.humanTimer / 60; int s = Data.humanTimer % 60;
    lcd.print("Timer: "); lcd.print(m); lcd.print(":"); 
    if(s<10) lcd.print("0"); lcd.print(s); lcd.print("     ");
  }
}