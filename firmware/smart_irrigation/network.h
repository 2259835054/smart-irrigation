/**
 * @file network.h
 * @brief 网络模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现WiFi连接、Web Server、RESTful API、OTA无线升级
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "sensors.h"
#include "irrigation.h"

// ==================== 全局对象 ====================
extern WebServer server;

// ==================== 函数声明 ====================

/**
 * @brief 初始化网络模块
 * 
 * 执行以下操作：
 * 1. 连接WiFi
 * 2. 注册Web Server路由
 * 3. 初始化OTA
 * 4. 启动Web Server
 */
void network_init();

/**
 * @brief 处理网络请求（在任务中调用）
 * 
 * 处理HTTP请求和OTA
 */
void network_handle();

/**
 * @brief 连接WiFi
 * @return true=连接成功, false=连接失败
 */
bool network_connect_wifi();

/**
 * @brief 检查WiFi连接状态
 * @return true=已连接, false=未连接
 */
bool network_is_connected();

/**
 * @brief 获取WiFi信号强度
 * @return RSSI值 (dBm)
 */
int network_get_rssi();

/**
 * @brief 获取IP地址
 * @return IP地址字符串
 */
String network_get_ip();

/**
 * @brief 获取MAC地址
 * @return MAC地址字符串
 */
String network_get_mac();

// ==================== Web Server 路由处理函数 ====================

/**
 * @brief 处理根路径请求 (/)
 * 
 * 返回Web控制页面HTML
 */
void handle_root();

/**
 * @brief 处理获取状态请求 (GET /api/status)
 * 
 * 返回JSON格式的传感器数据和系统状态
 */
void handle_api_status();

/**
 * @brief 处理获取历史数据请求 (GET /api/history)
 * 
 * 返回JSON格式的历史数据数组
 */
void handle_api_history();

/**
 * @brief 处理控制命令请求 (POST /api/control)
 * 
 * 接收JSON格式的控制命令：
 * {"channel": 0, "action": "start"|"stop"}
 */
void handle_api_control();

/**
 * @brief 处理模式切换请求 (POST /api/mode)
 * 
 * 接收JSON格式的模式设置：
 * {"mode": "auto_pid"|"auto_simple"|"timed"|"manual"}
 */
void handle_api_mode();

/**
 * @brief 处理阈值设置请求 (POST /api/threshold)
 * 
 * 接收JSON格式的阈值设置：
 * {"channel": 0, "dry": 55, "wet": 80}
 */
void handle_api_threshold();

/**
 * @brief 处理系统信息请求 (GET /api/system)
 * 
 * 返回系统信息JSON
 */
void handle_api_system();

/**
 * @brief 处理重启请求 (POST /api/reboot)
 * 
 * 重启ESP32
 */
void handle_api_reboot();

/**
 * @brief 处理404错误
 */
void handle_not_found();

/**
 * @brief 生成Web控制页面HTML
 * @return HTML字符串
 * 
 * 包含响应式布局、实时数据刷新、控制面板
 */
String generate_html_page();

/**
 * @brief 生成CSS样式
 * @return CSS字符串
 */
String generate_css();

/**
 * @brief 生成JavaScript代码
 * @return JavaScript字符串
 */
String generate_javascript();

/**
 * @brief 设置CORS头（允许跨域）
 */
void set_cors_headers();

/**
 * @brief OTA进度回调
 * @param progress 进度百分比 (0-100)
 */
void ota_on_progress(unsigned int progress, unsigned int total);

/**
 * @brief OTA错误回调
 * @param error 错误代码
 */
void ota_on_error(ota_error_t error);

#endif // NETWORK_H
