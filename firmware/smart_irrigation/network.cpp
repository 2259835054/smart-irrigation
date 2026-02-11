/**
 * @file network.cpp
 * @brief 网络模块实现文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现WiFi连接、Web Server、RESTful API、OTA无线升级
 * 提供响应式HTML界面，支持实时数据监控和远程控制
 */

#include "network.h"
#include "data_logger.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// ==================== 全局对象 ====================
WebServer server(WEB_SERVER_PORT);

// ==================== 内部变量 ====================
static bool wifiConnected = false;
static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 30000; // 重连间隔30秒

// ==================== WiFi连接函数 ====================

/**
 * @brief 连接WiFi
 * @return true=连接成功, false=连接失败
 */
bool network_connect_wifi() {
    Serial.println(F("正在连接WiFi..."));
    Serial.print(F("SSID: "));
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startAttemptTime = millis();
    
    // 等待连接，最多等待WIFI_CONNECT_TIMEOUT毫秒
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startAttemptTime < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println();
        Serial.println(F("WiFi连接成功！"));
        Serial.print(F("IP地址: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("信号强度: "));
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        return true;
    } else {
        wifiConnected = false;
        Serial.println();
        Serial.println(F("WiFi连接失败！"));
        return false;
    }
}

/**
 * @brief 检查WiFi连接状态
 * @return true=已连接, false=未连接
 */
bool network_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief 获取WiFi信号强度
 * @return RSSI值 (dBm)
 */
int network_get_rssi() {
    return WiFi.RSSI();
}

/**
 * @brief 获取IP地址
 * @return IP地址字符串
 */
String network_get_ip() {
    return WiFi.localIP().toString();
}

/**
 * @brief 获取MAC地址
 * @return MAC地址字符串
 */
String network_get_mac() {
    return WiFi.macAddress();
}

// ==================== OTA回调函数 ====================

/**
 * @brief OTA开始回调
 */
void ota_on_start() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
    } else { // U_SPIFFS
        type = "filesystem";
    }
    Serial.println("开始OTA更新: " + type);
    // 停止所有水泵
    irrigation_stop_all();
}

/**
 * @brief OTA结束回调
 */
void ota_on_end() {
    Serial.println("\nOTA更新完成");
}

/**
 * @brief OTA进度回调
 * @param progress 当前进度
 * @param total 总大小
 */
void ota_on_progress(unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress / (total / 100));
    if (percent != lastPercent && percent % 10 == 0) {
        Serial.printf("进度: %u%%\n", percent);
        lastPercent = percent;
    }
}

/**
 * @brief OTA错误回调
 * @param error 错误代码
 */
void ota_on_error(ota_error_t error) {
    Serial.printf("OTA错误[%u]: ", error);
    switch (error) {
        case OTA_AUTH_ERROR:
            Serial.println("认证失败");
            break;
        case OTA_BEGIN_ERROR:
            Serial.println("开始失败");
            break;
        case OTA_CONNECT_ERROR:
            Serial.println("连接失败");
            break;
        case OTA_RECEIVE_ERROR:
            Serial.println("接收失败");
            break;
        case OTA_END_ERROR:
            Serial.println("结束失败");
            break;
        default:
            Serial.println("未知错误");
    }
}

/**
 * @brief 初始化OTA
 */
void init_ota() {
    // 设置主机名
    ArduinoOTA.setHostname(SYSTEM_NAME);
    
    // 设置密码（可选，增加安全性）
    // ArduinoOTA.setPassword("admin");
    
    // 设置OTA端口（默认8266）
    // ArduinoOTA.setPort(8266);
    
    // 绑定回调函数
    ArduinoOTA.onStart(ota_on_start);
    ArduinoOTA.onEnd(ota_on_end);
    ArduinoOTA.onProgress(ota_on_progress);
    ArduinoOTA.onError(ota_on_error);
    
    // 启动OTA
    ArduinoOTA.begin();
    Serial.println(F("OTA已启动"));
}

/**
 * @brief 初始化mDNS
 */
void init_mdns() {
    if (MDNS.begin(SYSTEM_NAME)) {
        Serial.println(F("mDNS已启动"));
        Serial.print(F("访问地址: http://"));
        Serial.print(SYSTEM_NAME);
        Serial.println(F(".local"));
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println(F("mDNS启动失败"));
    }
}

// ==================== CORS支持 ====================

/**
 * @brief 设置CORS响应头（允许跨域请求）
 */
void set_cors_headers() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

/**
 * @brief 处理OPTIONS预检请求
 */
void handle_options() {
    set_cors_headers();
    server.send(204);
}

// ==================== HTML生成函数 ====================

/**
 * @brief 生成CSS样式
 * @return CSS字符串
 */
String generate_css() {
    return R"css(
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Microsoft YaHei", sans-serif;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    min-height: 100vh;
    padding: 20px;
    color: #333;
}
.container {
    max-width: 1200px;
    margin: 0 auto;
}
.header {
    background: white;
    padding: 20px 30px;
    border-radius: 15px;
    box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    margin-bottom: 20px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
}
.header h1 {
    color: #667eea;
    font-size: 28px;
    margin-bottom: 5px;
}
.header .version {
    color: #999;
    font-size: 14px;
}
.status-badge {
    display: inline-block;
    padding: 8px 16px;
    border-radius: 20px;
    font-size: 14px;
    font-weight: 600;
    margin: 5px;
}
.status-online {
    background: #10b981;
    color: white;
}
.status-offline {
    background: #ef4444;
    color: white;
}
.grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
    margin-bottom: 20px;
}
.card {
    background: white;
    padding: 25px;
    border-radius: 15px;
    box-shadow: 0 8px 32px rgba(0,0,0,0.1);
}
.card h2 {
    color: #667eea;
    font-size: 20px;
    margin-bottom: 20px;
    padding-bottom: 10px;
    border-bottom: 2px solid #f0f0f0;
}
.sensor-item {
    margin-bottom: 20px;
}
.sensor-label {
    display: flex;
    justify-content: space-between;
    margin-bottom: 8px;
    font-size: 14px;
    color: #666;
}
.sensor-value {
    font-size: 24px;
    font-weight: 700;
    color: #333;
}
.progress-bar {
    width: 100%;
    height: 20px;
    background: #f0f0f0;
    border-radius: 10px;
    overflow: hidden;
    margin-top: 8px;
}
.progress-fill {
    height: 100%;
    transition: width 0.5s ease, background 0.3s ease;
    border-radius: 10px;
}
.progress-dry { background: linear-gradient(90deg, #ef4444, #f59e0b); }
.progress-normal { background: linear-gradient(90deg, #f59e0b, #10b981); }
.progress-wet { background: linear-gradient(90deg, #10b981, #3b82f6); }
.channel-card {
    background: #f9fafb;
    padding: 15px;
    border-radius: 10px;
    margin-bottom: 15px;
    border-left: 4px solid #667eea;
}
.channel-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
}
.channel-title {
    font-size: 16px;
    font-weight: 600;
    color: #333;
}
.pump-status {
    display: inline-block;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    margin-right: 5px;
}
.pump-on {
    background: #10b981;
    box-shadow: 0 0 10px #10b981;
    animation: pulse 1s infinite;
}
.pump-off {
    background: #d1d5db;
}
@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
}
.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 8px;
    font-size: 14px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s;
    margin: 5px;
}
.btn:hover {
    transform: translateY(-2px);
    box-shadow: 0 4px 12px rgba(0,0,0,0.15);
}
.btn-primary {
    background: #667eea;
    color: white;
}
.btn-success {
    background: #10b981;
    color: white;
}
.btn-danger {
    background: #ef4444;
    color: white;
}
.btn-warning {
    background: #f59e0b;
    color: white;
}
.btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}
.control-group {
    margin-bottom: 15px;
}
.control-label {
    display: block;
    margin-bottom: 8px;
    font-size: 14px;
    font-weight: 600;
    color: #555;
}
select, input[type="range"] {
    width: 100%;
    padding: 10px;
    border: 2px solid #e5e7eb;
    border-radius: 8px;
    font-size: 14px;
    transition: border 0.3s;
}
select:focus, input[type="range"]:focus {
    outline: none;
    border-color: #667eea;
}
input[type="range"] {
    cursor: pointer;
}
.slider-value {
    display: inline-block;
    margin-left: 10px;
    font-weight: 600;
    color: #667eea;
}
.info-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 15px;
}
.info-item {
    background: #f9fafb;
    padding: 12px;
    border-radius: 8px;
}
.info-label {
    font-size: 12px;
    color: #999;
    margin-bottom: 5px;
}
.info-value {
    font-size: 16px;
    font-weight: 600;
    color: #333;
}
.alert {
    padding: 15px 20px;
    border-radius: 10px;
    margin-bottom: 20px;
    font-size: 14px;
}
.alert-warning {
    background: #fef3c7;
    color: #92400e;
    border-left: 4px solid #f59e0b;
}
.alert-danger {
    background: #fee2e2;
    color: #991b1b;
    border-left: 4px solid #ef4444;
}
.loading {
    text-align: center;
    padding: 20px;
    color: #999;
}
@media (max-width: 768px) {
    .header {
        flex-direction: column;
        align-items: flex-start;
    }
    .grid {
        grid-template-columns: 1fr;
    }
    .info-grid {
        grid-template-columns: 1fr;
    }
}
.footer {
    text-align: center;
    padding: 20px;
    color: white;
    font-size: 14px;
    margin-top: 20px;
}
)css";
}

/**
 * @brief 生成JavaScript代码
 * @return JavaScript字符串
 */
String generate_javascript() {
    return R"js(
let updateInterval;
let chartData = [];

// 页面加载完成后初始化
window.addEventListener('DOMContentLoaded', function() {
    loadStatus();
    startAutoUpdate();
});

// 开始自动更新
function startAutoUpdate() {
    updateInterval = setInterval(loadStatus, 3000);
}

// 停止自动更新
function stopAutoUpdate() {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
}

// 加载系统状态
function loadStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateUI(data);
        })
        .catch(error => {
            console.error('获取状态失败:', error);
            showError('连接失败，请检查网络');
        });
}

// 更新UI
function updateUI(data) {
    // 更新传感器数据
    for (let i = 0; i < 4; i++) {
        const moisture = data.channels[i].moisture;
        document.getElementById('moisture' + i).textContent = moisture.toFixed(1);
        
        const progressBar = document.getElementById('progress' + i);
        progressBar.style.width = moisture + '%';
        
        // 根据湿度值设置颜色
        if (moisture < 40) {
            progressBar.className = 'progress-fill progress-dry';
        } else if (moisture < 70) {
            progressBar.className = 'progress-fill progress-normal';
        } else {
            progressBar.className = 'progress-fill progress-wet';
        }
        
        // 更新水泵状态
        const pumpStatus = document.getElementById('pump' + i);
        pumpStatus.className = data.channels[i].pumpRunning ? 'pump-status pump-on' : 'pump-status pump-off';
        
        // 更新按钮状态
        const startBtn = document.getElementById('start' + i);
        const stopBtn = document.getElementById('stop' + i);
        if (data.mode === 'manual') {
            startBtn.disabled = false;
            stopBtn.disabled = false;
        } else {
            startBtn.disabled = true;
            stopBtn.disabled = true;
        }
    }
    
    // 更新环境数据
    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
    document.getElementById('light').textContent = data.lightLevel.toFixed(0);
    
    // 更新系统信息
    document.getElementById('mode').value = data.mode;
    document.getElementById('uptime').textContent = formatUptime(data.uptime);
    document.getElementById('rssi').textContent = data.rssi;
}

// 格式化运行时间
function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    
    if (days > 0) {
        return days + '天' + (hours % 24) + '时';
    } else if (hours > 0) {
        return hours + '时' + (minutes % 60) + '分';
    } else if (minutes > 0) {
        return minutes + '分' + (seconds % 60) + '秒';
    } else {
        return seconds + '秒';
    }
}

// 控制水泵
function controlPump(channel, action) {
    stopAutoUpdate();
    
    fetch('/api/control', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({channel: channel, action: action})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showSuccess(action === 'start' ? '水泵已启动' : '水泵已停止');
            setTimeout(loadStatus, 500);
        } else {
            showError(data.message || '操作失败');
        }
        startAutoUpdate();
    })
    .catch(error => {
        console.error('控制失败:', error);
        showError('控制失败，请重试');
        startAutoUpdate();
    });
}

// 切换模式
function changeMode() {
    const mode = document.getElementById('mode').value;
    stopAutoUpdate();
    
    fetch('/api/mode', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({mode: mode})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showSuccess('模式已切换');
            setTimeout(loadStatus, 500);
        } else {
            showError(data.message || '切换失败');
        }
        startAutoUpdate();
    })
    .catch(error => {
        console.error('切换模式失败:', error);
        showError('切换失败，请重试');
        startAutoUpdate();
    });
}

// 设置阈值
function setThreshold(channel) {
    const dry = document.getElementById('dry' + channel).value;
    const wet = document.getElementById('wet' + channel).value;
    
    if (parseFloat(dry) >= parseFloat(wet)) {
        showError('干燥阈值必须小于湿润阈值');
        return;
    }
    
    stopAutoUpdate();
    
    fetch('/api/threshold', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({channel: channel, dry: parseFloat(dry), wet: parseFloat(wet)})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showSuccess('阈值已设置');
        } else {
            showError(data.message || '设置失败');
        }
        startAutoUpdate();
    })
    .catch(error => {
        console.error('设置阈值失败:', error);
        showError('设置失败，请重试');
        startAutoUpdate();
    });
}

// 更新滑块显示值
function updateSliderValue(channel, type, value) {
    document.getElementById(type + channel + 'Value').textContent = value + '%';
}

// 重启系统
function rebootSystem() {
    if (!confirm('确定要重启系统吗？')) {
        return;
    }
    
    stopAutoUpdate();
    
    fetch('/api/reboot', {method: 'POST'})
    .then(() => {
        showSuccess('系统正在重启...');
        setTimeout(() => {
            location.reload();
        }, 10000);
    })
    .catch(error => {
        console.error('重启失败:', error);
        showError('重启失败');
    });
}

// 显示成功消息
function showSuccess(message) {
    alert('✓ ' + message);
}

// 显示错误消息
function showError(message) {
    alert('✗ ' + message);
}
)js";
}

/**
 * @brief 生成完整HTML页面
 * @return HTML字符串
 */
String generate_html_page() {
    String html = F("<!DOCTYPE html><html lang=\"zh-CN\"><head>");
    html += F("<meta charset=\"UTF-8\">");
    html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    html += F("<title>智能灌溉系统 - 控制面板</title>");
    html += F("<style>");
    html += generate_css();
    html += F("</style>");
    html += F("</head><body>");
    html += F("<div class=\"container\">");
    
    // 头部
    html += F("<div class=\"header\">");
    html += F("<div>");
    html += F("<h1>🌱 智能灌溉系统</h1>");
    html += F("<div class=\"version\">版本 ");
    html += FIRMWARE_VERSION;
    html += F(" | IP: ");
    html += network_get_ip();
    html += F("</div></div>");
    html += F("<div>");
    html += F("<span class=\"status-badge status-online\" id=\"statusBadge\">● 在线</span>");
    html += F("</div></div>");
    
    // 土壤湿度卡片
    html += F("<div class=\"grid\">");
    html += F("<div class=\"card\">");
    html += F("<h2>💧 土壤湿度监测</h2>");
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        html += F("<div class=\"channel-card\">");
        html += F("<div class=\"channel-header\">");
        html += F("<span class=\"channel-title\">");
        html += F("<span class=\"pump-status pump-off\" id=\"pump");
        html += String(i);
        html += F("\"></span>通道 ");
        html += String(i + 1);
        html += F("</span>");
        html += F("<span class=\"sensor-value\" id=\"moisture");
        html += String(i);
        html += F("\">0.0</span><span style=\"font-size:14px;color:#999\">%</span>");
        html += F("</div>");
        html += F("<div class=\"progress-bar\"><div class=\"progress-fill progress-normal\" id=\"progress");
        html += String(i);
        html += F("\" style=\"width:0%\"></div></div>");
        html += F("<div style=\"margin-top:10px\">");
        html += F("<button class=\"btn btn-success btn-sm\" id=\"start");
        html += String(i);
        html += F("\" onclick=\"controlPump(");
        html += String(i);
        html += F(",'start')\" disabled>启动</button>");
        html += F("<button class=\"btn btn-danger btn-sm\" id=\"stop");
        html += String(i);
        html += F("\" onclick=\"controlPump(");
        html += String(i);
        html += F(",'stop')\" disabled>停止</button>");
        html += F("</div></div>");
    }
    
    html += F("</div>");
    
    // 环境监测卡片
    html += F("<div class=\"card\">");
    html += F("<h2>🌡️ 环境监测</h2>");
    html += F("<div class=\"sensor-item\">");
    html += F("<div class=\"sensor-label\"><span>温度</span></div>");
    html += F("<div><span class=\"sensor-value\" id=\"temperature\">0.0</span> <span style=\"color:#999\">°C</span></div>");
    html += F("</div>");
    html += F("<div class=\"sensor-item\">");
    html += F("<div class=\"sensor-label\"><span>湿度</span></div>");
    html += F("<div><span class=\"sensor-value\" id=\"humidity\">0.0</span> <span style=\"color:#999\">%</span></div>");
    html += F("</div>");
    html += F("<div class=\"sensor-item\">");
    html += F("<div class=\"sensor-label\"><span>光照强度</span></div>");
    html += F("<div><span class=\"sensor-value\" id=\"light\">0</span> <span style=\"color:#999\">%</span></div>");
    html += F("</div>");
    html += F("</div>");
    
    // 系统信息卡片
    html += F("<div class=\"card\">");
    html += F("<h2>ℹ️ 系统信息</h2>");
    html += F("<div class=\"info-grid\">");
    html += F("<div class=\"info-item\"><div class=\"info-label\">运行时间</div>");
    html += F("<div class=\"info-value\" id=\"uptime\">0秒</div></div>");
    html += F("<div class=\"info-item\"><div class=\"info-label\">WiFi信号</div>");
    html += F("<div class=\"info-value\" id=\"rssi\">0 dBm</div></div>");
    html += F("<div class=\"info-item\"><div class=\"info-label\">MAC地址</div>");
    html += F("<div class=\"info-value\" style=\"font-size:12px\">");
    html += network_get_mac();
    html += F("</div></div>");
    html += F("<div class=\"info-item\"><div class=\"info-label\">版本号</div>");
    html += F("<div class=\"info-value\">");
    html += FIRMWARE_VERSION;
    html += F("</div></div>");
    html += F("</div>");
    html += F("<div style=\"margin-top:15px\">");
    html += F("<button class=\"btn btn-warning\" onclick=\"rebootSystem()\">重启系统</button>");
    html += F("</div>");
    html += F("</div>");
    html += F("</div>");
    
    // 控制面板
    html += F("<div class=\"card\">");
    html += F("<h2>⚙️ 控制面板</h2>");
    html += F("<div class=\"control-group\">");
    html += F("<label class=\"control-label\">工作模式</label>");
    html += F("<select id=\"mode\" onchange=\"changeMode()\">");
    html += F("<option value=\"auto_pid\">PID自动控制</option>");
    html += F("<option value=\"auto_simple\">简单阈值控制</option>");
    html += F("<option value=\"timed\">定时灌溉</option>");
    html += F("<option value=\"manual\">手动控制</option>");
    html += F("</select>");
    html += F("</div>");
    
    // 阈值设置
    for (int i = 0; i < NUM_CHANNELS; i++) {
        html += F("<div class=\"control-group\">");
        html += F("<label class=\"control-label\">通道 ");
        html += String(i + 1);
        html += F(" 阈值设置</label>");
        html += F("<div style=\"margin-bottom:10px\">");
        html += F("<label style=\"font-size:12px;color:#666\">干燥阈值: <span id=\"dry");
        html += String(i);
        html += F("Value\" class=\"slider-value\">55%</span></label>");
        html += F("<input type=\"range\" id=\"dry");
        html += String(i);
        html += F("\" min=\"0\" max=\"100\" value=\"55\" oninput=\"updateSliderValue(");
        html += String(i);
        html += F(",'dry',this.value)\">");
        html += F("</div>");
        html += F("<div style=\"margin-bottom:10px\">");
        html += F("<label style=\"font-size:12px;color:#666\">湿润阈值: <span id=\"wet");
        html += String(i);
        html += F("Value\" class=\"slider-value\">80%</span></label>");
        html += F("<input type=\"range\" id=\"wet");
        html += String(i);
        html += F("\" min=\"0\" max=\"100\" value=\"80\" oninput=\"updateSliderValue(");
        html += String(i);
        html += F(",'wet',this.value)\">");
        html += F("</div>");
        html += F("<button class=\"btn btn-primary\" onclick=\"setThreshold(");
        html += String(i);
        html += F(")\">应用设置</button>");
        html += F("</div>");
    }
    
    html += F("</div>");
    
    // 页脚
    html += F("<div class=\"footer\">");
    html += F("© 2026 智能灌溉系统 | 基于ESP32开发");
    html += F("</div>");
    
    html += F("</div>");
    html += F("<script>");
    html += generate_javascript();
    html += F("</script>");
    html += F("</body></html>");
    
    return html;
}

// ==================== API处理函数 ====================

/**
 * @brief 处理根路径请求
 */
void handle_root() {
    String html = generate_html_page();
    server.send(200, "text/html; charset=utf-8", html);
}

/**
 * @brief 处理状态API请求
 */
void handle_api_status() {
    set_cors_headers();
    
    // 创建JSON文档
    StaticJsonDocument<2048> doc;
    
    // 获取传感器数据
    SensorData sensorData = g_sensorData;
    IrrigationState irrigationState = g_irrigationState;
    
    // 添加通道数据
    JsonArray channels = doc.createNestedArray("channels");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        JsonObject channel = channels.createNestedObject();
        channel["moisture"] = sensorData.channels[i].soilMoisture;
        channel["pumpRunning"] = irrigationState.channels[i].pumpRunning;
        channel["pumpPWM"] = irrigationState.channels[i].pumpPWM;
        channel["dryThreshold"] = irrigationState.channels[i].dryThreshold;
        channel["wetThreshold"] = irrigationState.channels[i].wetThreshold;
        channel["enabled"] = irrigationState.channels[i].enabled;
        channel["waterCount"] = irrigationState.channels[i].waterCount;
    }
    
    // 添加环境数据
    doc["temperature"] = sensorData.temperature;
    doc["humidity"] = sensorData.humidity;
    doc["lightLevel"] = sensorData.lightLevel;
    doc["waterLevelOk"] = sensorData.waterLevelOk;
    
    // 添加系统信息
    doc["mode"] = irrigation_get_mode_str();
    doc["uptime"] = millis();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["systemEnabled"] = irrigationState.systemEnabled;
    doc["alarmActive"] = irrigationState.alarmActive;
    doc["alarmMessage"] = irrigationState.alarmMessage;
    
    // 序列化并发送
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json; charset=utf-8", response);
}

/**
 * @brief 处理历史数据API请求
 */
void handle_api_history() {
    set_cors_headers();
    
    // 获取最近100条历史记录
    String historyJson = datalog_get_history_json(100);
    
    server.send(200, "application/json; charset=utf-8", historyJson);
}

/**
 * @brief 处理控制API请求
 */
void handle_api_control() {
    set_cors_headers();
    
    // 解析JSON请求
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"JSON解析失败\"}");
        return;
    }
    
    int channel = doc["channel"];
    String action = doc["action"].as<String>();
    
    // 检查参数
    if (channel < 0 || channel >= NUM_CHANNELS) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"无效的通道号\"}");
        return;
    }
    
    // 检查是否为手动模式
    if (irrigation_get_mode() != MODE_MANUAL) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"请切换到手动模式\"}");
        return;
    }
    
    // 执行控制
    if (action == "start") {
        irrigation_start_pump(channel, 255);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"水泵已启动\"}");
    } else if (action == "stop") {
        irrigation_stop_pump(channel);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"水泵已停止\"}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"无效的操作\"}");
    }
}

/**
 * @brief 处理模式切换API请求
 */
void handle_api_mode() {
    set_cors_headers();
    
    // 解析JSON请求
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"JSON解析失败\"}");
        return;
    }
    
    String modeStr = doc["mode"].as<String>();
    IrrigationMode newMode;
    
    // 转换模式字符串
    if (modeStr == "auto_pid") {
        newMode = MODE_AUTO_PID;
    } else if (modeStr == "auto_simple") {
        newMode = MODE_AUTO_SIMPLE;
    } else if (modeStr == "timed") {
        newMode = MODE_TIMED;
    } else if (modeStr == "manual") {
        newMode = MODE_MANUAL;
    } else {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"无效的模式\"}");
        return;
    }
    
    // 切换模式
    irrigation_set_mode(newMode);
    
    // 保存配置
    datalog_save_config(g_irrigationState);
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"模式已切换\"}");
}

/**
 * @brief 处理阈值设置API请求
 */
void handle_api_threshold() {
    set_cors_headers();
    
    // 解析JSON请求
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"JSON解析失败\"}");
        return;
    }
    
    int channel = doc["channel"];
    float dry = doc["dry"];
    float wet = doc["wet"];
    
    // 设置阈值
    if (irrigation_set_threshold(channel, dry, wet)) {
        // 保存配置
        datalog_save_config(g_irrigationState);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"阈值已设置\"}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"参数错误\"}");
    }
}

/**
 * @brief 处理系统信息API请求
 */
void handle_api_system() {
    set_cors_headers();
    
    StaticJsonDocument<512> doc;
    
    // ESP32信息
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();
    
    // WiFi信息
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["gateway"] = WiFi.gatewayIP().toString();
    
    // 系统信息
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis();
    
    // 文件系统信息
    size_t totalBytes = 0;
    size_t usedBytes = 0;
    if (datalog_get_fs_info(totalBytes, usedBytes)) {
        doc["fsTotal"] = totalBytes;
        doc["fsUsed"] = usedBytes;
        doc["fsFree"] = totalBytes - usedBytes;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json; charset=utf-8", response);
}

/**
 * @brief 处理重启API请求
 */
void handle_api_reboot() {
    set_cors_headers();
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"系统正在重启\"}");
    
    delay(1000);
    ESP.restart();
}

/**
 * @brief 处理404错误
 */
void handle_not_found() {
    String message = "404 Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    
    server.send(404, "text/plain", message);
}

// ==================== 初始化和主循环 ====================

/**
 * @brief 初始化网络模块
 */
void network_init() {
    Serial.println(F("初始化网络模块..."));
    
    // 连接WiFi
    if (!network_connect_wifi()) {
        Serial.println(F("WiFi连接失败，将在后台重试"));
    }
    
    // 初始化mDNS
    if (wifiConnected) {
        init_mdns();
    }
    
    // 注册Web Server路由
    server.on("/", HTTP_GET, handle_root);
    server.on("/api/status", HTTP_GET, handle_api_status);
    server.on("/api/history", HTTP_GET, handle_api_history);
    server.on("/api/control", HTTP_POST, handle_api_control);
    server.on("/api/mode", HTTP_POST, handle_api_mode);
    server.on("/api/threshold", HTTP_POST, handle_api_threshold);
    server.on("/api/system", HTTP_GET, handle_api_system);
    server.on("/api/reboot", HTTP_POST, handle_api_reboot);
    
    // OPTIONS预检请求处理
    server.on("/api/control", HTTP_OPTIONS, handle_options);
    server.on("/api/mode", HTTP_OPTIONS, handle_options);
    server.on("/api/threshold", HTTP_OPTIONS, handle_options);
    server.on("/api/reboot", HTTP_OPTIONS, handle_options);
    
    // 404处理
    server.onNotFound(handle_not_found);
    
    // 启动Web Server
    server.begin();
    Serial.println(F("Web Server已启动"));
    
    // 初始化OTA
    if (wifiConnected) {
        init_ota();
    }
    
    Serial.println(F("网络模块初始化完成"));
}

/**
 * @brief 处理网络请求
 */
void network_handle() {
    // 检查WiFi连接状态
    if (WiFi.status() != WL_CONNECTED) {
        if (!wifiConnected) {
            // 尝试重连
            unsigned long now = millis();
            if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
                lastReconnectAttempt = now;
                Serial.println(F("尝试重新连接WiFi..."));
                if (network_connect_wifi()) {
                    init_mdns();
                    init_ota();
                }
            }
        }
        wifiConnected = false;
    } else {
        if (!wifiConnected) {
            Serial.println(F("WiFi已重新连接"));
            wifiConnected = true;
        }
        
        // 处理Web Server请求
        server.handleClient();
        
        // 处理OTA
        ArduinoOTA.handle();
        
        // 更新mDNS
        MDNS.update();
    }
}
