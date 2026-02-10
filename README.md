# 面向家庭盆栽的自动灌溉装置

**Smart Irrigation System for Home Plants**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B-orange.svg)

## 项目简介

本项目是一个基于 ESP32 平台开发的智能灌溉系统，专为家庭盆栽设计。系统能够自动检测土壤湿度、环境温湿度，并根据设定的策略自动灌溉植物，同时支持 OLED 实时显示和 WiFi 远程控制，让植物养护变得更加智能和便捷。

## 功能特性

✅ **土壤湿度实时检测** - 使用电容式传感器，精准监测土壤含水量

✅ **环境温湿度监测** - DHT22 传感器实时监测环境温湿度

✅ **三种灌溉模式**
   - 🤖 **自动模式**：根据土壤湿度自动灌溉
   - ⏰ **定时模式**：每 24 小时定时定量灌溉
   - 🎮 **手动模式**：通过按键或 Web 界面手动控制

✅ **OLED 实时状态显示** - 128x64 显示屏实时显示系统状态

✅ **水箱水位检测** - 低水位自动报警，防止空转损坏水泵

✅ **WiFi 远程控制** - 内置 Web 服务器，支持手机/电脑远程监控和控制

✅ **多重安全保护**
   - 浇水超时保护（单次最长 30 秒）
   - 缺水保护（检测到缺水立即停泵并报警）
   - 蜂鸣器声音提醒

## 硬件需求

### 核心组件
- ESP32 开发板 × 1
- 电容式土壤湿度传感器 × 1
- DHT22 温湿度传感器 × 1
- 0.96 寸 OLED 显示屏（SSD1306，I2C）× 1
- 微型潜水泵（5V）× 1
- MOS 管驱动模块或继电器模块 × 1
- 浮球水位传感器 × 1

### 辅助组件
- 蜂鸣器（有源 5V）× 1
- LED 指示灯 × 1
- 电阻（220Ω）× 1
- 按键开关 × 1
- 面包板 + 杜邦线若干
- USB 数据线 × 1
- 5V 电源适配器 × 1
- 水管（硅胶管）若干
- 水箱/容器 × 1

**参考预算**：约 80-120 元

详细物料清单请查看 [docs/BOM.md](docs/BOM.md)

## 软件需求

### 开发环境
- Arduino IDE 1.8.x 或更高版本
- 或 PlatformIO（推荐）

### 依赖库
```
- Adafruit SSD1306
- Adafruit GFX Library
- DHT sensor library
- Adafruit Unified Sensor
```

### ESP32 开发板支持
在 Arduino IDE 中添加 ESP32 开发板支持：
```
文件 -> 首选项 -> 附加开发板管理器网址
添加：https://dl.espressif.com/dl/package_esp32_index.json
```

## 接线说明

### 引脚连接表

| 传感器/模块 | ESP32 引脚 | 说明 |
|------------|-----------|------|
| 土壤湿度传感器 VCC | 3.3V | 供电 |
| 土壤湿度传感器 GND | GND | 接地 |
| 土壤湿度传感器 AOUT | GPIO 34 | 模拟输出 |
| DHT22 VCC | 3.3V | 供电 |
| DHT22 GND | GND | 接地 |
| DHT22 DATA | GPIO 4 | 数据线（需 4.7kΩ 上拉电阻）|
| 水位传感器 VCC | 5V | 供电 |
| 水位传感器 GND | GND | 接地 |
| 水位传感器 OUT | GPIO 5 | 数字输出 |
| 水泵驱动模块 VCC | 5V | 供电 |
| 水泵驱动模块 GND | GND | 接地 |
| 水泵驱动模块 IN | GPIO 16 | 控制信号 |
| OLED SDA | GPIO 21 | I2C 数据线 |
| OLED SCL | GPIO 22 | I2C 时钟线 |
| OLED VCC | 3.3V | 供电 |
| OLED GND | GND | 接地 |
| 按键 | GPIO 15 | 按键输入（内部上拉）|
| LED + | GPIO 2 | LED 正极（需串联电阻）|
| LED - | GND | LED 负极 |
| 蜂鸣器 + | GPIO 17 | 蜂鸣器正极 |
| 蜂鸣器 - | GND | 蜂鸣器负极 |

⚠️ **注意事项**：
- 水泵需要独立供电，不能直接连接 ESP32
- 所有模块必须共地（GND 连接在一起）
- DHT22 数据线需要 4.7kΩ 上拉电阻

详细接线说明请查看 [docs/wiring.md](docs/wiring.md)

## 快速开始

### 1. 硬件连接
按照接线说明连接所有硬件组件。

### 2. 配置 WiFi
编辑 `firmware/smart_irrigation/config.h` 文件，修改 WiFi 配置：
```cpp
#define WIFI_SSID           "你的WiFi名称"
#define WIFI_PASSWORD       "你的WiFi密码"
```

### 3. 安装依赖库
在 Arduino IDE 中：
```
工具 -> 管理库 -> 搜索并安装：
- Adafruit SSD1306
- Adafruit GFX Library
- DHT sensor library
- Adafruit Unified Sensor
```

或使用 PlatformIO：
```bash
cd firmware
pio lib install
```

### 4. 烧录程序
#### 使用 Arduino IDE：
1. 选择开发板：`工具 -> 开发板 -> ESP32 Arduino -> ESP32 Dev Module`
2. 选择端口：`工具 -> 端口 -> 选择对应的 COM 端口`
3. 上传程序：点击"上传"按钮

#### 使用 PlatformIO：
```bash
cd firmware
pio run --target upload
```

### 5. 查看串口输出
打开串口监视器（波特率 115200）查看系统运行状态。

## 使用说明

### 三种灌溉模式

#### 🤖 自动模式（默认）
- 系统自动监测土壤湿度
- 当湿度低于 60% 时自动启动灌溉
- 当湿度达到 80% 时自动停止灌溉
- 适合大多数家庭盆栽

#### ⏰ 定时模式
- 每 24 小时自动灌溉一次
- 每次灌溉持续 10 秒
- 适合需要定期浇水的植物

#### 🎮 手动模式
- 完全手动控制水泵开关
- 可通过按键或 Web 界面控制
- 适合特殊护理需求

### 按键操作

- **短按**（< 2 秒）：切换灌溉模式（自动→定时→手动）
- **长按**（≥ 2 秒）：在手动模式下启动/停止水泵

### Web 控制界面

1. 确保 ESP32 已连接到 WiFi
2. 在串口监视器中查看设备 IP 地址
3. 在浏览器中访问该 IP 地址（例如：http://192.168.1.100）
4. 在 Web 界面中可以：
   - 查看实时传感器数据
   - 查看系统运行状态
   - 手动控制浇水
   - 切换灌溉模式

### OLED 显示界面

显示内容包括：
- 土壤湿度百分比
- 环境温度（℃）
- 环境湿度（%）
- 水箱状态（OK / LOW）
- 水泵状态（ON / OFF）
- 当前灌溉模式

## 项目结构

```
smart-irrigation/
├── README.md                      # 项目说明文档
├── LICENSE                        # MIT 开源许可证
├── docs/                          # 文档目录
│   ├── BOM.md                     # 物料清单
│   ├── wiring.md                  # 接线说明
│   └── images/                    # 文档图片目录
├── hardware/                      # 硬件设计目录
│   └── schematic.md               # 电路原理图说明
├── firmware/                      # 固件源代码
│   ├── platformio.ini             # PlatformIO 项目配置
│   └── smart_irrigation/          # Arduino 项目目录
│       ├── smart_irrigation.ino   # 主程序
│       ├── config.h               # 配置文件
│       ├── sensors.h/.cpp         # 传感器模块
│       ├── irrigation.h/.cpp      # 灌溉控制模块
│       ├── display.h/.cpp         # OLED 显示模块
│       ├── network.h/.cpp         # WiFi 网络模块
│       └── button.h/.cpp          # 按键处理模块
└── .gitignore                     # Git 忽略文件
```

## 扩展方向

本项目可以进一步扩展和改进：

🔧 **硬件扩展**
- 添加光照传感器，实现光照强度监测
- 添加多路灌溉，支持多个盆栽同时管理
- 集成太阳能供电系统
- 添加摄像头模块，实现植物生长监控

📱 **软件扩展**
- 开发专用手机 APP（Android/iOS）
- 接入智能家居平台（Home Assistant、米家等）
- 添加数据记录和图表展示功能
- 接入天气 API，根据天气调整灌溉策略
- 机器学习优化灌溉算法

🌐 **云平台集成**
- 接入 MQTT 服务器实现远程监控
- 上传数据到云端进行分析
- 微信/邮件通知功能

## 许可证

本项目采用 MIT 许可证开源，详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 作者

Smart Irrigation Project Team

## 致谢

感谢所有为本项目提供帮助和支持的朋友们！

---

**如果这个项目对你有帮助，请给个 ⭐ Star 支持一下！**