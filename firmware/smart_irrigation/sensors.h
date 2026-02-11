/**
 * @file sensors.h
 * @brief 传感器模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 负责所有传感器的初始化、数据读取和滤波处理
 * 包括：土壤湿度传感器(4路)、DHT22温湿度传感器、光照传感器、水位传感器
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "config.h"

// ==================== 数据结构定义 ====================

/**
 * @brief 单通道数据结构
 */
struct ChannelData {
    float soilMoisture;     // 土壤湿度 0-100%
    bool  isValid;          // 数据是否有效
};

/**
 * @brief 传感器数据结构
 */
struct SensorData {
    ChannelData channels[NUM_CHANNELS]; // 4路土壤湿度
    float temperature;       // 环境温度 °C
    float humidity;          // 环境湿度 %
    float lightLevel;        // 光照强度 0-100%
    bool  waterLevelOk;      // 水箱水位是否正常
    unsigned long timestamp; // 数据时间戳 (millis)
};

/**
 * @brief 传感器校准数据结构
 */
struct CalibrationData {
    int dryValue;   // 干燥时ADC值
    int wetValue;   // 湿润时ADC值
};

// ==================== 全局传感器数据（由互斥量保护）====================
extern SensorData g_sensorData;
extern CalibrationData g_calibration[NUM_CHANNELS];

// ==================== 函数声明 ====================

/**
 * @brief 初始化所有传感器
 * 
 * 配置引脚模式、ADC分辨率、DHT传感器
 */
void sensors_init();

/**
 * @brief 读取所有传感器数据
 * @return 包含所有传感器数据的结构体
 * 
 * 执行以下操作：
 * 1. 读取4路土壤湿度（应用滑动窗口中值滤波）
 * 2. 读取DHT22温湿度
 * 3. 读取光照传感器
 * 4. 读取水位传感器
 */
SensorData sensors_read();

/**
 * @brief 读取单通道土壤湿度（带滤波）
 * @param channel 通道号 (0-3)
 * @return 湿度百分比 (0-100)
 * 
 * 使用滑动窗口中值滤波算法：
 * 1. 采集 ADC_SAMPLES(10) 个样本
 * 2. 对样本排序
 * 3. 取中值作为有效值
 * 4. 转换为百分比
 */
float sensors_read_soil_filtered(uint8_t channel);

/**
 * @brief 读取光照传感器
 * @return 光照强度百分比 (0-100)
 */
float sensors_read_light();

/**
 * @brief 读取水位传感器
 * @return true=水位正常, false=缺水
 */
bool sensors_read_water_level();

/**
 * @brief 校准土壤湿度传感器
 * @param channel 通道号 (0-3)
 * @param dryValue 干燥时的ADC值
 * @param wetValue 湿润时的ADC值
 * @return true=校准成功, false=参数错误
 * 
 * 在实际使用前需要将传感器分别放入干燥和湿润土壤中
 * 记录ADC值并调用此函数进行校准
 */
bool sensors_calibrate(uint8_t channel, int dryValue, int wetValue);

/**
 * @brief 将ADC原始值转换为湿度百分比
 * @param channel 通道号 (0-3)
 * @param rawValue ADC原始值
 * @return 湿度百分比 (0-100)
 */
float sensors_convert_to_moisture(uint8_t channel, int rawValue);

/**
 * @brief 获取ADC原始值（用于调试）
 * @param pin ADC引脚号
 * @return ADC原始值 (0-4095)
 */
int sensors_read_adc_raw(uint8_t pin);

/**
 * @brief 打印传感器数据到串口（调试用）
 * @param data 传感器数据结构
 */
void sensors_print(const SensorData &data);

#endif // SENSORS_H
