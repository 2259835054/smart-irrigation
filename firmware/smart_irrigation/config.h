/**
 * @file config.h
 * @brief 配置文件 - 引脚定义、参数配置、WiFi配置
 * @author Smart Irrigation Project
 * @date 2026
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== 引脚定义 ====================
#define SOIL_MOISTURE_PIN   34    // 土壤湿度传感器 (ADC)
#define DHT_PIN             4     // DHT温湿度传感器
#define WATER_LEVEL_PIN     5     // 水位传感器 (数字输入)
#define PUMP_PIN            16    // 水泵控制 (数字输出)
#define BUTTON_PIN          15    // 按键输入
#define LED_PIN             2     // LED指示灯
#define BUZZER_PIN          17    // 蜂鸣器

// ==================== 传感器配置 ====================
#define DHT_TYPE            DHT22
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define OLED_I2C_ADDR       0x3C

// ==================== 灌溉参数 ====================
#define SOIL_DRY_THRESHOLD  60    // 土壤湿度阈值（低于此值触发灌溉, %）
#define SOIL_WET_THRESHOLD  80    // 停止灌溉阈值（高于此值停止, %）
#define PUMP_MAX_DURATION   30000 // 单次最大浇水时间 (ms)
#define CHECK_INTERVAL      5000  // 传感器检测间隔 (ms)
#define TIMED_INTERVAL      (24UL * 60 * 60 * 1000)  // 定时模式间隔 24小时
#define TIMED_DURATION      10000 // 定时模式每次浇水时长 (ms)

// ==================== WiFi 配置 ====================
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#define WEB_SERVER_PORT     80

#endif // CONFIG_H
