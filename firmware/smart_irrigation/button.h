/**
 * @file button.h
 * @brief 按键处理模块头文件 - 按键检测和消抖
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

/**
 * @brief 初始化按键模块
 */
void button_init();

/**
 * @brief 检测按键状态
 * @return true 检测到短按
 * @return false 无按键或长按处理中
 */
bool button_check();

#endif // BUTTON_H
