# FreeRTOS 任务设计文档

## 文档概述

本文档详细说明了智能灌溉系统基于 FreeRTOS 的多任务设计，包括任务划分、优先级分配、同步机制、任务间通信以及调度策略。

## 1. FreeRTOS 任务概览

系统共运行 **8 个并发任务**，采用 **双核异构分配** 策略，充分利用 ESP32 的双核优势。

### 1.1 任务分配策略

**Core 0（传感器核心）— 硬实时任务**：
- TaskSensor（传感器采集）
- TaskIrrigation（灌溉控制）
- TaskButton（按键处理）
- TaskWatchdog（系统看门狗）

**Core 1（通信核心）— 软实时任务**：
- TaskDisplay（OLED 显示）
- TaskNetwork（Web 服务）
- TaskMQTT（MQTT 通信）
- TaskDataLog（数据记录）

### 1.2 任务优先级总览

| 优先级 | 任务名称 | 核心 | 说明 |
|--------|---------|------|------|
| **5** | TaskWatchdog | Core 0 | 最高优先级，监控系统健康 |
| **4** | TaskIrrigation | Core 0 | 灌溉控制，实时性要求高 |
| **3** | TaskSensor | Core 0 | 传感器采集，周期性执行 |
| **3** | TaskButton | Core 0 | 按键扫描，需快速响应 |
| **2** | TaskNetwork | Core 1 | Web 服务，响应用户请求 |
| **2** | TaskMQTT | Core 1 | MQTT 通信 |
| **1** | TaskDisplay | Core 1 | OLED 显示，可延迟 |
| **1** | TaskDataLog | Core 1 | 数据记录，低优先级 |

**设计理由**：
- 灌溉控制和看门狗为最高优先级，确保系统安全
- 传感器和按键为中等优先级，保证数据及时性
- 通信类任务为中低优先级，避免阻塞控制任务
- 显示和记录为最低优先级，允许延迟

## 2. 详细任务说明

### 2.1 TaskSensor — 传感器采集任务

**基本信息**：
- **任务名称**：TaskSensor
- **优先级**：3（中等）
- **堆栈大小**：4096 字节
- **CPU 核心**：Core 0
- **执行周期**：3 秒

**功能描述**：

该任务负责周期性采集所有传感器数据，并应用滤波算法。

**主要流程**：
```
1. 延迟 3 秒（vTaskDelay）
2. 读取 4 路土壤湿度传感器
   - 每路采集 10 个样本
   - 应用滑动窗口中值滤波
   - 转换为百分比（0-100%）
3. 读取 DHT22 温湿度传感器
   - 温度（-40 ~ 80°C）
   - 湿度（0 ~ 100%）
4. 读取光照传感器
   - BH1750 I2C 读取或 ADC 读取
   - 转换为百分比（0-100%）
5. 读取水位传感器
   - 数字开关量（HIGH/LOW）
6. 获取互斥量（xSensorMutex）
7. 更新全局变量 g_sensorData
8. 释放互斥量
9. 设置事件标志 EVT_SENSOR_READY
10. 返回步骤 1
```

**关键代码片段**：
```cpp
void TaskSensor(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(3000); // 3秒周期
    
    while (1) {
        // 读取传感器数据
        SensorData data = sensors_read();
        
        // 保护共享数据
        if (xSemaphoreTake(xSensorMutex, portMAX_DELAY)) {
            g_sensorData = data;
            xSemaphoreGive(xSensorMutex);
        }
        
        // 设置事件标志
        xEventGroupSetBits(xSystemEventGroup, EVT_SENSOR_READY);
        
        // 精确延时
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

**输出**：更新全局变量 `g_sensorData`，供其他任务使用。

### 2.2 TaskIrrigation — 灌溉控制任务

**基本信息**：
- **任务名称**：TaskIrrigation
- **优先级**：4（高）
- **堆栈大小**：4096 字节
- **CPU 核心**：Core 0
- **执行周期**：1 秒

**功能描述**：

该任务是系统的**核心控制任务**，根据传感器数据和当前模式执行灌溉逻辑。

**主要流程**：
```
1. 延迟 1 秒
2. 等待传感器数据就绪（EVT_SENSOR_READY）
3. 获取互斥量，读取 g_sensorData
4. 检查命令队列（xCommandQueue）
   - 如有命令，执行控制指令
5. 根据当前模式执行控制逻辑：
   - MODE_AUTO_PID：调用 PID 控制器
   - MODE_AUTO_SIMPLE：简单阈值控制
   - MODE_TIMED：定时灌溉
   - MODE_MANUAL：手动控制（跳过自动逻辑）
6. 执行安全检查：
   - 超时保护（单次浇水不超过 30 秒）
   - 冷却时间（两次浇水间隔 60 秒）
   - 缺水保护（水位传感器 LOW 时停泵）
7. 更新灌溉状态（g_irrigationState）
8. 返回步骤 1
```

**PID 控制示例**：
```cpp
// PID 模式
if (mode == MODE_AUTO_PID) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        if (!channel.enabled) continue;
        
        float moisture = g_sensorData.channels[ch].soilMoisture;
        float target = (channel.dryThreshold + channel.wetThreshold) / 2.0;
        
        // PID 计算
        float output = channel.pidController->compute(target, moisture);
        
        // 转换为 PWM（0-255）
        uint8_t pwm = constrain(output, 0, 255);
        
        if (pwm > 20) { // 死区
            irrigation_start_pump(ch, pwm);
        } else {
            irrigation_stop_pump(ch);
        }
    }
}
```

**输出**：PWM 信号（GPIO 16/17/18/19），控制水泵开关。

### 2.3 TaskDisplay — OLED 显示任务

**基本信息**：
- **任务名称**：TaskDisplay
- **优先级**：1（低）
- **堆栈大小**：4096 字节
- **CPU 核心**：Core 1
- **执行周期**：500 毫秒

**功能描述**：

更新 OLED 显示屏内容，实现 6 页面 UI 和数据图表。

**主要流程**：
```
1. 延迟 500 毫秒
2. 获取互斥量，读取传感器数据和灌溉状态
3. 根据当前页面索引渲染 UI：
   - 页面 0：主页（系统状态、IP、温湿度）
   - 页面 1：传感器数据（4 路湿度）
   - 页面 2：灌溉状态（水泵、PWM）
   - 页面 3：历史图表（湿度趋势）
   - 页面 4：系统信息（运行时间、内存）
   - 页面 5：配置页面（阈值设置）
4. 调用 display.display() 刷新屏幕
5. 返回步骤 1
```

**6 个页面内容**：

| 页面 | 内容 |
|------|------|
| 0 - 主页 | Logo、模式、IP 地址、温度、湿度 |
| 1 - 传感器 | 4 路土壤湿度柱状图、光照、水位 |
| 2 - 灌溉 | 4 路水泵状态、PWM 值、浇水次数 |
| 3 - 图表 | 湿度趋势折线图（最近 20 个数据点） |
| 4 - 系统 | 运行时间、WiFi 信号、内存使用 |
| 5 - 配置 | 干燥/湿润阈值、PID 参数 |

**页面切换**：通过按键事件切换，按 SELECT 键翻页。

### 2.4 TaskNetwork — Web 服务任务

**基本信息**：
- **任务名称**：TaskNetwork
- **优先级**：2（中低）
- **堆栈大小**：8192 字节（网络任务需要更大堆栈）
- **CPU 核心**：Core 1
- **执行周期**：100 毫秒

**功能描述**：

处理 HTTP 请求，提供 Web 界面和 REST API 服务。

**主要流程**：
```
1. 延迟 100 毫秒
2. 调用 server.handleClient() 处理 HTTP 请求
3. 如有 OTA 升级请求，处理固件上传
4. 返回步骤 1
```

**8 个 API 端点**：

| 端点 | 方法 | 功能 |
|------|------|------|
| `/api/status` | GET | 返回系统状态（模式、运行时间、报警） |
| `/api/history` | GET | 返回历史数据（JSON 数组） |
| `/api/control` | POST | 启动/停止水泵 |
| `/api/mode` | POST | 切换灌溉模式 |
| `/api/threshold` | POST | 设置阈值参数 |
| `/api/system` | GET | 返回系统信息（内存、WiFi） |
| `/api/reboot` | POST | 重启 ESP32 |
| `/update` | POST | OTA 固件升级 |

**响应式 Web 界面**：
- 自适应布局（PC/平板/手机）
- 实时数据刷新（JavaScript setInterval 2秒）
- Bootstrap 风格 UI

### 2.5 TaskMQTT — MQTT 通信任务

**基本信息**：
- **任务名称**：TaskMQTT
- **优先级**：2（中低）
- **堆栈大小**：6144 字节
- **CPU 核心**：Core 1
- **执行周期**：10 秒

**功能描述**：

与 MQTT Broker 通信，发布传感器数据并订阅控制命令。

**主要流程**：
```
1. 延迟 10 秒
2. 检查 MQTT 连接状态
   - 如未连接，尝试重连
3. 调用 client.loop() 处理订阅消息
4. 发布传感器数据到 home/irrigation/sensors
5. 发布系统状态到 home/irrigation/status
6. 返回步骤 1
```

**发布主题**：
- `home/irrigation/sensors` — JSON 格式传感器数据
- `home/irrigation/status` — JSON 格式系统状态

**订阅主题**：
- `home/irrigation/control` — 接收启动/停止命令
- `home/irrigation/mode` — 接收模式切换命令

**MQTT 回调函数**：
```cpp
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    // 解析 JSON 消息
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);
    
    if (strcmp(topic, "home/irrigation/control") == 0) {
        Command cmd;
        cmd.type = doc["action"] == "start" ? CMD_START_PUMP : CMD_STOP_PUMP;
        cmd.channel = doc["channel"];
        cmd.value = doc["pwm"];
        xQueueSend(xCommandQueue, &cmd, 0);
    }
}
```

### 2.6 TaskDataLog — 数据记录任务

**基本信息**：
- **任务名称**：TaskDataLog
- **优先级**：1（低）
- **堆栈大小**：4096 字节
- **CPU 核心**：Core 1
- **执行周期**：60 秒

**功能描述**：

将传感器数据和系统状态记录到 LittleFS 文件系统。

**主要流程**：
```
1. 延迟 60 秒
2. 获取互斥量，读取传感器数据和灌溉状态
3. 构造 JSON 对象
4. 追加到 /data_log.json 文件
5. 如文件超过最大条数（1440），删除最旧记录
6. 返回步骤 1
```

**JSON 格式示例**：
```json
{
  "timestamp": 1612345678,
  "channels": [
    {"id": 0, "moisture": 67.5, "pumpOn": true, "pwm": 180},
    {"id": 1, "moisture": 72.1, "pumpOn": false, "pwm": 0}
  ],
  "temperature": 23.5,
  "humidity": 65.2,
  "lightLevel": 45.0
}
```

**文件管理**：
- 保留最近 1440 条记录（24 小时 × 60 分钟）
- 循环覆盖旧数据
- 支持通过 Web API 查询历史

### 2.7 TaskButton — 按键处理任务

**基本信息**：
- **任务名称**：TaskButton
- **优先级**：3（中等）
- **堆栈大小**：2048 字节
- **CPU 核心**：Core 0
- **执行周期**：50 毫秒

**功能描述**：

扫描按键输入，实现状态机消抖，识别短按、长按、双击事件。

**主要流程**：
```
1. 延迟 50 毫秒
2. 读取 2 个按键 GPIO 状态
   - BUTTON_MODE_PIN (GPIO 15)
   - BUTTON_SELECT_PIN (GPIO 13)
3. 应用状态机消抖算法（3 次稳定读数）
4. 检测按键事件：
   - 短按（< 500ms）
   - 长按（> 2000ms）
   - 双击（两次短按间隔 < 300ms）
5. 构造 ButtonEvent 结构
6. 发送到 xButtonEventQueue
7. 返回步骤 1
```

**按键功能映射**：

| 按键 | 短按 | 长按 |
|------|------|------|
| MODE | 循环切换 4 种模式 | 进入配置菜单 |
| SELECT | OLED 翻页 | 确认设置 |

**状态机消抖**：
```
IDLE → PRESSED_1 → PRESSED_2 → PRESSED_3 → CONFIRMED
       ↓            ↓            ↓
     IDLE         IDLE         IDLE
```

只有连续 3 次读取为稳定状态才确认按键动作。

### 2.8 TaskWatchdog — 系统看门狗任务

**基本信息**：
- **任务名称**：TaskWatchdog
- **优先级**：5（最高）
- **堆栈大小**：4096 字节
- **CPU 核心**：Core 0
- **执行周期**：5 秒

**功能描述**：

监控系统健康状态，检测任务死锁或异常。

**主要流程**：
```
1. 延迟 5 秒
2. 检查各任务心跳计数器
   - 传感器任务应在 10 秒内更新
   - 灌溉任务应在 5 秒内更新
3. 检查系统关键状态
   - WiFi 连接状态
   - 内存使用率（< 80%）
   - 堆栈溢出检测
4. 如检测到异常：
   - 记录错误日志
   - 尝试恢复（重启任务）
   - 严重错误则重启系统
5. 喂狗（重置硬件看门狗定时器）
6. 返回步骤 1
```

**监控指标**：
- 任务心跳超时
- 内存泄漏（堆内存持续下降）
- 堆栈溢出
- WiFi 断开超过 1 分钟

## 3. 任务间同步机制

### 3.1 互斥量（Mutex）

**用途**：保护共享数据结构，避免竞争条件。

**全局互斥量**：
```cpp
SemaphoreHandle_t xSensorMutex; // 保护 g_sensorData
```

**使用场景**：
- TaskSensor 写入传感器数据时加锁
- TaskIrrigation、TaskDisplay、TaskNetwork 读取时加锁

**示例代码**：
```cpp
// 写入
if (xSemaphoreTake(xSensorMutex, portMAX_DELAY)) {
    g_sensorData = newData;
    xSemaphoreGive(xSensorMutex);
}

// 读取
if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100))) {
    float moisture = g_sensorData.channels[0].soilMoisture;
    xSemaphoreGive(xSensorMutex);
}
```

### 3.2 队列（Queue）

**用途**：任务间消息传递，解耦命令发送者和处理者。

**全局队列**：
```cpp
QueueHandle_t xCommandQueue;      // 控制命令队列（容量 10）
QueueHandle_t xButtonEventQueue;  // 按键事件队列（容量 5）
```

**命令队列流程**：
```
发送端（TaskNetwork/TaskMQTT/TaskButton）
    ↓
xQueueSend(xCommandQueue, &cmd, 0)
    ↓
接收端（TaskIrrigation）
    ↓
xQueueReceive(xCommandQueue, &cmd, 0)
    ↓
irrigation_process_command(cmd)
```

**命令结构**：
```cpp
struct Command {
    CommandType type;  // CMD_START_PUMP, CMD_STOP_PUMP, ...
    uint8_t channel;   // 0-3
    int32_t value;     // 附加参数（如 PWM 值）
};
```

### 3.3 事件组（Event Group）

**用途**：同步多个任务的状态标志。

**全局事件组**：
```cpp
EventGroupHandle_t xSystemEventGroup;
```

**事件位定义**：
```cpp
#define EVT_SENSOR_READY    (1 << 0)  // 传感器数据已更新
#define EVT_WIFI_CONNECTED  (1 << 1)  // WiFi 已连接
#define EVT_MQTT_CONNECTED  (1 << 2)  // MQTT 已连接
#define EVT_DATA_UPDATED    (1 << 3)  // 数据已更新
#define EVT_ALARM_ACTIVE    (1 << 4)  // 报警激活
```

**使用示例**：
```cpp
// 设置事件位
xEventGroupSetBits(xSystemEventGroup, EVT_SENSOR_READY);

// 等待事件位
EventBits_t bits = xEventGroupWaitBits(
    xSystemEventGroup,
    EVT_SENSOR_READY,  // 等待的位
    pdTRUE,            // 清除位
    pdFALSE,           // 任意位满足即可
    pdMS_TO_TICKS(5000) // 超时 5 秒
);
```

### 3.4 共享资源保护策略

**原则**：
1. **最小临界区**：持有锁的时间尽可能短
2. **避免嵌套锁**：防止死锁
3. **优先级继承**：使用互斥量而非二值信号量

**保护的共享资源**：
- `g_sensorData` — 传感器数据（xSensorMutex）
- `g_irrigationState` — 灌溉状态（内嵌于 irrigation 模块）
- LittleFS 文件系统 — 单任务访问（TaskDataLog）

## 4. 任务调度时序图

```
时间轴 (毫秒)
  0     50    100   500   1000  2000  3000  5000  10000  60000
  |-----|-----|-----|-----|-----|-----|-----|------|------|------|

TaskWatchdog (优先级5, Core0, 5s周期)
  └──────────────────────────────────────────┴───────────────────→

TaskIrrigation (优先级4, Core0, 1s周期)
  └────┴────┴────┴────┴────┴────┴────┴────┴────→

TaskSensor (优先级3, Core0, 3s周期)
  └──────────────────────────┴──────────────────────────→

TaskButton (优先级3, Core0, 50ms周期)
  └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─→

TaskNetwork (优先级2, Core1, 100ms周期)
  └──┴──┴──┴──┴──┴──┴──┴──┴──→

TaskMQTT (优先级2, Core1, 10s周期)
  └──────────────────────────────────────────────────┴───────────→

TaskDisplay (优先级1, Core1, 500ms周期)
  └────────┴────────┴────────┴────────→

TaskDataLog (优先级1, Core1, 60s周期)
  └──────────────────────────────────────────────────────────────┴→
```

**调度特点**：
- 高优先级任务优先执行（TaskWatchdog、TaskIrrigation）
- 相同优先级任务时间片轮转
- 阻塞的任务让出 CPU（vTaskDelay）
- 事件驱动（等待队列、事件组）

## 5. 优先级设计理由

### 5.1 优先级 5 — TaskWatchdog

**理由**：
- 系统安全的最后一道防线
- 必须能抢占所有其他任务
- 检测死锁和异常并及时恢复

### 5.2 优先级 4 — TaskIrrigation

**理由**：
- 灌溉控制是系统核心功能
- 实时性要求高（1 秒响应）
- 安全保护逻辑必须及时执行

### 5.3 优先级 3 — TaskSensor 和 TaskButton

**理由**：
- 传感器数据是控制的输入，需及时更新
- 按键需快速响应用户操作（< 100ms）
- 中等优先级平衡实时性和系统负载

### 5.4 优先级 2 — TaskNetwork 和 TaskMQTT

**理由**：
- 通信任务可容忍一定延迟
- 避免阻塞高优先级控制任务
- WiFi 中断可能占用较长时间

### 5.5 优先级 1 — TaskDisplay 和 TaskDataLog

**理由**：
- 显示更新延迟 500ms 用户不敏感
- 数据记录可推迟（60 秒周期）
- 最低优先级避免影响核心功能

## 6. 内存和性能分析

### 6.1 堆栈大小分配

| 任务 | 堆栈大小 | 实际使用 | 余量 |
|------|---------|---------|------|
| TaskSensor | 4096 | ~2500 | 39% |
| TaskIrrigation | 4096 | ~2800 | 32% |
| TaskDisplay | 4096 | ~3200 | 22% |
| TaskNetwork | 8192 | ~6500 | 21% |
| TaskMQTT | 6144 | ~4800 | 22% |
| TaskDataLog | 4096 | ~3000 | 27% |
| TaskButton | 2048 | ~800 | 61% |
| TaskWatchdog | 4096 | ~1200 | 71% |
| **总计** | **36864** | **~25000** | **32%** |

**说明**：
- TaskNetwork 堆栈最大（8KB），因 HTTP 处理需较大缓冲区
- TaskButton 堆栈最小（2KB），逻辑简单
- 总堆栈占用约 36KB，占 SRAM 的 7%

### 6.2 CPU 利用率估算

**Core 0**：
- TaskSensor：~5%（每 3 秒执行 150ms）
- TaskIrrigation：~8%（每 1 秒执行 80ms）
- TaskButton：~2%（每 50ms 执行 1ms）
- TaskWatchdog：~1%（每 5 秒执行 50ms）
- **总计**：~16%

**Core 1**：
- TaskDisplay：~10%（每 500ms 执行 50ms）
- TaskNetwork：~15%（每 100ms 执行 15ms）
- TaskMQTT：~2%（每 10 秒执行 200ms）
- TaskDataLog：~1%（每 60 秒执行 600ms）
- **总计**：~28%

**结论**：双核平均利用率约 22%，系统负载健康，留有充足余量。

## 7. 任务故障处理

### 7.1 任务死锁检测

TaskWatchdog 通过**心跳计数器**检测任务死锁：

```cpp
// 任务中递增心跳
g_taskHeartbeat[TASK_SENSOR]++;

// Watchdog 中检查
if (g_taskHeartbeat[TASK_SENSOR] == lastHeartbeat) {
    // 10秒未更新，任务疑似死锁
    Serial.println("TaskSensor 死锁！");
    // 尝试删除并重建任务
    vTaskDelete(hTaskSensor);
    xTaskCreatePinnedToCore(...);
}
```

### 7.2 堆栈溢出保护

FreeRTOS 提供堆栈溢出检测钩子函数：

```cpp
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("堆栈溢出：%s\n", pcTaskName);
    // 记录日志并重启
    ESP.restart();
}
```

### 7.3 内存泄漏监控

TaskWatchdog 定期检查可用堆内存：

```cpp
size_t freeHeap = ESP.getFreeHeap();
if (freeHeap < 50000) { // 低于 50KB 告警
    Serial.printf("内存不足：%d 字节\n", freeHeap);
}
```

## 8. 调试和监控

### 8.1 任务统计信息

通过 `vTaskList()` 获取任务运行状态：

```cpp
char buf[512];
vTaskList(buf);
Serial.println(buf);
```

输出示例：
```
Name          State  Prio  Stack  Num
--------------------------------------
TaskSensor    X      3     1596   2
TaskIrrigation R     4     1296   3
TaskDisplay   B      1     896    4
TaskNetwork   B      2     1692   5
```

**状态说明**：
- **R**：运行中（Running）
- **B**：阻塞（Blocked）
- **X**：就绪（Ready）

### 8.2 运行时统计

通过 `vTaskGetRunTimeStats()` 获取 CPU 使用率：

```cpp
char buf[512];
vTaskGetRunTimeStats(buf);
Serial.println(buf);
```

输出示例：
```
Task            Abs Time      % Time
--------------------------------------
TaskIrrigation  12345         25%
TaskNetwork     10000         20%
TaskSensor      8000          16%
```

## 9. 最佳实践总结

### 9.1 任务设计原则

1. **单一职责**：每个任务功能明确，不混杂无关逻辑
2. **周期执行**：使用 `vTaskDelayUntil` 实现精确周期
3. **事件驱动**：优先使用队列和事件组，避免轮询
4. **避免阻塞**：长时间操作分段执行或放到低优先级任务

### 9.2 同步机制选择

| 场景 | 推荐机制 |
|------|---------|
| 保护共享数据 | 互斥量（Mutex） |
| 任务间消息传递 | 队列（Queue） |
| 状态标志同步 | 事件组（Event Group） |
| 任务通知 | 直接任务通知（更轻量） |

### 9.3 性能优化建议

1. **减少锁持有时间**：先复制数据再释放锁
2. **批量处理**：避免频繁切换上下文
3. **优先级倒置避免**：使用优先级继承的互斥量
4. **看门狗超时合理设置**：给任务足够的执行时间

## 10. 总结

本系统的 FreeRTOS 任务设计充分体现了**实时操作系统**的优势：

✅ **并发执行**：8 个任务并发运行，充分利用双核资源
✅ **实时响应**：高优先级任务（灌溉控制、看门狗）快速响应
✅ **资源隔离**：互斥量和队列保证数据一致性
✅ **可维护性**：模块化任务设计，易于扩展和调试

通过合理的优先级分配、任务绑核策略和同步机制，系统实现了**高可靠性、高实时性、高并发性**的目标，是一款**毕业设计级别**的嵌入式物联网系统典范。
