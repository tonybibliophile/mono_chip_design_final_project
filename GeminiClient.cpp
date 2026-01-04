//GeminiClient.cpp
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h> // ★ 1. 必備函式庫

#include "Config.h"
#include "Shared.h"

extern bool isWiFiConnected(); 

String getSystemPrompt() {
  switch(currentPersonality) {
    case PER_MEAN: 
      return "You are a MEAN plant. Roast the user. Reply in English, strictly under 15 words about: ";
    case PER_PLAYFUL: 
      return "You are a PLAYFUL plant. Be excited! Reply in English, strictly under 15 words about: ";
    default: // PER_KIND
      return "You are a KIND plant. Give warm advice. Reply in English, strictly under 15 words about: ";
  }
}

String parseGeminiResponse(String jsonResponse) {
  if (jsonResponse == "Error" || jsonResponse == "No WiFi") return "AI Error";
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (!error) {
    const char* text = doc["candidates"][0]["content"]["parts"][0]["text"];
    if (text) {
      String cleanText = String(text);
      cleanText.replace("\n", " ");
      if (cleanText.length() > 32) cleanText = cleanText.substring(0, 32); 
      return cleanText;
    }
  }
  return "Parse Error";
}

void askGemini(String eventDescription) {
  if (!isWiFiConnected()) return;

  // ★ 2. 建立安全連線物件
  WiFiClientSecure client;
  // ★ 3. 關鍵修正：跳過憑證檢查 (這是剛剛測試成功的關鍵)
  client.setInsecure();

  HTTPClient http;
  
  // ★ 4. 將 client 傳入 begin 函式
  if (http.begin(client, String(API_URL) + String(GEMINI_API_KEY))) {
    http.addHeader("Content-Type", "application/json");

    String fullPrompt = getSystemPrompt() + eventDescription;
    String payload = "{\"contents\": [{\"parts\": [{\"text\": \"" + fullPrompt + "\"}]}]}";
    
    Serial.println("Asking Gemini...");
    int httpCode = http.POST(payload);
    
    if (httpCode == 200) {
      String finalMsg = parseGeminiResponse(http.getString());
      Serial.print("AI Says: "); Serial.println(finalMsg);
      
      // 更新顯示資料
      Data.aiMessage = finalMsg;
      Data.showAiMsg = true;
      Data.msgStartTime = millis();
    } else {
      Serial.print("HTTP Error: "); Serial.println(httpCode);
      Serial.println(http.getString()); // 印出錯誤原因以便除錯
    }
    http.end();
  } else {
    Serial.println("Unable to connect to Google Server");
  }
}