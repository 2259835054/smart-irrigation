/**
 * @file irrigation.cpp
 * @brief 灌溉控制模块实现 - 自动/定时/手动模式控制
 * @author Smart Irrigation Project
 * @date 2026
 */

#include "irrigation.h"
#include "config.h"

// 全局灌溉状态
static IrrigationState state = {
    false,           // pumpRunning
    0,               // pumpStartTime
    0,               // lastWaterTime
    0,               // waterCount
    MODE_AUTO        // mode
};

// 当前水位状态（用于缺水检测）
static bool currentWaterLevel = true;

/**
 * @brief 初始化灌溉控制模块
 */
void irrigation_init() {
    // 配置引脚模式
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // 确保水泵初始关闭
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    Serial.println("灌溉控制模块初始化完成");
}

/**
 * @brief 启动水泵
 */
void irrigation_start_pump() {
    // 检查水位
    if (!currentWaterLevel) {
        Serial.println("警告：水箱缺水，无法启动水泵");
        // 蜂鸣器报警 - 短响3次
        for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
        }
        return;
    }
    
    if (!state.pumpRunning) {
        digitalWrite(PUMP_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        state.pumpRunning = true;
        state.pumpStartTime = millis();
        state.lastWaterTime = millis();
        state.waterCount++;
        Serial.println("水泵启动");
    }
}

/**
 * @brief 停止水泵
 */
void irrigation_stop_pump() {
    if (state.pumpRunning) {
        digitalWrite(PUMP_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        state.pumpRunning = false;
        Serial.println("水泵停止");
    }
}

/**
 * @brief 更新灌溉控制逻辑
 * @param data 传感器数据引用
 */
void irrigation_update(SensorData &data) {
    // 更新当前水位状态
    currentWaterLevel = data.waterLevelOk;
    
    switch (state.mode) {
        case MODE_AUTO:
            // 自动模式：根据土壤湿度控制
            if (!state.pumpRunning && data.soilMoisture < SOIL_DRY_THRESHOLD) {
                Serial.println("自动模式：土壤干燥，启动灌溉");
                irrigation_start_pump();
            } else if (state.pumpRunning && data.soilMoisture >= SOIL_WET_THRESHOLD) {
                Serial.println("自动模式：土壤湿度达标，停止灌溉");
                irrigation_stop_pump();
            }
            break;
            
        case MODE_TIMED:
            // 定时模式：每24小时浇水一次
            if (!state.pumpRunning) {
                unsigned long timeSinceLastWater = millis() - state.lastWaterTime;
                if (timeSinceLastWater >= TIMED_INTERVAL) {
                    Serial.println("定时模式：时间到，启动灌溉");
                    irrigation_start_pump();
                }
            } else {
                // 定时模式下浇水达到设定时长后停止
                unsigned long pumpRunTime = millis() - state.pumpStartTime;
                if (pumpRunTime >= TIMED_DURATION) {
                    Serial.println("定时模式：浇水时长达标，停止灌溉");
                    irrigation_stop_pump();
                }
            }
            break;
            
        case MODE_MANUAL:
            // 手动模式：不自动控制
            break;
    }
}

/**
 * @brief 切换灌溉模式 (AUTO->TIMED->MANUAL->AUTO)
 */
void irrigation_switch_mode() {
    // 切换模式前先停止水泵
    if (state.pumpRunning) {
        irrigation_stop_pump();
    }
    
    switch (state.mode) {
        case MODE_AUTO:
            state.mode = MODE_TIMED;
            Serial.println("切换到定时模式");
            break;
        case MODE_TIMED:
            state.mode = MODE_MANUAL;
            Serial.println("切换到手动模式");
            break;
        case MODE_MANUAL:
            state.mode = MODE_AUTO;
            Serial.println("切换到自动模式");
            break;
    }
}

/**
 * @brief 手动模式下切换水泵状态
 */
void irrigation_manual_toggle() {
    if (state.mode != MODE_MANUAL) {
        return;
    }
    
    if (state.pumpRunning) {
        irrigation_stop_pump();
    } else {
        irrigation_start_pump();
    }
}

/**
 * @brief 安全检查 (超时保护、缺水保护)
 */
void irrigation_safety_check() {
    // 超时保护
    if (state.pumpRunning) {
        unsigned long pumpRunTime = millis() - state.pumpStartTime;
        if (pumpRunTime >= PUMP_MAX_DURATION) {
            Serial.println("安全保护：浇水超时，强制停止");
            irrigation_stop_pump();
        }
    }
    
    // 缺水保护
    if (state.pumpRunning && !currentWaterLevel) {
        Serial.println("安全保护：检测到缺水，强制停止水泵");
        irrigation_stop_pump();
        // 蜂鸣器报警 - 短响3次
        for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
        }
    }
}

/**
 * @brief 获取当前灌溉状态
 * @return IrrigationState& 状态结构体引用
 */
IrrigationState& irrigation_get_state() {
    return state;
}
