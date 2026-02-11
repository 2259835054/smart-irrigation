/**
 * @file config.h
 * @brief 智能灌溉系统全局配置文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 本文件包含系统所有配置参数：引脚定义、传感器参数、灌溉参数、
 * PID参数、FreeRTOS任务配置、WiFi/MQTT配置、数据记录配置等
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== 系统版本 ====================
#define FIRMWARE_VERSION    "2.0.0"
#define SYSTEM_NAME         "SmartIrrigation"

// ==================== 引脚定义 ====================
// 传感器引脚
#define SOIL_MOISTURE_PIN_1  34   // 通道1 土壤湿度 (ADC1_CH6)
#define SOIL_MOISTURE_PIN_2  35   // 通道2 土壤湿度 (ADC1_CH7)
#define SOIL_MOISTURE_PIN_3  32   // 通道3 土壤湿度 (ADC1_CH4)
#define SOIL_MOISTURE_PIN_4  33   // 通道4 土壤湿度 (ADC1_CH5)
#define DHT_PIN              4    // DHT22 温湿度传感器
#define WATER_LEVEL_PIN      5    // 水位传感器
#define LIGHT_SENSOR_PIN     36   // 光照传感器 (ADC1_CH0)

// 执行器引脚
#define PUMP_PIN_1           16   // 通道1 水泵
#define PUMP_PIN_2           17   // 通道2 水泵
#define PUMP_PIN_3           18   // 通道3 水泵
#define PUMP_PIN_4           19   // 通道4 水泵
#define LED_PIN              2    // 系统 LED
#define BUZZER_PIN           23   // 蜂鸣器

// 按键引脚
#define BUTTON_MODE_PIN      15   // 模式切换按键
#define BUTTON_SELECT_PIN    13   // 选择/确认按键

// OLED I2C
#define OLED_SDA             21
#define OLED_SCL             22

// ==================== 传感器配置 ====================
#define DHT_TYPE             DHT22
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        64
#define OLED_I2C_ADDR        0x3C
#define NUM_CHANNELS         4     // 灌溉通道数量
#define ADC_SAMPLES          10    // ADC 采样次数（用于滤波）

// ==================== 灌溉参数 ====================
#define SOIL_DRY_DEFAULT     55    // 默认干燥阈值 (%)
#define SOIL_WET_DEFAULT     80    // 默认湿润阈值 (%)
#define PUMP_MAX_DURATION    30000 // 单次最大浇水时间 (ms)
#define PUMP_COOLDOWN        60000 // 两次浇水最小间隔 (ms)
#define CHECK_INTERVAL       3000  // 传感器检测间隔 (ms)

// ==================== PID 参数 ====================
#define PID_KP               2.0
#define PID_KI               0.5
#define PID_KD               1.0
#define PID_OUTPUT_MIN       0
#define PID_OUTPUT_MAX       255   // PWM 输出上限
#define PID_SAMPLE_TIME      1000  // PID 采样周期 (ms)

// ==================== FreeRTOS 任务配置 ====================
#define TASK_SENSOR_STACK     4096
#define TASK_IRRIGATION_STACK 4096
#define TASK_DISPLAY_STACK    4096
#define TASK_NETWORK_STACK    8192
#define TASK_MQTT_STACK       4096
#define TASK_DATALOG_STACK    4096
#define TASK_BUTTON_STACK     2048
#define TASK_WATCHDOG_STACK   2048

#define TASK_SENSOR_PRIORITY     3
#define TASK_IRRIGATION_PRIORITY 4  // 最高优先级
#define TASK_DISPLAY_PRIORITY    1
#define TASK_NETWORK_PRIORITY    2
#define TASK_MQTT_PRIORITY       2
#define TASK_DATALOG_PRIORITY    1
#define TASK_BUTTON_PRIORITY     3
#define TASK_WATCHDOG_PRIORITY   5  // 看门狗最高

// ==================== WiFi 配置 ====================
#define WIFI_SSID            "YOUR_WIFI_SSID"
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_TIMEOUT 20000
#define WEB_SERVER_PORT      80

// ==================== MQTT 配置 ====================
#define MQTT_BROKER          "broker.emqx.io"  // 免费公共 MQTT 服务器
#define MQTT_PORT            1883
#define MQTT_CLIENT_ID       "smart_irrigation_01"
#define MQTT_USER            ""
#define MQTT_PASSWORD        ""
#define MQTT_TOPIC_PREFIX    "home/irrigation"
#define MQTT_PUB_INTERVAL    10000  // MQTT 发布间隔 (ms)

// ==================== 数据记录配置 ====================
#define DATA_LOG_INTERVAL    60000  // 数据记录间隔 (ms), 每分钟
#define MAX_LOG_ENTRIES      1440   // 最大记录条数 (24h x 60min)
#define LOG_FILE_PATH        "/data_log.json"
#define CONFIG_FILE_PATH     "/config.json"

// ==================== 低功耗配置 ====================
#define DEEP_SLEEP_ENABLED   false
#define DEEP_SLEEP_DURATION  300    // Deep Sleep 时长 (秒)
#define BATTERY_LOW_VOLTAGE  3.3    // 低电量阈值 (V)

#endif // CONFIG_H
