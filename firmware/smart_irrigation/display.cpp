/**
 * @file display.cpp
 * @brief OLED显示模块实现 - SSD1306显示控制
 * @author Smart Irrigation Project
 * @date 2026
 */

#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED显示对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/**
 * @brief 初始化OLED显示屏
 */
void display_init() {
    // 初始化I2C总线
    Wire.begin();
    
    // 初始化OLED显示屏
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("OLED初始化失败！");
        for (;;); // 停止运行
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    Serial.println("OLED显示模块初始化完成");
}

/**
 * @brief 显示开机画面
 */
void display_startup() {
    display.clearDisplay();
    
    // 标题
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.println("Smart");
    display.println("Irrigation");
    
    // 副标题
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println("Ver 1.0");
    
    display.display();
    delay(2000);
}

/**
 * @brief 获取模式名称（中文）
 */
const char* getModeString(IrrigationMode mode) {
    switch (mode) {
        case MODE_AUTO:   return "Auto";
        case MODE_TIMED:  return "Timed";
        case MODE_MANUAL: return "Manual";
        default:          return "Unknown";
    }
}

/**
 * @brief 更新主界面显示
 * @param data 传感器数据引用
 * @param state 灌溉状态引用
 */
void display_update(SensorData &data, IrrigationState &state) {
    display.clearDisplay();
    
    // 标题栏
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("== Smart Irrigation ==");
    
    // 土壤湿度
    display.setCursor(0, 12);
    display.print("Soil: ");
    display.print((int)data.soilMoisture);
    display.println("%");
    
    // 温度
    display.setCursor(0, 22);
    display.print("Temp: ");
    if (data.temperature > 0) {
        display.print(data.temperature, 1);
        display.println("C");
    } else {
        display.println("--");
    }
    
    // 湿度
    display.setCursor(0, 32);
    display.print("Humi: ");
    if (data.humidity > 0) {
        display.print((int)data.humidity);
        display.println("%");
    } else {
        display.println("--");
    }
    
    // 水箱状态
    display.setCursor(0, 42);
    display.print("Tank: ");
    display.println(data.waterLevelOk ? "OK" : "LOW!");
    
    // 水泵状态和模式
    display.setCursor(0, 52);
    display.print("Pump:");
    display.print(state.pumpRunning ? "ON " : "OFF");
    display.print(" [");
    display.print(getModeString(state.mode));
    display.println("]");
    
    display.display();
}
