# REST API 接口文档

## 文档概述

本文档详细说明智能灌溉系统提供的所有 REST API 接口，包括 HTTP 端点、请求参数、响应格式、状态码以及 MQTT 主题说明。

## 1. API 概览

系统提供 **8 个 REST API 端点**，基于 HTTP 协议，数据格式为 JSON。

| 序号 | 端点 | 方法 | 功能 | 认证 |
|------|------|------|------|------|
| 1 | `/api/status` | GET | 获取系统状态和传感器数据 | 否 |
| 2 | `/api/history` | GET | 获取历史数据记录 | 否 |
| 3 | `/api/control` | POST | 启动或停止水泵 | 否 |
| 4 | `/api/mode` | POST | 切换灌溉模式 | 否 |
| 5 | `/api/threshold` | POST | 设置阈值参数 | 否 |
| 6 | `/api/system` | GET | 获取系统信息 | 否 |
| 7 | `/api/reboot` | POST | 重启系统 | 否 |
| 8 | `/update` | POST | OTA 固件升级 | 否 |

**基础 URL**：`http://<ESP32_IP>`

**公共响应头**：
```
Content-Type: application/json
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
```

## 2. API 详细说明

### 2.1 GET /api/status — 获取系统状态

**功能描述**：

获取系统当前状态，包括传感器数据、灌溉状态、系统配置等。这是**最常用**的 API，Web 界面每 2 秒调用一次。

**请求示例**：

```http
GET /api/status HTTP/1.1
Host: 192.168.1.100
```

**响应示例**：

```json
{
  "timestamp": 1612345678,
  "mode": "auto_pid",
  "systemEnabled": true,
  "uptime": 123456,
  "alarmActive": false,
  "alarmMessage": "",
  "channels": [
    {
      "moisture": 67.5,
      "pumpRunning": true,
      "pumpPWM": 180,
      "dryThreshold": 55.0,
      "wetThreshold": 80.0,
      "enabled": true,
      "waterCount": 15
    },
    {
      "moisture": 72.1,
      "pumpRunning": false,
      "pumpPWM": 0,
      "dryThreshold": 55.0,
      "wetThreshold": 80.0,
      "enabled": true,
      "waterCount": 12
    },
    {
      "moisture": 58.3,
      "pumpRunning": true,
      "pumpPWM": 120,
      "dryThreshold": 50.0,
      "wetThreshold": 75.0,
      "enabled": true,
      "waterCount": 18
    },
    {
      "moisture": 81.0,
      "pumpRunning": false,
      "pumpPWM": 0,
      "dryThreshold": 60.0,
      "wetThreshold": 85.0,
      "enabled": false,
      "waterCount": 0
    }
  ],
  "temperature": 23.5,
  "humidity": 65.2,
  "lightLevel": 45.0,
  "waterLevelOk": true
}
```

**响应字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp` | number | UNIX 时间戳（秒） |
| `mode` | string | 当前模式：`auto_pid`/`auto_simple`/`timed`/`manual` |
| `systemEnabled` | boolean | 系统总开关 |
| `uptime` | number | 系统运行时间（毫秒） |
| `alarmActive` | boolean | 是否有报警 |
| `alarmMessage` | string | 报警信息 |
| `channels[]` | array | 4 路通道数据数组 |
| `channels[].moisture` | number | 土壤湿度 (0-100%) |
| `channels[].pumpRunning` | boolean | 水泵是否运行 |
| `channels[].pumpPWM` | number | PWM 输出值 (0-255) |
| `channels[].dryThreshold` | number | 干燥阈值 (%) |
| `channels[].wetThreshold` | number | 湿润阈值 (%) |
| `channels[].enabled` | boolean | 通道是否启用 |
| `channels[].waterCount` | number | 浇水次数 |
| `temperature` | number | 环境温度 (°C) |
| `humidity` | number | 环境湿度 (%) |
| `lightLevel` | number | 光照强度 (0-100%) |
| `waterLevelOk` | boolean | 水箱水位是否正常 |

**状态码**：

- `200 OK` — 成功获取状态

### 2.2 GET /api/history — 获取历史数据

**功能描述**：

获取最近记录的历史数据（最多 1440 条，24 小时）。

**请求示例**：

```http
GET /api/history HTTP/1.1
Host: 192.168.1.100
```

**URL 参数**（可选）：

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `limit` | number | 100 | 返回条数 (1-1440) |
| `offset` | number | 0 | 偏移量 |

**示例**：`/api/history?limit=50&offset=0`

**响应示例**：

```json
{
  "total": 1440,
  "count": 100,
  "data": [
    {
      "timestamp": 1612345678,
      "channels": [
        {"id": 0, "moisture": 67.5, "pumpOn": true, "pwm": 180},
        {"id": 1, "moisture": 72.1, "pumpOn": false, "pwm": 0},
        {"id": 2, "moisture": 58.3, "pumpOn": true, "pwm": 120},
        {"id": 3, "moisture": 81.0, "pumpOn": false, "pwm": 0}
      ],
      "temperature": 23.5,
      "humidity": 65.2,
      "lightLevel": 45.0
    },
    {
      "timestamp": 1612345618,
      "channels": [
        {"id": 0, "moisture": 66.8, "pumpOn": true, "pwm": 190},
        {"id": 1, "moisture": 71.5, "pumpOn": false, "pwm": 0},
        {"id": 2, "moisture": 57.9, "pumpOn": true, "pwm": 130},
        {"id": 3, "moisture": 80.2, "pumpOn": false, "pwm": 0}
      ],
      "temperature": 23.3,
      "humidity": 64.8,
      "lightLevel": 43.0
    }
  ]
}
```

**响应字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `total` | number | 总记录数 |
| `count` | number | 本次返回条数 |
| `data[]` | array | 历史数据数组 |
| `data[].timestamp` | number | 记录时间戳 |
| `data[].channels[]` | array | 通道数据 |
| `data[].temperature` | number | 温度 |
| `data[].humidity` | number | 湿度 |
| `data[].lightLevel` | number | 光照 |

**状态码**：

- `200 OK` — 成功获取历史数据
- `404 Not Found` — 历史数据文件不存在

### 2.3 POST /api/control — 启动或停止水泵

**功能描述**：

手动控制指定通道水泵的启动或停止。

**请求头**：

```
Content-Type: application/json
```

**请求体示例**：

**启动水泵**：
```json
{
  "channel": 0,
  "action": "start",
  "pwm": 200
}
```

**停止水泵**：
```json
{
  "channel": 1,
  "action": "stop"
}
```

**请求参数说明**：

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `channel` | number | 是 | 通道号 (0-3) |
| `action` | string | 是 | 动作：`start` 或 `stop` |
| `pwm` | number | 否 | PWM 值 (0-255)，仅 `start` 时有效，默认 255 |

**响应示例**：

**成功**：
```json
{
  "success": true,
  "message": "通道0水泵已启动，PWM=200"
}
```

**失败**：
```json
{
  "success": false,
  "message": "无效的通道号"
}
```

**状态码**：

- `200 OK` — 操作成功
- `400 Bad Request` — 参数错误或 JSON 解析失败
- `403 Forbidden` — 系统处于自动模式，拒绝手动控制

**注意事项**：

- 仅在 `manual` 模式下可完全手动控制
- 在自动模式下调用此 API 会返回 403 错误

### 2.4 POST /api/mode — 切换灌溉模式

**功能描述**：

切换系统的灌溉控制模式。

**请求体示例**：

```json
{
  "mode": "auto_pid"
}
```

**请求参数说明**：

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `mode` | string | 是 | 模式：`auto_pid`/`auto_simple`/`timed`/`manual` |

**模式说明**：

| 模式值 | 说明 |
|--------|------|
| `auto_pid` | PID 自动控制（精准闭环） |
| `auto_simple` | 简单阈值控制（开关控制） |
| `timed` | 定时灌溉 |
| `manual` | 手动控制 |

**响应示例**：

**成功**：
```json
{
  "success": true,
  "message": "已切换到PID自动模式"
}
```

**失败**：
```json
{
  "success": false,
  "message": "无效的模式参数"
}
```

**状态码**：

- `200 OK` — 模式切换成功
- `400 Bad Request` — 无效的模式参数

### 2.5 POST /api/threshold — 设置阈值参数

**功能描述**：

设置指定通道的干燥和湿润阈值。

**请求体示例**：

```json
{
  "channel": 0,
  "dryThreshold": 50.0,
  "wetThreshold": 75.0
}
```

**请求参数说明**：

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `channel` | number | 是 | 通道号 (0-3) |
| `dryThreshold` | number | 是 | 干燥阈值 (0-100%) |
| `wetThreshold` | number | 是 | 湿润阈值 (0-100%) |

**响应示例**：

**成功**：
```json
{
  "success": true,
  "message": "通道0阈值已更新"
}
```

**失败**：
```json
{
  "success": false,
  "message": "阈值参数错误：wetThreshold必须大于dryThreshold"
}
```

**状态码**：

- `200 OK` — 阈值设置成功
- `400 Bad Request` — 参数错误（阈值范围不合法或 wet < dry）

**参数校验规则**：

- `dryThreshold` 范围：0-100
- `wetThreshold` 范围：0-100
- `wetThreshold` 必须大于 `dryThreshold`
- 建议差值至少 10%，避免频繁开关

### 2.6 GET /api/system — 获取系统信息

**功能描述**：

获取 ESP32 系统信息，包括 WiFi、内存、固件版本等。

**请求示例**：

```http
GET /api/system HTTP/1.1
Host: 192.168.1.100
```

**响应示例**：

```json
{
  "version": "2.0.0",
  "uptime": 123456,
  "wifi": {
    "ssid": "MyWiFi",
    "ip": "192.168.1.100",
    "rssi": -65,
    "connected": true
  },
  "memory": {
    "heapSize": 327680,
    "freeHeap": 198432,
    "usedHeap": 129248,
    "heapUsage": 39.5
  },
  "flash": {
    "totalSize": 4194304,
    "usedSize": 1245184,
    "freeSize": 2949120,
    "flashUsage": 29.7
  },
  "mqtt": {
    "connected": true,
    "broker": "broker.emqx.io",
    "port": 1883
  }
}
```

**响应字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | string | 固件版本号 |
| `uptime` | number | 系统运行时间（毫秒） |
| `wifi.ssid` | string | WiFi 名称 |
| `wifi.ip` | string | IP 地址 |
| `wifi.rssi` | number | 信号强度 (dBm) |
| `wifi.connected` | boolean | WiFi 连接状态 |
| `memory.heapSize` | number | 堆总大小（字节） |
| `memory.freeHeap` | number | 可用堆（字节） |
| `memory.usedHeap` | number | 已用堆（字节） |
| `memory.heapUsage` | number | 堆使用率 (%) |
| `flash.totalSize` | number | Flash 总大小（字节） |
| `flash.usedSize` | number | 已用 Flash（字节） |
| `flash.freeSize` | number | 可用 Flash（字节） |
| `flash.flashUsage` | number | Flash 使用率 (%) |
| `mqtt.connected` | boolean | MQTT 连接状态 |
| `mqtt.broker` | string | MQTT 服务器地址 |
| `mqtt.port` | number | MQTT 端口 |

**状态码**：

- `200 OK` — 成功获取系统信息

### 2.7 POST /api/reboot — 重启系统

**功能描述**：

重启 ESP32 系统。

**请求体示例**：

```json
{
  "confirm": true
}
```

**请求参数说明**：

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `confirm` | boolean | 是 | 确认重启，必须为 `true` |

**响应示例**：

```json
{
  "success": true,
  "message": "系统将在3秒后重启"
}
```

**状态码**：

- `200 OK` — 重启命令已接受
- `400 Bad Request` — 缺少 `confirm` 参数

**注意事项**：

- 重启前会先停止所有水泵
- 系统将在 3 秒后重启
- 重启过程约需 5-10 秒

### 2.8 POST /update — OTA 固件升级

**功能描述**：

通过 HTTP 上传新固件进行 OTA（Over-The-Air）空中升级。

**请求示例**：

使用 `multipart/form-data` 上传 `.bin` 文件：

```http
POST /update HTTP/1.1
Host: 192.168.1.100
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="firmware"; filename="firmware.bin"
Content-Type: application/octet-stream

<二进制固件数据>
------WebKitFormBoundary--
```

**响应示例**：

**成功**：
```html
<html>
<body>
<h1>OTA 升级成功！</h1>
<p>系统将在5秒后重启...</p>
</body>
</html>
```

**失败**：
```html
<html>
<body>
<h1>OTA 升级失败！</h1>
<p>错误：固件大小超过分区限制</p>
</body>
</html>
```

**状态码**：

- `200 OK` — 升级成功
- `400 Bad Request` — 固件格式错误
- `500 Internal Server Error` — 升级失败

**使用方法**：

**方法 1：Web 界面上传**
1. 访问 `http://<ESP32_IP>/update`
2. 点击"选择文件"按钮
3. 选择 `.bin` 固件文件
4. 点击"上传"

**方法 2：命令行上传**
```bash
curl -F "firmware=@firmware.bin" http://192.168.1.100/update
```

**注意事项**：

- 固件文件大小不能超过分区限制（约 1.3MB）
- 升级过程中请勿断电
- 升级失败会自动回滚到旧版本
- 建议在升级前备份配置

## 3. MQTT 主题说明

除了 REST API，系统还支持 MQTT 协议进行物联网通信。

### 3.1 MQTT 连接参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| Broker | `broker.emqx.io` | MQTT 服务器地址 |
| Port | `1883` | MQTT 端口 |
| Client ID | `smart_irrigation_01` | 客户端 ID |
| Username | （空） | 用户名 |
| Password | （空） | 密码 |

### 3.2 发布主题（ESP32 → 云端）

#### 3.2.1 home/irrigation/sensors — 传感器数据

**发布频率**：每 10 秒

**消息格式**：

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

#### 3.2.2 home/irrigation/status — 系统状态

**发布频率**：每 10 秒或状态变化时

**消息格式**：

```json
{
  "mode": "auto_pid",
  "systemEnabled": true,
  "uptime": 123456,
  "alarmActive": false,
  "pumps": [
    {"channel": 0, "running": true, "pwm": 180},
    {"channel": 1, "running": false, "pwm": 0},
    {"channel": 2, "running": true, "pwm": 120},
    {"channel": 3, "running": false, "pwm": 0}
  ]
}
```

### 3.3 订阅主题（云端 → ESP32）

#### 3.3.1 home/irrigation/control — 控制命令

**消息格式**：

**启动水泵**：
```json
{
  "action": "start",
  "channel": 0,
  "pwm": 200
}
```

**停止水泵**：
```json
{
  "action": "stop",
  "channel": 1
}
```

**停止所有**：
```json
{
  "action": "stop_all"
}
```

#### 3.3.2 home/irrigation/mode — 模式切换

**消息格式**：

```json
{
  "mode": "auto_pid"
}
```

**可选值**：`auto_pid`、`auto_simple`、`timed`、`manual`

#### 3.3.3 home/irrigation/threshold — 阈值设置

**消息格式**：

```json
{
  "channel": 0,
  "dryThreshold": 50.0,
  "wetThreshold": 75.0
}
```

### 3.4 QoS 和保留消息

| 主题 | QoS | Retained |
|------|-----|----------|
| home/irrigation/sensors | 0 | 否 |
| home/irrigation/status | 1 | 是 |
| home/irrigation/control | 1 | 否 |
| home/irrigation/mode | 1 | 是 |
| home/irrigation/threshold | 1 | 否 |

**说明**：
- **QoS 0**：最多一次传输，快速但不保证送达
- **QoS 1**：至少一次传输，保证送达
- **Retained=是**：新订阅者会立即收到最后一条消息

## 4. 错误处理

### 4.1 HTTP 状态码总结

| 状态码 | 说明 | 常见原因 |
|--------|------|----------|
| `200 OK` | 请求成功 | - |
| `400 Bad Request` | 请求参数错误 | JSON 格式错误、参数缺失、参数范围不合法 |
| `403 Forbidden` | 操作被拒绝 | 自动模式下尝试手动控制 |
| `404 Not Found` | 资源不存在 | 历史文件不存在、无效的 URL |
| `500 Internal Server Error` | 服务器内部错误 | 文件系统错误、内存不足 |

### 4.2 错误响应格式

所有错误响应统一使用以下 JSON 格式：

```json
{
  "success": false,
  "message": "错误描述信息"
}
```

### 4.3 常见错误示例

**JSON 解析失败**：
```json
{
  "success": false,
  "message": "JSON解析失败"
}
```

**参数错误**：
```json
{
  "success": false,
  "message": "无效的通道号"
}
```

**权限错误**：
```json
{
  "success": false,
  "message": "系统处于自动模式，无法手动控制"
}
```

## 5. API 使用示例

### 5.1 JavaScript 示例

**获取系统状态**：
```javascript
fetch('http://192.168.1.100/api/status')
  .then(response => response.json())
  .then(data => {
    console.log('温度:', data.temperature);
    console.log('湿度:', data.humidity);
    console.log('通道0湿度:', data.channels[0].moisture);
  })
  .catch(error => console.error('错误:', error));
```

**启动水泵**：
```javascript
fetch('http://192.168.1.100/api/control', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    channel: 0,
    action: 'start',
    pwm: 200
  })
})
  .then(response => response.json())
  .then(data => console.log(data.message))
  .catch(error => console.error('错误:', error));
```

**切换模式**：
```javascript
fetch('http://192.168.1.100/api/mode', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    mode: 'auto_pid'
  })
})
  .then(response => response.json())
  .then(data => console.log(data.message));
```

### 5.2 Python 示例

**获取系统状态**：
```python
import requests
import json

response = requests.get('http://192.168.1.100/api/status')
data = response.json()

print(f"温度: {data['temperature']}°C")
print(f"湿度: {data['humidity']}%")
print(f"通道0湿度: {data['channels'][0]['moisture']}%")
```

**启动水泵**：
```python
import requests

url = 'http://192.168.1.100/api/control'
payload = {
    'channel': 0,
    'action': 'start',
    'pwm': 200
}

response = requests.post(url, json=payload)
print(response.json()['message'])
```

### 5.3 MQTT 客户端示例（Python）

**订阅传感器数据**：
```python
import paho.mqtt.client as mqtt
import json

def on_message(client, userdata, message):
    data = json.loads(message.payload.decode())
    print(f"温度: {data['temperature']}°C")
    print(f"湿度: {data['humidity']}%")

client = mqtt.Client()
client.on_message = on_message
client.connect("broker.emqx.io", 1883)
client.subscribe("home/irrigation/sensors")
client.loop_forever()
```

**发送控制命令**：
```python
import paho.mqtt.client as mqtt
import json

client = mqtt.Client()
client.connect("broker.emqx.io", 1883)

# 启动通道0水泵
command = {
    "action": "start",
    "channel": 0,
    "pwm": 200
}
client.publish("home/irrigation/control", json.dumps(command))

client.disconnect()
```

## 6. 安全建议

由于本系统未实现认证和加密，建议采取以下安全措施：

1. **局域网使用**：仅在家庭局域网内使用，不要暴露到公网
2. **路由器防火墙**：配置路由器防火墙规则
3. **VPN 访问**：如需远程访问，使用 VPN
4. **HTTPS**（可选）：使用 ESP32 的 SSL 库实现 HTTPS
5. **API Token**（可选）：在代码中添加 Token 认证

**改进方向**（未来版本）：
- 添加 API Key 认证
- 实现 HTTPS 加密传输
- 用户登录和权限管理
- 操作日志审计

## 7. 总结

本系统提供了完善的 REST API 和 MQTT 接口，支持 **Web 界面、移动应用、物联网平台** 等多种访问方式。API 设计遵循 RESTful 风格，响应格式统一为 JSON，易于集成和二次开发。

通过本文档，开发者可以：
- ✅ 快速集成智能灌溉系统到现有平台
- ✅ 开发自定义的 Web/移动界面
- ✅ 接入 Home Assistant、Node-RED 等智能家居平台
- ✅ 实现数据分析和可视化

**API 版本**：v2.0.0
**最后更新**：2026-02-11
