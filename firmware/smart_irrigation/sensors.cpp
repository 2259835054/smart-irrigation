/**
 * @file sensors.cpp
 * @brief 传感器模块实现 - 土壤湿度、温湿度、水位检测
 * @author Smart Irrigation Project
 * @date 2026
 */

#include "sensors.h"
#include "config.h"
#include <DHT.h>

// DHT传感器对象
DHT dht(DHT_PIN, DHT_TYPE);

/**
 * @brief 初始化传感器模块
 */
void sensors_init() {
    // 初始化DHT传感器
    dht.begin();
    
    // 配置引脚模式
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
    
    Serial.println("传感器模块初始化完成");
}

/**
 * @brief 读取所有传感器数据
 * @return SensorData 包含所有传感器读数的结构体
 */
SensorData sensors_read() {
    SensorData data;
    
    // 读取土壤湿度 (ADC: 0-4095, 转换为0-100%)
    // 注意：电容式传感器通常湿润时值小，干燥时值大
    // 因此需要反向映射
    int soilRaw = analogRead(SOIL_MOISTURE_PIN);
    data.soilMoisture = map(soilRaw, 4095, 0, 0, 100);  // 反向映射
    data.soilMoisture = constrain(data.soilMoisture, 0, 100);
    
    // 读取温湿度
    data.temperature = dht.readTemperature();
    data.humidity = dht.readHumidity();
    
    // 处理DHT读取错误 (NaN)
    if (isnan(data.temperature)) {
        data.temperature = 0.0;
    }
    if (isnan(data.humidity)) {
        data.humidity = 0.0;
    }
    
    // 读取水位传感器 (LOW=有水, HIGH=缺水)
    data.waterLevelOk = (digitalRead(WATER_LEVEL_PIN) == LOW);
    
    return data;
}
