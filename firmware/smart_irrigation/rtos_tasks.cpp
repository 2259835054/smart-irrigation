/**
 * @file rtos_tasks.cpp
 * @brief FreeRTOS 任务实现和同步机制
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 本文件实现了智能灌溉系统的核心调度逻辑，包括：
 * - 8个FreeRTOS任务的完整实现
 * - 多核心任务分配（ESP32双核）
 * - 任务间同步机制（互斥量、队列、事件组）
 * - 系统监控和统计
 */

#include "rtos_tasks.h"
#include "config.h"
#include "sensors.h"
#include "irrigation.h"
#include "display.h"
#include "network.h"
#include "mqtt_client.h"
#include "data_logger.h"
#include "button.h"
#include "power_manager.h"

// ==================== 全局同步对象定义 ====================

/**
 * @brief 传感器数据互斥量
 * 保护 g_sensorData 全局变量，防止多任务同时访问
 */
SemaphoreHandle_t xSensorMutex = NULL;

/**
 * @brief 灌溉命令队列
 * 用于从Web/MQTT/按键向灌溉任务发送控制命令
 * 队列长度：10条命令
 */
QueueHandle_t xCommandQueue = NULL;

/**
 * @brief 按键事件队列
 * 用于从按键扫描任务向主任务发送按键事件
 * 队列长度：5条事件
 */
QueueHandle_t xButtonEventQueue = NULL;

/**
 * @brief 系统事件组
 * 用于任务间的事件通知和同步
 * 各位定义见 rtos_tasks.h
 */
EventGroupHandle_t xSystemEventGroup = NULL;

// ==================== 任务句柄定义 ====================

TaskHandle_t hTaskSensor = NULL;
TaskHandle_t hTaskIrrigation = NULL;
TaskHandle_t hTaskDisplay = NULL;
TaskHandle_t hTaskNetwork = NULL;
TaskHandle_t hTaskMQTT = NULL;
TaskHandle_t hTaskDataLog = NULL;
TaskHandle_t hTaskButton = NULL;
TaskHandle_t hTaskWatchdog = NULL;

// ==================== 任务心跳计数器（用于看门狗监控）====================

static volatile uint32_t heartbeat_sensor = 0;
static volatile uint32_t heartbeat_irrigation = 0;
static volatile uint32_t heartbeat_display = 0;
static volatile uint32_t heartbeat_network = 0;
static volatile uint32_t heartbeat_mqtt = 0;
static volatile uint32_t heartbeat_datalog = 0;
static volatile uint32_t heartbeat_button = 0;

// ==================== 任务实现 ====================

/**
 * @brief 传感器数据采集任务
 * 
 * 优先级：3（中等）
 * 核心：Core 0
 * 周期：3秒
 * 
 * 主要功能：
 * 1. 读取所有传感器数据（土壤湿度、温湿度、光照、水位）
 * 2. 应用滤波算法提高精度
 * 3. 使用互斥量保护共享数据
 * 4. 设置事件标志通知其他任务
 */
void TaskSensor(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(3000); // 3秒周期
    
    // 初始化上次唤醒时间
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[传感器任务] 已启动 - Core 0, Priority 3");
    
    while (1) {
        // 更新心跳计数器
        heartbeat_sensor++;
        
        // 读取传感器数据
        Serial.println("[传感器任务] 开始读取传感器数据...");
        SensorData newData = sensors_read();
        
        // 获取互斥量，保护全局传感器数据
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // 临界区：更新全局传感器数据
            g_sensorData = newData;
            
            // 释放互斥量
            xSemaphoreGive(xSensorMutex);
            
            // 设置传感器数据就绪事件
            xEventGroupSetBits(xSystemEventGroup, EVT_SENSOR_READY | EVT_DATA_UPDATED);
            
            // 打印传感器数据（调试用）
            Serial.println("[传感器任务] 数据读取完成:");
            for (int i = 0; i < NUM_CHANNELS; i++) {
                Serial.printf("  通道%d: %.1f%% %s\n", 
                    i, 
                    g_sensorData.channels[i].soilMoisture,
                    g_sensorData.channels[i].isValid ? "有效" : "无效");
            }
            Serial.printf("  温度: %.1f°C, 湿度: %.1f%%, 光照: %.1f%%, 水位: %s\n",
                g_sensorData.temperature,
                g_sensorData.humidity,
                g_sensorData.lightLevel,
                g_sensorData.waterLevelOk ? "正常" : "缺水");
        } else {
            Serial.println("[传感器任务] 错误：无法获取互斥量");
        }
        
        // 检查传感器故障
        bool sensorError = false;
        for (int i = 0; i < NUM_CHANNELS; i++) {
            if (!g_sensorData.channels[i].isValid) {
                sensorError = true;
                Serial.printf("[传感器任务] 警告：通道%d传感器数据无效\n", i);
            }
        }
        
        // 检查水位传感器
        if (!g_sensorData.waterLevelOk) {
            xEventGroupSetBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
            Serial.println("[传感器任务] 警告：水箱缺水！");
            
            // 发送紧急停止命令
            Command cmd;
            cmd.type = CMD_STOP_PUMP;
            cmd.channel = 0xFF; // 0xFF表示所有通道
            cmd.value = 0;
            xQueueSend(xCommandQueue, &cmd, 0);
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 灌溉控制任务
 * 
 * 优先级：4（高）
 * 核心：Core 0
 * 周期：1秒
 * 
 * 主要功能：
 * 1. 等待传感器数据就绪事件
 * 2. 根据当前模式执行控制逻辑（PID/阈值/定时/手动）
 * 3. 处理命令队列中的控制命令
 * 4. 执行安全检查（超时保护、冷却时间、缺水保护）
 * 5. 更新水泵状态
 */
void TaskIrrigation(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1秒周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[灌溉任务] 已启动 - Core 0, Priority 4");
    
    while (1) {
        heartbeat_irrigation++;
        
        // 等待传感器数据就绪事件（最多等待500ms）
        EventBits_t bits = xEventGroupWaitBits(
            xSystemEventGroup,
            EVT_SENSOR_READY,
            pdFALSE,  // 不清除位
            pdTRUE,   // 等待所有位
            pdMS_TO_TICKS(500)
        );
        
        // 处理命令队列中的所有待处理命令
        Command cmd;
        while (xQueueReceive(xCommandQueue, &cmd, 0) == pdTRUE) {
            Serial.printf("[灌溉任务] 收到命令：类型=%d, 通道=%d, 值=%d\n", 
                cmd.type, cmd.channel, cmd.value);
            irrigation_process_command(cmd);
        }
        
        // 执行安全检查
        irrigation_safety_check();
        
        // 获取传感器数据（需要互斥量保护）
        SensorData localData;
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localData = g_sensorData;
            xSemaphoreGive(xSensorMutex);
        } else {
            Serial.println("[灌溉任务] 警告：无法获取传感器数据");
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }
        
        // 执行灌溉控制更新
        if (g_irrigationState.systemEnabled) {
            irrigation_update(localData);
            
            // 获取当前模式
            IrrigationMode mode = irrigation_get_mode();
            
            // 根据不同模式执行控制逻辑
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                if (!g_irrigationState.channels[ch].enabled) {
                    continue;
                }
                
                float moisture = localData.channels[ch].soilMoisture;
                if (!localData.channels[ch].isValid) {
                    Serial.printf("[灌溉任务] 通道%d数据无效，跳过控制\n", ch);
                    continue;
                }
                
                switch (mode) {
                    case MODE_AUTO_PID:
                        // PID自动控制（毕设亮点）
                        irrigation_pid_control(ch, moisture);
                        break;
                        
                    case MODE_AUTO_SIMPLE:
                        // 简单阈值控制
                        irrigation_threshold_control(ch, moisture);
                        break;
                        
                    case MODE_TIMED:
                        // 定时灌溉（根据配置的时间表）
                        {
                            unsigned long currentTime = millis();
                            unsigned long lastWater = g_irrigationState.channels[ch].lastWaterTime;
                            
                            // 每隔TIMED_INTERVAL浇水TIMED_DURATION秒
                            const unsigned long TIMED_INTERVAL = 3600000; // 1小时
                            const unsigned long TIMED_DURATION = 30000;   // 30秒
                            
                            if (currentTime - lastWater > TIMED_INTERVAL) {
                                if (!g_irrigationState.channels[ch].pumpRunning) {
                                    Serial.printf("[灌溉任务] 定时模式：启动通道%d\n", ch);
                                    irrigation_start_pump(ch, 200); // 80%速度
                                }
                            }
                            
                            if (g_irrigationState.channels[ch].pumpRunning) {
                                if (currentTime - g_irrigationState.channels[ch].pumpStartTime > TIMED_DURATION) {
                                    Serial.printf("[灌溉任务] 定时模式：停止通道%d\n", ch);
                                    irrigation_stop_pump(ch);
                                }
                            }
                        }
                        break;
                        
                    case MODE_MANUAL:
                        // 手动模式：不自动控制，仅响应命令
                        break;
                }
            }
            
            // 打印灌溉状态
            static int printCounter = 0;
            if (++printCounter >= 10) { // 每10秒打印一次
                printCounter = 0;
                Serial.printf("[灌溉任务] 模式=%s, 系统=%s\n", 
                    irrigation_get_mode_str(),
                    g_irrigationState.systemEnabled ? "启用" : "禁用");
                for (int i = 0; i < NUM_CHANNELS; i++) {
                    if (g_irrigationState.channels[i].pumpRunning) {
                        Serial.printf("  通道%d: 运行中, PWM=%d, 运行时长=%lus\n",
                            i,
                            g_irrigationState.channels[i].pumpPWM,
                            (millis() - g_irrigationState.channels[i].pumpStartTime) / 1000);
                    }
                }
            }
        } else {
            // 系统禁用时，确保所有水泵关闭
            static bool stopSent = false;
            if (!stopSent) {
                irrigation_stop_all();
                stopSent = true;
            }
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief OLED 显示更新任务
 * 
 * 优先级：1（低）
 * 核心：Core 1
 * 周期：500毫秒
 * 
 * 主要功能：
 * 1. 等待数据更新事件
 * 2. 读取传感器数据和系统状态
 * 3. 根据当前页面更新OLED显示
 * 4. 处理页面切换动画
 */
void TaskDisplay(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(500); // 500ms周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[显示任务] 已启动 - Core 1, Priority 1");
    
    while (1) {
        heartbeat_display++;
        
        // 等待数据更新事件
        xEventGroupWaitBits(
            xSystemEventGroup,
            EVT_DATA_UPDATED,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(100)
        );
        
        // 获取传感器数据和灌溉状态
        SensorData localData;
        IrrigationState localState;
        
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localData = g_sensorData;
            localState = g_irrigationState;
            xSemaphoreGive(xSensorMutex);
        } else {
            // 无法获取数据，显示错误信息
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println("数据读取错误");
            display.display();
            
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }
        
        // 更新显示内容
        display_update(localData, localState);
        
        // 检查WiFi连接状态并更新事件组
        if (WiFi.status() == WL_CONNECTED) {
            xEventGroupSetBits(xSystemEventGroup, EVT_WIFI_CONNECTED);
        } else {
            xEventGroupClearBits(xSystemEventGroup, EVT_WIFI_CONNECTED);
        }
        
        // 检查MQTT连接状态
        if (mqtt_is_connected()) {
            xEventGroupSetBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
        } else {
            xEventGroupClearBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 网络服务任务
 * 
 * 优先级：2（中低）
 * 核心：Core 1
 * 周期：100毫秒
 * 
 * 主要功能：
 * 1. 处理Web Server的HTTP请求
 * 2. 处理OTA无线升级
 * 3. 维护WiFi连接
 * 4. 提供RESTful API接口
 */
void TaskNetwork(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[网络任务] 已启动 - Core 1, Priority 2");
    
    // 等待WiFi连接成功
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        Serial.println("[网络任务] 等待WiFi连接...");
        vTaskDelay(pdMS_TO_TICKS(500));
        retryCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[网络任务] WiFi已连接，IP: %s\n", WiFi.localIP().toString().c_str());
        xEventGroupSetBits(xSystemEventGroup, EVT_WIFI_CONNECTED);
    } else {
        Serial.println("[网络任务] WiFi连接失败");
    }
    
    while (1) {
        heartbeat_network++;
        
        // 检查WiFi连接状态
        if (WiFi.status() != WL_CONNECTED) {
            xEventGroupClearBits(xSystemEventGroup, EVT_WIFI_CONNECTED);
            
            // 尝试重连（每30秒尝试一次）
            static unsigned long lastReconnect = 0;
            if (millis() - lastReconnect > 30000) {
                Serial.println("[网络任务] WiFi断开，尝试重连...");
                network_connect_wifi();
                lastReconnect = millis();
            }
        } else {
            xEventGroupSetBits(xSystemEventGroup, EVT_WIFI_CONNECTED);
        }
        
        // 处理网络请求
        network_handle();
        
        // 处理OTA
        ArduinoOTA.handle();
        
        // 监控网络活动（调试用）
        static int activityCounter = 0;
        if (++activityCounter >= 100) { // 每10秒打印一次
            activityCounter = 0;
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[网络任务] WiFi: %s, RSSI: %d dBm, IP: %s\n",
                    WiFi.SSID().c_str(),
                    WiFi.RSSI(),
                    WiFi.localIP().toString().c_str());
            }
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief MQTT 通信任务
 * 
 * 优先级：2（中低）
 * 核心：Core 1
 * 周期：10秒
 * 
 * 主要功能：
 * 1. 维护MQTT连接
 * 2. 定期发布传感器数据
 * 3. 发布系统状态
 * 4. 处理接收到的命令
 */
void TaskMQTT(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10000); // 10秒周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[MQTT任务] 已启动 - Core 1, Priority 2");
    
    // 等待WiFi连接
    while (!(xEventGroupGetBits(xSystemEventGroup) & EVT_WIFI_CONNECTED)) {
        Serial.println("[MQTT任务] 等待WiFi连接...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 尝试连接MQTT
    if (mqtt_connect()) {
        Serial.println("[MQTT任务] MQTT连接成功");
        xEventGroupSetBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
    } else {
        Serial.println("[MQTT任务] MQTT连接失败");
    }
    
    while (1) {
        heartbeat_mqtt++;
        
        // 检查MQTT连接状态
        if (!mqtt_is_connected()) {
            xEventGroupClearBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
            
            // 尝试重连
            if (xEventGroupGetBits(xSystemEventGroup) & EVT_WIFI_CONNECTED) {
                Serial.println("[MQTT任务] MQTT断开，尝试重连...");
                if (mqtt_connect()) {
                    Serial.println("[MQTT任务] MQTT重连成功");
                    xEventGroupSetBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
                }
            }
        } else {
            xEventGroupSetBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
        }
        
        // 处理MQTT消息循环
        mqtt_handle();
        
        // 发布传感器数据
        if (mqtt_is_connected()) {
            SensorData localData;
            if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                localData = g_sensorData;
                xSemaphoreGive(xSensorMutex);
                
                // 发布传感器数据到MQTT
                mqtt_publish_sensor_data(localData);
                Serial.println("[MQTT任务] 已发布传感器数据");
            }
            
            // 发布系统状态
            IrrigationState state = irrigation_get_state();
            
            // 构建状态JSON
            String statusJson = "{";
            statusJson += "\"mode\":\"" + String(irrigation_get_mode_str()) + "\",";
            statusJson += "\"enabled\":" + String(state.systemEnabled ? "true" : "false") + ",";
            statusJson += "\"uptime\":" + String(millis() / 1000) + ",";
            statusJson += "\"alarm\":" + String(state.alarmActive ? "true" : "false");
            if (state.alarmActive) {
                statusJson += ",\"alarm_msg\":\"" + state.alarmMessage + "\"";
            }
            statusJson += "}";
            
            mqttClient.publish(MQTT_TOPIC_STATUS, statusJson.c_str());
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 数据记录任务
 * 
 * 优先级：1（低）
 * 核心：Core 1
 * 周期：60秒
 * 
 * 主要功能：
 * 1. 定期将传感器数据和系统状态记录到LittleFS
 * 2. 实现环形缓冲区，避免存储溢出
 * 3. 生成历史数据JSON供Web查询
 */
void TaskDataLog(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(60000); // 60秒周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[数据记录任务] 已启动 - Core 1, Priority 1");
    
    while (1) {
        heartbeat_datalog++;
        
        // 等待数据更新事件
        xEventGroupWaitBits(
            xSystemEventGroup,
            EVT_DATA_UPDATED,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(5000)
        );
        
        // 获取传感器数据和灌溉状态
        SensorData localData;
        IrrigationState localState;
        
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            localData = g_sensorData;
            localState = g_irrigationState;
            xSemaphoreGive(xSensorMutex);
        } else {
            Serial.println("[数据记录任务] 警告：无法获取数据");
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }
        
        // 记录数据到文件系统
        Serial.println("[数据记录任务] 记录数据到LittleFS...");
        datalog_add_entry(localData, localState);
        
        // 打印记录信息
        Serial.printf("[数据记录任务] 数据已记录 - 时间戳: %lu\n", millis());
        Serial.printf("  通道湿度: [%.1f, %.1f, %.1f, %.1f]%%\n",
            localData.channels[0].soilMoisture,
            localData.channels[1].soilMoisture,
            localData.channels[2].soilMoisture,
            localData.channels[3].soilMoisture);
        Serial.printf("  环境: %.1f°C, %.1f%%, 光照: %.1f%%\n",
            localData.temperature,
            localData.humidity,
            localData.lightLevel);
        
        // 检查存储空间
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        float usagePercent = (float)usedBytes / totalBytes * 100.0;
        
        Serial.printf("[数据记录任务] 存储使用率: %.1f%% (%u/%u bytes)\n",
            usagePercent, usedBytes, totalBytes);
        
        // 如果存储空间不足，发出警告
        if (usagePercent > 90.0) {
            Serial.println("[数据记录任务] 警告：存储空间不足！");
            xEventGroupSetBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 按键检测任务
 * 
 * 优先级：3（中等）
 * 核心：Core 0
 * 周期：50毫秒
 * 
 * 主要功能：
 * 1. 扫描按键状态
 * 2. 实现状态机消抖
 * 3. 检测短按、长按、双击
 * 4. 将按键事件发送到队列
 */
void TaskButton(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 50ms周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[按键任务] 已启动 - Core 0, Priority 3");
    
    while (1) {
        heartbeat_button++;
        
        // 扫描按键
        button_scan();
        
        // 处理按键事件队列
        ButtonEvent event;
        if (xQueueReceive(xButtonEventQueue, &event, 0) == pdTRUE) {
            Serial.printf("[按键任务] 收到按键事件：类型=%d, 时间戳=%lu\n",
                event.type, event.timestamp);
            
            // 根据事件类型执行相应操作
            switch (event.type) {
                case BTN_EVT_MODE_SHORT:
                    Serial.println("[按键任务] MODE短按 - 切换显示页面");
                    display_next_page();
                    break;
                    
                case BTN_EVT_MODE_LONG:
                    Serial.println("[按键任务] MODE长按 - 切换灌溉模式");
                    {
                        IrrigationMode currentMode = irrigation_get_mode();
                        IrrigationMode newMode;
                        
                        switch (currentMode) {
                            case MODE_AUTO_PID:
                                newMode = MODE_AUTO_SIMPLE;
                                break;
                            case MODE_AUTO_SIMPLE:
                                newMode = MODE_TIMED;
                                break;
                            case MODE_TIMED:
                                newMode = MODE_MANUAL;
                                break;
                            case MODE_MANUAL:
                                newMode = MODE_AUTO_PID;
                                break;
                            default:
                                newMode = MODE_AUTO_PID;
                        }
                        
                        Command cmd;
                        cmd.type = CMD_SWITCH_MODE;
                        cmd.channel = 0;
                        cmd.value = newMode;
                        xQueueSend(xCommandQueue, &cmd, 0);
                    }
                    break;
                    
                case BTN_EVT_SELECT_SHORT:
                    Serial.println("[按键任务] SELECT短按 - 确认/选择");
                    // 可以用于菜单选择等功能
                    break;
                    
                case BTN_EVT_SELECT_LONG:
                    Serial.println("[按键任务] SELECT长按 - 手动浇水开关");
                    {
                        // 切换通道0的手动浇水
                        if (g_irrigationState.channels[0].pumpRunning) {
                            Command cmd;
                            cmd.type = CMD_STOP_PUMP;
                            cmd.channel = 0;
                            cmd.value = 0;
                            xQueueSend(xCommandQueue, &cmd, 0);
                        } else {
                            Command cmd;
                            cmd.type = CMD_MANUAL_WATER;
                            cmd.channel = 0;
                            cmd.value = 200; // PWM值
                            xQueueSend(xCommandQueue, &cmd, 0);
                        }
                    }
                    break;
                    
                case BTN_EVT_SELECT_DOUBLE:
                    Serial.println("[按键任务] SELECT双击 - 紧急停止所有水泵");
                    {
                        Command cmd;
                        cmd.type = CMD_STOP_PUMP;
                        cmd.channel = 0xFF; // 所有通道
                        cmd.value = 0;
                        xQueueSend(xCommandQueue, &cmd, 0);
                        
                        xEventGroupSetBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
                    }
                    break;
            }
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 系统看门狗任务
 * 
 * 优先级：5（最高）
 * 核心：Core 0
 * 周期：5秒
 * 
 * 主要功能：
 * 1. 监控各任务心跳计数器
 * 2. 检测任务卡死或异常
 * 3. 记录系统运行统计
 * 4. 执行电源管理策略
 * 5. 触发系统恢复机制
 */
void TaskWatchdog(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5000); // 5秒周期
    
    xLastWakeTime = xTaskGetTickCount();
    
    Serial.println("[看门狗任务] 已启动 - Core 0, Priority 5");
    
    // 保存上一次的心跳值
    uint32_t last_heartbeat_sensor = 0;
    uint32_t last_heartbeat_irrigation = 0;
    uint32_t last_heartbeat_display = 0;
    uint32_t last_heartbeat_network = 0;
    uint32_t last_heartbeat_mqtt = 0;
    uint32_t last_heartbeat_datalog = 0;
    uint32_t last_heartbeat_button = 0;
    
    while (1) {
        // 检查各任务心跳
        bool taskHealthy = true;
        
        if (heartbeat_sensor == last_heartbeat_sensor) {
            Serial.println("[看门狗] 警告：传感器任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_sensor = heartbeat_sensor;
        
        if (heartbeat_irrigation == last_heartbeat_irrigation) {
            Serial.println("[看门狗] 警告：灌溉任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_irrigation = heartbeat_irrigation;
        
        if (heartbeat_display == last_heartbeat_display) {
            Serial.println("[看门狗] 警告：显示任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_display = heartbeat_display;
        
        if (heartbeat_network == last_heartbeat_network) {
            Serial.println("[看门狗] 警告：网络任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_network = heartbeat_network;
        
        if (heartbeat_mqtt == last_heartbeat_mqtt) {
            Serial.println("[看门狗] 警告：MQTT任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_mqtt = heartbeat_mqtt;
        
        if (heartbeat_datalog == last_heartbeat_datalog) {
            Serial.println("[看门狗] 警告：数据记录任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_datalog = heartbeat_datalog;
        
        if (heartbeat_button == last_heartbeat_button) {
            Serial.println("[看门狗] 警告：按键任务可能已停止响应");
            taskHealthy = false;
        }
        last_heartbeat_button = heartbeat_button;
        
        if (!taskHealthy) {
            Serial.println("[看门狗] 系统健康检查失败！");
            xEventGroupSetBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
            
            // 严重情况下可以触发系统重启
            // ESP.restart();
        }
        
        // 打印系统统计信息
        Serial.println("========== 系统统计 ==========");
        Serial.printf("运行时间: %lu秒\n", millis() / 1000);
        Serial.printf("空闲堆内存: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("最小空闲堆: %u bytes\n", ESP.getMinFreeHeap());
        Serial.printf("堆大小: %u bytes\n", ESP.getHeapSize());
        
        // 打印任务心跳
        Serial.println("任务心跳计数:");
        Serial.printf("  传感器: %u\n", heartbeat_sensor);
        Serial.printf("  灌溉: %u\n", heartbeat_irrigation);
        Serial.printf("  显示: %u\n", heartbeat_display);
        Serial.printf("  网络: %u\n", heartbeat_network);
        Serial.printf("  MQTT: %u\n", heartbeat_mqtt);
        Serial.printf("  数据记录: %u\n", heartbeat_datalog);
        Serial.printf("  按键: %u\n", heartbeat_button);
        
        // 打印事件组状态
        EventBits_t bits = xEventGroupGetBits(xSystemEventGroup);
        Serial.println("系统事件:");
        Serial.printf("  传感器就绪: %s\n", (bits & EVT_SENSOR_READY) ? "是" : "否");
        Serial.printf("  WiFi连接: %s\n", (bits & EVT_WIFI_CONNECTED) ? "是" : "否");
        Serial.printf("  MQTT连接: %s\n", (bits & EVT_MQTT_CONNECTED) ? "是" : "否");
        Serial.printf("  数据更新: %s\n", (bits & EVT_DATA_UPDATED) ? "是" : "否");
        Serial.printf("  报警激活: %s\n", (bits & EVT_ALARM_ACTIVE) ? "是" : "否");
        
        // 获取任务运行时统计
        char statsBuffer[512];
        vTaskGetRunTimeStats(statsBuffer);
        Serial.println("任务CPU使用率:");
        Serial.println(statsBuffer);
        
        Serial.println("==============================");
        
        // 检查电源管理
        power_update();
        
        // 检查内存泄漏
        if (ESP.getFreeHeap() < 10000) { // 小于10KB
            Serial.println("[看门狗] 警告：内存不足！");
            xEventGroupSetBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
        }
        
        // 清除报警状态（如果条件恢复正常）
        if (taskHealthy && ESP.getFreeHeap() > 20000) {
            xEventGroupClearBits(xSystemEventGroup, EVT_ALARM_ACTIVE);
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ==================== 初始化和工具函数 ====================

/**
 * @brief 初始化 FreeRTOS 资源并创建所有任务
 * 
 * 此函数执行以下操作：
 * 1. 创建互斥量、队列、事件组
 * 2. 使用 xTaskCreatePinnedToCore 创建8个任务
 * 3. 将任务绑定到指定的CPU核心
 * 4. 设置任务优先级
 * 
 * 任务分配策略：
 * - Core 0：传感器、灌溉、按键、看门狗（硬实时任务）
 * - Core 1：显示、网络、MQTT、数据记录（软实时任务）
 */
void rtos_init() {
    Serial.println("========== FreeRTOS 初始化 ==========");
    
    // ==================== 创建同步对象 ====================
    
    Serial.println("[RTOS] 创建互斥量...");
    xSensorMutex = xSemaphoreCreateMutex();
    if (xSensorMutex == NULL) {
        Serial.println("[RTOS] 错误：无法创建传感器互斥量！");
        return;
    }
    
    Serial.println("[RTOS] 创建命令队列...");
    xCommandQueue = xQueueCreate(10, sizeof(Command));
    if (xCommandQueue == NULL) {
        Serial.println("[RTOS] 错误：无法创建命令队列！");
        return;
    }
    
    Serial.println("[RTOS] 创建按键事件队列...");
    xButtonEventQueue = xQueueCreate(5, sizeof(ButtonEvent));
    if (xButtonEventQueue == NULL) {
        Serial.println("[RTOS] 错误：无法创建按键事件队列！");
        return;
    }
    
    Serial.println("[RTOS] 创建系统事件组...");
    xSystemEventGroup = xEventGroupCreate();
    if (xSystemEventGroup == NULL) {
        Serial.println("[RTOS] 错误：无法创建系统事件组！");
        return;
    }
    
    // ==================== 创建任务 ====================
    
    BaseType_t result;
    
    // 任务1：传感器数据采集任务
    Serial.println("[RTOS] 创建传感器任务...");
    result = xTaskCreatePinnedToCore(
        TaskSensor,           // 任务函数
        "TaskSensor",         // 任务名称
        4096,                 // 堆栈大小（字节）
        NULL,                 // 任务参数
        3,                    // 优先级：3（中等）
        &hTaskSensor,         // 任务句柄
        0                     // CPU核心：Core 0
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建传感器任务！");
    }
    
    // 任务2：灌溉控制任务
    Serial.println("[RTOS] 创建灌溉任务...");
    result = xTaskCreatePinnedToCore(
        TaskIrrigation,
        "TaskIrrigation",
        4096,
        NULL,
        4,                    // 优先级：4（高）
        &hTaskIrrigation,
        0                     // CPU核心：Core 0
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建灌溉任务！");
    }
    
    // 任务3：OLED显示任务
    Serial.println("[RTOS] 创建显示任务...");
    result = xTaskCreatePinnedToCore(
        TaskDisplay,
        "TaskDisplay",
        4096,
        NULL,
        1,                    // 优先级：1（低）
        &hTaskDisplay,
        1                     // CPU核心：Core 1
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建显示任务！");
    }
    
    // 任务4：网络服务任务
    Serial.println("[RTOS] 创建网络任务...");
    result = xTaskCreatePinnedToCore(
        TaskNetwork,
        "TaskNetwork",
        8192,                 // 网络任务需要更大堆栈
        NULL,
        2,                    // 优先级：2（中低）
        &hTaskNetwork,
        1                     // CPU核心：Core 1
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建网络任务！");
    }
    
    // 任务5：MQTT通信任务
    Serial.println("[RTOS] 创建MQTT任务...");
    result = xTaskCreatePinnedToCore(
        TaskMQTT,
        "TaskMQTT",
        6144,
        NULL,
        2,                    // 优先级：2（中低）
        &hTaskMQTT,
        1                     // CPU核心：Core 1
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建MQTT任务！");
    }
    
    // 任务6：数据记录任务
    Serial.println("[RTOS] 创建数据记录任务...");
    result = xTaskCreatePinnedToCore(
        TaskDataLog,
        "TaskDataLog",
        4096,
        NULL,
        1,                    // 优先级：1（低）
        &hTaskDataLog,
        1                     // CPU核心：Core 1
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建数据记录任务！");
    }
    
    // 任务7：按键检测任务
    Serial.println("[RTOS] 创建按键任务...");
    result = xTaskCreatePinnedToCore(
        TaskButton,
        "TaskButton",
        2048,
        NULL,
        3,                    // 优先级：3（中等）
        &hTaskButton,
        0                     // CPU核心：Core 0
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建按键任务！");
    }
    
    // 任务8：系统看门狗任务
    Serial.println("[RTOS] 创建看门狗任务...");
    result = xTaskCreatePinnedToCore(
        TaskWatchdog,
        "TaskWatchdog",
        4096,
        NULL,
        5,                    // 优先级：5（最高）
        &hTaskWatchdog,
        0                     // CPU核心：Core 0
    );
    if (result != pdPASS) {
        Serial.println("[RTOS] 错误：无法创建看门狗任务！");
    }
    
    Serial.println("========== FreeRTOS 初始化完成 ==========");
    Serial.println("任务分配:");
    Serial.println("  Core 0: 传感器(P3), 灌溉(P4), 按键(P3), 看门狗(P5)");
    Serial.println("  Core 1: 显示(P1), 网络(P2), MQTT(P2), 数据记录(P1)");
    Serial.println("==========================================");
}

/**
 * @brief 获取系统运行统计信息
 * @return 格式化的统计字符串
 * 
 * 包含以下信息：
 * - 系统运行时间
 * - 内存使用情况
 * - 各任务状态
 * - CPU使用率
 * - 事件组状态
 */
String rtos_get_stats() {
    String stats = "===== RTOS 运行统计 =====\n";
    
    // 系统信息
    stats += "系统运行时间: " + String(millis() / 1000) + " 秒\n";
    stats += "空闲堆内存: " + String(ESP.getFreeHeap()) + " bytes\n";
    stats += "最小空闲堆: " + String(ESP.getMinFreeHeap()) + " bytes\n";
    stats += "堆使用率: " + String(100 - (ESP.getFreeHeap() * 100 / ESP.getHeapSize())) + "%\n\n";
    
    // 任务心跳
    stats += "任务心跳计数:\n";
    stats += "  传感器: " + String(heartbeat_sensor) + "\n";
    stats += "  灌溉: " + String(heartbeat_irrigation) + "\n";
    stats += "  显示: " + String(heartbeat_display) + "\n";
    stats += "  网络: " + String(heartbeat_network) + "\n";
    stats += "  MQTT: " + String(heartbeat_mqtt) + "\n";
    stats += "  数据记录: " + String(heartbeat_datalog) + "\n";
    stats += "  按键: " + String(heartbeat_button) + "\n\n";
    
    // 事件组状态
    EventBits_t bits = xEventGroupGetBits(xSystemEventGroup);
    stats += "系统事件:\n";
    stats += "  传感器就绪: " + String((bits & EVT_SENSOR_READY) ? "是" : "否") + "\n";
    stats += "  WiFi连接: " + String((bits & EVT_WIFI_CONNECTED) ? "是" : "否") + "\n";
    stats += "  MQTT连接: " + String((bits & EVT_MQTT_CONNECTED) ? "是" : "否") + "\n";
    stats += "  数据更新: " + String((bits & EVT_DATA_UPDATED) ? "是" : "否") + "\n";
    stats += "  报警激活: " + String((bits & EVT_ALARM_ACTIVE) ? "是" : "否") + "\n\n";
    
    // 队列状态
    stats += "队列状态:\n";
    stats += "  命令队列: " + String(uxQueueMessagesWaiting(xCommandQueue)) + "/10\n";
    stats += "  按键队列: " + String(uxQueueMessagesWaiting(xButtonEventQueue)) + "/5\n\n";
    
    // 任务状态
    stats += "任务状态:\n";
    char taskListBuffer[512];
    vTaskList(taskListBuffer);
    stats += String(taskListBuffer) + "\n";
    
    stats += "=========================\n";
    
    return stats;
}
