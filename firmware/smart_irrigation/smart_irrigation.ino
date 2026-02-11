/**
 * @file smart_irrigation.ino
 * @brief 智能灌溉系统主程序 - 毕业设计增强版
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 本程序是基于ESP32的智能灌溉系统，采用FreeRTOS多任务架构
 * 核心特性：
 * - FreeRTOS 8任务实时调度
 * - PID精确控制算法
 * - MQTT物联网通信
 * - Web可视化界面
 * - OTA无线升级
 * - LittleFS数据持久化
 * - 低功耗管理
 * 
 * 硬件平台：ESP32 (ESP-IDF + Arduino framework)
 * 开发工具：PlatformIO
 */

#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "irrigation.h"
#include "pid_controller.h"
#include "display.h"
#include "button.h"
#include "network.h"
#include "mqtt_client.h"
#include "data_logger.h"
#include "power_manager.h"
#include "rtos_tasks.h"

/**
 * @brief 系统初始化
 * 
 * 执行流程：
 * 1. 串口初始化
 * 2. 检查唤醒原因
 * 3. 初始化各功能模块
 * 4. 显示开机动画
 * 5. 加载保存的配置
 * 6. 连接WiFi和MQTT
 * 7. 创建FreeRTOS任务
 */
void setup() {
    // ==================== 1. 串口初始化 ====================
    Serial.begin(115200);
    delay(500);  // 等待串口稳定
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("   智能灌溉系统 - 毕业设计增强版");
    Serial.println("   Smart Irrigation System v2.0");
    Serial.println("========================================");
    Serial.printf("固件版本: %s\n", FIRMWARE_VERSION);
    Serial.printf("系统名称: %s\n", SYSTEM_NAME);
    Serial.printf("编译时间: %s %s\n", __DATE__, __TIME__);
    Serial.println("========================================");
    Serial.println();
    
    // ==================== 2. 检查唤醒原因 ====================
    power_init();
    
    // 如果是从Deep Sleep唤醒，执行特殊处理
    WakeupReason wakeupReason = power_check_wakeup_reason();
    if (wakeupReason == WAKEUP_TIMER) {
        Serial.println("[Setup] 定时器唤醒，执行快速采样...");
        // TODO: 快速采样模式（可选）
    }
    
    // ==================== 3. 初始化各功能模块 ====================
    Serial.println("[Setup] 初始化传感器模块...");
    sensors_init();
    
    Serial.println("[Setup] 初始化灌溉控制模块...");
    irrigation_init();
    
    Serial.println("[Setup] 初始化显示模块...");
    display_init();
    
    Serial.println("[Setup] 显示开机动画...");
    display_startup();  // 显示Logo和加载进度
    
    Serial.println("[Setup] 初始化按键模块...");
    button_init();
    
    Serial.println("[Setup] 初始化数据记录模块...");
    datalog_init();
    
    // ==================== 4. 加载保存的配置 ====================
    Serial.println("[Setup] 加载系统配置...");
    IrrigationState &state = irrigation_get_state();
    if (datalog_load_config(state)) {
        Serial.println("[Setup] 配置加载成功");
    } else {
        Serial.println("[Setup] 使用默认配置");
    }
    
    // ==================== 5. 初始化网络 ====================
    Serial.println("[Setup] 初始化WiFi和Web服务器...");
    network_init();
    
    Serial.println("[Setup] 初始化MQTT客户端...");
    mqtt_init();
    
    // ==================== 6. 创建 FreeRTOS 任务 ====================
    Serial.println("[Setup] 创建FreeRTOS任务...");
    rtos_init();
    
    // ==================== 7. 系统准备就绪 ====================
    Serial.println();
    Serial.println("========================================");
    Serial.println("      系统初始化完成！");
    Serial.println("========================================");
    Serial.printf("WiFi IP: %s\n", network_get_ip().c_str());
    Serial.printf("Web界面: http://%s\n", network_get_ip().c_str());
    Serial.printf("mDNS: http://SmartIrrigation.local\n");
    Serial.println("========================================");
    Serial.println();
    
    // 打印系统信息
    Serial.println("--- 系统信息 ---");
    Serial.printf("ESP32 芯片: %s Rev %d\n", 
                  ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("CPU频率: %u MHz\n", ESP.getCpuFreqMhz());
    Serial.printf("Flash大小: %u KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("可用内存: %u KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("PSRAM: %s\n", psramFound() ? "有" : "无");
    Serial.println("----------------");
    Serial.println();
    
    // 打印传感器初始数据
    Serial.println("--- 传感器初始读取 ---");
    SensorData initialData = sensors_read();
    sensors_print(initialData);
    Serial.println();
    
    // 打印灌溉系统状态
    irrigation_print_status();
    Serial.println();
    
    // 打印电源信息
    power_print_info();
    Serial.println();
    
    // 蜂鸣器提示系统启动
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    
    Serial.println("[Setup] 系统运行中...");
    Serial.println();
}

/**
 * @brief 主循环
 * 
 * 在FreeRTOS环境下，loop()的作用被弱化
 * 大部分工作由FreeRTOS任务完成
 * 这里只需处理ArduinoOTA
 */
void loop() {
    // ArduinoOTA处理（必须在loop中调用）
    ArduinoOTA.handle();
    
    // 延时，让出CPU给其他任务
    vTaskDelay(pdMS_TO_TICKS(100));
}
