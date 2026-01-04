//Peripherals.h
#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <LiquidCrystal_I2C.h>
//#include <BluetoothSerial.h>

// 硬體物件宣告
extern U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2;
extern LiquidCrystal_I2C lcd;
extern BluetoothSerial SerialBT;

// 函式宣告
void initPeripherals();                
void playTone(int freq, int durationMs); 
void drawCurrentStateView(unsigned long now); 
void updateLCDText(unsigned long now);        

#endif