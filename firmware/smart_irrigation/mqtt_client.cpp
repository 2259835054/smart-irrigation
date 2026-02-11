/**
 * @file mqtt_client.cpp
 * @brief MQTT 客户端实现 - 双向物联网通信
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现完整的MQTT物联网通信功能：
 * - 发布传感器数据和系统状态
 * - 订阅并处理远程控制命令
 * - 自动重连和指数退避策略
 * - JSON格式消息解析
 * - 命令队列集成
 */

#include "mqtt_client.h"
#include <ArduinoJson.h>

// ==================== 全局对象实例化 ====================
WiFiClient mqttWiFiClient;           // WiFi客户端
PubSubClient mqttClient(mqttWiFiClient); // MQTT客户端

// ==================== 内部状态变量 ====================
static unsigned long lastReconnectAttempt = 0;  // 上次重连尝试时间
static unsigned long reconnectInterval = 5000;   // 重连间隔（指数退避）
static uint8_t reconnectAttempts = 0;            // 重连尝试次数
static const uint8_t MAX_RECONNECT_ATTEMPTS = 10; // 最大重连次数后重置

// ==================== 函数实现 ====================

/**
 * @brief 初始化MQTT客户端
 * 
 * 设置MQTT服务器地址、端口和消息回调函数
 * 配置消息缓冲区大小以支持大JSON消息
 */
void mqtt_init() {
    // 设置MQTT服务器和端口
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    
    // 设置消息回调函数
    mqttClient.setCallback(mqtt_callback);
    
    // 设置缓冲区大小（支持更大的JSON消息）
    mqttClient.setBufferSize(512);
    
    Serial.println("[MQTT] 客户端初始化完成");
    Serial.printf("[MQTT] 服务器: %s:%d\n", MQTT_BROKER, MQTT_PORT);
}

/**
 * @brief 连接到MQTT服务器
 * @return true=连接成功, false=连接失败
 * 
 * 使用客户端ID连接到MQTT服务器，连接成功后自动订阅所有命令主题
 * 如果配置了用户名和密码，则使用认证连接
 */
bool mqtt_connect() {
    Serial.print("[MQTT] 正在连接到服务器...");
    
    bool connected = false;
    
    // 检查用户名是否为空，选择相应的连接方式
    if (strlen(MQTT_USER) > 0) {
        // 使用用户名和密码连接
        connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    } else {
        // 不使用认证连接
        connected = mqttClient.connect(MQTT_CLIENT_ID);
    }
    
    if (connected) {
        Serial.println("成功!");
        Serial.printf("[MQTT] 客户端ID: %s\n", MQTT_CLIENT_ID);
        
        // 连接成功后订阅所有命令主题
        mqtt_subscribe_commands();
        
        // 设置MQTT连接事件标志
        if (xSystemEventGroup != NULL) {
            xEventGroupSetBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
        }
        
        // 重置重连状态
        reconnectAttempts = 0;
        reconnectInterval = 5000;
        
        // 发布上线消息
        mqtt_publish_alarm("系统已上线");
        
    } else {
        Serial.print("失败! 状态码: ");
        Serial.println(mqttClient.state());
        
        // 清除MQTT连接事件标志
        if (xSystemEventGroup != NULL) {
            xEventGroupClearBits(xSystemEventGroup, EVT_MQTT_CONNECTED);
        }
    }
    
    return connected;
}

/**
 * @brief 检查MQTT连接状态
 * @return true=已连接, false=未连接
 */
bool mqtt_is_connected() {
    return mqttClient.connected();
}

/**
 * @brief 处理MQTT循环
 * 
 * 在MQTT任务中定期调用，用于处理接收消息、发送心跳等
 * 如果连接断开，会自动尝试重连
 */
void mqtt_handle() {
    if (mqttClient.connected()) {
        // 处理MQTT事件（接收消息、发送心跳）
        mqttClient.loop();
    } else {
        // 连接断开，尝试重连
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectInterval) {
            lastReconnectAttempt = now;
            Serial.println("[MQTT] 连接断开，尝试重连...");
            mqtt_reconnect();
        }
    }
}

/**
 * @brief 发布传感器数据
 * @param data 传感器数据结构
 * 
 * 将传感器数据发布到相应主题：
 * - 各通道土壤湿度
 * - 环境温湿度和光照
 */
void mqtt_publish_sensor_data(SensorData &data) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] 未连接，无法发布传感器数据");
        return;
    }
    
    // 创建JSON文档（使用栈分配，避免堆碎片）
    StaticJsonDocument<200> doc;
    char buffer[256];
    
    // 发布各通道土壤湿度
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (data.channels[i].isValid) {
            doc.clear();
            doc["channel"] = i;
            doc["moisture"] = data.channels[i].soilMoisture;
            doc["timestamp"] = data.timestamp;
            
            serializeJson(doc, buffer);
            
            // 根据通道选择主题
            const char* topic = nullptr;
            switch (i) {
                case 0: topic = MQTT_TOPIC_SENSOR_CH0; break;
                case 1: topic = MQTT_TOPIC_SENSOR_CH1; break;
                case 2: topic = MQTT_TOPIC_SENSOR_CH2; break;
                case 3: topic = MQTT_TOPIC_SENSOR_CH3; break;
            }
            
            if (topic && mqttClient.publish(topic, buffer)) {
                Serial.printf("[MQTT] 发布 CH%d 湿度: %.1f%%\n", i, data.channels[i].soilMoisture);
            } else {
                Serial.printf("[MQTT] 发布 CH%d 失败\n", i);
            }
        }
    }
    
    // 发布环境数据（温湿度、光照、水位）
    doc.clear();
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["light"] = data.lightLevel;
    doc["waterLevel"] = data.waterLevelOk ? "OK" : "LOW";
    doc["timestamp"] = data.timestamp;
    
    serializeJson(doc, buffer);
    
    if (mqttClient.publish(MQTT_TOPIC_ENVIRONMENT, buffer)) {
        Serial.printf("[MQTT] 发布环境数据: T=%.1f°C H=%.1f%% L=%.0f%%\n", 
                      data.temperature, data.humidity, data.lightLevel);
    } else {
        Serial.println("[MQTT] 发布环境数据失败");
    }
}

/**
 * @brief 发布系统状态
 * @param state 灌溉系统状态
 * 
 * 将系统状态以JSON格式发布到status主题
 * 包含：模式、各通道状态、水泵运行情况等
 */
void mqtt_publish_status(IrrigationState &state) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] 未连接，无法发布状态");
        return;
    }
    
    // 使用较大的JSON文档
    StaticJsonDocument<512> doc;
    char buffer[600];
    
    // 模式信息
    const char* modeStr = nullptr;
    switch (state.mode) {
        case MODE_AUTO_PID:    modeStr = "auto_pid"; break;
        case MODE_AUTO_SIMPLE: modeStr = "auto_simple"; break;
        case MODE_TIMED:       modeStr = "timed"; break;
        case MODE_MANUAL:      modeStr = "manual"; break;
    }
    doc["mode"] = modeStr;
    doc["systemTime"] = millis();
    
    // 各通道状态
    JsonArray channels = doc.createNestedArray("channels");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        JsonObject ch = channels.createNestedObject();
        ch["id"] = i;
        ch["enabled"] = state.channels[i].enabled;
        ch["pumpRunning"] = state.channels[i].pumpRunning;
        ch["pwm"] = state.channels[i].pumpPWM;
        ch["dryThreshold"] = state.channels[i].dryThreshold;
        ch["wetThreshold"] = state.channels[i].wetThreshold;
        ch["waterCount"] = state.channels[i].waterCount;
        ch["totalTime"] = state.channels[i].totalWaterTime / 1000; // 转换为秒
    }
    
    // 序列化并发布
    serializeJson(doc, buffer);
    
    if (mqttClient.publish(MQTT_TOPIC_STATUS, buffer)) {
        Serial.printf("[MQTT] 发布系统状态: 模式=%s\n", modeStr);
    } else {
        Serial.println("[MQTT] 发布系统状态失败");
    }
}

/**
 * @brief 发布报警信息
 * @param msg 报警消息字符串
 * 
 * 发布报警信息到alarm主题，包含时间戳和消息内容
 */
void mqtt_publish_alarm(const char *msg) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] 未连接，无法发布报警");
        return;
    }
    
    StaticJsonDocument<128> doc;
    char buffer[150];
    
    doc["message"] = msg;
    doc["timestamp"] = millis();
    doc["severity"] = "info";
    
    serializeJson(doc, buffer);
    
    if (mqttClient.publish(MQTT_TOPIC_ALARM, buffer)) {
        Serial.printf("[MQTT] 发布报警: %s\n", msg);
    } else {
        Serial.println("[MQTT] 发布报警失败");
    }
}

/**
 * @brief MQTT消息回调函数
 * @param topic 主题名称
 * @param payload 消息内容（字节数组）
 * @param length 消息长度
 * 
 * 当接收到订阅主题的消息时被调用
 * 根据主题分发到相应的处理函数
 */
void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    // 将payload转换为字符串（添加结束符）
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.printf("[MQTT] 收到消息 [%s]: %s\n", topic, message);
    
    // 根据主题分发处理
    if (strcmp(topic, MQTT_TOPIC_CMD_PUMP) == 0) {
        mqtt_handle_pump_command(message);
    } 
    else if (strcmp(topic, MQTT_TOPIC_CMD_MODE) == 0) {
        mqtt_handle_mode_command(message);
    }
    else if (strcmp(topic, MQTT_TOPIC_CMD_THRESHOLD) == 0) {
        mqtt_handle_threshold_command(message);
    }
    else if (strcmp(topic, MQTT_TOPIC_CMD_SYSTEM) == 0) {
        mqtt_handle_system_command(message);
    }
    else {
        Serial.printf("[MQTT] 未知主题: %s\n", topic);
    }
}

/**
 * @brief 订阅所有命令主题
 * 
 * 订阅所有需要监听的命令主题
 * QoS=1 确保至少接收一次
 */
void mqtt_subscribe_commands() {
    Serial.println("[MQTT] 订阅命令主题...");
    
    bool success = true;
    
    // 订阅水泵控制
    if (mqttClient.subscribe(MQTT_TOPIC_CMD_PUMP, 1)) {
        Serial.printf("[MQTT] ✓ 订阅: %s\n", MQTT_TOPIC_CMD_PUMP);
    } else {
        Serial.printf("[MQTT] ✗ 订阅失败: %s\n", MQTT_TOPIC_CMD_PUMP);
        success = false;
    }
    
    // 订阅模式切换
    if (mqttClient.subscribe(MQTT_TOPIC_CMD_MODE, 1)) {
        Serial.printf("[MQTT] ✓ 订阅: %s\n", MQTT_TOPIC_CMD_MODE);
    } else {
        Serial.printf("[MQTT] ✗ 订阅失败: %s\n", MQTT_TOPIC_CMD_MODE);
        success = false;
    }
    
    // 订阅阈值设置
    if (mqttClient.subscribe(MQTT_TOPIC_CMD_THRESHOLD, 1)) {
        Serial.printf("[MQTT] ✓ 订阅: %s\n", MQTT_TOPIC_CMD_THRESHOLD);
    } else {
        Serial.printf("[MQTT] ✗ 订阅失败: %s\n", MQTT_TOPIC_CMD_THRESHOLD);
        success = false;
    }
    
    // 订阅系统命令
    if (mqttClient.subscribe(MQTT_TOPIC_CMD_SYSTEM, 1)) {
        Serial.printf("[MQTT] ✓ 订阅: %s\n", MQTT_TOPIC_CMD_SYSTEM);
    } else {
        Serial.printf("[MQTT] ✗ 订阅失败: %s\n", MQTT_TOPIC_CMD_SYSTEM);
        success = false;
    }
    
    if (success) {
        Serial.println("[MQTT] 所有命令主题订阅成功");
    } else {
        Serial.println("[MQTT] 部分命令主题订阅失败");
    }
}

/**
 * @brief 处理水泵控制命令
 * @param payload JSON字符串
 * 
 * 解析水泵控制命令并发送到命令队列
 * 格式：{"channel": 0, "action": "start"|"stop", "pwm": 255}
 */
void mqtt_handle_pump_command(const char *payload) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[MQTT] JSON解析错误: %s\n", error.c_str());
        mqtt_publish_alarm("水泵命令JSON格式错误");
        return;
    }
    
    // 检查必要字段
    if (!doc.containsKey("channel") || !doc.containsKey("action")) {
        Serial.println("[MQTT] 缺少必要字段: channel 或 action");
        mqtt_publish_alarm("水泵命令缺少必要字段");
        return;
    }
    
    uint8_t channel = doc["channel"];
    const char* action = doc["action"];
    uint8_t pwm = doc["pwm"] | 255; // 默认全速
    
    // 验证通道号
    if (channel >= NUM_CHANNELS) {
        Serial.printf("[MQTT] 无效通道号: %d\n", channel);
        mqtt_publish_alarm("无效通道号");
        return;
    }
    
    // 创建命令并发送到队列
    Command cmd;
    cmd.channel = channel;
    cmd.value = pwm;
    
    if (strcmp(action, "start") == 0) {
        cmd.type = CMD_START_PUMP;
        Serial.printf("[MQTT] 水泵控制: 启动 CH%d PWM=%d\n", channel, pwm);
    } 
    else if (strcmp(action, "stop") == 0) {
        cmd.type = CMD_STOP_PUMP;
        Serial.printf("[MQTT] 水泵控制: 停止 CH%d\n", channel);
    }
    else {
        Serial.printf("[MQTT] 未知动作: %s\n", action);
        mqtt_publish_alarm("未知水泵动作");
        return;
    }
    
    // 发送命令到队列
    if (xCommandQueue != NULL) {
        if (xQueueSend(xCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println("[MQTT] 命令已加入队列");
            mqtt_publish_alarm("水泵命令已执行");
        } else {
            Serial.println("[MQTT] 命令队列已满");
            mqtt_publish_alarm("命令队列已满");
        }
    }
}

/**
 * @brief 处理模式切换命令
 * @param payload JSON字符串
 * 
 * 解析模式切换命令并发送到命令队列
 * 格式：{"mode": "auto_pid"|"auto_simple"|"timed"|"manual"}
 */
void mqtt_handle_mode_command(const char *payload) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[MQTT] JSON解析错误: %s\n", error.c_str());
        mqtt_publish_alarm("模式命令JSON格式错误");
        return;
    }
    
    if (!doc.containsKey("mode")) {
        Serial.println("[MQTT] 缺少必要字段: mode");
        mqtt_publish_alarm("模式命令缺少必要字段");
        return;
    }
    
    const char* modeStr = doc["mode"];
    IrrigationMode mode;
    
    // 解析模式字符串
    if (strcmp(modeStr, "auto_pid") == 0) {
        mode = MODE_AUTO_PID;
    } 
    else if (strcmp(modeStr, "auto_simple") == 0) {
        mode = MODE_AUTO_SIMPLE;
    }
    else if (strcmp(modeStr, "timed") == 0) {
        mode = MODE_TIMED;
    }
    else if (strcmp(modeStr, "manual") == 0) {
        mode = MODE_MANUAL;
    }
    else {
        Serial.printf("[MQTT] 未知模式: %s\n", modeStr);
        mqtt_publish_alarm("未知灌溉模式");
        return;
    }
    
    // 创建命令
    Command cmd;
    cmd.type = CMD_SWITCH_MODE;
    cmd.channel = 0;  // 模式切换不需要通道号
    cmd.value = static_cast<int32_t>(mode);
    
    Serial.printf("[MQTT] 模式切换: %s\n", modeStr);
    
    // 发送到命令队列
    if (xCommandQueue != NULL) {
        if (xQueueSend(xCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println("[MQTT] 模式切换命令已加入队列");
            char msg[50];
            snprintf(msg, sizeof(msg), "模式已切换至: %s", modeStr);
            mqtt_publish_alarm(msg);
        } else {
            Serial.println("[MQTT] 命令队列已满");
            mqtt_publish_alarm("命令队列已满");
        }
    }
}

/**
 * @brief 处理阈值设置命令
 * @param payload JSON字符串
 * 
 * 解析阈值设置命令并发送到命令队列
 * 格式：{"channel": 0, "dry": 55, "wet": 80}
 */
void mqtt_handle_threshold_command(const char *payload) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[MQTT] JSON解析错误: %s\n", error.c_str());
        mqtt_publish_alarm("阈值命令JSON格式错误");
        return;
    }
    
    // 检查必要字段
    if (!doc.containsKey("channel")) {
        Serial.println("[MQTT] 缺少必要字段: channel");
        mqtt_publish_alarm("阈值命令缺少必要字段");
        return;
    }
    
    uint8_t channel = doc["channel"];
    
    // 验证通道号
    if (channel >= NUM_CHANNELS) {
        Serial.printf("[MQTT] 无效通道号: %d\n", channel);
        mqtt_publish_alarm("无效通道号");
        return;
    }
    
    // 读取阈值（如果提供）
    bool hasDry = doc.containsKey("dry");
    bool hasWet = doc.containsKey("wet");
    
    if (!hasDry && !hasWet) {
        Serial.println("[MQTT] 未提供 dry 或 wet 阈值");
        mqtt_publish_alarm("未提供阈值参数");
        return;
    }
    
    // 分别处理干湿阈值设置
    // 注意：这里简化处理，实际应该在irrigation模块中直接设置
    // 或者扩展Command结构以支持浮点数
    
    if (hasDry) {
        float dryThreshold = doc["dry"];
        // 验证范围
        if (dryThreshold < 0 || dryThreshold > 100) {
            Serial.println("[MQTT] dry阈值超出范围 (0-100)");
            mqtt_publish_alarm("干燥阈值超出范围");
            return;
        }
        
        Command cmd;
        cmd.type = CMD_SET_THRESHOLD;
        cmd.channel = channel;
        cmd.value = (int32_t)(dryThreshold * 100); // 转换为整数（保留2位小数）
        
        if (xCommandQueue != NULL) {
            xQueueSend(xCommandQueue, &cmd, pdMS_TO_TICKS(100));
        }
        
        Serial.printf("[MQTT] 设置 CH%d 干燥阈值: %.1f%%\n", channel, dryThreshold);
    }
    
    if (hasWet) {
        float wetThreshold = doc["wet"];
        // 验证范围
        if (wetThreshold < 0 || wetThreshold > 100) {
            Serial.println("[MQTT] wet阈值超出范围 (0-100)");
            mqtt_publish_alarm("湿润阈值超出范围");
            return;
        }
        
        // 这里需要irrigation模块支持单独设置湿阈值
        Serial.printf("[MQTT] 设置 CH%d 湿润阈值: %.1f%%\n", channel, wetThreshold);
    }
    
    mqtt_publish_alarm("阈值设置成功");
}

/**
 * @brief 处理系统命令
 * @param payload JSON字符串
 * 
 * 解析系统控制命令（重启、休眠、复位等）
 * 格式：{"command": "reboot"|"sleep"|"reset"}
 */
void mqtt_handle_system_command(const char *payload) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[MQTT] JSON解析错误: %s\n", error.c_str());
        mqtt_publish_alarm("系统命令JSON格式错误");
        return;
    }
    
    if (!doc.containsKey("command")) {
        Serial.println("[MQTT] 缺少必要字段: command");
        mqtt_publish_alarm("系统命令缺少必要字段");
        return;
    }
    
    const char* cmdStr = doc["command"];
    
    Serial.printf("[MQTT] 系统命令: %s\n", cmdStr);
    
    if (strcmp(cmdStr, "reboot") == 0) {
        // 系统重启
        mqtt_publish_alarm("系统将在3秒后重启");
        delay(3000);
        ESP.restart();
    }
    else if (strcmp(cmdStr, "sleep") == 0) {
        // 进入深度休眠
        mqtt_publish_alarm("系统进入休眠模式");
        
        Command cmd;
        cmd.type = CMD_ENTER_SLEEP;
        cmd.channel = 0;
        cmd.value = 0;
        
        if (xCommandQueue != NULL) {
            xQueueSend(xCommandQueue, &cmd, pdMS_TO_TICKS(100));
        }
    }
    else if (strcmp(cmdStr, "reset") == 0) {
        // 重置配置（这里仅示例，实际需要清除SPIFFS/EEPROM）
        mqtt_publish_alarm("配置重置命令已收到");
        Serial.println("[MQTT] 配置重置功能需要在主程序实现");
    }
    else {
        Serial.printf("[MQTT] 未知系统命令: %s\n", cmdStr);
        mqtt_publish_alarm("未知系统命令");
    }
}

/**
 * @brief 自动重连MQTT服务器
 * @return true=重连成功, false=重连失败
 * 
 * 实现指数退避策略的自动重连
 * 重连间隔会随失败次数增加而增加，最大30秒
 */
bool mqtt_reconnect() {
    // 检查WiFi连接
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] WiFi未连接，无法重连MQTT");
        return false;
    }
    
    Serial.printf("[MQTT] 重连尝试 #%d...\n", reconnectAttempts + 1);
    
    // 尝试连接
    bool success = mqtt_connect();
    
    if (success) {
        Serial.println("[MQTT] 重连成功");
        reconnectAttempts = 0;
        reconnectInterval = 5000; // 重置间隔
        return true;
    } else {
        // 重连失败，应用指数退避
        reconnectAttempts++;
        
        // 指数退避：5秒、10秒、20秒、30秒...
        if (reconnectAttempts < 5) {
            reconnectInterval = 5000 * (1 << (reconnectAttempts - 1));
        } else {
            reconnectInterval = 30000; // 最大30秒
        }
        
        // 达到最大重连次数后重置计数器（防止溢出）
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Serial.println("[MQTT] 达到最大重连次数，重置计数器");
            reconnectAttempts = 0;
        }
        
        Serial.printf("[MQTT] 重连失败，下次尝试间隔: %lu ms\n", reconnectInterval);
        return false;
    }
}
