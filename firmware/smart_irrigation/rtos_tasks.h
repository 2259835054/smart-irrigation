/**
 * @file rtos_tasks.h
 * @brief FreeRTOS 任务声明和同步对象定义
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 本文件定义了系统所有 FreeRTOS 任务、同步机制（互斥量、队列、事件组）
 * 以及任务间通信的消息结构
 */

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

// ==================== 事件组位定义 ====================
#define EVT_SENSOR_READY    (1 << 0)  // 传感器数据已更新
#define EVT_WIFI_CONNECTED  (1 << 1)  // WiFi 已连接
#define EVT_MQTT_CONNECTED  (1 << 2)  // MQTT 已连接
#define EVT_DATA_UPDATED    (1 << 3)  // 数据已更新
#define EVT_ALARM_ACTIVE    (1 << 4)  // 报警激活

// ==================== 命令队列消息类型 ====================
enum CommandType {
    CMD_START_PUMP,      // 启动水泵
    CMD_STOP_PUMP,       // 停止水泵
    CMD_SWITCH_MODE,     // 切换模式
    CMD_SET_THRESHOLD,   // 设置阈值
    CMD_MANUAL_WATER,    // 手动浇水
    CMD_ENTER_SLEEP      // 进入休眠
};

/**
 * @brief 命令消息结构
 */
struct Command {
    CommandType type;    // 命令类型
    uint8_t channel;     // 通道号 0-3
    int32_t value;       // 附加参数
};

/**
 * @brief 按键事件类型
 */
enum ButtonEventType {
    BTN_EVT_MODE_SHORT,      // MODE 键短按
    BTN_EVT_MODE_LONG,       // MODE 键长按
    BTN_EVT_SELECT_SHORT,    // SELECT 键短按
    BTN_EVT_SELECT_LONG,     // SELECT 键长按
    BTN_EVT_SELECT_DOUBLE    // SELECT 键双击
};

/**
 * @brief 按键事件消息结构
 */
struct ButtonEvent {
    ButtonEventType type;
    unsigned long timestamp;
};

// ==================== 全局同步对象（extern）====================
extern SemaphoreHandle_t xSensorMutex;        // 传感器数据互斥量
extern QueueHandle_t xCommandQueue;           // 命令队列
extern QueueHandle_t xButtonEventQueue;       // 按键事件队列
extern EventGroupHandle_t xSystemEventGroup;  // 系统事件组

// ==================== 任务句柄（用于监控和调试）====================
extern TaskHandle_t hTaskSensor;
extern TaskHandle_t hTaskIrrigation;
extern TaskHandle_t hTaskDisplay;
extern TaskHandle_t hTaskNetwork;
extern TaskHandle_t hTaskMQTT;
extern TaskHandle_t hTaskDataLog;
extern TaskHandle_t hTaskButton;
extern TaskHandle_t hTaskWatchdog;

// ==================== 任务函数声明 ====================
/**
 * @brief 传感器数据采集任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：3秒
 * 核心：Core 0
 * 功能：读取所有传感器数据，应用滤波算法，更新共享数据结构
 */
void TaskSensor(void *pvParameters);

/**
 * @brief 灌溉控制任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：1秒
 * 核心：Core 0
 * 功能：根据传感器数据和当前模式执行灌溉控制逻辑（PID/阈值/定时）
 */
void TaskIrrigation(void *pvParameters);

/**
 * @brief OLED 显示更新任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：500毫秒
 * 核心：Core 1
 * 功能：更新 OLED 显示内容，处理页面切换
 */
void TaskDisplay(void *pvParameters);

/**
 * @brief 网络服务任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：100毫秒
 * 核心：Core 1
 * 功能：处理 Web Server HTTP 请求和 OTA 更新
 */
void TaskNetwork(void *pvParameters);

/**
 * @brief MQTT 通信任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：10秒
 * 核心：Core 1
 * 功能：发布传感器数据到 MQTT 服务器，处理订阅的命令
 */
void TaskMQTT(void *pvParameters);

/**
 * @brief 数据记录任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：60秒
 * 核心：Core 1
 * 功能：将传感器数据和系统状态记录到 LittleFS
 */
void TaskDataLog(void *pvParameters);

/**
 * @brief 按键检测任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：50毫秒
 * 核心：Core 0
 * 功能：扫描按键输入，实现状态机消抖，发送按键事件
 */
void TaskButton(void *pvParameters);

/**
 * @brief 系统看门狗任务
 * @param pvParameters 任务参数（未使用）
 * 
 * 任务周期：5秒
 * 核心：Core 0
 * 功能：监控各任务心跳，检测系统异常
 */
void TaskWatchdog(void *pvParameters);

// ==================== 初始化函数 ====================
/**
 * @brief 初始化 FreeRTOS 资源并创建所有任务
 * 
 * 创建互斥量、队列、事件组
 * 使用 xTaskCreatePinnedToCore 创建8个任务并绑定到指定核心
 */
void rtos_init();

/**
 * @brief 获取系统运行统计信息
 * @return 格式化的统计字符串
 */
String rtos_get_stats();

#endif // RTOS_TASKS_H
