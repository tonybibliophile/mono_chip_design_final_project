#include "Shared.h"
#include "Config.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Nordic UART Service (通用標準)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// 藍牙連線狀態回呼
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      AppData.bleConnected = true;
      
      // 【強制修正】只要連上，直接踢進待機狀態，不用等 Logic 檢查
      // 這樣可以解決卡在 BT 畫面的問題
      if (currentState == S_WAIT_BT) {
          currentState = S_IDLE;
          AppData.stateStartTime = millis();
          Serial.println("Force State Change -> IDLE");
      }
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      AppData.bleConnected = false;
    }
};

// 藍牙寫入回呼
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        String cmd = String(rxValue.c_str());
        cmd.trim(); 
        Serial.print("BT RX: "); Serial.println(cmd);

        // 邏輯 1: 收到 WATER
        if (cmd.equalsIgnoreCase("WATER")) {
            AppData.flagHumanDrank = true; 
            Serial.println("Action: Flag Set (Water)");
            if (deviceConnected) {
                pTxCharacteristic->setValue("OK: Party!\n");
                pTxCharacteristic->notify();
            }
        }
        // 邏輯 2: 收到數字 (設定時間)
        else {
            int newMinutes = cmd.toInt();
            if (newMinutes > 0) {
                AppData.humanWaterInterval = newMinutes * 60;
                AppData.flagHumanDrank = true; 
                Serial.printf("Action: Set %d mins\n", newMinutes);
                if (deviceConnected) {
                   String msg = "OK: " + String(newMinutes) + "m\n";
                   pTxCharacteristic->setValue((uint8_t*)msg.c_str(), msg.length());
                   pTxCharacteristic->notify();
                }
            }
        }
      }
    }
};

void BLE_Init() {
  BLEDevice::init(BLE_DEVICE_NAME); 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("BLE Started. Waiting for connection...");
}

void Task_BLE(void *pv) {
    BLE_Init();
    for (;;) {
        // 斷線重連處理
        if (!deviceConnected && oldDeviceConnected) {
            delay(500); 
            pServer->startAdvertising(); 
            Serial.println("Restart advertising");
            oldDeviceConnected = deviceConnected;
        }
        // 連線處理
        if (deviceConnected && !oldDeviceConnected) {
            oldDeviceConnected = deviceConnected;
            Serial.println("Device Connected");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}