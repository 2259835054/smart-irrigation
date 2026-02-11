/**
 * @file pid_controller.h
 * @brief PID 控制器头文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 实现标准PID控制算法，用于精确的土壤湿度控制
 * 包含抗积分饱和、微分滤波等优化措施
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>

/**
 * @brief PID 控制器类
 * 
 * 实现离散PID控制算法:
 * output = Kp*e(t) + Ki*∫e(t)dt + Kd*de(t)/dt
 * 
 * 其中：
 * - e(t) = setpoint - input (误差)
 * - ∫e(t)dt 为误差的积分
 * - de(t)/dt 为误差的微分
 */
class PIDController {
private:
    float kp, ki, kd;           // PID 三个参数
    float setpoint;             // 目标值（目标湿度）
    float integral;             // 积分累积
    float prevError;            // 上次误差
    float prevInput;            // 上次输入（用于微分计算）
    float outputMin, outputMax; // 输出限幅
    unsigned long lastTime;     // 上次计算时间
    bool  firstRun;             // 首次运行标志
    
    // 抗积分饱和
    float integralMax;          // 积分项最大值
    
    // 内部状态（用于调试）
    float lastError;            // 最后一次误差
    float lastDerivative;       // 最后一次微分项
    float lastOutput;           // 最后一次输出
    
public:
    /**
     * @brief 构造函数
     * @param kp 比例系数
     * @param ki 积分系数
     * @param kd 微分系数
     * @param outMin 输出最小值
     * @param outMax 输出最大值
     */
    PIDController(float kp, float ki, float kd, float outMin, float outMax);
    
    /**
     * @brief 设置目标值
     * @param sp 目标设定点（目标湿度）
     */
    void setSetpoint(float sp);
    
    /**
     * @brief 获取当前目标值
     * @return 目标设定点
     */
    float getSetpoint();
    
    /**
     * @brief 设置PID参数
     * @param kp 比例系数
     * @param ki 积分系数
     * @param kd 微分系数
     */
    void setTunings(float kp, float ki, float kd);
    
    /**
     * @brief 获取PID参数
     * @param kp 输出参数：比例系数
     * @param ki 输出参数：积分系数
     * @param kd 输出参数：微分系数
     */
    void getTunings(float &kp, float &ki, float &kd);
    
    /**
     * @brief PID核心计算函数
     * @param input 当前输入值（当前湿度）
     * @return PID输出值（PWM值 0-255）
     * 
     * 计算步骤：
     * 1. 计算误差 error = setpoint - input
     * 2. 计算比例项 P = Kp * error
     * 3. 更新积分项 I += Ki * error * dt （带抗饱和限幅）
     * 4. 计算微分项 D = Kd * (error - prevError) / dt
     * 5. 输出 = P + I + D
     * 6. 输出限幅到 [outputMin, outputMax]
     */
    float compute(float input);
    
    /**
     * @brief 重置PID状态
     * 
     * 清零积分项、误差项等，用于模式切换或重新启动
     */
    void reset();
    
    /**
     * @brief 设置输出限幅
     * @param min 最小输出
     * @param max 最大输出
     */
    void setOutputLimits(float min, float max);
    
    /**
     * @brief 设置积分限幅（抗积分饱和）
     * @param max 积分项最大值
     */
    void setIntegralLimit(float max);
    
    // ==================== 调试接口 ====================
    
    /**
     * @brief 获取最后一次误差
     * @return 误差值
     */
    float getLastError() const;
    
    /**
     * @brief 获取积分项当前值
     * @return 积分值
     */
    float getIntegral() const;
    
    /**
     * @brief 获取最后一次微分项
     * @return 微分值
     */
    float getDerivative() const;
    
    /**
     * @brief 获取最后一次输出
     * @return 输出值
     */
    float getOutput() const;
    
    /**
     * @brief 打印PID状态到串口（调试用）
     */
    void printStatus() const;
};

#endif // PID_CONTROLLER_H
