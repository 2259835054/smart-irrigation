/**
 * @file power_manager.cpp
 * @brief 电源管理模块实现
 * @author Smart Irrigation Project
 * @date 2026-02-11
 */

#include "power_manager.h"

// ==================== RTC 内存变量 ====================
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long totalSleepTime = 0;

// ==================== 私有变量 ====================
static const int BATTERY_ADC_PIN = 35;  // 电池电压检测引脚（如果有）

/**
 * @brief 初始化电源管理模块
 */
void power_init() {
    Serial.println("[Power] 初始化电源管理模块...");
    
    // 增加启动计数
    bootCount++;
    
    // 配置电池电压检测ADC
    pinMode(BATTERY_ADC_PIN, INPUT);
    
    // 打印唤醒信息
    WakeupReason reason = power_check_wakeup_reason();
    power_print_wakeup_reason(reason);
    
    Serial.printf("[Power] 启动次数：%d\n", bootCount);
    Serial.printf("[Power] 累计休眠时间：%lu 秒\n", totalSleepTime);
    
    Serial.println("[Power] 电源管理模块初始化完成");
}

/**
 * @brief 进入Deep Sleep模式
 */
void power_enter_deep_sleep(uint32_t seconds) {
    Serial.printf("[Power] 准备进入Deep Sleep，时长=%u秒\n", seconds);
    
    // 配置定时器唤醒
    power_config_timer_wakeup(seconds);
    
    // 配置按键唤醒（MODE键）
    power_config_ext0_wakeup((gpio_num_t)BUTTON_MODE_PIN, 0);
    
    // 更新累计休眠时间
    totalSleepTime += seconds;
    
    // 等待串口输出完成
    Serial.flush();
    delay(100);
    
    // 进入Deep Sleep
    esp_deep_sleep_start();
}

/**
 * @brief 进入Light Sleep模式
 */
void power_enter_light_sleep(uint32_t ms) {
    Serial.printf("[Power] 进入Light Sleep，时长=%u ms\n", ms);
    
    // 配置定时器唤醒
    esp_sleep_enable_timer_wakeup(ms * 1000);
    
    // 等待串口输出完成
    Serial.flush();
    delay(10);
    
    // 进入Light Sleep
    esp_light_sleep_start();
    
    Serial.println("[Power] 从Light Sleep唤醒");
}

/**
 * @brief 配置定时器唤醒
 */
void power_config_timer_wakeup(uint32_t seconds) {
    // 配置定时器唤醒源（微秒）
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
}

/**
 * @brief 配置外部中断唤醒（EXT0）
 */
void power_config_ext0_wakeup(gpio_num_t pin, int level) {
    // 配置EXT0唤醒源
    // level: 0=低电平唤醒, 1=高电平唤醒
    esp_sleep_enable_ext0_wakeup(pin, level);
}

/**
 * @brief 检查唤醒原因
 */
WakeupReason power_check_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            return WAKEUP_EXT0;
        case ESP_SLEEP_WAKEUP_EXT1:
            return WAKEUP_EXT1;
        case ESP_SLEEP_WAKEUP_TIMER:
            return WAKEUP_TIMER;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return WAKEUP_TOUCHPAD;
        case ESP_SLEEP_WAKEUP_ULP:
            return WAKEUP_ULP;
        case ESP_SLEEP_WAKEUP_GPIO:
            return WAKEUP_GPIO;
        default:
            return WAKEUP_UNDEFINED;
    }
}

/**
 * @brief 打印唤醒原因到串口
 */
void power_print_wakeup_reason(WakeupReason reason) {
    Serial.print("[Power] 唤醒原因：");
    
    switch (reason) {
        case WAKEUP_EXT0:
            Serial.println("外部中断0（按键唤醒）");
            break;
        case WAKEUP_EXT1:
            Serial.println("外部中断1");
            break;
        case WAKEUP_TIMER:
            Serial.println("定时器唤醒");
            break;
        case WAKEUP_TOUCHPAD:
            Serial.println("触摸板唤醒");
            break;
        case WAKEUP_ULP:
            Serial.println("ULP协处理器唤醒");
            break;
        case WAKEUP_GPIO:
            Serial.println("GPIO唤醒");
            break;
        default:
            Serial.println("正常启动（非唤醒）");
            break;
    }
}

/**
 * @brief 获取电池电压
 */
float power_get_battery_voltage() {
    // 读取ADC值
    int adcValue = analogRead(BATTERY_ADC_PIN);
    
    // 电压分压器：R1=100K, R2=100K
    // Vbat = (ADC / 4095) * 3.3V * 2
    float voltage = (adcValue / 4095.0) * 3.3 * 2.0;
    
    // 如果ADC值很低，说明没有电池，返回USB供电电压
    if (adcValue < 100) {
        return 5.0;  // USB供电
    }
    
    return voltage;
}

/**
 * @brief 检查是否低电量
 */
bool power_is_battery_low() {
    float voltage = power_get_battery_voltage();
    return (voltage < BATTERY_LOW_VOLTAGE && voltage < 5.0);
}

/**
 * @brief 获取电池电量百分比
 */
uint8_t power_get_battery_percent() {
    float voltage = power_get_battery_voltage();
    
    // USB供电
    if (voltage >= 4.5) {
        return 100;
    }
    
    // 锂电池电压范围：4.2V(100%) - 3.3V(0%)
    float percent = (voltage - 3.3) / (4.2 - 3.3) * 100.0;
    percent = constrain(percent, 0, 100);
    
    return (uint8_t)percent;
}

/**
 * @brief 启用WiFi省电模式
 */
void power_enable_wifi_power_save() {
    WiFi.setSleep(true);
    Serial.println("[Power] WiFi省电模式：启用");
}

/**
 * @brief 禁用WiFi省电模式
 */
void power_disable_wifi_power_save() {
    WiFi.setSleep(false);
    Serial.println("[Power] WiFi省电模式：禁用");
}

/**
 * @brief 降低CPU频率以节能
 */
void power_set_cpu_frequency(uint32_t mhz) {
    // ESP32支持：240, 160, 80 MHz
    if (mhz != 80 && mhz != 160 && mhz != 240) {
        Serial.printf("[Power] 错误：不支持的CPU频率 %u MHz\n", mhz);
        return;
    }
    
    setCpuFrequencyMhz(mhz);
    Serial.printf("[Power] CPU频率设置为：%u MHz\n", mhz);
}

/**
 * @brief 获取当前CPU频率
 */
uint32_t power_get_cpu_frequency() {
    return getCpuFrequencyMhz();
}

/**
 * @brief 打印电源信息到串口
 */
void power_print_info() {
    Serial.println("========== 电源信息 ==========");
    Serial.printf("CPU频率：%u MHz\n", power_get_cpu_frequency());
    Serial.printf("电池电压：%.2f V\n", power_get_battery_voltage());
    Serial.printf("电池电量：%u %%\n", power_get_battery_percent());
    Serial.printf("低电量警告：%s\n", power_is_battery_low() ? "是" : "否");
    Serial.printf("启动次数：%d\n", bootCount);
    Serial.printf("累计休眠：%lu 秒\n", totalSleepTime);
    Serial.println("==============================");
}
