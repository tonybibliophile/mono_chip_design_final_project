#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi & API ---
#define WIFI_SSID       "Tranquility"
#define WIFI_PASS       "31415tapun"
#define GEMINI_API_KEY  "AIzaSyBG27TJti6g5NSLTJxdK6Omi-yTXNQHgCc"
#define API_URL         "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent?key="

// --- BLE ---
#define ENABLE_BLE      1  
#define BLE_DEVICE_NAME "SmartPlant_S3"

// --- Hardware Pins (ESP32-S3) ---
#define PIN_SOIL        2
#define PIN_BUZZER      4
#define PIN_BUTTON      14

// I2C (LCD)
#define PIN_I2C_SDA     47
#define PIN_I2C_SCL     48
#define LCD_ADDRESS     0x27

// SPI (OLED)
#define PIN_OLED_CLK    15
#define PIN_OLED_MOSI   7
#define PIN_OLED_RES    6
#define PIN_OLED_DC     9
#define PIN_OLED_CS     46

// --- Settings ---
#define SOIL_DRY_THRESHOLD 1000
#define SHOW_AI_DURATION   10000

#endif