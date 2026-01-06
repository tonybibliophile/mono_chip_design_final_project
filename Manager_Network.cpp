#include "Shared.h"
#include "Config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

void Network_Init() {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    Serial.print("[WiFi] Connecting");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500); Serial.print("."); retry++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        AppData.wifiConnected = true;
        Serial.println("\n[WiFi] Connected!");
    } else {
        AppData.wifiConnected = false;
        Serial.println("\n[WiFi] Failed (Skip)");
    }
}

String _parseGemini(String jsonResponse) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonResponse);
    if (!error) {
        // 檢查是否被安全攔截
        if (!doc.containsKey("candidates")) {
             if (doc.containsKey("error")) {
                 Serial.print("[API Error] "); Serial.println((const char*)doc["error"]["message"]);
             }
             return "API Error";
        }

        const char* text = doc["candidates"][0]["content"]["parts"][0]["text"];
        if (text) {
            String t = String(text);
            t.replace("\n", " "); t.replace("\r", " "); t.trim();
            if (t.length() > 64) return t.substring(0, 64);
            return t;
        } else {
            // 如果 text 是 null，通常是因為被 Block 掉了
            const char* reason = doc["candidates"][0]["finishReason"];
            Serial.print("[AI] No text. Reason: "); Serial.println(reason);
            return "Blocked: " + String(reason);
        }
    }
    return "JSON Error";
}

void Task_Network(void *pv) {
    Network_Init();

    while (true) {
        if (AppData.requestAI) {
            // 1. 設定讀取中 (讓 LCD 顯示 Thinking)
            AppData.isAiLoading = true; 
            Serial.println("[AI] Request Start...");

            if (WiFi.status() == WL_CONNECTED) {
                AppData.wifiConnected = true;
                WiFiClientSecure client; client.setInsecure();
                HTTPClient http;
                String url = String(API_URL) + String(GEMINI_API_KEY);
                
                String role = "You are a plant. 1 short sentence(less than 20 charaters in cluding space): "; 
                if (currentPersonality == PER_MEAN) role = "You are a RUDE plant. Roast me in 1 sentence(less than 20 charaters in cluding space): ";
                else if (currentPersonality == PER_PLAYFUL) role = "You are a FUNNY plant. Joke in 1 sentence(less than 20 charaters in cluding space): ";
                
                // 【關鍵修正】加入 safetySettings: BLOCK_NONE 防止惡魔對話被擋
                // 這樣 Google 就不會因為這句話太兇而回傳 null
                String payload = "{";
                payload += "\"contents\":[{\"parts\":[{\"text\":\"" + role + AppData.aiPromptType + "\"}]}],";
                payload += "\"safetySettings\": [";
                payload += "{\"category\": \"HARM_CATEGORY_HARASSMENT\", \"threshold\": \"BLOCK_NONE\"},";
                payload += "{\"category\": \"HARM_CATEGORY_HATE_SPEECH\", \"threshold\": \"BLOCK_NONE\"},";
                payload += "{\"category\": \"HARM_CATEGORY_SEXUALLY_EXPLICIT\", \"threshold\": \"BLOCK_NONE\"},";
                payload += "{\"category\": \"HARM_CATEGORY_DANGEROUS_CONTENT\", \"threshold\": \"BLOCK_NONE\"}";
                payload += "],";
                payload += "\"generationConfig\": {\"maxOutputTokens\": 25}";
                payload += "}";
                
                if (http.begin(client, url)) {
                    http.addHeader("Content-Type", "application/json");
                    int code = http.POST(payload);
                    if (code == 200) {
                        String res = http.getString();
                        AppData.aiMessage = _parseGemini(res);
                        AppData.showAiMsg = true;
                        AppData.msgStartTime = millis();
                    } else {
                        Serial.print("[AI] HTTP Error: "); Serial.println(code);
                        AppData.aiMessage = "Err: " + String(code);
                        AppData.showAiMsg = true;
                        AppData.msgStartTime = millis();
                    }
                    http.end();
                }
            } else {
                AppData.aiMessage = "No WiFi";
                AppData.showAiMsg = true;
                AppData.msgStartTime = millis();
            }
            
            // 2. 讀取結束
            AppData.isAiLoading = false; 
            AppData.requestAI = false; 
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}