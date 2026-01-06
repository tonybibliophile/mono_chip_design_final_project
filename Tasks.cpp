#include "Config.h"
#include "Shared.h"
#include "Peripherals.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

void connectWiFi();
void askGemini(String event);

volatile SystemState lastState = S_WAIT_BT;
bool deviceConnected=false;
BLECharacteristic* pTX;
BLECharacteristic* pRX;
std::string bleRxBuffer="";

// ---------- BLE Callbacks ----------
class ServerCB: public BLEServerCallbacks{
  void onConnect(BLEServer* s){ deviceConnected=true; }
  void onDisconnect(BLEServer* s){ deviceConnected=false; }
};
class CharCB: public BLECharacteristicCallbacks{
  void onWrite(BLECharacteristic* c){ bleRxBuffer=c->getValue(); }
};

// ---------- Logic Task ----------
void TaskLogic(void* pv){
 (void)pv;
 bool lastSoilDry=true;
 for(;;){
   int val=analogRead(PIN_SOIL);
   Data.isSoilDry=(val>SOIL_DRY_THRESHOLD);
   bool eventPlantWatered=(lastSoilDry&&!Data.isSoilDry);
   lastSoilDry=Data.isSoilDry;
   unsigned long now=millis();

   switch(currentState){
     case S_WAIT_BT: currentState=S_IDLE; break;
     default:
       if(eventPlantWatered){ currentState=S_PARTY_PLANT; Data.stateStartTime=now;}
       else if(Data.isSoilDry) currentState=S_WARN_PLANT;
       else if((currentState==S_PARTY_HUMAN||currentState==S_PARTY_PLANT)&&now-Data.stateStartTime>TIME_PARTY_HUMAN) currentState=S_IDLE;

       static int tick=0;
       if(currentState==S_IDLE && ++tick>=10){ tick=0; if(Data.humanTimer>0) Data.humanTimer--; if(Data.humanTimer==0) currentState=S_WARN_HUMAN;}
       break;
   }

   if(currentState!=lastState){
     String prompt="";
     switch(currentState){
       case S_WARN_PLANT: prompt="My soil is dry!"; break;
       case S_WARN_HUMAN: prompt="Human forgot to drink!"; break;
       case S_PARTY_PLANT: prompt="I just got watered!"; break;
       case S_PARTY_HUMAN: prompt="Human drank water!"; break;
     }
     if(prompt!="") askGemini(prompt);
     lastState=currentState;
   }
   vTaskDelay(100/portTICK_PERIOD_MS);
 }
}

// ---------- Display Task ----------
void TaskDisplay(void* pv){
 (void)pv;
 for(;;){
   unsigned long now=millis();
   drawCurrentStateView(now);
   //updateLCDText(now);
   vTaskDelay(30/portTICK_PERIOD_MS);
 }
}

// ---------- Sound Task ----------
void TaskSound(void* pv){
 (void)pv;
 SystemState lastS=S_WAIT_BT;
 for(;;){
   if(currentState!=lastS){
     if(currentState==S_PARTY_HUMAN){ playTone(NOTE_G4,80); playTone(NOTE_C5,80); playTone(NOTE_E5,80); playTone(0,0);}
     else if(currentState==S_PARTY_PLANT){ playTone(NOTE_C6,100); playTone(NOTE_C6,100); playTone(0,0);}
     lastS=currentState;
   }
   switch(currentState){
     case S_WARN_HUMAN: playTone(800,200); playTone(1200,200); break;
     case S_WARN_PLANT: playTone(NOTE_G5,400); playTone(0,2000); break;
     default: playTone(0,100); break;
   }
 }
}

// ---------- BLE Input Task ----------
void TaskInput(void* pv){
 (void)pv;
 BLEDevice::init(BLE_DEVICE_NAME);
 BLEServer* srv=BLEDevice::createServer();
 srv->setCallbacks(new ServerCB());
 BLEService* svc=srv->createService(BLE_SERVICE_UUID);
 pTX=svc->createCharacteristic(BLE_CHAR_TX_UUID,BLECharacteristic::PROPERTY_NOTIFY);
 pTX->addDescriptor(new BLE2902());
 pRX=svc->createCharacteristic(BLE_CHAR_RX_UUID,BLECharacteristic::PROPERTY_WRITE);
 pRX->setCallbacks(new CharCB());
 svc->start();
 srv->getAdvertising()->start();

 for(;;){
   if(!bleRxBuffer.empty()){
     String cmd=String(bleRxBuffer.c_str()); bleRxBuffer.clear();
     cmd.trim();
     if(cmd.equalsIgnoreCase("water")) { currentState=S_PARTY_PLANT; Data.stateStartTime=millis(); }
     else if(cmd.equalsIgnoreCase("dry")) { currentState=S_WARN_PLANT; }
     else if(cmd.equalsIgnoreCase("reset")) { currentState=S_IDLE; }

     if(deviceConnected){
       String msg="State: "+String(currentState);
       pTX->setValue(msg.c_str()); pTX->notify();
     }
   }
   vTaskDelay(50/portTICK_PERIOD_MS);
 }
}
