/**
 * @file smart_irrigation.ino
 * @brief 智能灌溉系统主程序
 * @author Smart Irrigation Project
 * @date 2026
 * 
 * 功能特性：
 * - 土壤湿度实时检测
 * - 环境温湿度监测
 * - 三种灌溉模式（自动/定时/手动）
 * - OLED实时状态显示
 * - 水箱水位检测与低水位报警
 * - WiFi连接与Web控制界面
 * - 安全保护（浇水超时保护、缺水保护）
 */

#include "config.h"
#include "sensors.h"
#include "irrigation.h"
#include "display.h"
#include "network.h"
#include "button.h"

// 全局变量
SensorData sensorData;
IrrigationState irrigationState;
unsigned long lastCheckTime = 0;

/**
 * @brief 初始化函数 - 系统启动时执行一次
 */
void setup() {
    // 初始化串口通信
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================");
    Serial.println("   智能灌溉系统启动中...");
    Serial.println("   Smart Irrigation System Starting...");
    Serial.println("========================================\n");
    
    // 初始化各个模块
    sensors_init();      // 传感器模块
    irrigation_init();   // 灌溉控制模块
    display_init();      // OLED显示模块
    button_init();       // 按键模块
    
    // 显示开机画面
    display_startup();
    
    // 初始化WiFi和Web服务器
    network_init();
    
    // 设置网络模块的数据指针
    extern void network_set_data(SensorData*, IrrigationState*);
    network_set_data(&sensorData, &irrigationState);
    
    Serial.println("\n系统初始化完成！");
    Serial.println("开始运行主循环...\n");
}

/**
 * @brief 主循环函数 - 持续执行
 */
void loop() {
    // 1. 检测按键
    button_check();
    
    // 2. 定时读取传感器数据
    unsigned long currentTime = millis();
    if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
        lastCheckTime = currentTime;
        
        // 读取传感器
        sensorData = sensors_read();
        
        // 串口输出传感器数据（用于调试）
        Serial.println("========== 传感器数据 ==========");
        Serial.print("土壤湿度: ");
        Serial.print(sensorData.soilMoisture, 1);
        Serial.println(" %");
        
        Serial.print("温度: ");
        if (sensorData.temperature > 0) {
            Serial.print(sensorData.temperature, 1);
            Serial.println(" °C");
        } else {
            Serial.println("读取失败");
        }
        
        Serial.print("湿度: ");
        if (sensorData.humidity > 0) {
            Serial.print(sensorData.humidity, 1);
            Serial.println(" %");
        } else {
            Serial.println("读取失败");
        }
        
        Serial.print("水箱状态: ");
        Serial.println(sensorData.waterLevelOk ? "正常" : "缺水！");
        
        // 获取灌溉状态
        irrigationState = irrigation_get_state();
        
        // 串口输出系统状态
        Serial.println("\n========== 系统状态 ==========");
        Serial.print("灌溉模式: ");
        switch (irrigationState.mode) {
            case MODE_AUTO:   Serial.println("自动模式"); break;
            case MODE_TIMED:  Serial.println("定时模式"); break;
            case MODE_MANUAL: Serial.println("手动模式"); break;
        }
        
        Serial.print("水泵状态: ");
        Serial.println(irrigationState.pumpRunning ? "运行中" : "已停止");
        
        Serial.print("浇水次数: ");
        Serial.println(irrigationState.waterCount);
        
        Serial.print("WiFi状态: ");
        Serial.println(network_is_connected() ? "已连接" : "未连接");
        
        Serial.println("================================\n");
    }
    
    // 3. 更新灌溉控制逻辑
    irrigation_update(sensorData);
    
    // 4. 安全检查
    irrigation_safety_check();
    
    // 5. 更新OLED显示
    irrigationState = irrigation_get_state();
    display_update(sensorData, irrigationState);
    
    // 6. 处理Web请求
    network_handle_client();
    
    // 短暂延迟，避免CPU占用过高
    delay(100);
}
