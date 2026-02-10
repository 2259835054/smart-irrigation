/**
 * @file sensors.h
 * @brief 传感器模块头文件 - 土壤湿度、温湿度、水位检测
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

/**
 * @brief 传感器数据结构体
 */
struct SensorData {
    float soilMoisture;   // 土壤湿度百分比 (0-100%)
    float temperature;    // 温度 (℃)
    float humidity;       // 空气湿度 (%)
    bool waterLevelOk;    // 水箱水位是否正常 (true=有水)
};

/**
 * @brief 初始化传感器模块
 */
void sensors_init();

/**
 * @brief 读取所有传感器数据
 * @return SensorData 包含所有传感器读数的结构体
 */
SensorData sensors_read();

#endif // SENSORS_H
