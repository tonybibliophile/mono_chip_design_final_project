//Networkhelper.cpp
#include <WiFi.h>
#include "Config.h"
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd; 

void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Step 1: WiFi");
  lcd.setCursor(0, 1); lcd.print("Connecting...");
  
  // ★ 1. 強制設定為 STA 模式 (不讓它變成熱點)
  WiFi.mode(WIFI_STA);
  // ★ 2. 關鍵指令：關閉 WiFi 省電模式 (全力搶網速，減少與藍牙衝突)
  WiFi.setSleep(false);

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("\nConnecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(dots, 1); lcd.print(".");
    dots++;
    if(dots > 15) { dots = 0; lcd.setCursor(0, 1); lcd.print("                "); }
  }

  Serial.println("\nWiFi Connected!");
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi: OK!");
  lcd.setCursor(0, 1); lcd.print("Step 2: App BT");
  delay(3000);
}

bool isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}