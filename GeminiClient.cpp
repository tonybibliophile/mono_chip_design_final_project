#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "Config.h"
#include "Shared.h"

extern bool isWiFiConnected();

String getSystemPrompt() {
 switch(currentPersonality){
   case PER_MEAN: return "You are a MEAN plant. Roast the user. Reply in English, strictly under 15 words about: ";
   case PER_PLAYFUL: return "You are a PLAYFUL plant. Be excited! Reply in English, strictly under 15 words about: ";
   default: return "You are a KIND plant. Give warm advice. Reply in English, strictly under 15 words about: ";
 }
}

String parseGeminiResponse(String jsonResponse){
 if(jsonResponse=="Error"||jsonResponse=="No WiFi") return "AI Error";
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc,jsonResponse);
 if(!error){
   const char* text = doc["candidates"][0]["content"][0]["text"];
   if(text){ String t=text; t.replace("\n"," "); if(t.length()>32) t=t.substring(0,32); return t;}
 }
 return "Parse Error";
}

void askGemini(String eventDescription){
 if(!isWiFiConnected()) return;
 WiFiClientSecure client; client.setInsecure();
 HTTPClient http;
 if(http.begin(client, String(API_URL)+String(GEMINI_API_KEY))){
   http.addHeader("Content-Type","application/json");
   String payload="{\"contents\":[{\"parts\":[{\"text\":\""+getSystemPrompt()+eventDescription+"\"}]}]}";
   int httpCode=http.POST(payload);
   if(httpCode==200){
     String finalMsg=parseGeminiResponse(http.getString());
     Serial.print("AI Says: "); Serial.println(finalMsg);
     Data.aiMessage=finalMsg;
     Data.showAiMsg=true;
     Data.msgStartTime=millis();
   } else { Serial.print("HTTP Error: "); Serial.println(httpCode); Serial.println(http.getString()); }
   http.end();
 } else Serial.println("Cannot connect server");
}
