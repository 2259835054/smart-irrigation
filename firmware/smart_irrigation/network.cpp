/**
 * @file network.cpp
 * @brief WiFi网络模块实现 - WiFi连接和Web服务器
 * @author Smart Irrigation Project
 * @date 2026
 */

#include "network.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>

// Web服务器对象
WebServer server(WEB_SERVER_PORT);

// WiFi连接状态
static bool wifiConnected = false;

// 外部数据引用（需要在主程序中更新）
static SensorData* currentData = nullptr;
static IrrigationState* currentState = nullptr;

/**
 * @brief 设置当前数据指针（由主程序调用）
 */
void network_set_data(SensorData* data, IrrigationState* state) {
    currentData = data;
    currentState = state;
}

/**
 * @brief 生成HTML主页
 */
String generateHTML() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<title>智能灌溉系统</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += "h1 { color: #2c3e50; text-align: center; }";
    html += ".data { margin: 15px 0; padding: 10px; background-color: #ecf0f1; border-radius: 5px; }";
    html += ".label { font-weight: bold; color: #34495e; }";
    html += ".value { color: #2980b9; font-size: 1.2em; }";
    html += ".button { background-color: #3498db; color: white; padding: 12px 24px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
    html += ".button:hover { background-color: #2980b9; }";
    html += ".status { padding: 10px; margin: 10px 0; border-radius: 5px; text-align: center; font-weight: bold; }";
    html += ".status-on { background-color: #2ecc71; color: white; }";
    html += ".status-off { background-color: #95a5a6; color: white; }";
    html += ".warning { background-color: #e74c3c; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🌱 智能灌溉系统</h1>";
    
    if (currentData && currentState) {
        // 传感器数据
        html += "<div class='data'>";
        html += "<span class='label'>土壤湿度：</span>";
        html += "<span class='value'>" + String((int)currentData->soilMoisture) + "%</span>";
        html += "</div>";
        
        html += "<div class='data'>";
        html += "<span class='label'>环境温度：</span>";
        html += "<span class='value'>" + String(currentData->temperature, 1) + "°C</span>";
        html += "</div>";
        
        html += "<div class='data'>";
        html += "<span class='label'>环境湿度：</span>";
        html += "<span class='value'>" + String((int)currentData->humidity) + "%</span>";
        html += "</div>";
        
        // 水箱状态
        if (!currentData->waterLevelOk) {
            html += "<div class='warning'>⚠️ 警告：水箱缺水！</div>";
        } else {
            html += "<div class='data'><span class='label'>水箱状态：</span><span class='value'>正常</span></div>";
        }
        
        // 水泵状态
        String pumpStatus = currentState->pumpRunning ? "status-on'>🚿 水泵运行中" : "status-off'>⏸️ 水泵已停止";
        html += "<div class='status " + pumpStatus + "</div>";
        
        // 灌溉模式
        html += "<div class='data'>";
        html += "<span class='label'>灌溉模式：</span>";
        html += "<span class='value'>";
        switch (currentState->mode) {
            case MODE_AUTO:   html += "自动模式"; break;
            case MODE_TIMED:  html += "定时模式"; break;
            case MODE_MANUAL: html += "手动模式"; break;
        }
        html += "</span></div>";
        
        // 浇水统计
        html += "<div class='data'>";
        html += "<span class='label'>浇水次数：</span>";
        html += "<span class='value'>" + String(currentState->waterCount) + " 次</span>";
        html += "</div>";
    }
    
    // 控制按钮
    html += "<div style='text-align: center; margin-top: 20px;'>";
    html += "<button class='button' onclick=\"location.href='/toggle'\">手动浇水</button>";
    html += "<button class='button' onclick=\"location.href='/mode'\">切换模式</button>";
    html += "</div>";
    
    html += "<p style='text-align: center; color: #7f8c8d; margin-top: 20px;'>页面每5秒自动刷新</p>";
    html += "</div></body></html>";
    
    return html;
}

/**
 * @brief 处理根路径请求
 */
void handleRoot() {
    server.send(200, "text/html", generateHTML());
}

/**
 * @brief 处理手动浇水请求
 */
void handleToggle() {
    irrigation_manual_toggle();
    server.sendHeader("Location", "/");
    server.send(303);
}

/**
 * @brief 处理模式切换请求
 */
void handleMode() {
    irrigation_switch_mode();
    server.sendHeader("Location", "/");
    server.send(303);
}

/**
 * @brief 初始化WiFi连接
 */
void network_init() {
    Serial.println("正在连接WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi连接成功！");
        Serial.print("IP地址：");
        Serial.println(WiFi.localIP());
        
        // 配置Web服务器路由
        server.on("/", handleRoot);
        server.on("/toggle", handleToggle);
        server.on("/mode", handleMode);
        
        // 启动Web服务器
        server.begin();
        Serial.println("Web服务器已启动");
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi连接失败，将以离线模式运行");
    }
}

/**
 * @brief 处理Web客户端请求
 */
void network_handle_client() {
    if (wifiConnected) {
        server.handleClient();
    }
}

/**
 * @brief 获取WiFi连接状态
 */
bool network_is_connected() {
    return wifiConnected;
}
