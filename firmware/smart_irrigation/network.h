/**
 * @file network.h
 * @brief WiFi网络模块头文件 - WiFi连接和Web服务器
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include "sensors.h"
#include "irrigation.h"

/**
 * @brief 设置当前数据指针（由主程序调用）
 * @param data 传感器数据指针
 * @param state 灌溉状态指针
 */
void network_set_data(SensorData* data, IrrigationState* state);

/**
 * @brief 初始化WiFi连接
 */
void network_init();

/**
 * @brief 处理Web客户端请求
 */
void network_handle_client();

/**
 * @brief 获取WiFi连接状态
 * @return true WiFi已连接
 * @return false WiFi未连接
 */
bool network_is_connected();

#endif // NETWORK_H
