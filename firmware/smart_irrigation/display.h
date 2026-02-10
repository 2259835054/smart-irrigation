/**
 * @file display.h
 * @brief OLED显示模块头文件 - SSD1306显示控制
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "sensors.h"
#include "irrigation.h"

/**
 * @brief 初始化OLED显示屏
 */
void display_init();

/**
 * @brief 显示开机画面
 */
void display_startup();

/**
 * @brief 更新主界面显示
 * @param data 传感器数据引用
 * @param state 灌溉状态引用
 */
void display_update(SensorData &data, IrrigationState &state);

#endif // DISPLAY_H
