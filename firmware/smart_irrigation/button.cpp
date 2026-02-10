/**
 * @file button.cpp
 * @brief 按键处理模块实现 - 按键检测和消抖
 * @author Smart Irrigation Project
 * @date 2026
 */

#include "button.h"
#include "config.h"
#include "irrigation.h"

// 按键状态变量
static unsigned long lastDebounceTime = 0;
static unsigned long buttonPressTime = 0;
static bool lastButtonState = HIGH;
static bool buttonPressed = false;
static bool longPressHandled = false;

const unsigned long DEBOUNCE_DELAY = 300;  // 消抖延迟 (ms)
const unsigned long LONG_PRESS_TIME = 2000; // 长按时间阈值 (ms)

/**
 * @brief 初始化按键模块
 */
void button_init() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("按键模块初始化完成");
}

/**
 * @brief 检测按键状态
 * @return true 检测到短按
 * @return false 无按键或长按处理中
 */
bool button_check() {
    bool currentButtonState = digitalRead(BUTTON_PIN);
    bool shortPressDetected = false;
    
    // 检测按键按下
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        buttonPressTime = millis();
        buttonPressed = true;
        longPressHandled = false;
    }
    
    // 检测按键释放
    if (currentButtonState == HIGH && lastButtonState == LOW) {
        if (buttonPressed && !longPressHandled) {
            // 消抖检查
            unsigned long pressDuration = millis() - buttonPressTime;
            if (pressDuration >= DEBOUNCE_DELAY && pressDuration < LONG_PRESS_TIME) {
                // 短按：切换模式
                shortPressDetected = true;
                irrigation_switch_mode();
                Serial.println("按键：短按 - 切换模式");
            }
        }
        buttonPressed = false;
        longPressHandled = false;
    }
    
    // 检测长按（按键仍然按下）
    if (currentButtonState == LOW && buttonPressed && !longPressHandled) {
        unsigned long pressDuration = millis() - buttonPressTime;
        if (pressDuration >= LONG_PRESS_TIME) {
            // 长按：在手动模式下切换水泵
            longPressHandled = true;
            IrrigationState& state = irrigation_get_state();
            if (state.mode == MODE_MANUAL) {
                irrigation_manual_toggle();
                Serial.println("按键：长按 - 手动模式切换水泵");
            } else {
                Serial.println("按键：长按 - 仅在手动模式下有效");
            }
        }
    }
    
    lastButtonState = currentButtonState;
    return shortPressDetected;
}
