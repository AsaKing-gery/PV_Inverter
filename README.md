# PV Micro-inverter 光伏微型逆变器

基于STM32F4系列MCU的光伏微型逆变器固件项目，支持MPPT最大功率点跟踪、SPWM正弦波生成和MQTT远程监控。

## 项目概述

本项目实现了一个完整的光伏微型逆变器控制系统，主要功能包括：

- **MPPT控制**：采用扰动观察法(P&O)实现最大功率点跟踪
- **SPWM生成**：基于查表法的正弦脉宽调制，驱动H桥逆变
- **PLL锁相环**：SOGI-PLL实现电网同步
- **保护功能**：过压、过流、过温等多重保护
- **远程监控**：通过ESP32实现MQTT数据上云
- **本地显示**：LCD触摸屏人机交互界面

## 硬件架构

### 主控制器
- **MCU**: STM32F407VGT6 (168MHz, Cortex-M4)
- **RTOS**: FreeRTOS
- **开发环境**: Keil MDK-ARM

### 通信模块
- **ESP32-C3**: WiFi/MQTT桥接模块
  - UART0 (GPIO20/21): USB调试输出
  - UART1 (GPIO6/7): 连接STM32

### 关键外设
| 外设 | 功能 | 备注 |
|------|------|------|
| TIM1 | MPPT PWM | 20kHz, 控制Buck电路 |
| TIM8 | SPWM | 10kHz, 中心对齐模式 |
| ADC1 | 模拟采样 | 16通道DMA传输 |
| TIM2/TIM3 | 电网同步 | PLL相位检测 |
| USART1 | ESP32通信 | 115200bps |
| USART2 | RS485/Modbus | 预留 |
| FSMC | LCD接口 | 8080并行总线 |
| SPI1 | 触摸屏 | XPT2046 |

## 软件架构

### 目录结构
```
PV_Inverter/
├── Core/                       # 应用层代码
│   ├── Inc/                    # 头文件
│   │   ├── main.h              # 主程序头文件
│   │   ├── mppt.h              # MPPT控制器
│   │   ├── spwm.h              # SPWM生成器
│   │   ├── pll_sogi.h          # SOGI-PLL锁相环
│   │   ├── data.h              # 数据结构定义
│   │   ├── protection.h        # 保护功能
│   │   ├── display.h           # LCD显示驱动
│   │   ├── display_ui.h        # UI界面
│   │   ├── adc_convert.h       # ADC转换
│   │   ├── thd_calculator.h    # THD计算
│   │   ├── rs485_modbus.h      # Modbus通信
│   │   └── esp32_wifi.h        # ESP32通信
│   └── Src/                    # 源文件
│       ├── main.c              # 主程序入口
│       ├── task.c              # FreeRTOS任务
│       ├── mppt.c              # MPPT算法实现
│       ├── spwm.c              # SPWM波形生成
│       ├── pll_sogi.c          # PLL锁相环
│       ├── data.c              # 数据管理
│       ├── protection.c        # 保护逻辑
│       ├── display.c           # 显示驱动
│       ├── display_ui.c        # UI逻辑
│       ├── adc.c               # ADC配置
│       ├── adc_convert.c       # 电压电流转换
│       ├── thd_calculator.c    # 谐波分析
│       ├── rs485_modbus.c      # Modbus协议
│       └── esp32_wifi.c        # WiFi通信
├── Drivers/                    # 底层驱动
│   ├── CMSIS/                  # ARM标准库
│   └── STM32F4xx_HAL_Driver/   # HAL库
├── ESP32_Firmware/             # ESP32固件
│   └── esp32_mqtt_bridge.ino   # MQTT桥接程序
└── MDK-ARM/                    # Keil工程文件
```

### 核心算法

#### 1. MPPT (扰动观察法)
```c
// MPPT状态机
typedef enum {
    MPPT_STATE_SCANNING = 0,    // 扫描模式
    MPPT_STATE_TRACKING,        // 跟踪模式
    MPPT_STATE_LOCKED           // 锁定模式
} MPPT_State_t;
```

#### 2. SPWM (正弦脉宽调制)
- 256点正弦查表
- 50μs更新周期
- 双极性调制

#### 3. SOGI-PLL (二阶广义积分器锁相环)
- 电网频率跟踪
- 相位同步
- 支持50/60Hz电网

### 数据帧格式 (STM32 → ESP32)

| 偏移 | 长度 | 内容 | 说明 |
|------|------|------|------|
| 0 | 2 | 帧头 | 0xAA 0x55 |
| 2 | 1 | 长度 | 72字节 |
| 3 | 4 | 时间戳 | 系统运行时间 |
| 7 | 4 | PV电压 | 浮点数(V) |
| 11 | 4 | PV电流 | 浮点数(A) |
| 15 | 4 | PV功率 | 浮点数(W) |
| 19 | 4 | 母线电压 | 浮点数(V) |
| 23 | 4 | 交流电压 | 浮点数(V) |
| 27 | 4 | 交流电流 | 浮点数(A) |
| 31 | 4 | 交流功率 | 浮点数(W) |
| 35 | 4 | 交流频率 | 浮点数(Hz) |
| 39 | 4 | 功率因数 | 浮点数 |
| 43 | 4 | 前级温度 | 浮点数(°C) |
| 47 | 4 | 后级温度 | 浮点数(°C) |
| 51 | 4 | MPPT占空比 | 浮点数(%) |
| 55 | 4 | MPPT效率 | 浮点数(%) |
| 59 | 4 | THD | 浮点数(%) |
| 63 | 4 | 电网频率 | 浮点数(Hz) |
| 67 | 4 | 电网相位 | 浮点数(°) |
| 71 | 4 | 状态字 | 32位状态 |
| 75 | 1 | CRC8 | 校验码 |

### MQTT主题

```
pv/devices/PV-INV-001/data      # 实时数据上报
pv/devices/PV-INV-001/status    # 设备状态
pv/devices/PV-INV-001/control   # 远程控制
pv/devices/PV-INV-001/event     # 事件告警
```

### JSON数据格式
```json
{
  "ts": 1234567890,
  "pv": {
    "voltage": 12.50,
    "current": 1.234,
    "power": 15.43
  },
  "bus": {
    "voltage": 11.80
  },
  "ac": {
    "voltage": 2.20,
    "current": 0.650,
    "power": 1.43,
    "frequency": 50.00,
    "pf": 0.98
  },
  "temp": {
    "mosfet_front": 45.5,
    "mosfet_rear": 42.0
  },
  "mppt": {
    "state": "TRACKING",
    "duty": 65,
    "efficiency": 98.5
  },
  "inverter": {
    "state": "RUN",
    "mode": "ON_GRID",
    "syncState": "SYNCED",
    "thd": 2.5,
    "gridFreq": 50.00,
    "gridPhase": 0.5,
    "faultCode": 0,
    "faultDesc": ""
  }
}
```

## 故障代码

| 代码 | 名称 | 描述 |
|------|------|------|
| 0x00 | FAULT_NONE | 无故障 |
| 0x01 | FAULT_PV_UV | 光伏欠压 |
| 0x02 | FAULT_OC | 输出过流 |
| 0x04 | FAULT_OT_FRONT | 前级过温 |
| 0x08 | FAULT_OT_REAR | 后级过温 |
| 0x0C | FAULT_OT_BOTH | 前后级过温 |

## 开发环境

### 硬件要求
- STM32F4开发板
- ESP32-C3模块
- 光伏面板 (建议18V/20W)
- Buck-Boost变换器
- H桥逆变电路
- LCD显示屏 (可选)

### 软件要求
- Keil MDK-ARM 5.38+
- STM32CubeMX 6.9+
- Arduino IDE (ESP32固件)
- Python 3.8+ (MQTT测试)

### 编译烧录

#### STM32固件
1. 使用Keil打开 `MDK-ARM/PV_Inverter.uvprojx`
2. 配置调试器 (ST-Link/J-Link)
3. 编译并下载

#### ESP32固件
1. 使用Arduino IDE打开 `ESP32_Firmware/esp32_mqtt_bridge.ino`
2. 配置WiFi和MQTT参数
3. 选择ESP32-C3开发板
4. 编译并上传

## 配置说明

### WiFi配置 (ESP32)
```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

### MQTT配置 (ESP32)
```cpp
const char* MQTT_BROKER_IP = "192.168.1.100";
const int MQTT_BROKER_PORT = 1883;
const char* MQTT_CLIENT_ID = "PV_Inverter_001";
const char* MQTT_USERNAME = "inverter";
const char* MQTT_PASSWORD = "password";
```

## 调试接口

- **USB-UART0**: 调试日志输出 (115200bps)
- **USART1**: STM32-ESP32通信 (115200bps)
- **SWD**: 程序调试/烧录

## 安全警告

⚠️ **高压危险**: 本项目涉及电力电子转换，请确保：
- 使用隔离变压器进行测试
- 佩戴适当的防护装备
- 在通风良好的环境中操作
- 遵守当地电气安全规范

## 许可证

本项目基于STMicroelectronics HAL库开发，遵循相关许可证条款。

## 作者

登格鲁金

## 更新日志

### v1.0.0 (2026-06)
- 初始版本发布
- 实现基础MPPT和SPWM功能
- 添加MQTT远程监控
- 支持LCD本地显示
