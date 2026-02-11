/**
 * @file mqtt_client.h
 * @brief MQTT 客户端模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现MQTT物联网通信，支持：
 * - 发布传感器数据
 * - 订阅控制命令
 * - 自动重连
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "sensors.h"
#include "irrigation.h"
#include "rtos_tasks.h"

// ==================== MQTT 主题定义 ====================
#define MQTT_TOPIC_STATUS       "home/irrigation/status"
#define MQTT_TOPIC_SENSOR_CH0   "home/irrigation/sensor/ch0"
#define MQTT_TOPIC_SENSOR_CH1   "home/irrigation/sensor/ch1"
#define MQTT_TOPIC_SENSOR_CH2   "home/irrigation/sensor/ch2"
#define MQTT_TOPIC_SENSOR_CH3   "home/irrigation/sensor/ch3"
#define MQTT_TOPIC_ENVIRONMENT  "home/irrigation/environment"
#define MQTT_TOPIC_ALARM        "home/irrigation/alarm"

#define MQTT_TOPIC_CMD_PUMP      "home/irrigation/cmd/pump"
#define MQTT_TOPIC_CMD_MODE      "home/irrigation/cmd/mode"
#define MQTT_TOPIC_CMD_THRESHOLD "home/irrigation/cmd/threshold"
#define MQTT_TOPIC_CMD_SYSTEM    "home/irrigation/cmd/system"

// ==================== 全局对象 ====================
extern WiFiClient mqttWiFiClient;
extern PubSubClient mqttClient;

// ==================== 函数声明 ====================

/**
 * @brief 初始化MQTT客户端
 * 
 * 设置MQTT服务器、端口、回调函数
 */
void mqtt_init();

/**
 * @brief 连接到MQTT服务器
 * @return true=连接成功, false=连接失败
 * 
 * 连接成功后自动订阅命令主题
 */
bool mqtt_connect();

/**
 * @brief 检查MQTT连接状态
 * @return true=已连接, false=未连接
 */
bool mqtt_is_connected();

/**
 * @brief 处理MQTT循环（在任务中调用）
 * 
 * 处理接收消息和维持连接
 */
void mqtt_handle();

/**
 * @brief 发布传感器数据
 * @param data 传感器数据结构
 * 
 * 发布所有传感器数据到相应主题
 */
void mqtt_publish_sensor_data(SensorData &data);

/**
 * @brief 发布系统状态
 * @param state 灌溉系统状态
 * 
 * 发布系统状态JSON到status主题
 */
void mqtt_publish_status(IrrigationState &state);

/**
 * @brief 发布报警信息
 * @param msg 报警消息
 * 
 * 发布报警信息到alarm主题
 */
void mqtt_publish_alarm(const char *msg);

/**
 * @brief MQTT消息回调函数
 * @param topic 主题
 * @param payload 消息内容
 * @param length 消息长度
 * 
 * 解析接收到的命令并发送到命令队列
 */
void mqtt_callback(char *topic, byte *payload, unsigned int length);

/**
 * @brief 订阅所有命令主题
 */
void mqtt_subscribe_commands();

/**
 * @brief 处理水泵控制命令
 * @param payload JSON字符串
 * 
 * 格式：{"channel": 0, "action": "start"|"stop", "pwm": 255}
 */
void mqtt_handle_pump_command(const char *payload);

/**
 * @brief 处理模式切换命令
 * @param payload JSON字符串
 * 
 * 格式：{"mode": "auto_pid"|"auto_simple"|"timed"|"manual"}
 */
void mqtt_handle_mode_command(const char *payload);

/**
 * @brief 处理阈值设置命令
 * @param payload JSON字符串
 * 
 * 格式：{"channel": 0, "dry": 55, "wet": 80}
 */
void mqtt_handle_threshold_command(const char *payload);

/**
 * @brief 处理系统命令
 * @param payload JSON字符串
 * 
 * 格式：{"command": "reboot"|"sleep"|"reset"}
 */
void mqtt_handle_system_command(const char *payload);

/**
 * @brief 重连MQTT（带退避延迟）
 * @return true=重连成功, false=重连失败
 */
bool mqtt_reconnect();

#endif // MQTT_CLIENT_H
