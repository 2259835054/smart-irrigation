/**
 * @file irrigation.h
 * @brief 灌溉控制模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现多路独立灌溉控制，支持多种控制模式：
 * - PID自动控制（毕设亮点）
 * - 简单阈值控制
 * - 定时灌溉
 * - 手动控制
 */

#ifndef IRRIGATION_H
#define IRRIGATION_H

#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "pid_controller.h"
#include "rtos_tasks.h"

// ==================== 枚举定义 ====================

/**
 * @brief 灌溉模式枚举
 */
enum IrrigationMode {
    MODE_AUTO_PID,    // PID 自动控制（毕设亮点）
    MODE_AUTO_SIMPLE, // 简单阈值控制
    MODE_TIMED,       // 定时灌溉
    MODE_MANUAL       // 手动控制
};

// ==================== 数据结构定义 ====================

/**
 * @brief 单通道状态结构
 */
struct ChannelState {
    bool     pumpRunning;          // 水泵是否运行
    uint8_t  pumpPWM;              // PWM 输出值 0-255
    unsigned long pumpStartTime;   // 水泵启动时间
    unsigned long lastWaterTime;   // 上次浇水时间
    unsigned long totalWaterTime;  // 累计浇水时间 (ms)
    int      waterCount;           // 浇水次数
    float    dryThreshold;         // 干燥阈值 (%)
    float    wetThreshold;         // 湿润阈值 (%)
    bool     enabled;              // 通道是否启用
    
    // PID 控制器（每通道独立）
    PIDController *pidController;  // PID 控制器指针
};

/**
 * @brief 灌溉系统状态结构
 */
struct IrrigationState {
    ChannelState channels[NUM_CHANNELS]; // 4路通道状态
    IrrigationMode mode;                 // 当前控制模式
    bool     systemEnabled;              // 系统总开关
    unsigned long uptime;                // 系统运行时间 (ms)
    bool     alarmActive;                // 报警状态
    String   alarmMessage;               // 报警信息
};

// ==================== 全局状态（由互斥量保护）====================
extern IrrigationState g_irrigationState;

// ==================== 函数声明 ====================

/**
 * @brief 初始化灌溉控制模块
 * 
 * 执行以下操作：
 * 1. 配置所有水泵引脚为输出
 * 2. 配置PWM通道（使用ESP32 LEDC）
 * 3. 初始化各通道PID控制器
 * 4. 加载保存的配置
 * 5. 设置默认阈值
 */
void irrigation_init();

/**
 * @brief 更新灌溉控制（主控制循环）
 * @param data 当前传感器数据
 * 
 * 根据当前模式执行相应的控制逻辑
 */
void irrigation_update(SensorData &data);

/**
 * @brief PID 控制模式更新
 * @param channel 通道号 (0-3)
 * @param currentMoisture 当前湿度
 * 
 * 使用PID算法计算PWM输出值并控制水泵
 */
void irrigation_pid_control(uint8_t channel, float currentMoisture);

/**
 * @brief 简单阈值控制模式更新
 * @param channel 通道号 (0-3)
 * @param currentMoisture 当前湿度
 * 
 * 湿度低于干燥阈值时启动水泵
 * 湿度高于湿润阈值时停止水泵
 */
void irrigation_threshold_control(uint8_t channel, float currentMoisture);

/**
 * @brief 启动指定通道水泵
 * @param channel 通道号 (0-3)
 * @param pwm PWM值 (0-255)，0表示关闭，255表示全速
 * 
 * 支持PWM调速，实现精细的水量控制
 */
void irrigation_start_pump(uint8_t channel, uint8_t pwm);

/**
 * @brief 停止指定通道水泵
 * @param channel 通道号 (0-3)
 */
void irrigation_stop_pump(uint8_t channel);

/**
 * @brief 紧急停止所有水泵
 * 
 * 用于报警或异常情况
 */
void irrigation_stop_all();

/**
 * @brief 设置通道阈值
 * @param channel 通道号 (0-3)
 * @param dry 干燥阈值 (%)
 * @param wet 湿润阈值 (%)
 * @return true=设置成功, false=参数错误
 */
bool irrigation_set_threshold(uint8_t channel, float dry, float wet);

/**
 * @brief 切换灌溉模式
 * @param newMode 新模式
 */
void irrigation_set_mode(IrrigationMode newMode);

/**
 * @brief 获取当前模式
 * @return 当前灌溉模式
 */
IrrigationMode irrigation_get_mode();

/**
 * @brief 获取模式名称字符串
 * @param mode 模式枚举
 * @return 模式名称（中文）
 */
const char* irrigation_get_mode_str(IrrigationMode mode);

/**
 * @brief 获取当前模式名称字符串
 * @return 当前模式名称（中文）
 */
const char* irrigation_get_mode_str();

/**
 * @brief 安全检查
 * 
 * 检查以下安全条件：
 * 1. 单次浇水超时保护（超过PUMP_MAX_DURATION自动停止）
 * 2. 冷却时间检查（两次浇水间隔不少于PUMP_COOLDOWN）
 * 3. 缺水保护（水位传感器检测到缺水时停止所有水泵）
 * 4. 传感器故障检测
 */
void irrigation_safety_check();

/**
 * @brief 处理命令队列中的命令
 * @param cmd 命令结构
 * 
 * 处理来自Web、MQTT、按键的控制命令
 */
void irrigation_process_command(Command &cmd);

/**
 * @brief 获取灌溉系统状态
 * @return 灌溉状态结构引用
 */
IrrigationState& irrigation_get_state();

/**
 * @brief 启用/禁用系统
 * @param enabled true=启用, false=禁用
 */
void irrigation_enable(bool enabled);

/**
 * @brief 启用/禁用指定通道
 * @param channel 通道号 (0-3)
 * @param enabled true=启用, false=禁用
 */
void irrigation_enable_channel(uint8_t channel, bool enabled);

/**
 * @brief 设置PID参数
 * @param channel 通道号 (0-3)
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 */
void irrigation_set_pid_params(uint8_t channel, float kp, float ki, float kd);

/**
 * @brief 打印灌溉状态到串口（调试用）
 */
void irrigation_print_status();

#endif // IRRIGATION_H
