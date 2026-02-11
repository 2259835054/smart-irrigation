/**
 * @file sensors.cpp
 * @brief 传感器模块实现
 * @author Smart Irrigation Project
 * @date 2026-02-11
 */

#include "sensors.h"
#include <DHT.h>

// ==================== 全局对象 ====================
DHT dht(DHT_PIN, DHT_TYPE);

// ==================== 全局变量 ====================
SensorData g_sensorData;
CalibrationData g_calibration[NUM_CHANNELS];

// 滑动窗口滤波缓冲区
static int sampleBuffer[NUM_CHANNELS][ADC_SAMPLES];
static int sampleIndex[NUM_CHANNELS] = {0};

// ==================== 私有函数声明 ====================
static void bubble_sort(int arr[], int n);
static int get_median(int arr[], int n);

/**
 * @brief 初始化所有传感器
 */
void sensors_init() {
    Serial.println("[Sensors] 初始化传感器模块...");
    
    // 配置ADC分辨率为12位 (0-4095)
    analogReadResolution(12);
    
    // 配置ADC衰减，扩展电压范围到0-3.3V
    analogSetAttenuation(ADC_11db);
    
    // 配置土壤湿度传感器引脚
    pinMode(SOIL_MOISTURE_PIN_1, INPUT);
    pinMode(SOIL_MOISTURE_PIN_2, INPUT);
    pinMode(SOIL_MOISTURE_PIN_3, INPUT);
    pinMode(SOIL_MOISTURE_PIN_4, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    
    // 配置水位传感器引脚（数字输入）
    pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
    
    // 初始化DHT22温湿度传感器
    dht.begin();
    
    // 初始化默认校准值
    // 电容式土壤湿度传感器典型值：
    // 空气中（干燥）：约2800-3100
    // 水中（湿润）：约1200-1500
    for (int i = 0; i < NUM_CHANNELS; i++) {
        g_calibration[i].dryValue = 3000;  // 干燥时ADC值
        g_calibration[i].wetValue = 1300;  // 湿润时ADC值
        
        // 初始化滤波缓冲区
        for (int j = 0; j < ADC_SAMPLES; j++) {
            sampleBuffer[i][j] = 0;
        }
        sampleIndex[i] = 0;
    }
    
    // 初始化传感器数据结构
    g_sensorData.timestamp = 0;
    g_sensorData.temperature = 0.0;
    g_sensorData.humidity = 0.0;
    g_sensorData.lightLevel = 0.0;
    g_sensorData.waterLevelOk = true;
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        g_sensorData.channels[i].soilMoisture = 0.0;
        g_sensorData.channels[i].isValid = false;
    }
    
    Serial.println("[Sensors] 传感器模块初始化完成");
}

/**
 * @brief 读取所有传感器数据
 */
SensorData sensors_read() {
    SensorData data;
    data.timestamp = millis();
    
    // 1. 读取4路土壤湿度（带滤波）
    data.channels[0].soilMoisture = sensors_read_soil_filtered(0);
    data.channels[0].isValid = true;
    
    data.channels[1].soilMoisture = sensors_read_soil_filtered(1);
    data.channels[1].isValid = true;
    
    data.channels[2].soilMoisture = sensors_read_soil_filtered(2);
    data.channels[2].isValid = true;
    
    data.channels[3].soilMoisture = sensors_read_soil_filtered(3);
    data.channels[3].isValid = true;
    
    // 2. 读取DHT22温湿度
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    // 检查读取是否成功
    if (!isnan(temp) && !isnan(hum)) {
        data.temperature = temp;
        data.humidity = hum;
    } else {
        // 读取失败，使用上次值
        data.temperature = g_sensorData.temperature;
        data.humidity = g_sensorData.humidity;
        Serial.println("[Sensors] 警告：DHT22读取失败");
    }
    
    // 3. 读取光照传感器
    data.lightLevel = sensors_read_light();
    
    // 4. 读取水位传感器
    data.waterLevelOk = sensors_read_water_level();
    
    // 更新全局数据
    g_sensorData = data;
    
    return data;
}

/**
 * @brief 读取单通道土壤湿度（带滑动窗口中值滤波）
 */
float sensors_read_soil_filtered(uint8_t channel) {
    if (channel >= NUM_CHANNELS) {
        Serial.printf("[Sensors] 错误：无效通道号 %d\n", channel);
        return 0.0;
    }
    
    // 获取引脚号
    uint8_t pin;
    switch (channel) {
        case 0: pin = SOIL_MOISTURE_PIN_1; break;
        case 1: pin = SOIL_MOISTURE_PIN_2; break;
        case 2: pin = SOIL_MOISTURE_PIN_3; break;
        case 3: pin = SOIL_MOISTURE_PIN_4; break;
        default: return 0.0;
    }
    
    // 采集ADC_SAMPLES个样本
    int samples[ADC_SAMPLES];
    for (int i = 0; i < ADC_SAMPLES; i++) {
        samples[i] = analogRead(pin);
        delayMicroseconds(100);  // 采样间隔
    }
    
    // 对样本进行排序
    bubble_sort(samples, ADC_SAMPLES);
    
    // 取中值
    int medianValue = get_median(samples, ADC_SAMPLES);
    
    // 更新滑动窗口
    sampleBuffer[channel][sampleIndex[channel]] = medianValue;
    sampleIndex[channel] = (sampleIndex[channel] + 1) % ADC_SAMPLES;
    
    // 转换为湿度百分比
    float moisture = sensors_convert_to_moisture(channel, medianValue);
    
    return moisture;
}

/**
 * @brief 读取光照传感器
 */
float sensors_read_light() {
    // 读取ADC值
    int rawValue = analogRead(LIGHT_SENSOR_PIN);
    
    // 转换为百分比（0-100%）
    // 光敏电阻：光强时电阻小，ADC值低
    // ADC值范围：0（最亮）- 4095（最暗）
    float lightPercent = map(rawValue, 0, 4095, 100, 0);
    lightPercent = constrain(lightPercent, 0, 100);
    
    return lightPercent;
}

/**
 * @brief 读取水位传感器
 */
bool sensors_read_water_level() {
    // 浮球开关：高电平=有水，低电平=缺水
    // 使用上拉电阻，浮球断开时为高电平
    int value = digitalRead(WATER_LEVEL_PIN);
    return (value == HIGH);
}

/**
 * @brief 校准土壤湿度传感器
 */
bool sensors_calibrate(uint8_t channel, int dryValue, int wetValue) {
    if (channel >= NUM_CHANNELS) {
        Serial.printf("[Sensors] 校准失败：无效通道号 %d\n", channel);
        return false;
    }
    
    if (dryValue <= wetValue) {
        Serial.printf("[Sensors] 校准失败：干燥值必须大于湿润值\n");
        return false;
    }
    
    g_calibration[channel].dryValue = dryValue;
    g_calibration[channel].wetValue = wetValue;
    
    Serial.printf("[Sensors] 通道%d 校准成功：干燥=%d, 湿润=%d\n", 
                  channel, dryValue, wetValue);
    
    return true;
}

/**
 * @brief 将ADC原始值转换为湿度百分比
 */
float sensors_convert_to_moisture(uint8_t channel, int rawValue) {
    if (channel >= NUM_CHANNELS) {
        return 0.0;
    }
    
    int dryVal = g_calibration[channel].dryValue;
    int wetVal = g_calibration[channel].wetValue;
    
    // 线性映射：干燥值=0%，湿润值=100%
    // 注意：电容式传感器的ADC值与湿度成反比
    float moisture = map(rawValue, dryVal, wetVal, 0, 100);
    moisture = constrain(moisture, 0, 100);
    
    return moisture;
}

/**
 * @brief 获取ADC原始值
 */
int sensors_read_adc_raw(uint8_t pin) {
    return analogRead(pin);
}

/**
 * @brief 打印传感器数据到串口
 */
void sensors_print(const SensorData &data) {
    Serial.println("========== 传感器数据 ==========");
    Serial.printf("时间戳: %lu ms\n", data.timestamp);
    Serial.printf("温度: %.1f °C\n", data.temperature);
    Serial.printf("湿度: %.1f %%\n", data.humidity);
    Serial.printf("光照: %.1f %%\n", data.lightLevel);
    Serial.printf("水位: %s\n", data.waterLevelOk ? "正常" : "缺水");
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        Serial.printf("通道%d 土壤湿度: %.1f %% [%s]\n", 
                      i, 
                      data.channels[i].soilMoisture,
                      data.channels[i].isValid ? "有效" : "无效");
    }
    Serial.println("================================");
}

// ==================== 私有函数实现 ====================

/**
 * @brief 冒泡排序（用于中值滤波）
 */
static void bubble_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

/**
 * @brief 获取中值
 */
static int get_median(int arr[], int n) {
    // 数组应已排序
    if (n % 2 == 0) {
        // 偶数个元素，取中间两个的平均值
        return (arr[n / 2 - 1] + arr[n / 2]) / 2;
    } else {
        // 奇数个元素，取中间值
        return arr[n / 2];
    }
}
