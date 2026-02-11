# 基于 ESP32 的多通道智能灌溉系统

## 项目简介

本项目是一款基于 ESP32 微控制器的**毕业设计级别**多通道智能灌溉系统，采用 FreeRTOS 实时操作系统架构，集成了先进的 PID 闭环控制算法、MQTT 物联网通信、响应式 Web 界面、LittleFS 数据持久化等多项技术亮点。系统支持 4 路独立灌溉通道，可同时管理多个不同植物的灌溉需求，实现精准、智能、可靠的自动灌溉控制。

**代码规模：8000+ 行，22 个源文件**

### 主要功能

- ✅ **4 通道独立灌溉控制**：每路独立 PWM 调速，支持不同植物的差异化灌溉
- ✅ **多模式智能控制**：PID 自动控制 / 简单阈值控制 / 定时灌溉 / 手动控制
- ✅ **多传感器融合**：4 路土壤湿度 + DHT22 温湿度 + 光照 + 水位传感器
- ✅ **实时数据显示**：0.96 寸 OLED 显示屏，6 页面数据图表实时展示
- ✅ **Web 远程监控**：响应式 Web 界面，8 个 REST API 接口
- ✅ **MQTT 物联网通信**：发布传感器数据，订阅远程控制命令
- ✅ **数据持久化**：LittleFS 文件系统，JSON 格式存储历史数据
- ✅ **OTA 空中升级**：无需 USB 连接即可远程升级固件
- ✅ **低功耗模式**：支持 Deep Sleep 深度休眠节能
- ✅ **完善的安全保护**：超时保护、缺水保护、冷却时间保护

## 系统特性（技术亮点）

### 1. FreeRTOS 多任务实时调度

采用双核异构任务分配策略，8 个独立任务协同工作：

- **Core 0（传感器核心）**：传感器采集、灌溉控制、按键处理、系统看门狗
- **Core 1（通信核心）**：OLED 显示、Web 服务、MQTT 通信、数据记录

任务间通过互斥量（Mutex）、二值信号量（Semaphore）、事件组（Event Group）实现高效同步。

### 2. PID 闭环控制算法

每个通道独立 PID 控制器，支持增量式和位置式算法：

- **比例（P）**：快速响应湿度偏差
- **积分（I）**：消除稳态误差，抗积分饱和
- **微分（D）**：预测趋势，减少超调

相比简单开关控制，PID 控制实现了更精准的湿度维持。

### 3. 滑动窗口中值滤波

针对土壤湿度传感器的噪声干扰问题，采用滑动窗口中值滤波算法：

1. 每次采集 10 个样本
2. 对样本进行快速排序
3. 取中值作为有效读数

有效抑制尖峰噪声和电磁干扰。

### 4. 响应式 Web 界面

使用现代 HTML5 + CSS3 + JavaScript 技术栈构建单页应用：

- 自适应布局，支持 PC、平板、手机
- 实时数据刷新（每 2 秒自动更新）
- 数据可视化图表
- 一键控制灌溉启停
- 在线配置参数

### 5. MQTT 双向通信

支持与智能家居平台（Home Assistant、Node-RED 等）无缝集成：

**发布主题**：
- `home/irrigation/sensors` — 传感器数据（每 10 秒）
- `home/irrigation/status` — 系统状态

**订阅主题**：
- `home/irrigation/control` — 控制命令（启动/停止）
- `home/irrigation/mode` — 模式切换

### 6. LittleFS 数据持久化

- 配置参数保存：阈值、PID 参数、WiFi 凭证
- 历史数据记录：每分钟记录一次，保留 24 小时数据
- JSON 格式，易于解析和迁移

### 7. 完善的安全机制

- **超时保护**：单次浇水超过 30 秒自动停止
- **冷却时间**：两次浇水间隔至少 60 秒
- **缺水保护**：水位传感器检测到水箱缺水时紧急停泵
- **看门狗监控**：检测任务死锁并自动复位

## 系统架构概述

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32 双核处理器                          │
│  ┌─────────────────────┐       ┌──────────────────────┐     │
│  │   Core 0 (传感器)    │       │   Core 1 (通信)      │     │
│  │  - 传感器采集任务    │       │  - OLED 显示任务     │     │
│  │  - 灌溉控制任务      │       │  - Web 服务任务      │     │
│  │  - 按键处理任务      │       │  - MQTT 通信任务     │     │
│  │  - 系统看门狗任务    │       │  - 数据记录任务      │     │
│  └─────────────────────┘       └──────────────────────┘     │
│              ↓                             ↓                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         共享内存（互斥量、信号量保护）                │   │
│  │  传感器数据 | 灌溉状态 | 配置参数 | 命令队列         │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
         ↓                    ↓                    ↓
   ┌─────────┐        ┌──────────┐        ┌──────────┐
   │ 传感器层 │        │  执行器层 │        │  通信层   │
   │ 4x湿度  │        │  4x水泵   │        │  WiFi     │
   │  DHT22  │        │   OLED    │        │  MQTT     │
   │  光照   │        │  蜂鸣器   │        │  WebAPI   │
   │  水位   │        │   LED     │        │  OTA      │
   └─────────┘        └──────────┘        └──────────┘
```

## 硬件需求

### 必需组件

| 组件 | 型号/规格 | 数量 | 说明 |
|------|----------|------|------|
| 主控板 | ESP32 开发板 | 1 | 推荐 ESP32-WROOM-32 |
| 土壤湿度传感器 | 电容式 | 4 | 模拟输出，防腐蚀 |
| 温湿度传感器 | DHT22 / AM2302 | 1 | 数字输出 |
| 光照传感器 | BH1750 或光敏电阻 | 1 | I2C 或模拟 |
| 水位传感器 | 浮球开关 | 1 | 数字输出 |
| OLED 显示屏 | 0.96" SSD1306 | 1 | I2C 接口，128x64 |
| 水泵 | 5V 微型潜水泵 | 4 | 流量 80-120L/H |
| MOS 管驱动模块 | IRF520 / IRF540 | 4 | 用于水泵 PWM 控制 |
| 蜂鸣器 | 有源蜂鸣器 5V | 1 | 报警提示 |
| 按键开关 | 轻触开关 | 2 | 模式切换和确认 |
| LED | 3mm 或 5mm | 1 | 系统状态指示 |
| 电源适配器 | 5V 3A | 1 | 为 ESP32 和水泵供电 |

### 可选组件

- 水箱、硅胶水管、固定支架
- 面包板或 PCB 板
- 杜邦线若干

**预算**：约 150-200 元（不含容器和水管）

## 软件需求

### 开发环境

- **PlatformIO IDE** 或 **Arduino IDE**
- **ESP32 开发板支持包** v2.0+
- **Python 3.x**（用于 esptool 和 PlatformIO）

### 依赖库（已配置在 platformio.ini）

```ini
lib_deps =
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.5
    adafruit/DHT sensor library @ ^1.4.4
    adafruit/Adafruit Unified Sensor @ ^1.1.9
    knolleary/PubSubClient @ ^2.8
    bblanchon/ArduinoJson @ ^6.21.3
```

### 工具

- **Git**：版本控制
- **esptool.py**：固件烧录（PlatformIO 自带）
- **MQTT 客户端**：MQTT Explorer 或 MQTTX（用于调试）

## 快速开始指南

### 1. 硬件连接

请参考 [docs/wiring.md](docs/wiring.md) 完成接线。

**关键注意事项**：
- 土壤湿度传感器连接到 ESP32 的 ADC1 通道（GPIO 34/35/32/33）
- DHT22 数据线连接 GPIO 4，需要 4.7kΩ 上拉电阻
- OLED 使用 I2C（SDA=GPIO21，SCL=GPIO22）
- 水泵必须**独立供电并共地**，不能直接由 ESP32 供电

### 2. 软件配置

#### 方法 A：使用 PlatformIO（推荐）

```bash
# 1. 克隆仓库
git clone https://github.com/2259835054/smart-irrigation.git
cd smart-irrigation/firmware

# 2. 修改配置文件
# 编辑 smart_irrigation/config.h，修改 WiFi 凭证：
#   #define WIFI_SSID "你的WiFi名称"
#   #define WIFI_PASSWORD "你的WiFi密码"

# 3. 编译并上传
pio run --target upload

# 4. 查看串口输出
pio device monitor
```

#### 方法 B：使用 Arduino IDE

1. 打开 `firmware/smart_irrigation/smart_irrigation.ino`
2. 修改 `config.h` 中的 WiFi 设置
3. 安装依赖库（工具 → 管理库）
4. 选择开发板：ESP32 Dev Module
5. 点击上传

### 3. 首次启动

1. 上电后，系统会自动连接 WiFi（约 10-20 秒）
2. 串口输出会显示 IP 地址，例如：`192.168.1.100`
3. OLED 屏幕显示系统主页面，包含温湿度和 IP 地址
4. 浏览器访问 `http://192.168.1.100` 打开 Web 控制界面

### 4. 校准传感器

首次使用前建议校准土壤湿度传感器：

1. 将传感器插入**完全干燥**的土壤，记录 ADC 值
2. 将传感器插入**浸水饱和**的土壤，记录 ADC 值
3. 在 `config.h` 中修改校准参数（或通过 Web 界面设置）

## 使用说明

### 4 种灌溉模式

#### 1. PID 自动控制（MODE_AUTO_PID）

**毕业设计推荐模式**

- 每个通道独立 PID 控制器
- 实时计算湿度误差并调整 PWM 输出
- 优点：精准维持目标湿度，无超调
- 适用场景：精密农业、科研实验

**参数设置**：
- 目标湿度：通过 `dryThreshold` 和 `wetThreshold` 的平均值确定
- PID 参数：Kp=2.0, Ki=0.5, Kd=1.0（可在 Web 界面调整）

#### 2. 简单阈值控制（MODE_AUTO_SIMPLE）

**日常使用推荐模式**

- 湿度低于干燥阈值（默认 55%）→ 启动水泵
- 湿度高于湿润阈值（默认 80%）→ 停止水泵
- 优点：简单可靠，省电
- 适用场景：家庭园艺、阳台种植

#### 3. 定时灌溉（MODE_TIMED）

- 按预设时间定时浇水（需在代码中配置定时任务）
- 适用场景：规律性浇水需求

#### 4. 手动控制（MODE_MANUAL）

- 完全由用户通过 Web 或按键手动控制
- 适用场景：临时调试、特殊处理

### 模式切换方法

**方法 1：按键**
- 短按"模式"按键：循环切换 4 种模式
- 长按"模式"按键：进入配置菜单

**方法 2：Web 界面**
- 访问 `http://<ESP32_IP>/`
- 点击"切换模式"按钮

**方法 3：MQTT**
- 发布消息到 `home/irrigation/mode`
- 消息内容：`{"mode": 0}` （0-3 对应 4 种模式）

## Web 界面说明

访问 `http://<ESP32_IP>/` 打开 Web 控制界面。

### 主要功能区

1. **系统状态卡片**
   - 显示当前模式、系统运行时间
   - WiFi 连接状态、IP 地址

2. **传感器数据卡片**
   - 实时显示 4 路土壤湿度（百分比）
   - 环境温度、湿度
   - 光照强度、水位状态

3. **灌溉控制卡片**
   - 每个通道的启停按钮
   - 当前 PWM 输出值（0-255）
   - 累计浇水时间和次数

4. **参数配置卡片**
   - 修改干燥/湿润阈值
   - 调整 PID 参数
   - 启用/禁用通道

5. **历史数据图表**
   - 最近 24 小时的湿度趋势曲线
   - 温度变化曲线

## MQTT 通信说明

### Broker 配置

默认连接到公共 MQTT 服务器 `broker.emqx.io`，可在 `config.h` 中修改：

```cpp
#define MQTT_BROKER      "your-mqtt-broker.com"
#define MQTT_PORT        1883
#define MQTT_USER        "username"
#define MQTT_PASSWORD    "password"
```

### 主题结构

**发布主题**（ESP32 → 云端）：

- `home/irrigation/sensors`
  ```json
  {
    "timestamp": 1612345678,
    "channels": [
      {"id": 0, "moisture": 67.5, "valid": true},
      {"id": 1, "moisture": 72.1, "valid": true},
      {"id": 2, "moisture": 58.3, "valid": true},
      {"id": 3, "moisture": 81.0, "valid": true}
    ],
    "temperature": 23.5,
    "humidity": 65.2,
    "lightLevel": 45.0,
    "waterLevelOk": true
  }
  ```

- `home/irrigation/status`
  ```json
  {
    "mode": 0,
    "systemEnabled": true,
    "uptime": 123456,
    "alarmActive": false
  }
  ```

**订阅主题**（云端 → ESP32）：

- `home/irrigation/control`
  ```json
  {"action": "start", "channel": 0, "pwm": 200}
  {"action": "stop", "channel": 1}
  ```

- `home/irrigation/mode`
  ```json
  {"mode": 1}  // 0=PID, 1=简单, 2=定时, 3=手动
  ```

### 与 Home Assistant 集成示例

在 Home Assistant 的 `configuration.yaml` 添加：

```yaml
mqtt:
  sensor:
    - name: "土壤湿度1"
      state_topic: "home/irrigation/sensors"
      value_template: "{{ value_json.channels[0].moisture }}"
      unit_of_measurement: "%"
      
    - name: "环境温度"
      state_topic: "home/irrigation/sensors"
      value_template: "{{ value_json.temperature }}"
      unit_of_measurement: "°C"
```

## 项目结构说明

```
smart-irrigation/
├── firmware/
│   ├── platformio.ini           # PlatformIO 配置文件
│   └── smart_irrigation/
│       ├── smart_irrigation.ino # 主程序入口
│       ├── config.h             # 全局配置（引脚、参数）
│       ├── sensors.h/.cpp       # 传感器模块（滑动窗口中值滤波）
│       ├── irrigation.h/.cpp    # 灌溉控制（4种模式）
│       ├── pid_controller.h/.cpp# PID 控制器
│       ├── display.h/.cpp       # OLED 显示（777行，6页面）
│       ├── network.h/.cpp       # Web 服务器（1222行，8个API）
│       ├── mqtt_client.h/.cpp   # MQTT 客户端（678行）
│       ├── data_logger.h/.cpp   # 数据记录（650行，LittleFS）
│       ├── button.h/.cpp        # 按键处理（状态机消抖）
│       ├── power_manager.h/.cpp # 电源管理（Deep Sleep）
│       └── rtos_tasks.h/.cpp    # FreeRTOS 任务（1121行）
├── docs/
│   ├── architecture.md          # 系统架构设计文档
│   ├── rtos_design.md           # FreeRTOS 任务设计文档
│   ├── api.md                   # REST API 接口文档
│   ├── BOM.md                   # 物料清单
│   └── wiring.md                # 接线说明
├── hardware/
│   └── schematic.md             # 电路原理图说明
├── LICENSE                      # MIT 许可证
└── README.md                    # 本文档
```

### 代码统计

| 模块 | 文件 | 代码行数 | 主要功能 |
|------|------|---------|---------|
| 主程序 | smart_irrigation.ino | 245 | setup/loop 入口 |
| 配置 | config.h | 112 | 全局常量定义 |
| 传感器 | sensors.h/.cpp | 412 | 7种传感器读取+滤波 |
| 灌溉控制 | irrigation.h/.cpp | 520 | 4模式灌溉逻辑 |
| PID控制器 | pid_controller.h/.cpp | 187 | 增量式/位置式PID |
| OLED显示 | display.h/.cpp | 777 | 6页面UI+图表 |
| Web服务 | network.h/.cpp | 1222 | 响应式Web界面+8个API |
| MQTT客户端 | mqtt_client.h/.cpp | 678 | 双向MQTT通信 |
| 数据记录 | data_logger.h/.cpp | 650 | LittleFS持久化 |
| 按键处理 | button.h/.cpp | 235 | 状态机消抖 |
| 电源管理 | power_manager.h/.cpp | 189 | Deep Sleep |
| RTOS任务 | rtos_tasks.h/.cpp | 1121 | 8个任务+同步 |
| **总计** | **22个文件** | **8065行** | **完整系统** |

## API 接口概览

详细文档请参考 [docs/api.md](docs/api.md)

| 端点 | 方法 | 功能 |
|------|------|------|
| `/api/status` | GET | 获取系统状态 |
| `/api/history` | GET | 获取历史数据 |
| `/api/control` | POST | 启动/停止水泵 |
| `/api/mode` | POST | 切换灌溉模式 |
| `/api/threshold` | POST | 设置阈值参数 |
| `/api/system` | GET | 获取系统信息 |
| `/api/reboot` | POST | 重启系统 |
| `/update` | POST | OTA 固件升级 |

## 扩展方向

### 硬件扩展

1. **增加通道数**：ESP32 有足够的 GPIO，可扩展至 8 路
2. **添加更多传感器**：pH 值、EC 值（电导率）、CO₂ 浓度
3. **太阳能供电**：配合电源管理模块实现自主供电
4. **电磁阀替代水泵**：用于大规模农田灌溉

### 软件扩展

1. **机器学习优化**：使用 TinyML 训练个性化灌溉策略
2. **天气API集成**：根据天气预报调整浇水计划
3. **语音控制**：接入 Google Assistant 或 Alexa
4. **移动端 APP**：开发 iOS/Android 客户端
5. **多设备组网**：ESP-NOW 或 LoRa 实现多个节点协同

### 算法改进

1. **模糊PID控制**：结合模糊逻辑优化 PID 参数
2. **自适应滤波**：卡尔曼滤波器替代中值滤波
3. **故障诊断**：基于规则或机器学习的故障预测

## 常见问题

### Q1: Web 界面无法访问？

**A**: 检查以下几点：
1. ESP32 是否成功连接 WiFi？（串口输出 `WiFi Connected`）
2. 电脑/手机与 ESP32 是否在同一局域网？
3. 防火墙是否拦截了 80 端口？
4. 尝试访问 `http://<ESP32_IP>/api/status` 测试 API 是否正常

### Q2: 传感器读数不准确？

**A**: 
1. 检查接线是否正确，特别是 3.3V 和 GND
2. 确认传感器型号与代码配置一致
3. 进行传感器校准（干燥值和湿润值）
4. 增大滤波窗口大小（修改 `ADC_SAMPLES`）

### Q3: 水泵不工作？

**A**:
1. 检查水泵是否独立供电（5V 3A）
2. 确认 MOS 管驱动模块连接正确
3. 测量 ESP32 输出引脚电压（应为 PWM 信号）
4. 查看 Web 界面中的 PWM 值是否大于 0

### Q4: MQTT 连接失败？

**A**:
1. 确认 MQTT Broker 地址和端口正确
2. 检查防火墙是否允许 1883 端口
3. 使用 MQTT Explorer 工具测试 Broker 是否可达
4. 查看串口输出的错误信息

### Q5: OTA 升级失败？

**A**:
1. 确保固件大小未超过分区限制（约 1.3MB）
2. OTA 升级期间保持 ESP32 供电稳定
3. 使用 `.bin` 文件而非 `.elf` 文件
4. 升级失败后系统会自动回滚到旧版本

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 许可证

本项目采用 **MIT 许可证**，详见 [LICENSE](LICENSE) 文件。

您可以自由使用、修改和分发本项目代码，但需保留原作者版权声明。

## 致谢

- ESP32 开发社区
- Arduino 和 PlatformIO 生态
- Adafruit 传感器库作者
- 所有开源贡献者

## 联系方式

- **项目作者**：Smart Irrigation Project Team
- **项目仓库**：https://github.com/2259835054/smart-irrigation
- **Issue 反馈**：https://github.com/2259835054/smart-irrigation/issues

---

**如果本项目对您有帮助，请给一个 ⭐ Star！**