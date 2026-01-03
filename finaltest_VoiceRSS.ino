/**
 * Project: Smart Plant AI v4.0 (Audio Merged Edition)
 * Description: 整合雲端記憶 + Gemini AI + 修正後的藍牙語音 (無 OLED)
 */
//ESP32 Dev Module
//Tools->Partition scheme "Huge App, 3MB No OTA"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "BluetoothA2DPSource.h"

// **********************************************************
// [CONFIG] 使用者設定區
// **********************************************************
const char* WIFI_SSID = "WIFI ID";
const char* WIFI_PASS = "WIFI Password";    
const char* API_KEY   = "API"; //Google AI KEY 你自己換
const char* TTS_API_KEY = "f9d8462218ff4e7da10bcccbcd0dc8b2";  //Voice RSS的KEY，留著就好我用學校mail辦的沒差，相關設定在"downloadTTS(String text)"
const char* G_SCRIPT_URL = "https://script.google.com/macros/s/AKfycby74LL1Sj9drFx7NRC95bISBw9PODzohxX-BFaDsIcUJUbGZPdVhHRhiWToAy1pTvUxFQ/exec";
const char* GEMINI_MODEL = "gemini-2.5-flash"; 
const char* BT_SPEAKER_NAME = "BT earphone name"; // 請確認喇叭名稱，必須一模一樣，基本上都能連上啦

// **********************************************************
// [DATA] 資料結構 & 變數
// **********************************************************
enum Persona { P_ROASTER, P_SERGEANT, P_EMO, P_TSUNDERE };
const char* PERSONA_NAMES[] = {"Roaster", "Sergeant", "Emo", "Tsundere"};
const char* SYSTEM_INSTRUCTIONS[] = {
  "Mean plant. Insult user. Max 20 characters.",
  "Drill Sergeant. YELL COMMANDS. Max 20 chars.",
  "Sad phrases. Max 20 characters.",
  "Tsundere. Short & cute. Max 20 chars."
};
Persona currentPersona = P_ROASTER;
String currentState = "S_IDLE";
int mockSoilPercent = 60;
char lastAiResponse[128] = "";

// [AUDIO 變數]
BluetoothA2DPSource a2dp_source;
File ttsFile;
bool isPlaying = false;
bool isWarmingUp = true;
int silenceCounter = 0;
const int END_PADDING_FRAMES = 50;
bool wifiMode = true; // 追蹤目前是 WiFi 模式還是藍牙模式

// **********************************************************
// [CALLBACK] 藍牙音訊處理 (修正版：Mono -> Stereo)
// **********************************************************
int32_t get_sound_data(Frame *data, int32_t len) {
  if (!isPlaying) return 0;

  // 1. 暖身靜音
  if (isWarmingUp) {
      static int warmUpCount = 0;
      memset(data, 0, len * 4); 
      warmUpCount++;
      if (warmUpCount > 20) { 
          isWarmingUp = false;
          warmUpCount = 0;
      }
      return len;
  }

  // 2. 讀取檔案 (Mono 轉 Stereo)
  if (ttsFile && ttsFile.available()) {
      int32_t reqLen = len;
      if (reqLen > 256) reqLen = 256; // 限制緩衝大小

      size_t bytesNeeded = reqLen * 2; // Mono 只需要一半
      if (ttsFile.available() < bytesNeeded) {
         bytesNeeded = ttsFile.available();
         if (bytesNeeded % 2 != 0) bytesNeeded--; 
      }
      
      int16_t tempBuffer[512]; 
      int bytesRead = ttsFile.read((uint8_t*)tempBuffer, bytesNeeded);
      int samplesRead = bytesRead / 2;

      for (int i = 0; i < samplesRead; i++) {
          data[i].channel1 = tempBuffer[i]; // 左聲道
          data[i].channel2 = tempBuffer[i]; // 右聲道
      }
      
      if (samplesRead < len) {
          for (int i = samplesRead; i < len; i++) {
             data[i].channel1 = 0;
             data[i].channel2 = 0;
          }
      }
      return len;
  }

  // 3. 結尾靜音
  if (silenceCounter < END_PADDING_FRAMES) {
      memset(data, 0, len * 4); 
      silenceCounter++;
      return len;
  }

  // 4. 結束
  isPlaying = false;
  Serial.println("[Audio] 播放結束");
  // 這裡不自動切回 WiFi，等待下一次指令觸發重連
  return 0; 
}

// 藍牙狀態回調
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
     Serial.println("[BT] 喇叭已連線");
     isWarmingUp = true;
     silenceCounter = 0;
  }
}

// **********************************************************
// [NETWORK] 網路管理
// **********************************************************
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  // 如果正在跑藍牙，先停止藍牙以釋放 Radio
  if (a2dp_source.is_connected()) {
     a2dp_source.end(); 
     delay(500);
  }

  Serial.println("[System] 切換至 WiFi 模式...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print(".");
    retry++;
  }
  Serial.println("\n[System] WiFi 已連線");
  wifiMode = true;
}

// **********************************************************
// [CLOUD] 雲端功能 (Sync/Upload)
// **********************************************************
void syncFromCloud() {
  ensureWiFi();
  Serial.println("[Sync] 同步雲端記憶...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  if (http.begin(client, G_SCRIPT_URL)) {
    int code = http.GET();
    if (code == 200) {
      String mem = http.getString();
      if (mem.length() > 0) {
        strncpy(lastAiResponse, mem.c_str(), 127);
        Serial.println("[Sync] 記憶更新: " + mem);
      }
    }
    http.end();
  }
  client.stop();
}

void uploadToCloud(String response) {
  ensureWiFi();
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  if (http.begin(client, G_SCRIPT_URL)) {
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["persona"] = PERSONA_NAMES[currentPersona];
    doc["state"] = currentState;
    doc["soil"] = mockSoilPercent;
    doc["response"] = response;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    http.POST(jsonStr); // 不特別等待回應以加快速度
    http.end();
  }
  client.stop();
}

// **********************************************************
// [TTS] 下載語音
// **********************************************************
bool downloadTTS(String text) {
  ensureWiFi();
  Serial.println("[TTS] 下載語音...");
  WiFiClientSecure client;
  client.setInsecure(); // TTS 有時也需要 SSL，視 API 而定，VoiceRSS HTTP 也可以
  HTTPClient http;

  String encoded = "";
  for (int i = 0; i < text.length(); i++) {
    if (text[i] == ' ') encoded += "%20";
    else encoded += text[i];
  }

  // *** 使用 mono 單聲道 ***
  String url = "http://api.voicerss.org/?key=" + String(TTS_API_KEY) + 
               "&hl=en-gb&c=WAV&f=44khz_16bit_mono&src=" + encoded;

  if (http.begin(url)) {
    int code = http.GET();
    if (code == 200) {
       if (SPIFFS.exists("/tts.wav")) SPIFFS.remove("/tts.wav");
       
       File f = SPIFFS.open("/tts.wav", FILE_WRITE);
       http.writeToStream(&f);
       f.close();
       Serial.println("[TTS] 下載成功");
       http.end();
       return true;
    } else {
       Serial.printf("[TTS] 失敗 Code: %d\n", code);
    }
    http.end();
  }
  return false;
}

// **********************************************************
// [PLAY] 播放程序
// **********************************************************
void playAudio() {
  // 1. 斷開 WiFi 以確保藍牙訊號與記憶體
  Serial.println("[System] 切換至藍牙模式 (WiFi OFF)...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000); // 重要：給 Radio 切換時間

  // 2. 準備檔案
  ttsFile = SPIFFS.open("/tts.wav", FILE_READ);
  if (ttsFile.size() > 44) ttsFile.seek(44); // 跳過檔頭

  // 3. 啟動藍牙
  // 如果之前已經 start 過，reconnect 即可，但為了穩定重新 start 較保險
  isPlaying = true;
  a2dp_source.start(BT_SPEAKER_NAME, get_sound_data); 
  Serial.println("[BT] 等待喇叭連線與播放...");
}

// **********************************************************
// [AI] 核心邏輯
// **********************************************************
void callGemini() {
  ensureWiFi(); // 確保網路
  syncFromCloud(); // 同步
  
  Serial.println("...等待連線釋放 (防-11錯誤)...");
  delay(1500); 

  Serial.println("[AI] 呼叫 Gemini...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000); 

  String url = "https://generativelanguage.googleapis.com/v1beta/models/" + String(GEMINI_MODEL) + ":generateContent?key=" + String(API_KEY);
  
  JsonDocument doc;
  String prompt = "[ROLE]:" + String(SYSTEM_INSTRUCTIONS[currentPersona]) + "\n";
  if (strlen(lastAiResponse) > 0) {
    prompt += "[PREV]: \"" + String(lastAiResponse) + "\"\n";
  }
  prompt += "[NOW]: State=" + currentState + ", Soil=" + String(mockSoilPercent) + "%.\n";
  prompt += "[OUT]: Max 20 chars."; // 限制短一點適合語音
  
  doc["contents"][0]["parts"][0]["text"] = prompt;
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonStr);
    
    if (code == 200) {
      JsonDocument resDoc;
      deserializeJson(resDoc, http.getString());
      const char* aiText = resDoc["candidates"][0]["content"]["parts"][0]["text"];
      
      if (aiText) {
        String textStr = String(aiText);
        textStr.trim();
        Serial.println("\n>>> [AI]: " + textStr);
        
        // 1. 更新記憶
        strncpy(lastAiResponse, textStr.c_str(), 127);
        // 2. 上傳雲端 (先做這個，趁 WiFi 還在)
        delay(500);
        uploadToCloud(textStr);
        // 3. 下載語音 (還是 WiFi)
        if (downloadTTS(textStr)) {
            // 4. 播放 (切換到藍牙)
            playAudio();
        }
      }
    } else {
      Serial.printf("[AI Error] %d\n", code);
    }
    http.end();
  }
  client.stop();
}

// **********************************************************
// [UI] 介面顯示輔助
// **********************************************************
void printMenu() {
  Serial.println("\n\n=============================================");
  Serial.println("      🌱 Smart Plant AI - 控制中心 (v4.0)     ");
  Serial.println("=============================================");
  Serial.printf(" [目前狀態] 人格: %-10s | 濕度: %d%%\n", PERSONA_NAMES[currentPersona], mockSoilPercent);
  Serial.println("---------------------------------------------");
  Serial.println(" 指令列表：");
  Serial.println("  [ai]   : 啟動 AI 對話 (雲端同步 -> 生成 -> 語音)");
  Serial.println("  [p]    : 切換植物人格 (Roaster/Sergeant...)");
  Serial.println("  [sync] : 手動同步雲端記憶");
  Serial.println("  [dry]  : 測試模擬 - 土壤乾燥 (10%)");
  Serial.println("  [wet]  : 測試模擬 - 土壤濕潤 (80%)");
  Serial.println("=============================================");
}


// **********************************************************
// [SYSTEM]
// **********************************************************
void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Mount Failed");

  // 初始化藍牙
  a2dp_source.set_auto_reconnect(false);
  a2dp_source.set_volume(100);
  a2dp_source.set_on_connection_state_changed(connection_state_changed);

  ensureWiFi();
  
  // --- 修改這裡 ---
  printMenu(); 
  Serial.print("\n[系統待機] 請輸入指令 > ");
  // ---------------
}

// 顯示提示符號的小工具
void showPrompt() {
  Serial.println("\n---------------------------------------------");
  Serial.print("[系統待機] 下一步要這盆植物做什麼？ > ");
}

// 新增一個變數來追蹤是否正在等待語音結束
bool waitingForAudio = false;

void loop() {
  // --- [狀態檢查區] ---
  
  // 如果我們正在等待語音，且現在 isPlaying 變成了 false (代表剛播完)
  if (waitingForAudio && !isPlaying) {
      waitingForAudio = false; // 解除等待鎖定
      
      // 這裡可以選擇是否要自動切回 WiFi，或者等下一次指令再切
      // 為了省電與邏輯單純，我們維持現狀，等下次指令再切
      
      showPrompt(); // 語音結束了，現在才顯示提示
  }

  // 如果正在播放或等待連線中，暫停處理任何新指令，也不要印 prompt
  if (waitingForAudio) {
      delay(100); // 讓出 CPU 給藍牙任務
      return; 
  }

  // --- [指令輸入區] ---
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    Serial.println("\n[收到指令]: " + input);

    if (input == "ai") {
      callGemini(); // 執行完這行後，如果成功，isPlaying 會變成 true
      
      if (isPlaying) {
        // 如果成功進入播放模式，我們標記要等待
        waitingForAudio = true;
        // 注意：這裡 "不" 呼叫 showPrompt()，留給上面的狀態檢查區去呼叫
      } else {
        // 如果下載失敗或沒聲音，直接顯示提示
        showPrompt();
      }
    }
    else if (input == "sync") {
      syncFromCloud();
      showPrompt(); // 非語音指令，做完立刻顯示
    }
    else if (input == "p") {
       currentPersona = (Persona)((int)currentPersona + 1);
       if (currentPersona > 3) currentPersona = P_ROASTER;
       Serial.printf("[設定] 人格已切換為: %s\n", PERSONA_NAMES[currentPersona]);
       showPrompt();
    }
    else if (input == "dry") {
       mockSoilPercent = 10;
       currentState = "S_WARN_PLANT";
       Serial.println("[模擬] 土壤變乾了 (10%)");
       showPrompt();
    }
    else if (input == "wet") {
       mockSoilPercent = 80;
       currentState = "S_IDLE";
       Serial.println("[模擬] 土壤變濕潤了 (80%)");
       showPrompt();
    }
    else if (input == "menu" || input == "?") { 
       printMenu();
       showPrompt();
    }
    else {
       Serial.println("[錯誤] 無效指令");
       showPrompt();
    }
  }
  
  // 輕微 delay 保持系統穩定
  delay(10);
}