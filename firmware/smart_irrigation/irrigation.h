/**
 * @file irrigation.h
 * @brief 灌溉控制模块头文件 - 自动/定时/手动模式控制
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef IRRIGATION_H
#define IRRIGATION_H

#include <Arduino.h>
#include "sensors.h"

/**
 * @brief 灌溉模式枚举
 */
enum IrrigationMode {
    MODE_AUTO,    // 自动模式：根据土壤湿度自动灌溉
    MODE_TIMED,   // 定时模式：定时定量灌溉
    MODE_MANUAL   // 手动模式：手动控制水泵
};

/**
 * @brief 灌溉系统状态结构体
 */
struct IrrigationState {
    bool pumpRunning;           // 水泵是否正在运行
    unsigned long pumpStartTime; // 水泵启动时间 (ms)
    unsigned long lastWaterTime; // 上次浇水时间 (ms)
    int waterCount;             // 浇水次数统计
    IrrigationMode mode;        // 当前灌溉模式
};

/**
 * @brief 初始化灌溉控制模块
 */
void irrigation_init();

/**
 * @brief 更新灌溉控制逻辑
 * @param data 传感器数据引用
 */
void irrigation_update(SensorData &data);

/**
 * @brief 启动水泵
 */
void irrigation_start_pump();

/**
 * @brief 停止水泵
 */
void irrigation_stop_pump();

/**
 * @brief 切换灌溉模式 (AUTO->TIMED->MANUAL)
 */
void irrigation_switch_mode();

/**
 * @brief 手动模式下切换水泵状态
 */
void irrigation_manual_toggle();

/**
 * @brief 安全检查 (超时保护、缺水保护)
 */
void irrigation_safety_check();

/**
 * @brief 获取当前灌溉状态
 * @return IrrigationState& 状态结构体引用
 */
IrrigationState& irrigation_get_state();

#endif // IRRIGATION_H
