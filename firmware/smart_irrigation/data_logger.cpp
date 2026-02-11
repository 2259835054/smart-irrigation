/**
 * @file data_logger.cpp
 * @brief 数据记录模块实现文件
 * @author Smart Irrigation Project
 * @date 2026-02-11
 * 
 * 基于LittleFS的数据持久化实现，包括：
 * - 传感器数据历史记录（JSON格式）
 * - 系统配置保存/加载
 * - 文件系统管理功能
 * - 环形缓冲区日志存储
 */

#include "data_logger.h"

// ==================== 常量定义 ====================

// 文件路径定义
#define LOG_FILE_PATH       "/datalog.json"    // 数据日志文件
#define CONFIG_FILE_PATH    "/config.json"     // 配置文件
#define MAX_LOG_ENTRIES     1440               // 最大日志条目数（24小时，每分钟一条）

// JSON缓冲区大小
#define JSON_BUFFER_SIZE    8192               // JSON序列化缓冲区大小
#define CONFIG_JSON_SIZE    2048               // 配置文件JSON大小

// ==================== 内部变量 ====================

static bool fs_mounted = false;                // 文件系统挂载状态

// ==================== 内部辅助函数 ====================

/**
 * @brief 检查文件系统是否已挂载
 * @return true=已挂载, false=未挂载
 */
static bool check_fs_mounted() {
    if (!fs_mounted) {
        Serial.println("[DataLog] 错误：文件系统未挂载！");
        return false;
    }
    return true;
}

/**
 * @brief 读取JSON数组从文件
 * @param filename 文件路径
 * @param doc JSON文档引用
 * @return true=读取成功, false=读取失败
 */
static bool read_json_array(const char *filename, DynamicJsonDocument &doc) {
    if (!check_fs_mounted()) {
        return false;
    }

    // 打开文件读取
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.print("[DataLog] 警告：无法打开文件 ");
        Serial.println(filename);
        return false;
    }

    // 反序列化JSON
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("[DataLog] JSON反序列化失败: ");
        Serial.println(error.c_str());
        return false;
    }

    return true;
}

/**
 * @brief 写入JSON数组到文件
 * @param filename 文件路径
 * @param doc JSON文档引用
 * @return true=写入成功, false=写入失败
 */
static bool write_json_array(const char *filename, DynamicJsonDocument &doc) {
    if (!check_fs_mounted()) {
        return false;
    }

    // 打开文件写入
    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.print("[DataLog] 错误：无法创建文件 ");
        Serial.println(filename);
        return false;
    }

    // 序列化JSON到文件
    size_t bytesWritten = serializeJson(doc, file);
    file.close();

    if (bytesWritten == 0) {
        Serial.println("[DataLog] 错误：写入文件失败！");
        return false;
    }

    Serial.print("[DataLog] 写入文件成功: ");
    Serial.print(bytesWritten);
    Serial.println(" 字节");
    return true;
}

/**
 * @brief 创建空的日志文件
 * @return true=创建成功, false=创建失败
 */
static bool create_empty_log() {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    JsonArray array = doc.to<JsonArray>();  // 创建空数组
    
    if (!write_json_array(LOG_FILE_PATH, doc)) {
        Serial.println("[DataLog] 错误：无法创建空日志文件！");
        return false;
    }
    
    Serial.println("[DataLog] 创建空日志文件成功");
    return true;
}

/**
 * @brief 创建默认配置文件
 * @return true=创建成功, false=创建失败
 */
static bool create_default_config() {
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    
    // 创建默认配置
    doc["mode"] = "manual";
    doc["pid_enabled"] = false;
    
    // 各通道配置
    JsonArray thresholds = doc.createNestedArray("thresholds");
    JsonArray enabled = doc.createNestedArray("enabled");
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        thresholds.add(50.0);  // 默认阈值50%
        enabled.add(true);     // 默认启用
    }
    
    // PID参数
    JsonObject pid = doc.createNestedObject("pid");
    pid["kp"] = 2.0;
    pid["ki"] = 0.5;
    pid["kd"] = 1.0;
    
    if (!write_json_array(CONFIG_FILE_PATH, doc)) {
        Serial.println("[DataLog] 错误：无法创建默认配置文件！");
        return false;
    }
    
    Serial.println("[DataLog] 创建默认配置文件成功");
    return true;
}

// ==================== 公共函数实现 ====================

/**
 * @brief 初始化数据记录模块
 */
void datalog_init() {
    Serial.println("[DataLog] 初始化数据记录模块...");
    
    // 挂载LittleFS文件系统
    if (!LittleFS.begin()) {
        Serial.println("[DataLog] 错误：LittleFS挂载失败！");
        Serial.println("[DataLog] 尝试格式化文件系统...");
        
        // 尝试格式化后重新挂载
        if (LittleFS.format()) {
            Serial.println("[DataLog] 格式化成功");
            if (LittleFS.begin()) {
                Serial.println("[DataLog] 重新挂载成功");
                fs_mounted = true;
            } else {
                Serial.println("[DataLog] 致命错误：重新挂载失败！");
                return;
            }
        } else {
            Serial.println("[DataLog] 致命错误：格式化失败！");
            return;
        }
    } else {
        Serial.println("[DataLog] LittleFS挂载成功");
        fs_mounted = true;
    }
    
    // 检查日志文件是否存在
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        Serial.println("[DataLog] 日志文件不存在，创建新文件...");
        create_empty_log();
    } else {
        Serial.println("[DataLog] 日志文件已存在");
        // 验证文件格式
        DynamicJsonDocument doc(JSON_BUFFER_SIZE);
        if (!read_json_array(LOG_FILE_PATH, doc)) {
            Serial.println("[DataLog] 警告：日志文件格式错误，重新创建...");
            LittleFS.remove(LOG_FILE_PATH);
            create_empty_log();
        } else {
            Serial.print("[DataLog] 日志条目数: ");
            Serial.println(doc.as<JsonArray>().size());
        }
    }
    
    // 检查配置文件是否存在
    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
        Serial.println("[DataLog] 配置文件不存在，创建默认配置...");
        create_default_config();
    } else {
        Serial.println("[DataLog] 配置文件已存在");
    }
    
    // 打印文件系统信息
    datalog_print_fs_info();
    
    Serial.println("[DataLog] 初始化完成");
}

/**
 * @brief 添加一条记录
 */
void datalog_add_entry(SensorData &sensor, IrrigationState &irrigation) {
    if (!check_fs_mounted()) {
        return;
    }
    
    // 读取现有日志
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    if (!read_json_array(LOG_FILE_PATH, doc)) {
        // 如果读取失败，创建新的空数组
        doc.to<JsonArray>();
        Serial.println("[DataLog] 创建新的日志数组");
    }
    
    JsonArray array = doc.as<JsonArray>();
    
    // 检查是否超过最大条目数，删除最旧的记录
    if (array.size() >= MAX_LOG_ENTRIES) {
        // 删除第一个元素（最旧的）
        array.remove(0);
        Serial.println("[DataLog] 日志已满，删除最旧记录");
    }
    
    // 创建新的日志条目
    JsonObject entry = array.createNestedObject();
    
    // 添加时间戳
    entry["timestamp"] = millis();
    
    // 添加土壤湿度数据
    JsonArray moisture = entry.createNestedArray("moisture");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        moisture.add(sensor.soilMoisture[i]);
    }
    
    // 添加环境数据
    entry["temperature"] = sensor.temperature;
    entry["humidity"] = sensor.humidity;
    entry["light"] = sensor.lightLevel;
    
    // 添加水泵状态
    JsonArray pumps = entry.createNestedArray("pumps");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        pumps.add(irrigation.pumpStatus[i] ? 1 : 0);
    }
    
    // 添加灌溉模式
    entry["mode"] = irrigation.mode == IrrigationMode::MANUAL ? "manual" : 
                    irrigation.mode == IrrigationMode::AUTO ? "auto" : "pid";
    
    // 写入文件
    if (write_json_array(LOG_FILE_PATH, doc)) {
        Serial.print("[DataLog] 添加日志成功，当前条目数: ");
        Serial.println(array.size());
    } else {
        Serial.println("[DataLog] 添加日志失败！");
    }
}

/**
 * @brief 获取历史数据JSON
 */
String datalog_get_history_json(int count) {
    if (!check_fs_mounted()) {
        return "[]";
    }
    
    // 读取日志文件
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    if (!read_json_array(LOG_FILE_PATH, doc)) {
        Serial.println("[DataLog] 读取日志失败，返回空数组");
        return "[]";
    }
    
    JsonArray array = doc.as<JsonArray>();
    int totalEntries = array.size();
    
    // 如果count为0或大于总数，返回全部
    if (count <= 0 || count >= totalEntries) {
        String output;
        serializeJson(doc, output);
        return output;
    }
    
    // 创建新的文档包含最后N条记录
    DynamicJsonDocument resultDoc(JSON_BUFFER_SIZE);
    JsonArray resultArray = resultDoc.to<JsonArray>();
    
    // 计算起始索引
    int startIndex = totalEntries - count;
    
    // 复制最后N条记录
    for (int i = startIndex; i < totalEntries; i++) {
        resultArray.add(array[i]);
    }
    
    // 序列化为字符串
    String output;
    serializeJson(resultDoc, output);
    
    Serial.print("[DataLog] 返回 ");
    Serial.print(count);
    Serial.print(" 条记录（共 ");
    Serial.print(totalEntries);
    Serial.println(" 条）");
    
    return output;
}

/**
 * @brief 清空日志
 */
void datalog_clear() {
    if (!check_fs_mounted()) {
        return;
    }
    
    Serial.println("[DataLog] 清空日志文件...");
    
    // 删除现有文件
    if (LittleFS.exists(LOG_FILE_PATH)) {
        if (LittleFS.remove(LOG_FILE_PATH)) {
            Serial.println("[DataLog] 删除旧日志文件成功");
        } else {
            Serial.println("[DataLog] 错误：删除旧日志文件失败！");
            return;
        }
    }
    
    // 创建新的空日志文件
    if (create_empty_log()) {
        Serial.println("[DataLog] 日志已清空");
    } else {
        Serial.println("[DataLog] 错误：清空日志失败！");
    }
}

/**
 * @brief 获取日志条目数量
 */
int datalog_get_entry_count() {
    if (!check_fs_mounted()) {
        return 0;
    }
    
    // 读取日志文件
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    if (!read_json_array(LOG_FILE_PATH, doc)) {
        return 0;
    }
    
    JsonArray array = doc.as<JsonArray>();
    return array.size();
}

/**
 * @brief 保存系统配置
 */
void datalog_save_config(IrrigationState &state) {
    if (!check_fs_mounted()) {
        return;
    }
    
    Serial.println("[DataLog] 保存系统配置...");
    
    // 创建JSON文档
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    
    // 保存灌溉模式
    if (state.mode == IrrigationMode::MANUAL) {
        doc["mode"] = "manual";
    } else if (state.mode == IrrigationMode::AUTO) {
        doc["mode"] = "auto";
    } else {
        doc["mode"] = "pid";
    }
    
    // 保存PID启用状态
    doc["pid_enabled"] = state.pidEnabled;
    
    // 保存各通道阈值
    JsonArray thresholds = doc.createNestedArray("thresholds");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        thresholds.add(state.moistureThresholds[i]);
    }
    
    // 保存各通道启用状态
    JsonArray enabled = doc.createNestedArray("enabled");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        enabled.add(state.channelEnabled[i] ? 1 : 0);
    }
    
    // 保存PID参数
    JsonObject pid = doc.createNestedObject("pid");
    pid["kp"] = state.pidKp;
    pid["ki"] = state.pidKi;
    pid["kd"] = state.pidKd;
    
    // 写入文件
    if (write_json_array(CONFIG_FILE_PATH, doc)) {
        Serial.println("[DataLog] 配置保存成功");
    } else {
        Serial.println("[DataLog] 配置保存失败！");
    }
}

/**
 * @brief 加载系统配置
 */
bool datalog_load_config(IrrigationState &state) {
    if (!check_fs_mounted()) {
        return false;
    }
    
    Serial.println("[DataLog] 加载系统配置...");
    
    // 检查配置文件是否存在
    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
        Serial.println("[DataLog] 配置文件不存在");
        return false;
    }
    
    // 读取JSON文档
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    if (!read_json_array(CONFIG_FILE_PATH, doc)) {
        Serial.println("[DataLog] 读取配置文件失败");
        return false;
    }
    
    // 加载灌溉模式
    const char* mode = doc["mode"] | "manual";
    if (strcmp(mode, "manual") == 0) {
        state.mode = IrrigationMode::MANUAL;
    } else if (strcmp(mode, "auto") == 0) {
        state.mode = IrrigationMode::AUTO;
    } else {
        state.mode = IrrigationMode::PID_CONTROL;
    }
    
    // 加载PID启用状态
    state.pidEnabled = doc["pid_enabled"] | false;
    
    // 加载各通道阈值
    JsonArray thresholds = doc["thresholds"];
    if (thresholds) {
        for (int i = 0; i < NUM_CHANNELS && i < thresholds.size(); i++) {
            state.moistureThresholds[i] = thresholds[i] | 50.0;
        }
    }
    
    // 加载各通道启用状态
    JsonArray enabled = doc["enabled"];
    if (enabled) {
        for (int i = 0; i < NUM_CHANNELS && i < enabled.size(); i++) {
            state.channelEnabled[i] = enabled[i] | true;
        }
    }
    
    // 加载PID参数
    JsonObject pid = doc["pid"];
    if (pid) {
        state.pidKp = pid["kp"] | 2.0;
        state.pidKi = pid["ki"] | 0.5;
        state.pidKd = pid["kd"] | 1.0;
    }
    
    Serial.println("[DataLog] 配置加载成功");
    Serial.print("[DataLog] 模式: ");
    Serial.println(mode);
    Serial.print("[DataLog] PID启用: ");
    Serial.println(state.pidEnabled ? "是" : "否");
    
    return true;
}

/**
 * @brief 获取文件系统信息
 */
bool datalog_get_fs_info(size_t &total, size_t &used) {
    if (!check_fs_mounted()) {
        total = 0;
        used = 0;
        return false;
    }
    
    // 获取文件系统信息
    FSInfo fs_info;
    if (!LittleFS.info(fs_info)) {
        Serial.println("[DataLog] 错误：无法获取文件系统信息！");
        total = 0;
        used = 0;
        return false;
    }
    
    total = fs_info.totalBytes;
    used = fs_info.usedBytes;
    
    return true;
}

/**
 * @brief 读取文件内容
 */
String datalog_read_file(const char *filename) {
    if (!check_fs_mounted()) {
        return "";
    }
    
    // 打开文件
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.print("[DataLog] 错误：无法打开文件 ");
        Serial.println(filename);
        return "";
    }
    
    // 读取内容
    String content = file.readString();
    file.close();
    
    Serial.print("[DataLog] 读取文件成功: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(content.length());
    Serial.println(" 字节)");
    
    return content;
}

/**
 * @brief 写入文件内容
 */
bool datalog_write_file(const char *filename, const String &content) {
    if (!check_fs_mounted()) {
        return false;
    }
    
    // 打开文件写入
    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.print("[DataLog] 错误：无法创建文件 ");
        Serial.println(filename);
        return false;
    }
    
    // 写入内容
    size_t bytesWritten = file.print(content);
    file.close();
    
    if (bytesWritten == content.length()) {
        Serial.print("[DataLog] 写入文件成功: ");
        Serial.print(filename);
        Serial.print(" (");
        Serial.print(bytesWritten);
        Serial.println(" 字节)");
        return true;
    } else {
        Serial.println("[DataLog] 错误：写入文件失败！");
        return false;
    }
}

/**
 * @brief 删除文件
 */
bool datalog_delete_file(const char *filename) {
    if (!check_fs_mounted()) {
        return false;
    }
    
    // 检查文件是否存在
    if (!LittleFS.exists(filename)) {
        Serial.print("[DataLog] 警告：文件不存在 ");
        Serial.println(filename);
        return false;
    }
    
    // 删除文件
    if (LittleFS.remove(filename)) {
        Serial.print("[DataLog] 删除文件成功: ");
        Serial.println(filename);
        return true;
    } else {
        Serial.print("[DataLog] 错误：删除文件失败 ");
        Serial.println(filename);
        return false;
    }
}

/**
 * @brief 列出目录内容
 */
String datalog_list_dir(const char *dirname) {
    if (!check_fs_mounted()) {
        return "";
    }
    
    String result = "";
    
    // 打开目录
    Dir dir = LittleFS.openDir(dirname);
    
    Serial.print("[DataLog] 列出目录: ");
    Serial.println(dirname);
    Serial.println("[DataLog] ========================================");
    
    int fileCount = 0;
    
    // 遍历目录
    while (dir.next()) {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        
        // 添加到结果字符串
        result += fileName;
        result += " (";
        result += String(fileSize);
        result += " 字节)\n";
        
        // 同时打印到串口
        Serial.print("[DataLog] ");
        Serial.print(fileName);
        Serial.print(" - ");
        Serial.print(fileSize);
        Serial.println(" 字节");
        
        fileCount++;
    }
    
    Serial.println("[DataLog] ========================================");
    Serial.print("[DataLog] 共 ");
    Serial.print(fileCount);
    Serial.println(" 个文件");
    
    return result;
}

/**
 * @brief 格式化文件系统
 */
bool datalog_format() {
    Serial.println("[DataLog] ========================================");
    Serial.println("[DataLog] 警告：即将格式化文件系统！");
    Serial.println("[DataLog] 警告：所有数据将被删除！");
    Serial.println("[DataLog] ========================================");
    
    if (!fs_mounted) {
        Serial.println("[DataLog] 错误：文件系统未挂载！");
        return false;
    }
    
    // 卸载文件系统
    LittleFS.end();
    fs_mounted = false;
    
    Serial.println("[DataLog] 开始格式化...");
    
    // 格式化
    if (!LittleFS.format()) {
        Serial.println("[DataLog] 错误：格式化失败！");
        return false;
    }
    
    Serial.println("[DataLog] 格式化成功");
    
    // 重新挂载
    if (LittleFS.begin()) {
        Serial.println("[DataLog] 重新挂载成功");
        fs_mounted = true;
        
        // 创建默认文件
        create_empty_log();
        create_default_config();
        
        return true;
    } else {
        Serial.println("[DataLog] 错误：重新挂载失败！");
        return false;
    }
}

/**
 * @brief 打印文件系统信息到串口
 */
void datalog_print_fs_info() {
    size_t total = 0;
    size_t used = 0;
    
    if (!datalog_get_fs_info(total, used)) {
        Serial.println("[DataLog] 无法获取文件系统信息");
        return;
    }
    
    size_t free = total - used;
    float usedPercent = (float)used / (float)total * 100.0;
    
    Serial.println("[DataLog] ========================================");
    Serial.println("[DataLog] 文件系统信息:");
    Serial.print("[DataLog] 总空间: ");
    Serial.print(total);
    Serial.println(" 字节");
    Serial.print("[DataLog] 已用: ");
    Serial.print(used);
    Serial.print(" 字节 (");
    Serial.print(usedPercent, 1);
    Serial.println("%)");
    Serial.print("[DataLog] 可用: ");
    Serial.print(free);
    Serial.println(" 字节");
    Serial.println("[DataLog] ========================================");
}
