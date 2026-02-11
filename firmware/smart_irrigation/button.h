/**
 * @file button.h
 * @brief 按键处理模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现按键状态机消抖，支持：
 * - 短按
 * - 长按 (>2秒)
 * - 双击
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "config.h"
#include "rtos_tasks.h"

// ==================== 常量定义 ====================
#define DEBOUNCE_DELAY       50    // 消抖延时 (ms)
#define LONG_PRESS_TIME      2000  // 长按时间阈值 (ms)
#define DOUBLE_CLICK_TIME    500   // 双击间隔时间 (ms)

// ==================== 枚举定义 ====================

/**
 * @brief 按键状态枚举
 */
enum ButtonState {
    BTN_IDLE,        // 空闲状态
    BTN_DEBOUNCE,    // 消抖状态
    BTN_PRESSED,     // 按下状态
    BTN_LONG_PRESS,  // 长按状态
    BTN_RELEASED     // 释放状态
};

// ==================== 数据结构 ====================

/**
 * @brief 单个按键状态结构
 */
struct Button {
    uint8_t pin;                    // 按键引脚
    ButtonState state;              // 当前状态
    bool currentReading;            // 当前读取值
    bool lastReading;               // 上次读取值
    unsigned long lastDebounceTime; // 上次消抖时间
    unsigned long pressStartTime;   // 按下开始时间
    unsigned long lastClickTime;    // 上次点击时间
    bool longPressSent;             // 长按事件是否已发送
    uint8_t clickCount;             // 点击计数（用于双击检测）
};

// ==================== 全局按键对象 ====================
extern Button g_buttonMode;
extern Button g_buttonSelect;

// ==================== 函数声明 ====================

/**
 * @brief 初始化按键模块
 * 
 * 配置按键引脚为输入上拉模式
 */
void button_init();

/**
 * @brief 按键扫描和处理（在任务中调用）
 * 
 * 执行以下操作：
 * 1. 读取按键状态
 * 2. 状态机处理
 * 3. 生成按键事件并发送到队列
 */
void button_scan();

/**
 * @brief 更新单个按键状态
 * @param btn 按键结构指针
 * @param eventType 事件类型（如果检测到事件）
 * @return true=检测到事件, false=无事件
 * 
 * 使用状态机实现消抖和长按检测
 */
bool button_update(Button *btn, ButtonEventType &eventType);

/**
 * @brief 读取按键电平
 * @param pin 按键引脚
 * @return true=按下, false=未按下
 */
bool button_read(uint8_t pin);

/**
 * @brief 发送按键事件到队列
 * @param eventType 事件类型
 */
void button_send_event(ButtonEventType eventType);

/**
 * @brief 处理按键事件（在其他任务中调用）
 * @param event 按键事件
 * 
 * 根据按键事件执行相应操作：
 * - MODE短按：切换OLED页面
 * - MODE长按：切换灌溉模式
 * - SELECT短按：确认/选择
 * - SELECT长按：手动浇水开关
 * - SELECT双击：紧急停止所有水泵
 */
void button_handle_event(const ButtonEvent &event);

/**
 * @brief 重置按键状态
 * @param btn 按键结构指针
 */
void button_reset(Button *btn);

#endif // BUTTON_H
