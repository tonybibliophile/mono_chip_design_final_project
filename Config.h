    #ifndef CONFIG_H
    #define CONFIG_H

    #include <Arduino.h>

    // ==============================
    // [USER SETTINGS] 使用者設定區
    // ==============================

    // 1. WiFi 設定
    #define WIFI_SSID      "Tranquility"
    #define WIFI_PASS      "31415tapun"

    // 2. Gemini API 設定
    #define GEMINI_API_KEY "AIzaSyAidIZjieqfT7eZlAKOZ-r_WOxiU6LLHm8"
    #define API_URL        "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key="

    // 3. 系統參數
    #define SHOW_AI_DURATION  10000 // AI 訊息顯示時間 (ms)
    #define SOIL_DRY_THRESHOLD 3000
    #define SOIL_VAL_AIR       4095
    #define SOIL_VAL_WATER     300

    #define TIME_PARTY_PLANT    8000
    #define TIME_PARTY_HUMAN    4000

    // ==============================
    // [HARDWARE] 硬體腳位
    // ==============================
    #define PIN_OLED_CLK   12
    #define PIN_OLED_MOSI  11
    #define PIN_OLED_RES   10
    #define PIN_OLED_DC    9
    #define PIN_OLED_CS    13

    #define PIN_I2C_SDA    4
    #define PIN_I2C_SCL    5
    #define LCD_ADDRESS    0x27

    #define PIN_SOIL       2
    #define PIN_BUZZER     15
    #define PIN_BUTTON     14
    #define BUZZER_CHANNEL 0

    // 音階頻率
    #define NOTE_G4  392
    #define NOTE_C5  523
    #define NOTE_E5  659
    #define NOTE_G5  784
    #define NOTE_C6  1047
    #define NOTE_D6  1175
    #define NOTE_E6  1319
    #define NOTE_C7  2093

    // ==============================
    // [BLE] 設定
    // ==============================
    #define BLE_DEVICE_NAME "SmartPlant"
    #define BLE_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
    #define BLE_CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
    #define BLE_CHAR_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

    #endif
