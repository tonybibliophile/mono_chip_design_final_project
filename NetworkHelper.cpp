// Networkhelper.cpp (安全模式版本)
#include <WiFi.h>
#include "Config.h"

// 暫時移除外部顯示物件，避免干擾
// extern U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2; 

void connectWiFi() {
  Serial.print("\n[System] Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // 關閉省電模式，提高穩定性
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tryCount++;
    
    // 如果連線超過 30 次 (15秒) 還連不上，就先放棄，避免無限卡死
    if (tryCount > 30) {
      Serial.println("\n[Error] WiFi Connect Failed! Check SSID/PASS.");
      return; // 讓系統繼續往下跑，不要卡死在這裡
    }
  }

  Serial.println("\n[System] WiFi Connected!");
  Serial.print("[System] IP: ");
  Serial.println(WiFi.localIP());
}

bool isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}
// 其他 AI 相關函式保持不變...