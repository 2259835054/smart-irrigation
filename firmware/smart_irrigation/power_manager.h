/**
 * @file power_manager.h
 * @brief 电源管理模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现低功耗管理，包括：
 * - Deep Sleep 深度休眠
 * - Light Sleep 轻度休眠
 * - 电池电压监测
 * - 唤醒源配置
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"

// ==================== RTC 内存变量（Deep Sleep保持）====================
// 使用 RTC_DATA_ATTR 属性定义的变量在Deep Sleep期间保持数据
extern RTC_DATA_ATTR int bootCount;
extern RTC_DATA_ATTR unsigned long totalSleepTime;

// ==================== 唤醒原因枚举 ====================
enum WakeupReason {
    WAKEUP_UNDEFINED,
    WAKEUP_EXT0,          // 外部中断0（按键唤醒）
    WAKEUP_EXT1,          // 外部中断1
    WAKEUP_TIMER,         // 定时器唤醒
    WAKEUP_TOUCHPAD,      // 触摸板唤醒
    WAKEUP_ULP,           // ULP协处理器唤醒
    WAKEUP_GPIO           // GPIO唤醒
};

// ==================== 函数声明 ====================

/**
 * @brief 初始化电源管理模块
 * 
 * 配置ADC用于电池电压检测
 */
void power_init();

/**
 * @brief 进入Deep Sleep模式
 * @param seconds 休眠时长（秒）
 * 
 * Deep Sleep特点：
 * - 功耗最低（约10μA）
 * - 只保留RTC内存和RTC外设
 * - 唤醒后从头执行setup()
 * - 可通过定时器、外部中断、触摸板等唤醒
 */
void power_enter_deep_sleep(uint32_t seconds);

/**
 * @brief 进入Light Sleep模式
 * @param ms 休眠时长（毫秒）
 * 
 * Light Sleep特点：
 * - 功耗较低（约0.8mA）
 * - 保留所有内存
 * - 唤醒后从休眠点继续执行
 * - WiFi断开但可快速恢复
 */
void power_enter_light_sleep(uint32_t ms);

/**
 * @brief 配置定时器唤醒
 * @param seconds 定时器时长（秒）
 */
void power_config_timer_wakeup(uint32_t seconds);

/**
 * @brief 配置外部中断唤醒（EXT0）
 * @param pin 唤醒引脚
 * @param level 唤醒电平（0=低电平, 1=高电平）
 * 
 * 例如：配置按键按下时唤醒
 */
void power_config_ext0_wakeup(gpio_num_t pin, int level);

/**
 * @brief 检查唤醒原因
 * @return 唤醒原因枚举
 * 
 * 在setup()中调用，判断是正常启动还是从休眠唤醒
 */
WakeupReason power_check_wakeup_reason();

/**
 * @brief 打印唤醒原因到串口
 * @param reason 唤醒原因
 */
void power_print_wakeup_reason(WakeupReason reason);

/**
 * @brief 获取电池电压
 * @return 电池电压（伏特）
 * 
 * 通过ADC读取电池电压（需要外部分压电路）
 * 如果没有电池供电，返回固定值
 */
float power_get_battery_voltage();

/**
 * @brief 检查是否低电量
 * @return true=低电量, false=电量正常
 * 
 * 电压低于BATTERY_LOW_VOLTAGE时返回true
 */
bool power_is_battery_low();

/**
 * @brief 获取电池电量百分比
 * @return 电量百分比 (0-100)
 * 
 * 基于电压估算电量：
 * - 4.2V = 100%
 * - 3.7V = 50%
 * - 3.3V = 0%
 */
uint8_t power_get_battery_percent();

/**
 * @brief 启用WiFi省电模式
 * 
 * 降低WiFi发射功率，节省能耗
 */
void power_enable_wifi_power_save();

/**
 * @brief 禁用WiFi省电模式
 * 
 * 恢复WiFi正常功率
 */
void power_disable_wifi_power_save();

/**
 * @brief 降低CPU频率以节能
 * @param mhz 目标频率（80/160/240 MHz）
 */
void power_set_cpu_frequency(uint32_t mhz);

/**
 * @brief 获取当前CPU频率
 * @return CPU频率（MHz）
 */
uint32_t power_get_cpu_frequency();

/**
 * @brief 打印电源信息到串口（调试用）
 */
void power_print_info();

#endif // POWER_MANAGER_H
