#include "Shared.h"
#include "Config.h"
#include "Melodies.h" 
#include <U8g2lib.h>
#include <LiquidCrystal_I2C.h>

U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, PIN_OLED_CLK, PIN_OLED_MOSI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

void playTone(int freq, int durationMs) {
  if (freq > 0) tone(PIN_BUZZER, freq);
  else noTone(PIN_BUZZER);
  if (durationMs > 0) vTaskDelay(durationMs / portTICK_PERIOD_MS);
}

void Peripherals_Init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init(); lcd.backlight();
  lcd.clear(); lcd.print("System Init...");
  u8g2.begin(); u8g2.setDrawColor(1);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SOIL, INPUT);
}

// --- OLED 動畫 (簡化顯示，請保留你原本完整的動畫函式) ---
// 這裡只列出函式頭，請用你原本的動畫內容，或者直接使用上面完整版提供的
void view_WaitBT(unsigned long t) { u8g2.setFont(u8g2_font_unifont_t_symbols); if ((t/500)%2==0) u8g2.drawGlyph(54, 40, 0x231B); else u8g2.drawGlyph(54, 40, 0x23F3); u8g2.setFont(u8g2_font_ncenB08_tr); u8g2.drawStr(25, 60, "Connect BT..."); }
void view_Idle(unsigned long t) { int eyeH = (t % 3000 < 150) ? 1 : 12; u8g2.drawFilledEllipse(44, 30, 10, eyeH); u8g2.drawFilledEllipse(84, 30, 10, eyeH); u8g2.drawLine(60, 50, 68, 50); }
void view_PlantSad(unsigned long t) { u8g2.drawLine(34, 30, 54, 30); u8g2.drawLine(74, 30, 94, 30); u8g2.drawCircle(64, 52, 6, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT); int tearY = 40 + ((t / 50) % 20); u8g2.drawDisc(40, tearY, 3); u8g2.drawDisc(88, tearY, 3); }
void view_HumanPanic(unsigned long t) { u8g2.drawCircle(44, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(44, 30, 3); u8g2.drawCircle(84, 30, 12, U8G2_DRAW_ALL); u8g2.drawDisc(84, 30, 3); int y = 50; if ((t / 100) % 2 == 0) { u8g2.drawLine(55, y, 65, y+3); u8g2.drawLine(65, y+3, 75, y); } else { u8g2.drawLine(55, y+3, 65, y); u8g2.drawLine(65, y, 75, y+3); } u8g2.setFont(u8g2_font_open_iconic_weather_2x_t); if ((t / 200) % 2 == 0) u8g2.drawGlyph(100, 20, 64); }
void view_Party(unsigned long now, unsigned long startT) { unsigned long elapsed = now - startT; int cx = 64; int cy = 32; if (elapsed < 2500) { float angle = (now / 100.0); cx = 64 + (int)(cos(angle) * 10); cy = 32 + (int)(sin(angle) * 10); u8g2.drawCircle(cx-15, cy, 10, U8G2_DRAW_ALL); u8g2.drawCircle(cx+15, cy, 10, U8G2_DRAW_ALL); u8g2.drawCircle(cx, cy+15, 12, U8G2_DRAW_LOWER_RIGHT|U8G2_DRAW_LOWER_LEFT); } else { float jump = abs(sin(now / 80.0)) * 15; cy = 45 - (int)jump; u8g2.drawEllipse(cx-20, cy, 8, 10); u8g2.drawEllipse(cx+20, cy, 8, 10); u8g2.drawCircle(cx, cy+15, 5, U8G2_DRAW_ALL); u8g2.setFont(u8g2_font_unifont_t_symbols); if ((now/100)%2==0) u8g2.drawGlyph(5, 20, 0x2605); else u8g2.drawGlyph(110, 20, 0x2605); } }
void view_Demon(unsigned long t) { int dx = (t % 100 < 50) ? -2 : 2; int dy = (t % 150 < 75) ? -1 : 1; u8g2.drawTriangle(40+dx, 25+dy, 30+dx, 5+dy, 50+dx, 25+dy); u8g2.drawTriangle(88+dx, 25+dy, 98+dx, 5+dy, 78+dx, 25+dy); u8g2.drawTriangle(35+dx, 30+dy, 60+dx, 30+dy, 55+dx, 42+dy); u8g2.drawTriangle(68+dx, 30+dy, 93+dx, 30+dy, 73+dx, 42+dy); int pupilX = (t % 1000 < 500) ? -2 : 2; u8g2.setDrawColor(0); u8g2.drawDisc(48+dx+pupilX, 34+dy, 2); u8g2.drawDisc(80+dx+pupilX, 34+dy, 2); u8g2.setDrawColor(1); if ((t / 200) % 2 == 0) { u8g2.drawBox(44+dx, 50+dy, 40, 12); u8g2.setDrawColor(0); u8g2.drawTriangle(44+dx, 50+dy, 50+dx, 50+dy, 47+dx, 56+dy); u8g2.drawTriangle(77+dx, 50+dy, 83+dx, 50+dy, 80+dx, 56+dy); u8g2.drawTriangle(54+dx, 62+dy, 60+dx, 62+dy, 57+dx, 56+dy); u8g2.drawTriangle(67+dx, 62+dy, 73+dx, 62+dy, 70+dx, 56+dy); u8g2.setDrawColor(1); } else { u8g2.drawBox(44+dx, 55+dy, 40, 5); u8g2.setDrawColor(0); u8g2.drawLine(44+dx, 57+dy, 84+dx, 57+dy); u8g2.drawLine(54+dx, 55+dy, 54+dx, 60+dy); u8g2.drawLine(64+dx, 55+dy, 64+dx, 60+dy); u8g2.drawLine(74+dx, 55+dy, 74+dx, 60+dy); u8g2.setDrawColor(1); } if ((t / 100) % 2 == 0) { u8g2.setFont(u8g2_font_open_iconic_thing_2x_t); u8g2.drawGlyph(0, 45, 78); u8g2.drawGlyph(110, 45, 78); } }

// 任務主體
void Task_Display(void *pv) {
  int displayState = 0; // 0=Normal, 1=Loading, 2=Result

  for (;;) {
    unsigned long now = millis();

    // 1. OLED 更新
    u8g2.clearBuffer();
    switch(currentState) {
      case S_WAIT_BT:    view_WaitBT(now); break;
      case S_IDLE:       view_Idle(now); break;
      case S_WARN_PLANT: view_PlantSad(now); break;
      case S_WARN_HUMAN: view_HumanPanic(now); break;
      case S_PARTY_PLANT: 
      case S_PARTY_HUMAN: view_Party(now, AppData.stateStartTime); break;
      case S_CRITICAL:   view_Demon(now); break;
    }
    u8g2.setFont(u8g2_font_5x7_tr); u8g2.setCursor(90, 8);
    if(currentPersonality == PER_KIND) u8g2.print("KIND");
    else if(currentPersonality == PER_MEAN) u8g2.print("MEAN");
    else u8g2.print("PLAY");
    u8g2.sendBuffer();

    // 2. LCD 更新 (AI 優先顯示邏輯)
    static unsigned long lastLcdUpdate = 0;
    if (now - lastLcdUpdate > 200) {
      lastLcdUpdate = now;

      // 優先級 1: 顯示 "AI Thinking..."
      if (AppData.isAiLoading) {
          if (displayState != 1) { lcd.clear(); displayState = 1; }
          lcd.setCursor(0, 0); lcd.print("AI Thinking...");
          lcd.setCursor(0, 1); lcd.print("Please wait...");
      }
      // 優先級 2: 顯示 AI 結果
      else if (AppData.showAiMsg && (now - AppData.msgStartTime < SHOW_AI_DURATION)) {
          if (displayState != 2) { lcd.clear(); displayState = 2; }
          lcd.setCursor(0, 0); 
          // lcd.print("AI: ");
          if(AppData.aiMessage.length() > 0) lcd.print(AppData.aiMessage.substring(0, 12));
          lcd.setCursor(0, 1); 
          if(AppData.aiMessage.length() > 12) lcd.print(AppData.aiMessage.substring(12, 28));
      }
      // 優先級 3: 正常顯示
      else {
          AppData.showAiMsg = false; 
          if (displayState != 0) { lcd.clear(); displayState = 0; }
          lcd.setCursor(0, 0);
          switch (currentState) {
            case S_WAIT_BT: lcd.print("Step 1: App BT  "); break;
            case S_IDLE:    lcd.print("Plant: Happy :) "); break;
            case S_WARN_PLANT: lcd.print("Plant: THIRSTY! "); break;
            case S_WARN_HUMAN: lcd.print("Human: DRINK!   "); break;
            case S_PARTY_PLANT:lcd.print("Watering...     "); break;
            case S_PARTY_HUMAN:lcd.print("Refilling...    "); break;
            case S_CRITICAL:   lcd.print("CRITICAL ERROR! "); break;
          }
          lcd.setCursor(0, 1);
          int m = AppData.humanTimer / 60; int s = AppData.humanTimer % 60;
          lcd.print("Timer: "); lcd.print(m); lcd.print(":"); 
          if(s<10) lcd.print("0"); lcd.print(s); lcd.print("      ");
      }
    }
    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}

void Task_Sound(void *pv) {
  SystemState lastStateSound = S_WAIT_BT;
  for (;;) {
    if (currentState != lastStateSound) {
       if (currentState == S_IDLE && lastStateSound != S_WAIT_BT) {
           playTone(NOTE_E6, 150); noTone(PIN_BUZZER);
       }
       lastStateSound = currentState;
    }
    switch (currentState) {
      case S_CRITICAL:
          playTone(100, 80); playTone(0, 20); playTone(150, 80);  
          playTone(0, 20); playTone(100, 80);
          noTone(PIN_BUZZER); vTaskDelay(100 / portTICK_PERIOD_MS); 
          break;
      case S_WARN_HUMAN: 
          playTone(NOTE_C6, 100); playTone(0, 50); playTone(NOTE_C6, 100); 
          noTone(PIN_BUZZER); vTaskDelay(500 / portTICK_PERIOD_MS); 
          break;
      case S_WARN_PLANT: 
          playTone(NOTE_F4, 200); playTone(NOTE_E4, 200); playTone(NOTE_D4, 400); 
          noTone(PIN_BUZZER); vTaskDelay(1000 / portTICK_PERIOD_MS); 
          break;
      case S_PARTY_PLANT:
      case S_PARTY_HUMAN:
          playTone(NOTE_C5, 100); playTone(NOTE_E5, 100); 
          playTone(NOTE_G5, 100); playTone(NOTE_C6, 200);
          noTone(PIN_BUZZER); vTaskDelay(200 / portTICK_PERIOD_MS); 
          break;
      default:
          noTone(PIN_BUZZER); vTaskDelay(100 / portTICK_PERIOD_MS); 
          break;
    }
  }
}