/**
 * @file data_logger.h
 * @brief 数据记录模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现LittleFS数据持久化，包括：
 * - 传感器数据历史记录
 * - 系统配置保存/加载
 * - JSON格式存储
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "irrigation.h"

// ==================== 数据结构定义 ====================

/**
 * @brief 日志条目结构
 */
struct LogEntry {
    unsigned long timestamp;                    // 时间戳（millis或NTP时间）
    float soilMoisture[NUM_CHANNELS];          // 4路土壤湿度
    float temperature;                          // 环境温度
    float humidity;                             // 环境湿度
    float lightLevel;                           // 光照强度
    bool  pumpStatus[NUM_CHANNELS];            // 4路水泵状态
};

// ==================== 函数声明 ====================

/**
 * @brief 初始化数据记录模块
 * 
 * 执行以下操作：
 * 1. 挂载LittleFS文件系统
 * 2. 检查并创建日志文件
 * 3. 检查并创建配置文件
 */
void datalog_init();

/**
 * @brief 添加一条记录
 * @param sensor 传感器数据
 * @param irrigation 灌溉系统状态
 * 
 * 将当前数据添加到日志文件（JSON数组格式）
 * 超过MAX_LOG_ENTRIES后覆盖最旧数据（环形缓冲）
 */
void datalog_add_entry(SensorData &sensor, IrrigationState &irrigation);

/**
 * @brief 获取历史数据JSON
 * @param count 获取最近N条记录（0=全部）
 * @return JSON字符串
 * 
 * 返回格式：
 * [
 *   {"timestamp": 123456, "ch0": 65.2, "ch1": 70.5, ...},
 *   ...
 * ]
 */
String datalog_get_history_json(int count);

/**
 * @brief 清空日志
 * 
 * 删除日志文件并重新创建
 */
void datalog_clear();

/**
 * @brief 获取日志条目数量
 * @return 条目数量
 */
int datalog_get_entry_count();

/**
 * @brief 保存系统配置
 * @param state 灌溉系统状态
 * 
 * 将用户配置保存到JSON文件：
 * - 灌溉模式
 * - 各通道阈值
 * - 各通道启用状态
 * - PID参数
 */
void datalog_save_config(IrrigationState &state);

/**
 * @brief 加载系统配置
 * @param state 灌溉系统状态（输出参数）
 * @return true=加载成功, false=加载失败或文件不存在
 * 
 * 从JSON文件加载用户配置
 */
bool datalog_load_config(IrrigationState &state);

/**
 * @brief 获取文件系统信息
 * @param total 总空间（字节）
 * @param used 已用空间（字节）
 * @return true=成功, false=失败
 */
bool datalog_get_fs_info(size_t &total, size_t &used);

/**
 * @brief 读取文件内容
 * @param filename 文件路径
 * @return 文件内容字符串
 */
String datalog_read_file(const char *filename);

/**
 * @brief 写入文件内容
 * @param filename 文件路径
 * @param content 内容字符串
 * @return true=写入成功, false=写入失败
 */
bool datalog_write_file(const char *filename, const String &content);

/**
 * @brief 删除文件
 * @param filename 文件路径
 * @return true=删除成功, false=删除失败
 */
bool datalog_delete_file(const char *filename);

/**
 * @brief 列出目录内容
 * @param dirname 目录路径
 * @return 文件列表字符串
 */
String datalog_list_dir(const char *dirname);

/**
 * @brief 格式化文件系统
 * @return true=格式化成功, false=格式化失败
 * 
 * 警告：此操作将删除所有数据！
 */
bool datalog_format();

/**
 * @brief 打印文件系统信息到串口（调试用）
 */
void datalog_print_fs_info();

#endif // DATA_LOGGER_H
