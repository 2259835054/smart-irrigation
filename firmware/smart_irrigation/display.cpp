/**
 * @file display.cpp
 * @brief OLED 显示模块实现文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现OLED多页面显示系统，包括：
 * - 主页：概览所有通道
 * - 通道详情页：单通道详细数据 + PID 状态
 * - 数据图表页：湿度趋势折线图
 * - 设置页：阈值和模式配置
 * - 网络状态页：WiFi/MQTT 连接信息
 * - 关于页：版本、运行时间、统计信息
 */

#include "display.h"
#include <WiFi.h>

// ==================== 全局对象 ====================

// OLED 显示对象（使用 I2C 通信）
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==================== 内部状态变量 ====================

// 当前显示页面
static DisplayPage s_currentPage = PAGE_MAIN;

// 当前选中的通道（用于通道详情页）
static uint8_t s_currentChannel = 0;

// 图表数据缓冲区（环形缓冲区，每通道保存最近30个数据点）
#define CHART_DATA_SIZE 30
static float s_chartData[NUM_CHANNELS][CHART_DATA_SIZE];
static int s_chartDataCount[NUM_CHANNELS] = {0, 0, 0, 0};
static int s_chartDataIndex[NUM_CHANNELS] = {0, 0, 0, 0};

// 报警信息状态
static bool s_alertActive = false;
static String s_alertMessage = "";
static unsigned long s_alertStartTime = 0;
#define ALERT_DURATION 3000  // 报警显示持续时间 3秒

// 网络状态缓存（避免频繁调用WiFi API）
static int s_lastRSSI = -100;
static unsigned long s_lastRSSIUpdate = 0;
#define RSSI_UPDATE_INTERVAL 2000  // RSSI 更新间隔 2秒

// ==================== 初始化函数 ====================

/**
 * @brief 初始化 OLED 显示屏
 */
void display_init() {
    // 配置 I2C 引脚
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // 初始化 SSD1306 显示屏（128x64 像素，I2C 地址 0x3C）
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println(F("[显示] OLED 初始化失败！"));
        return;
    }
    
    // 设置文本颜色（白色）
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    // 清屏
    display.clearDisplay();
    display.display();
    
    // 初始化图表数据缓冲区
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        for (int i = 0; i < CHART_DATA_SIZE; i++) {
            s_chartData[ch][i] = 0.0;
        }
    }
    
    Serial.println(F("[显示] OLED 初始化完成"));
}

/**
 * @brief 显示开机动画
 */
void display_startup() {
    display.clearDisplay();
    
    // ===== 第一阶段：显示 Logo 和系统名称 =====
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.print(F("Smart"));
    display.setCursor(4, 28);
    display.print(F("Irrigation"));
    
    // 绘制水滴装饰图标
    display_draw_droplet_icon(100, 15, true);
    display_draw_droplet_icon(108, 25, false);
    
    display.display();
    delay(1500);
    
    // ===== 第二阶段：加载进度条 =====
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.print(F("System Loading..."));
    
    // 显示版本号
    display.setCursor(30, 25);
    display.print(F("Ver "));
    display.print(FIRMWARE_VERSION);
    
    // 绘制进度条框架
    int barX = 14, barY = 45, barW = 100, barH = 10;
    display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
    display.display();
    
    // 模拟加载过程（分5个阶段）
    const char* stages[] = {
        "Init Sensors...",
        "Init Network...",
        "Init Irrigation...",
        "Init Display...",
        "Ready!"
    };
    
    for (int i = 0; i < 5; i++) {
        // 更新状态文本
        display.fillRect(0, 35, 128, 8, SSD1306_BLACK);
        display.setCursor(0, 35);
        display.print(stages[i]);
        
        // 更新进度条
        float percent = (i + 1) * 20.0;
        display_draw_progress_bar(barX, barY, barW, barH, percent);
        
        display.display();
        delay(400);
    }
    
    delay(500);
    
    // ===== 第三阶段：欢迎信息 =====
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(25, 25);
    display.print(F("Welcome!"));
    display.display();
    delay(800);
}

/**
 * @brief 更新显示内容（根据当前页面路由到对应的绘制函数）
 */
void display_update(SensorData &data, IrrigationState &state) {
    // 检查报警状态是否超时
    if (s_alertActive && (millis() - s_alertStartTime > ALERT_DURATION)) {
        s_alertActive = false;
    }
    
    display.clearDisplay();
    
    // 根据当前页面绘制内容
    switch (s_currentPage) {
        case PAGE_MAIN:
            display_draw_main_page(data, state);
            break;
        case PAGE_CHANNEL:
            display_draw_channel_page(data, state);
            break;
        case PAGE_CHART:
            display_draw_chart_page(data, state);
            break;
        case PAGE_SETTINGS:
            display_draw_settings_page(data, state);
            break;
        case PAGE_NETWORK:
            display_draw_network_page(data, state);
            break;
        case PAGE_ABOUT:
            display_draw_about_page(data, state);
            break;
        default:
            break;
    }
    
    // 如果有报警信息，覆盖显示
    if (s_alertActive) {
        display.fillRect(0, 20, 128, 24, SSD1306_BLACK);
        display.drawRect(2, 20, 124, 24, SSD1306_WHITE);
        display.drawRect(3, 21, 122, 22, SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(8, 28);
        display.print(s_alertMessage);
    }
    
    display.display();
}

/**
 * @brief 绘制主页（概览所有4个通道）
 */
void display_draw_main_page(SensorData &data, IrrigationState &state) {
    // 标题栏
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("=== Main ==="));
    
    // WiFi 图标
    if (millis() - s_lastRSSIUpdate > RSSI_UPDATE_INTERVAL) {
        s_lastRSSI = WiFi.RSSI();
        s_lastRSSIUpdate = millis();
    }
    display_draw_wifi_icon(95, 0, s_lastRSSI);
    
    // 系统状态指示
    display.setCursor(115, 0);
    if (state.systemEnabled) {
        display.print(F("ON"));
    } else {
        display.print(F("OFF"));
    }
    
    // 分隔线
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // 显示4个通道的湿度和状态（2x2 布局）
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        int x = (ch % 2) * 64;
        int y = 12 + (ch / 2) * 24;
        
        // 通道标签
        display.setCursor(x + 2, y);
        display.print(F("CH"));
        display.print(ch + 1);
        display.print(F(":"));
        
        // 湿度值
        if (data.channels[ch].isValid) {
            display.setCursor(x + 24, y);
            display.print((int)data.channels[ch].soilMoisture);
            display.print(F("%"));
        } else {
            display.setCursor(x + 24, y);
            display.print(F("--"));
        }
        
        // 水滴图标（运行时填充）
        bool pumping = state.channels[ch].pumpRunning;
        display_draw_droplet_icon(x + 52, y, pumping);
        
        // 进度条（显示湿度）
        if (data.channels[ch].isValid) {
            int barY = y + 10;
            display_draw_progress_bar(x + 2, barY, 58, 6, data.channels[ch].soilMoisture);
        }
    }
    
    // 底部信息栏
    display.drawLine(0, 60, 128, 60, SSD1306_WHITE);
    display.setCursor(0, 62);
    
    // 显示当前模式
    const char* modeStr = irrigation_get_mode_str(state.mode);
    display.print(modeStr);
    
    // 环境温湿度
    display.setCursor(70, 62);
    display.print((int)data.temperature);
    display.print(F("C "));
    display.print((int)data.humidity);
    display.print(F("%"));
}

/**
 * @brief 绘制通道详情页（单通道详细信息 + PID 状态）
 */
void display_draw_channel_page(SensorData &data, IrrigationState &state) {
    uint8_t ch = s_currentChannel;
    
    // 标题栏
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("== Channel "));
    display.print(ch + 1);
    display.print(F(" =="));
    
    // 水滴图标
    display_draw_droplet_icon(110, 0, state.channels[ch].pumpRunning);
    
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // 当前湿度（大字体显示）
    display.setTextSize(2);
    display.setCursor(0, 14);
    if (data.channels[ch].isValid) {
        display.print((int)data.channels[ch].soilMoisture);
        display.print(F("%"));
    } else {
        display.print(F("--"));
    }
    
    // 状态文本
    display.setTextSize(1);
    display.setCursor(70, 16);
    if (state.channels[ch].pumpRunning) {
        display.print(F("Watering"));
    } else if (!state.channels[ch].enabled) {
        display.print(F("Disabled"));
    } else {
        display.print(F("Standby"));
    }
    
    // PWM 输出值（仅PID模式显示）
    if (state.mode == MODE_AUTO_PID) {
        display.setCursor(70, 24);
        display.print(F("PWM:"));
        display.print(state.channels[ch].pumpPWM);
    }
    
    // 阈值设置
    display.setCursor(0, 32);
    display.print(F("Dry:"));
    display.print((int)state.channels[ch].dryThreshold);
    display.print(F("% Wet:"));
    display.print((int)state.channels[ch].wetThreshold);
    display.print(F("%"));
    
    // 浇水统计
    display.setCursor(0, 42);
    display.print(F("Count:"));
    display.print(state.channels[ch].waterCount);
    
    display.setCursor(70, 42);
    display.print(F("Time:"));
    display.print(state.channels[ch].totalWaterTime / 1000);
    display.print(F("s"));
    
    // PID 参数（仅PID模式显示）
    if (state.mode == MODE_AUTO_PID && state.channels[ch].pidController != nullptr) {
        display.setCursor(0, 52);
        display.print(F("PID: Kp="));
        display.print(state.channels[ch].pidController->getKp(), 1);
        
        display.setCursor(70, 52);
        display.print(F("Ki="));
        display.print(state.channels[ch].pidController->getKi(), 1);
    }
    
    // 页面指示器
    display.setCursor(115, 56);
    display.print(ch + 1);
    display.print(F("/4"));
}

/**
 * @brief 绘制图表页（湿度趋势折线图）
 */
void display_draw_chart_page(SensorData &data, IrrigationState &state) {
    uint8_t ch = s_currentChannel;
    
    // 标题
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Chart CH"));
    display.print(ch + 1);
    
    // 当前值
    display.setCursor(70, 0);
    if (data.channels[ch].isValid) {
        display.print(F("Now:"));
        display.print((int)data.channels[ch].soilMoisture);
        display.print(F("%"));
    }
    
    // 绘制图表（使用整个屏幕下半部分）
    if (s_chartDataCount[ch] > 0) {
        char title[16];
        snprintf(title, sizeof(title), "CH%d Moisture", ch + 1);
        display_draw_chart(s_chartData[ch], s_chartDataCount[ch], 0, 10, 128, 54, title);
    } else {
        display.setCursor(20, 30);
        display.print(F("No data yet..."));
    }
}

/**
 * @brief 绘制设置页（阈值和模式配置）
 */
void display_draw_settings_page(SensorData &data, IrrigationState &state) {
    display.setTextSize(1);
    
    // 标题
    display.setCursor(0, 0);
    display.print(F("=== Settings ==="));
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // 系统状态
    display.setCursor(0, 12);
    display.print(F("System: "));
    display.print(state.systemEnabled ? F("Enabled") : F("Disabled"));
    
    // 当前模式
    display.setCursor(0, 22);
    display.print(F("Mode: "));
    const char* modeStr = irrigation_get_mode_str(state.mode);
    display.print(modeStr);
    
    // 分隔线
    display.drawLine(0, 32, 128, 32, SSD1306_WHITE);
    
    // 通道阈值设置（显示所有通道）
    display.setCursor(0, 34);
    display.print(F("Thresholds (Dry/Wet):"));
    
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        display.setCursor(0, 44 + ch * 8);
        display.print(F("CH"));
        display.print(ch + 1);
        display.print(F(": "));
        display.print((int)state.channels[ch].dryThreshold);
        display.print(F("/"));
        display.print((int)state.channels[ch].wetThreshold);
        display.print(F("% "));
        
        // 状态指示
        if (!state.channels[ch].enabled) {
            display.print(F("[OFF]"));
        }
    }
}

/**
 * @brief 绘制网络状态页（WiFi/MQTT 连接信息）
 */
void display_draw_network_page(SensorData &data, IrrigationState &state) {
    display.setTextSize(1);
    
    // 标题
    display.setCursor(0, 0);
    display.print(F("=== Network ==="));
    
    // WiFi 图标
    if (millis() - s_lastRSSIUpdate > RSSI_UPDATE_INTERVAL) {
        s_lastRSSI = WiFi.RSSI();
        s_lastRSSIUpdate = millis();
    }
    display_draw_wifi_icon(105, 0, s_lastRSSI);
    
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // WiFi 状态
    display.setCursor(0, 12);
    display.print(F("WiFi: "));
    if (WiFi.status() == WL_CONNECTED) {
        display.print(F("Connected"));
    } else {
        display.print(F("Disconnected"));
    }
    
    // SSID
    if (WiFi.status() == WL_CONNECTED) {
        display.setCursor(0, 22);
        display.print(F("SSID: "));
        String ssid = WiFi.SSID();
        if (ssid.length() > 16) {
            display.print(ssid.substring(0, 16));
        } else {
            display.print(ssid);
        }
    }
    
    // IP 地址
    if (WiFi.status() == WL_CONNECTED) {
        display.setCursor(0, 32);
        display.print(F("IP: "));
        display.print(WiFi.localIP().toString());
    }
    
    // RSSI 信号强度
    if (WiFi.status() == WL_CONNECTED) {
        display.setCursor(0, 42);
        display.print(F("RSSI: "));
        display.print(s_lastRSSI);
        display.print(F(" dBm"));
        
        // 信号强度进度条
        int signalPercent = constrain(map(s_lastRSSI, -100, -40, 0, 100), 0, 100);
        display_draw_progress_bar(0, 50, 100, 6, signalPercent);
    }
    
    // MQTT 状态（简化显示，实际需要导入 MQTT 客户端状态）
    display.setCursor(0, 58);
    display.print(F("MQTT: "));
    display.print(F("broker.emqx.io"));
}

/**
 * @brief 绘制关于页（版本、运行时间、统计信息）
 */
void display_draw_about_page(SensorData &data, IrrigationState &state) {
    display.setTextSize(1);
    
    // 标题
    display.setCursor(0, 0);
    display.print(F("=== About ==="));
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // 系统名称
    display.setCursor(0, 12);
    display.print(F("Smart Irrigation"));
    
    // 版本号
    display.setCursor(0, 22);
    display.print(F("Version: "));
    display.print(FIRMWARE_VERSION);
    
    // 运行时间
    unsigned long uptime = millis() / 1000;  // 秒
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;
    
    display.setCursor(0, 32);
    display.print(F("Uptime: "));
    display.print(hours);
    display.print(F("h "));
    display.print(minutes);
    display.print(F("m "));
    display.print(seconds);
    display.print(F("s"));
    
    // 累计浇水时间
    unsigned long totalWaterTime = 0;
    int totalWaterCount = 0;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        totalWaterTime += state.channels[ch].totalWaterTime;
        totalWaterCount += state.channels[ch].waterCount;
    }
    
    display.setCursor(0, 42);
    display.print(F("Total Water: "));
    display.print(totalWaterTime / 1000);
    display.print(F("s"));
    
    display.setCursor(0, 52);
    display.print(F("Water Count: "));
    display.print(totalWaterCount);
    
    // 内存信息
    display.setCursor(0, 62);
    display.print(F("Free Heap: "));
    display.print(ESP.getFreeHeap() / 1024);
    display.print(F("KB"));
}

/**
 * @brief 绘制通用折线图
 */
void display_draw_chart(float *data, int count, int x, int y, int width, int height, const char *title) {
    // 绘制图表边框
    display.drawRect(x, y, width, height, SSD1306_WHITE);
    
    // 如果没有数据，直接返回
    if (count < 2) {
        return;
    }
    
    // 计算数据的最大值和最小值（用于Y轴缩放）
    float minVal = data[0];
    float maxVal = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    // 避免除以零，确保至少有10的范围
    if (maxVal - minVal < 10.0) {
        float mid = (maxVal + minVal) / 2.0;
        minVal = mid - 5.0;
        maxVal = mid + 5.0;
    }
    
    // 绘制折线（从右往左，最新数据在右侧）
    int chartInnerWidth = width - 4;
    int chartInnerHeight = height - 4;
    
    for (int i = 0; i < count - 1; i++) {
        // 计算点的位置
        int x1 = x + 2 + (i * chartInnerWidth) / (count - 1);
        int x2 = x + 2 + ((i + 1) * chartInnerWidth) / (count - 1);
        
        // 将数据值映射到Y轴坐标（注意屏幕Y轴向下）
        int y1 = y + chartInnerHeight - ((data[i] - minVal) * chartInnerHeight / (maxVal - minVal));
        int y2 = y + chartInnerHeight - ((data[i + 1] - minVal) * chartInnerHeight / (maxVal - minVal));
        
        // 绘制线段
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
    
    // 绘制最大值、最小值、当前值标签（在图表上方）
    display.setTextSize(1);
    
    // 最小值
    display.setCursor(x, y - 8);
    display.print((int)minVal);
    
    // 最大值
    display.setCursor(x + 30, y - 8);
    display.print((int)maxVal);
    
    // 当前值（最右侧点）
    display.setCursor(x + 60, y - 8);
    display.print(F("Cur:"));
    display.print((int)data[count - 1]);
}

/**
 * @brief 添加数据点到图表缓冲区（环形缓冲区）
 */
void display_add_chart_data(uint8_t channel, float value) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    // 将数据添加到环形缓冲区
    s_chartData[channel][s_chartDataIndex[channel]] = value;
    s_chartDataIndex[channel] = (s_chartDataIndex[channel] + 1) % CHART_DATA_SIZE;
    
    // 更新数据点计数（最多30个）
    if (s_chartDataCount[channel] < CHART_DATA_SIZE) {
        s_chartDataCount[channel]++;
    }
}

/**
 * @brief 绘制进度条
 */
void display_draw_progress_bar(int x, int y, int width, int height, float percent) {
    // 限制百分比范围
    percent = constrain(percent, 0.0, 100.0);
    
    // 绘制外框
    display.drawRect(x, y, width, height, SSD1306_WHITE);
    
    // 计算填充宽度
    int fillWidth = (int)((width - 2) * percent / 100.0);
    
    // 绘制填充部分
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

/**
 * @brief 绘制WiFi信号图标
 */
void display_draw_wifi_icon(int x, int y, int rssi) {
    // 根据RSSI判断信号强度（绘制不同高度的波纹）
    // RSSI > -50: 4格满信号
    // RSSI > -60: 3格信号
    // RSSI > -70: 2格信号
    // RSSI > -80: 1格信号
    // RSSI <= -80: 无信号
    
    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;
    
    // 绘制信号条（从低到高）
    for (int i = 0; i < 4; i++) {
        int barHeight = (i + 1) * 2;
        int barX = x + i * 3;
        int barY = y + 8 - barHeight;
        
        if (i < bars) {
            // 填充
            display.fillRect(barX, barY, 2, barHeight, SSD1306_WHITE);
        } else {
            // 空心
            display.drawRect(barX, barY, 2, barHeight, SSD1306_WHITE);
        }
    }
}

/**
 * @brief 绘制水滴图标
 */
void display_draw_droplet_icon(int x, int y, bool filled) {
    // 水滴形状：圆形底部 + 三角形顶部
    // 使用像素点绘制简化的水滴
    
    if (filled) {
        // 填充水滴
        display.fillCircle(x + 3, y + 5, 3, SSD1306_WHITE);
        display.fillTriangle(x + 3, y, x + 1, y + 4, x + 5, y + 4, SSD1306_WHITE);
    } else {
        // 空心水滴
        display.drawCircle(x + 3, y + 5, 3, SSD1306_WHITE);
        display.drawTriangle(x + 3, y, x + 1, y + 4, x + 5, y + 4, SSD1306_WHITE);
    }
}

/**
 * @brief 显示报警信息覆盖层
 */
void display_show_alert(const char *msg) {
    s_alertActive = true;
    s_alertMessage = String(msg);
    s_alertStartTime = millis();
}

/**
 * @brief 切换到下一页
 */
void display_next_page() {
    s_currentPage = (DisplayPage)((s_currentPage + 1) % PAGE_COUNT);
}

/**
 * @brief 切换到上一页
 */
void display_prev_page() {
    if (s_currentPage == 0) {
        s_currentPage = (DisplayPage)(PAGE_COUNT - 1);
    } else {
        s_currentPage = (DisplayPage)(s_currentPage - 1);
    }
}

/**
 * @brief 设置当前页面
 */
void display_set_page(DisplayPage page) {
    if (page >= 0 && page < PAGE_COUNT) {
        s_currentPage = page;
    }
}

/**
 * @brief 获取当前页面
 */
DisplayPage display_get_page() {
    return s_currentPage;
}

/**
 * @brief 设置当前通道（用于通道详情页和图表页）
 */
void display_set_channel(uint8_t channel) {
    if (channel < NUM_CHANNELS) {
        s_currentChannel = channel;
    }
}

/**
 * @brief 获取当前通道
 */
uint8_t display_get_channel() {
    return s_currentChannel;
}

/**
 * @brief 清屏
 */
void display_clear() {
    display.clearDisplay();
}

/**
 * @brief 刷新显示
 */
void display_refresh() {
    display.display();
}
