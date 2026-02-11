/**
 * @file irrigation.cpp
 * @brief 灌溉控制模块实现
 * @author Smart Irrigation Project
 * @date 2026-02-11
 */

#include "irrigation.h"

// ==================== 全局变量 ====================
IrrigationState g_irrigationState;

// PWM 通道配置（ESP32 LEDC）
#define PWM_FREQ      5000   // PWM频率 5kHz
#define PWM_RESOLUTION 8     // 8位分辨率 (0-255)
#define PWM_CHANNEL_0  0
#define PWM_CHANNEL_1  1
#define PWM_CHANNEL_2  2
#define PWM_CHANNEL_3  3

/**
 * @brief 初始化灌溉控制模块
 */
void irrigation_init() {
    Serial.println("[Irrigation] 初始化灌溉控制模块...");
    
    // 配置水泵引脚为输出
    pinMode(PUMP_PIN_1, OUTPUT);
    pinMode(PUMP_PIN_2, OUTPUT);
    pinMode(PUMP_PIN_3, OUTPUT);
    pinMode(PUMP_PIN_4, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // 初始化所有水泵为关闭状态
    digitalWrite(PUMP_PIN_1, LOW);
    digitalWrite(PUMP_PIN_2, LOW);
    digitalWrite(PUMP_PIN_3, LOW);
    digitalWrite(PUMP_PIN_4, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    // 配置PWM通道
    ledcSetup(PWM_CHANNEL_0, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_3, PWM_FREQ, PWM_RESOLUTION);
    
    // 绑定PWM通道到引脚
    ledcAttachPin(PUMP_PIN_1, PWM_CHANNEL_0);
    ledcAttachPin(PUMP_PIN_2, PWM_CHANNEL_1);
    ledcAttachPin(PUMP_PIN_3, PWM_CHANNEL_2);
    ledcAttachPin(PUMP_PIN_4, PWM_CHANNEL_3);
    
    // 初始化系统状态
    g_irrigationState.mode = MODE_AUTO_PID;
    g_irrigationState.systemEnabled = true;
    g_irrigationState.uptime = 0;
    g_irrigationState.alarmActive = false;
    g_irrigationState.alarmMessage = "";
    
    // 初始化各通道状态
    for (int i = 0; i < NUM_CHANNELS; i++) {
        g_irrigationState.channels[i].pumpRunning = false;
        g_irrigationState.channels[i].pumpPWM = 0;
        g_irrigationState.channels[i].pumpStartTime = 0;
        g_irrigationState.channels[i].lastWaterTime = 0;
        g_irrigationState.channels[i].totalWaterTime = 0;
        g_irrigationState.channels[i].waterCount = 0;
        g_irrigationState.channels[i].dryThreshold = SOIL_DRY_DEFAULT;
        g_irrigationState.channels[i].wetThreshold = SOIL_WET_DEFAULT;
        g_irrigationState.channels[i].enabled = true;
        
        // 创建PID控制器
        g_irrigationState.channels[i].pidController = new PIDController(
            PID_KP, PID_KI, PID_KD, 
            PID_OUTPUT_MIN, PID_OUTPUT_MAX
        );
        
        // 设置PID目标值为湿润阈值
        g_irrigationState.channels[i].pidController->setSetpoint(SOIL_WET_DEFAULT);
    }
    
    Serial.println("[Irrigation] 灌溉控制模块初始化完成");
}

/**
 * @brief 更新灌溉控制
 */
void irrigation_update(SensorData &data) {
    // 更新运行时间
    g_irrigationState.uptime = millis();
    
    // 安全检查
    irrigation_safety_check();
    
    // 如果系统未启用，停止所有水泵
    if (!g_irrigationState.systemEnabled) {
        irrigation_stop_all();
        return;
    }
    
    // 根据模式更新各通道
    for (int i = 0; i < NUM_CHANNELS; i++) {
        // 跳过未启用的通道
        if (!g_irrigationState.channels[i].enabled) {
            continue;
        }
        
        // 检查数据有效性
        if (!data.channels[i].isValid) {
            continue;
        }
        
        float currentMoisture = data.channels[i].soilMoisture;
        
        // 根据模式执行相应控制
        switch (g_irrigationState.mode) {
            case MODE_AUTO_PID:
                irrigation_pid_control(i, currentMoisture);
                break;
                
            case MODE_AUTO_SIMPLE:
                irrigation_threshold_control(i, currentMoisture);
                break;
                
            case MODE_TIMED:
                // 定时模式由外部命令触发
                break;
                
            case MODE_MANUAL:
                // 手动模式由外部命令控制
                break;
        }
    }
}

/**
 * @brief PID 控制模式更新
 */
void irrigation_pid_control(uint8_t channel, float currentMoisture) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    ChannelState *ch = &g_irrigationState.channels[channel];
    
    // 计算PID输出
    float pidOutput = ch->pidController->compute(currentMoisture);
    
    // 将PID输出映射到PWM值 (0-255)
    uint8_t pwm = (uint8_t)pidOutput;
    
    // 如果输出很小（小于阈值），完全关闭水泵
    const uint8_t MIN_PWM_THRESHOLD = 30;
    if (pwm < MIN_PWM_THRESHOLD) {
        if (ch->pumpRunning) {
            irrigation_stop_pump(channel);
        }
        return;
    }
    
    // 如果当前湿度已达到或超过目标值，停止浇水
    if (currentMoisture >= ch->pidController->getSetpoint()) {
        if (ch->pumpRunning) {
            irrigation_stop_pump(channel);
        }
        return;
    }
    
    // 检查冷却时间
    unsigned long now = millis();
    if (ch->lastWaterTime > 0 && 
        (now - ch->lastWaterTime) < PUMP_COOLDOWN) {
        // 还在冷却期，不启动水泵
        return;
    }
    
    // 启动或更新PWM
    irrigation_start_pump(channel, pwm);
}

/**
 * @brief 简单阈值控制模式更新
 */
void irrigation_threshold_control(uint8_t channel, float currentMoisture) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    ChannelState *ch = &g_irrigationState.channels[channel];
    
    // 如果湿度低于干燥阈值，启动水泵
    if (currentMoisture < ch->dryThreshold) {
        // 检查冷却时间
        unsigned long now = millis();
        if (ch->lastWaterTime > 0 && 
            (now - ch->lastWaterTime) < PUMP_COOLDOWN) {
            return;  // 还在冷却期
        }
        
        if (!ch->pumpRunning) {
            // 启动水泵（全速）
            irrigation_start_pump(channel, 255);
        }
    }
    // 如果湿度高于湿润阈值，停止水泵
    else if (currentMoisture >= ch->wetThreshold) {
        if (ch->pumpRunning) {
            irrigation_stop_pump(channel);
        }
    }
}

/**
 * @brief 启动指定通道水泵
 */
void irrigation_start_pump(uint8_t channel, uint8_t pwm) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    ChannelState *ch = &g_irrigationState.channels[channel];
    
    // 设置PWM输出
    ledcWrite(channel, pwm);
    
    // 更新状态
    if (!ch->pumpRunning) {
        ch->pumpStartTime = millis();
        ch->waterCount++;
        Serial.printf("[Irrigation] 通道%d 水泵启动, PWM=%d\n", channel, pwm);
    }
    
    ch->pumpRunning = true;
    ch->pumpPWM = pwm;
    
    // 点亮系统LED
    digitalWrite(LED_PIN, HIGH);
}

/**
 * @brief 停止指定通道水泵
 */
void irrigation_stop_pump(uint8_t channel) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    ChannelState *ch = &g_irrigationState.channels[channel];
    
    if (!ch->pumpRunning) {
        return;  // 已经停止
    }
    
    // 停止PWM输出
    ledcWrite(channel, 0);
    
    // 更新状态
    unsigned long now = millis();
    unsigned long runTime = now - ch->pumpStartTime;
    ch->totalWaterTime += runTime;
    ch->lastWaterTime = now;
    ch->pumpRunning = false;
    ch->pumpPWM = 0;
    
    Serial.printf("[Irrigation] 通道%d 水泵停止, 运行时间=%lu ms\n", 
                  channel, runTime);
    
    // 检查是否所有水泵都停止，如果是则关闭LED
    bool anyRunning = false;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (g_irrigationState.channels[i].pumpRunning) {
            anyRunning = true;
            break;
        }
    }
    if (!anyRunning) {
        digitalWrite(LED_PIN, LOW);
    }
}

/**
 * @brief 紧急停止所有水泵
 */
void irrigation_stop_all() {
    Serial.println("[Irrigation] 紧急停止所有水泵！");
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        irrigation_stop_pump(i);
    }
    
    digitalWrite(LED_PIN, LOW);
}

/**
 * @brief 设置通道阈值
 */
bool irrigation_set_threshold(uint8_t channel, float dry, float wet) {
    if (channel >= NUM_CHANNELS) {
        return false;
    }
    
    if (dry >= wet || dry < 0 || wet > 100) {
        Serial.printf("[Irrigation] 阈值设置失败：干燥阈值必须小于湿润阈值\n");
        return false;
    }
    
    g_irrigationState.channels[channel].dryThreshold = dry;
    g_irrigationState.channels[channel].wetThreshold = wet;
    
    // 同时更新PID目标值
    g_irrigationState.channels[channel].pidController->setSetpoint(wet);
    
    Serial.printf("[Irrigation] 通道%d 阈值更新：干燥=%.1f%%, 湿润=%.1f%%\n",
                  channel, dry, wet);
    
    return true;
}

/**
 * @brief 切换灌溉模式
 */
void irrigation_set_mode(IrrigationMode newMode) {
    if (newMode == g_irrigationState.mode) {
        return;
    }
    
    Serial.printf("[Irrigation] 模式切换：%s -> %s\n",
                  irrigation_get_mode_str(g_irrigationState.mode),
                  irrigation_get_mode_str(newMode));
    
    // 切换模式前停止所有水泵
    irrigation_stop_all();
    
    // 重置所有PID控制器
    if (newMode == MODE_AUTO_PID) {
        for (int i = 0; i < NUM_CHANNELS; i++) {
            g_irrigationState.channels[i].pidController->reset();
        }
    }
    
    g_irrigationState.mode = newMode;
}

/**
 * @brief 获取当前模式
 */
IrrigationMode irrigation_get_mode() {
    return g_irrigationState.mode;
}

/**
 * @brief 获取模式名称字符串
 */
const char* irrigation_get_mode_str(IrrigationMode mode) {
    switch (mode) {
        case MODE_AUTO_PID:    return "PID自动控制";
        case MODE_AUTO_SIMPLE: return "阈值自动控制";
        case MODE_TIMED:       return "定时灌溉";
        case MODE_MANUAL:      return "手动控制";
        default:               return "未知模式";
    }
}

/**
 * @brief 获取当前模式名称字符串
 */
const char* irrigation_get_mode_str() {
    return irrigation_get_mode_str(g_irrigationState.mode);
}

/**
 * @brief 安全检查
 */
void irrigation_safety_check() {
    unsigned long now = millis();
    bool hasAlarm = false;
    String alarmMsg = "";
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ChannelState *ch = &g_irrigationState.channels[i];
        
        if (!ch->pumpRunning) {
            continue;
        }
        
        // 检查1：单次浇水超时保护
        unsigned long runTime = now - ch->pumpStartTime;
        if (runTime > PUMP_MAX_DURATION) {
            Serial.printf("[Irrigation] 警告：通道%d 浇水超时，强制停止\n", i);
            irrigation_stop_pump(i);
            hasAlarm = true;
            alarmMsg = "通道" + String(i) + "浇水超时";
        }
    }
    
    // 检查2：缺水保护
    if (!g_sensorData.waterLevelOk) {
        Serial.println("[Irrigation] 警告：水位过低，停止所有水泵");
        irrigation_stop_all();
        hasAlarm = true;
        alarmMsg = "水位过低，请加水";
    }
    
    // 更新报警状态
    g_irrigationState.alarmActive = hasAlarm;
    if (hasAlarm) {
        g_irrigationState.alarmMessage = alarmMsg;
        // 蜂鸣器报警
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
    } else {
        g_irrigationState.alarmMessage = "";
    }
}

/**
 * @brief 处理命令队列中的命令
 */
void irrigation_process_command(Command &cmd) {
    Serial.printf("[Irrigation] 处理命令：类型=%d, 通道=%d, 值=%d\n",
                  cmd.type, cmd.channel, cmd.value);
    
    switch (cmd.type) {
        case CMD_START_PUMP:
            if (cmd.channel < NUM_CHANNELS) {
                uint8_t pwm = (cmd.value > 0 && cmd.value <= 255) ? cmd.value : 255;
                irrigation_start_pump(cmd.channel, pwm);
            }
            break;
            
        case CMD_STOP_PUMP:
            if (cmd.channel < NUM_CHANNELS) {
                irrigation_stop_pump(cmd.channel);
            } else if (cmd.channel == 255) {
                irrigation_stop_all();
            }
            break;
            
        case CMD_SWITCH_MODE:
            irrigation_set_mode((IrrigationMode)cmd.value);
            break;
            
        case CMD_SET_THRESHOLD:
            // value 编码为 dry*100 + wet （需要外部解码）
            break;
            
        case CMD_MANUAL_WATER:
            // 手动浇水（临时切换到手动模式）
            break;
            
        case CMD_ENTER_SLEEP:
            // 进入休眠（由power_manager处理）
            break;
    }
}

/**
 * @brief 获取灌溉系统状态
 */
IrrigationState& irrigation_get_state() {
    return g_irrigationState;
}

/**
 * @brief 启用/禁用系统
 */
void irrigation_enable(bool enabled) {
    g_irrigationState.systemEnabled = enabled;
    Serial.printf("[Irrigation] 系统%s\n", enabled ? "启用" : "禁用");
    
    if (!enabled) {
        irrigation_stop_all();
    }
}

/**
 * @brief 启用/禁用指定通道
 */
void irrigation_enable_channel(uint8_t channel, bool enabled) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    g_irrigationState.channels[channel].enabled = enabled;
    Serial.printf("[Irrigation] 通道%d %s\n", 
                  channel, enabled ? "启用" : "禁用");
    
    if (!enabled) {
        irrigation_stop_pump(channel);
    }
}

/**
 * @brief 设置PID参数
 */
void irrigation_set_pid_params(uint8_t channel, float kp, float ki, float kd) {
    if (channel >= NUM_CHANNELS) {
        return;
    }
    
    g_irrigationState.channels[channel].pidController->setTunings(kp, ki, kd);
    Serial.printf("[Irrigation] 通道%d PID参数更新：Kp=%.2f, Ki=%.2f, Kd=%.2f\n",
                  channel, kp, ki, kd);
}

/**
 * @brief 打印灌溉状态到串口
 */
void irrigation_print_status() {
    Serial.println("========== 灌溉系统状态 ==========");
    Serial.printf("系统：%s\n", g_irrigationState.systemEnabled ? "启用" : "禁用");
    Serial.printf("模式：%s\n", irrigation_get_mode_str());
    Serial.printf("运行时间：%lu ms\n", g_irrigationState.uptime);
    Serial.printf("报警：%s\n", g_irrigationState.alarmActive ? "是" : "否");
    if (g_irrigationState.alarmActive) {
        Serial.printf("报警信息：%s\n", g_irrigationState.alarmMessage.c_str());
    }
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ChannelState *ch = &g_irrigationState.channels[i];
        Serial.printf("--- 通道 %d ---\n", i);
        Serial.printf("  启用：%s\n", ch->enabled ? "是" : "否");
        Serial.printf("  水泵：%s (PWM=%d)\n", 
                      ch->pumpRunning ? "运行" : "停止", ch->pumpPWM);
        Serial.printf("  阈值：干燥=%.1f%%, 湿润=%.1f%%\n",
                      ch->dryThreshold, ch->wetThreshold);
        Serial.printf("  统计：浇水%d次, 累计%lu ms\n",
                      ch->waterCount, ch->totalWaterTime);
    }
    Serial.println("==================================");
}
