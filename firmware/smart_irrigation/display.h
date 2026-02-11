/**
 * @file display.h
 * @brief OLED 显示模块头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现OLED多页面显示系统，包括：
 * - 主页：概览所有通道
 * - 通道详情页
 * - 数据图表页
 * - 设置页
 * - 网络状态页
 * - 关于页
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "sensors.h"
#include "irrigation.h"

// ==================== 页面枚举 ====================

/**
 * @brief 显示页面枚举
 */
enum DisplayPage {
    PAGE_MAIN,       // 主页：概览所有通道湿度 + 系统状态
    PAGE_CHANNEL,    // 通道详情：单通道详细数据 + PID 状态
    PAGE_CHART,      // 简易图表：湿度趋势折线（最近 30 个点）
    PAGE_SETTINGS,   // 设置页面：阈值调整、模式切换
    PAGE_NETWORK,    // 网络状态：WiFi/MQTT 连接信息
    PAGE_ABOUT,      // 关于：版本号、运行时间、累计浇水
    PAGE_COUNT       // 页面总数
};

// ==================== 全局对象 ====================
extern Adafruit_SSD1306 display;

// ==================== 函数声明 ====================

/**
 * @brief 初始化 OLED 显示屏
 * 
 * 配置I2C通信，初始化SSD1306驱动
 */
void display_init();

/**
 * @brief 显示开机动画
 * 
 * 显示Logo、进度条、版本号
 */
void display_startup();

/**
 * @brief 更新显示内容
 * @param data 传感器数据
 * @param state 灌溉系统状态
 * 
 * 根据当前页面渲染相应内容
 */
void display_update(SensorData &data, IrrigationState &state);

/**
 * @brief 切换到下一页
 */
void display_next_page();

/**
 * @brief 切换到上一页
 */
void display_prev_page();

/**
 * @brief 设置当前页面
 * @param page 页面枚举
 */
void display_set_page(DisplayPage page);

/**
 * @brief 获取当前页面
 * @return 当前页面枚举
 */
DisplayPage display_get_page();

/**
 * @brief 设置当前通道（用于通道详情页）
 * @param channel 通道号 (0-3)
 */
void display_set_channel(uint8_t channel);

/**
 * @brief 获取当前通道
 * @return 通道号
 */
uint8_t display_get_channel();

/**
 * @brief 绘制主页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_main_page(SensorData &data, IrrigationState &state);

/**
 * @brief 绘制通道详情页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_channel_page(SensorData &data, IrrigationState &state);

/**
 * @brief 绘制图表页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_chart_page(SensorData &data, IrrigationState &state);

/**
 * @brief 绘制设置页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_settings_page(SensorData &data, IrrigationState &state);

/**
 * @brief 绘制网络状态页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_network_page(SensorData &data, IrrigationState &state);

/**
 * @brief 绘制关于页
 * @param data 传感器数据
 * @param state 灌溉系统状态
 */
void display_draw_about_page(SensorData &data, IrrigationState &state);

/**
 * @brief 在OLED上绘制简易折线图
 * @param data 数据数组
 * @param count 数据点数量
 * @param x 图表左上角X坐标
 * @param y 图表左上角Y坐标
 * @param width 图表宽度
 * @param height 图表高度
 * @param title 图表标题
 * 
 * 自动缩放Y轴，显示最大/最小/当前值
 */
void display_draw_chart(float *data, int count, int x, int y, int width, int height, const char *title);

/**
 * @brief 显示报警信息覆盖层
 * @param msg 报警信息
 * 
 * 在当前页面上方显示报警信息，持续3秒
 */
void display_show_alert(const char *msg);

/**
 * @brief 添加数据点到图表缓冲区
 * @param channel 通道号 (0-3)
 * @param value 数据值
 * 
 * 维护环形缓冲区，最多保存30个数据点
 */
void display_add_chart_data(uint8_t channel, float value);

/**
 * @brief 绘制进度条
 * @param x X坐标
 * @param y Y坐标
 * @param width 宽度
 * @param height 高度
 * @param percent 百分比 (0-100)
 */
void display_draw_progress_bar(int x, int y, int width, int height, float percent);

/**
 * @brief 绘制WiFi信号图标
 * @param x X坐标
 * @param y Y坐标
 * @param rssi 信号强度 (dBm)
 */
void display_draw_wifi_icon(int x, int y, int rssi);

/**
 * @brief 绘制水滴图标
 * @param x X坐标
 * @param y Y坐标
 * @param filled 是否填充
 */
void display_draw_droplet_icon(int x, int y, bool filled);

/**
 * @brief 清屏
 */
void display_clear();

/**
 * @brief 刷新显示
 */
void display_refresh();

#endif // DISPLAY_H
