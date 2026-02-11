/**
 * @file button.cpp
 * @brief 按键处理模块实现
 * @author Smart Irrigation Project
 * @date 2026-02-11
 */

#include "button.h"
#include "display.h"
#include "irrigation.h"

// ==================== 全局变量 ====================
Button g_buttonMode;
Button g_buttonSelect;

/**
 * @brief 初始化按键模块
 */
void button_init() {
    Serial.println("[Button] 初始化按键模块...");
    
    // 配置MODE按键
    g_buttonMode.pin = BUTTON_MODE_PIN;
    g_buttonMode.state = BTN_IDLE;
    g_buttonMode.currentReading = false;
    g_buttonMode.lastReading = false;
    g_buttonMode.lastDebounceTime = 0;
    g_buttonMode.pressStartTime = 0;
    g_buttonMode.lastClickTime = 0;
    g_buttonMode.longPressSent = false;
    g_buttonMode.clickCount = 0;
    
    // 配置SELECT按键
    g_buttonSelect.pin = BUTTON_SELECT_PIN;
    g_buttonSelect.state = BTN_IDLE;
    g_buttonSelect.currentReading = false;
    g_buttonSelect.lastReading = false;
    g_buttonSelect.lastDebounceTime = 0;
    g_buttonSelect.pressStartTime = 0;
    g_buttonSelect.lastClickTime = 0;
    g_buttonSelect.longPressSent = false;
    g_buttonSelect.clickCount = 0;
    
    // 配置引脚为输入上拉
    pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
    
    Serial.println("[Button] 按键模块初始化完成");
}

/**
 * @brief 按键扫描和处理
 */
void button_scan() {
    ButtonEventType eventType;
    
    // 更新MODE按键
    if (button_update(&g_buttonMode, eventType)) {
        // 生成事件
        if (eventType == BTN_EVT_MODE_SHORT || eventType == BTN_EVT_MODE_LONG) {
            button_send_event(eventType);
        }
    }
    
    // 更新SELECT按键
    if (button_update(&g_buttonSelect, eventType)) {
        // 生成事件
        if (eventType == BTN_EVT_SELECT_SHORT || 
            eventType == BTN_EVT_SELECT_LONG ||
            eventType == BTN_EVT_SELECT_DOUBLE) {
            button_send_event(eventType);
        }
    }
}

/**
 * @brief 更新单个按键状态（状态机）
 */
bool button_update(Button *btn, ButtonEventType &eventType) {
    unsigned long now = millis();
    bool reading = button_read(btn->pin);
    bool eventDetected = false;
    
    // 状态机处理
    switch (btn->state) {
        case BTN_IDLE:
            if (reading) {
                // 检测到按下
                btn->state = BTN_DEBOUNCE;
                btn->lastDebounceTime = now;
            }
            break;
            
        case BTN_DEBOUNCE:
            if (reading) {
                // 仍然按下
                if ((now - btn->lastDebounceTime) >= DEBOUNCE_DELAY) {
                    // 消抖完成，确认按下
                    btn->state = BTN_PRESSED;
                    btn->pressStartTime = now;
                    btn->longPressSent = false;
                }
            } else {
                // 抖动，返回空闲
                btn->state = BTN_IDLE;
            }
            break;
            
        case BTN_PRESSED:
            if (reading) {
                // 继续按下，检查是否长按
                if (!btn->longPressSent && 
                    (now - btn->pressStartTime) >= LONG_PRESS_TIME) {
                    // 长按事件
                    btn->state = BTN_LONG_PRESS;
                    btn->longPressSent = true;
                    
                    // 生成长按事件
                    if (btn->pin == BUTTON_MODE_PIN) {
                        eventType = BTN_EVT_MODE_LONG;
                    } else {
                        eventType = BTN_EVT_SELECT_LONG;
                    }
                    eventDetected = true;
                }
            } else {
                // 释放
                btn->state = BTN_RELEASED;
            }
            break;
            
        case BTN_LONG_PRESS:
            if (!reading) {
                // 长按后释放
                btn->state = BTN_IDLE;
            }
            break;
            
        case BTN_RELEASED:
            // 检查是否为短按或双击
            if (!btn->longPressSent) {
                // 检查双击
                if ((now - btn->lastClickTime) < DOUBLE_CLICK_TIME) {
                    // 双击（仅SELECT键支持）
                    if (btn->pin == BUTTON_SELECT_PIN) {
                        eventType = BTN_EVT_SELECT_DOUBLE;
                        eventDetected = true;
                    }
                    btn->clickCount = 0;
                } else {
                    // 单击
                    btn->clickCount = 1;
                    btn->lastClickTime = now;
                    
                    // 生成短按事件
                    if (btn->pin == BUTTON_MODE_PIN) {
                        eventType = BTN_EVT_MODE_SHORT;
                    } else {
                        eventType = BTN_EVT_SELECT_SHORT;
                    }
                    eventDetected = true;
                }
            }
            
            btn->state = BTN_IDLE;
            break;
    }
    
    btn->lastReading = reading;
    return eventDetected;
}

/**
 * @brief 读取按键电平
 */
bool button_read(uint8_t pin) {
    // 使用上拉电阻，按下时为LOW
    return (digitalRead(pin) == LOW);
}

/**
 * @brief 发送按键事件到队列
 */
void button_send_event(ButtonEventType eventType) {
    if (xButtonEventQueue == NULL) {
        return;
    }
    
    ButtonEvent event;
    event.type = eventType;
    event.timestamp = millis();
    
    // 发送到队列（不阻塞）
    xQueueSend(xButtonEventQueue, &event, 0);
}

/**
 * @brief 处理按键事件
 */
void button_handle_event(const ButtonEvent &event) {
    Serial.printf("[Button] 处理按键事件：类型=%d\n", event.type);
    
    switch (event.type) {
        case BTN_EVT_MODE_SHORT:
            // MODE短按：切换OLED页面
            display_next_page();
            Serial.println("[Button] MODE短按 -> 切换页面");
            break;
            
        case BTN_EVT_MODE_LONG:
            // MODE长按：切换灌溉模式
            {
                IrrigationMode currentMode = irrigation_get_mode();
                IrrigationMode newMode;
                
                // 循环切换模式
                switch (currentMode) {
                    case MODE_AUTO_PID:
                        newMode = MODE_AUTO_SIMPLE;
                        break;
                    case MODE_AUTO_SIMPLE:
                        newMode = MODE_TIMED;
                        break;
                    case MODE_TIMED:
                        newMode = MODE_MANUAL;
                        break;
                    case MODE_MANUAL:
                        newMode = MODE_AUTO_PID;
                        break;
                    default:
                        newMode = MODE_AUTO_PID;
                }
                
                irrigation_set_mode(newMode);
                Serial.printf("[Button] MODE长按 -> 切换模式到 %s\n",
                              irrigation_get_mode_str(newMode));
            }
            break;
            
        case BTN_EVT_SELECT_SHORT:
            // SELECT短按：切换通道（在通道详情页）
            {
                DisplayPage currentPage = display_get_page();
                if (currentPage == PAGE_CHANNEL) {
                    uint8_t channel = display_get_channel();
                    channel = (channel + 1) % NUM_CHANNELS;
                    display_set_channel(channel);
                    Serial.printf("[Button] SELECT短按 -> 切换到通道%d\n", channel);
                }
            }
            break;
            
        case BTN_EVT_SELECT_LONG:
            // SELECT长按：手动浇水开关（切换当前通道）
            {
                uint8_t channel = display_get_channel();
                IrrigationState &state = irrigation_get_state();
                
                if (state.channels[channel].pumpRunning) {
                    // 停止
                    irrigation_stop_pump(channel);
                    Serial.printf("[Button] SELECT长按 -> 停止通道%d\n", channel);
                } else {
                    // 启动
                    irrigation_start_pump(channel, 255);
                    Serial.printf("[Button] SELECT长按 -> 启动通道%d\n", channel);
                }
            }
            break;
            
        case BTN_EVT_SELECT_DOUBLE:
            // SELECT双击：紧急停止所有水泵
            irrigation_stop_all();
            Serial.println("[Button] SELECT双击 -> 紧急停止所有水泵！");
            
            // 蜂鸣器提示
            digitalWrite(BUZZER_PIN, HIGH);
            delay(200);
            digitalWrite(BUZZER_PIN, LOW);
            break;
    }
}

/**
 * @brief 重置按键状态
 */
void button_reset(Button *btn) {
    btn->state = BTN_IDLE;
    btn->currentReading = false;
    btn->lastReading = false;
    btn->lastDebounceTime = 0;
    btn->pressStartTime = 0;
    btn->lastClickTime = 0;
    btn->longPressSent = false;
    btn->clickCount = 0;
}
