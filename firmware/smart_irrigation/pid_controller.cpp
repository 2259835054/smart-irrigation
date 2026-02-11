/**
 * @file pid_controller.cpp
 * @brief PID 控制器实现
 * @author Smart Irrigation Project
 * @date 2026-02-11
 */

#include "pid_controller.h"

/**
 * @brief 构造函数
 */
PIDController::PIDController(float kp, float ki, float kd, float outMin, float outMax) {
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
    this->outputMin = outMin;
    this->outputMax = outMax;
    
    this->setpoint = 0.0;
    this->integral = 0.0;
    this->prevError = 0.0;
    this->prevInput = 0.0;
    this->lastTime = 0;
    this->firstRun = true;
    
    // 设置积分限幅为输出范围
    this->integralMax = (outMax - outMin) / 2.0;
    
    this->lastError = 0.0;
    this->lastDerivative = 0.0;
    this->lastOutput = 0.0;
}

/**
 * @brief 设置目标值
 */
void PIDController::setSetpoint(float sp) {
    this->setpoint = sp;
}

/**
 * @brief 获取当前目标值
 */
float PIDController::getSetpoint() {
    return this->setpoint;
}

/**
 * @brief 设置PID参数
 */
void PIDController::setTunings(float kp, float ki, float kd) {
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
}

/**
 * @brief 获取PID参数
 */
void PIDController::getTunings(float &kp, float &ki, float &kd) {
    kp = this->kp;
    ki = this->ki;
    kd = this->kd;
}

/**
 * @brief PID核心计算函数
 */
float PIDController::compute(float input) {
    unsigned long now = millis();
    
    // 首次运行初始化
    if (firstRun) {
        lastTime = now;
        prevInput = input;
        prevError = setpoint - input;
        firstRun = false;
        return 0.0;
    }
    
    // 计算时间间隔（秒）
    float dt = (now - lastTime) / 1000.0;
    
    // 如果时间间隔太小，跳过本次计算
    if (dt < 0.001) {
        return lastOutput;
    }
    
    // 步骤1：计算误差
    // error = setpoint - input
    // 正误差表示当前值低于目标值（需要加大输出）
    float error = setpoint - input;
    lastError = error;
    
    // 步骤2：计算比例项
    // P = Kp * error
    float pTerm = kp * error;
    
    // 步骤3：计算积分项（带抗饱和限幅）
    // I += Ki * error * dt
    integral += error * dt;
    
    // 积分限幅，防止积分饱和
    if (integral > integralMax) {
        integral = integralMax;
    } else if (integral < -integralMax) {
        integral = -integralMax;
    }
    
    float iTerm = ki * integral;
    
    // 步骤4：计算微分项
    // 使用误差微分（而非输入微分）
    // D = Kd * (error - prevError) / dt
    float derivative = (error - prevError) / dt;
    lastDerivative = derivative;
    float dTerm = kd * derivative;
    
    // 步骤5：计算总输出
    // output = P + I + D
    float output = pTerm + iTerm + dTerm;
    
    // 步骤6：输出限幅
    if (output > outputMax) {
        output = outputMax;
    } else if (output < outputMin) {
        output = outputMin;
    }
    
    // 保存状态用于下次计算
    prevError = error;
    prevInput = input;
    lastTime = now;
    lastOutput = output;
    
    return output;
}

/**
 * @brief 重置PID状态
 */
void PIDController::reset() {
    integral = 0.0;
    prevError = 0.0;
    prevInput = 0.0;
    lastError = 0.0;
    lastDerivative = 0.0;
    lastOutput = 0.0;
    lastTime = millis();
    firstRun = true;
}

/**
 * @brief 设置输出限幅
 */
void PIDController::setOutputLimits(float min, float max) {
    if (min >= max) {
        return;
    }
    outputMin = min;
    outputMax = max;
    
    // 同时调整积分限幅
    integralMax = (max - min) / 2.0;
}

/**
 * @brief 设置积分限幅
 */
void PIDController::setIntegralLimit(float max) {
    if (max < 0) {
        return;
    }
    integralMax = max;
}

/**
 * @brief 获取最后一次误差
 */
float PIDController::getLastError() const {
    return lastError;
}

/**
 * @brief 获取积分项当前值
 */
float PIDController::getIntegral() const {
    return integral;
}

/**
 * @brief 获取最后一次微分项
 */
float PIDController::getDerivative() const {
    return lastDerivative;
}

/**
 * @brief 获取最后一次输出
 */
float PIDController::getOutput() const {
    return lastOutput;
}

/**
 * @brief 打印PID状态到串口
 */
void PIDController::printStatus() const {
    Serial.println("========== PID 状态 ==========");
    Serial.printf("目标值(Setpoint): %.2f\n", setpoint);
    Serial.printf("参数: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", kp, ki, kd);
    Serial.printf("误差(Error): %.2f\n", lastError);
    Serial.printf("积分(Integral): %.2f\n", integral);
    Serial.printf("微分(Derivative): %.2f\n", lastDerivative);
    Serial.printf("输出(Output): %.2f (限幅: %.2f - %.2f)\n", 
                  lastOutput, outputMin, outputMax);
    Serial.printf("P项: %.2f, I项: %.2f, D项: %.2f\n",
                  kp * lastError, ki * integral, kd * lastDerivative);
    Serial.println("==============================");
}
