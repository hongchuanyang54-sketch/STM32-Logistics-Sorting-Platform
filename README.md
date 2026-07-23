# 物流分拣平台设计与实现

基于 **STM32F103C8T6** 的智能物流分拣仿真平台，涵盖光电计数器、传送带控制器、分拣器控制器三大功能模块，通过 **EMQX + Python 网关 + MQTT** 实现物联网全链路接入。

> 📖 重庆邮电大学 · 嵌入式及其应用开发综合实验 · 2026

---

## 📋 项目概览

```
┌──────────────────────────────────────────────────┐
│                   Qt 上位机                        │
│               (远程监控 & 控制)                     │
└─────────────────┬────────────────────────────────┘
                  │ MQTT
┌─────────────────▼────────────────────────────────┐
│               EMQX Broker                         │
│            (消息中转 & 设备管理)                     │
└─────────────────┬────────────────────────────────┘
                  │ MQTT
┌─────────────────▼────────────────────────────────┐
│            Python 协议网关                         │
│    (串口 ↔ MQTT 双向转换 · 设备 ID 路由)            │
└──────┬──────────┬──────────┬─────────────────────┘
       │ UART     │ UART     │ UART
┌──────▼───┐ ┌───▼────┐ ┌──▼──────────┐
│ 光电计数器 │ │传送带控制器│ │ 分拣器控制器  │
│  (lab7)  │ │  (lab8) │ │   (LAB9)    │
│ 数码管显示 │ │PWM 调速 │ │ OLED + 分类  │
└──────────┘ └────────┘ └─────────────┘
```

## 🧩 三大功能模块

### 1. 光电计数器 (`lab7/`)

物料数量统计与实时显示。

| 功能 | 说明 |
|------|------|
| 数码管显示 | 2 位共阳极动态扫描，PB0~PB7 段选 + PB8/PB9 位选 |
| 按键控制 | KEY1(加) / KEY2(减) / KEY3(清零)，外部中断触发 |
| 长按连加连减 | 按住 > 200ms 后每 200ms 自动重复，松开即停 |
| 串口远程设定 | 接收 `{"GoodsNumber":"x"}\r\n` 设置 0~99 任意值 |
| 事件上报 | 按键触发后发送 `ADD` / `SUB` / `ZERO` / `ESTOP` / `RESET` |
| 紧急停机 | KEY4(PA0) 触发全局急停 |

**源码入口**: [`lab7/Core/Src/main.c`](lab7/Core/Src/main.c)

### 2. 传送带控制器 (`lab8/`)

双模式电机调速系统，支持模拟量与数字指令控制。

| 功能 | 说明 |
|------|------|
| ADC 模拟调速 | PA5 采集电位器电压，10 档映射，滞回滤波防抖 |
| PWM 输出 | PA0 (TIM2_CH1) → L298N 电机驱动，1kHz |
| UART 数字调速 | `F/R + 档位` 控制正反转 + 速度；`SPD+/-` 步进；`STOP` 停止 |
| 10 档精细控制 | 每档 10% 占空比，支持 `SPD X` 直接跳档 |
| 缓增缓减 | 每 15ms 步进 `GEAR_STEP`，平滑过渡无冲击 |
| 安全换向 | 先减速停转→延时→换向→平滑重启 |
| OLED 显示 | 软件 I²C (PA3/PA4)，实时显示模式/档位/占空比/方向 |
| JSON 远程调速 | 接收 `{"Speed":"x"}\r\n` 进入 UART 模式 |
| 防回环 | 自动过滤本机发出的状态上报消息 |
| 状态上报 | 每 100ms 发送 `Speed:x\r\n` |

**源码入口**: [`lab8/Core/Src/main.c`](lab8/Core/Src/main.c)

### 3. 分拣器控制器 (`LAB9/`)

接收视觉识别结果，OLED 图形化展示分拣信息。

| 功能 | 说明 |
|------|------|
| OLED 汉字显示 | 软件 I²C (PA6/PA7)，SSD1306 驱动，16×16 点阵字模 |
| 双串口通信 | USART1 接收分拣指令，USART2 调试输出 |
| JSON 解析 | 自实现解析器，提取 `Down`/`Up`/`Left`/`Right` 字段 |
| Good 前缀去除 | 不区分大小写剥离 `Good` 前缀（`GoodA` → `A`） |
| 双缓冲接收 | ISR 写 `frame_buffer` → 主循环原子读取，线程安全 |
| 帧边界检测 | `}` 结束 + `\n` 确认完整帧 |

**源码入口**: [`LAB9/Core/Src/main.c`](LAB9/Core/Src/main.c)

## 🛰️ MQTT 物联网接入

### 主题体系

| 主题 | 方向 | 用途 |
|------|------|------|
| `/convey/status` | 上行 | 传送带状态上报 |
| `/convey/speed` | 下行 | 传送带调速指令 |
| `/sort/status` | 上行 | 分拣器状态上报 |
| `/sort/direction` | 下行 | 分拣方向控制 |
| `/counter/status` | 上行 | 计数器状态上报 |
| `/counter/add` `/counter/sub` `/counter/zero` | 上行 | 按键事件 |
| `/emergency/stop` `/emergency/reset` | 双向 | 全局急停 / 一键复位 |

### 数据格式示例

**上行（设备 → 云端）：**
```json
{"Source":"Convey_Gateway","ConveyInfo":[{"ID":"0","Name":"Convey_0","Speed":5,"Status":true}]}
```

**下行（云端 → 设备）：**
```json
{"ConveyInfo":[{"ID":"0","Name":"Convey_0","Speed":7,"Status":true}]}
```

### 急停联动

当任意设备触发 `ESTOP` 时，Python 网关立即执行：
1. 快照当前所有传送带转速至 `pre_estop_speeds`
2. 本地广播停机指令至全部虚拟串口
3. 经由 MQTT 同步零速状态至 Qt 上位机

`RESET` 一键恢复：从内存快照读取急停前转速并逐设备恢复。

## 🔧 技术栈

| 层级 | 技术 |
|------|------|
| 主控 | STM32F103C8T6 (ARM Cortex-M3, 72MHz) |
| 固件 | STM32 HAL 库 · Keil MDK (uVision 5) |
| 配置 | STM32CubeMX |
| 仿真 | Proteus 8 Professional |
| 串口 | VSPD 虚拟串口 + COMPIM 组件 |
| 网关 | Python 3.8+ · paho-mqtt · pyserial |
| 消息队列 | EMQX 5.3.2 (本地部署) |
| 上位机 | Qt (console.exe + display.exe) |

## 🚀 快速开始

### 前置环境

1. **Keil MDK** — 编译 STM32 固件
2. **Proteus 8** — 电路仿真运行
3. **Python 3.8+** — 网关脚本
   ```bash
   pip install paho-mqtt pyserial
   ```
4. **EMQX** — MQTT Broker
   ```bash
   # 下载解压后
   .\bin\emqx start
   # 访问 http://localhost:18083 管理控制台
   ```
5. **VSPD** — 创建虚拟串口对

### 运行步骤

1. 用 Keil 编译各 lab 的工程，生成 HEX 文件
2. 在 Proteus 中打开对应电路，加载 HEX 到 STM32
3. 启动 EMQX，运行 Python 网关脚本
4. 启动 Qt 上位机（console.exe / display.exe）
5. 操作按键或发送串口指令，观察全链路响应

### 工程结构

```
📦 STM32-Logistics-Sorting-Platform
├── lab7/                          # 光电计数器
│   ├── Core/
│   │   ├── Inc/                   # 头文件 (gpio.h, usart.h, main.h ...)
│   │   └── Src/                   # 源文件 (main.c, gpio.c, usart.c ...)
│   └── Drivers/                   # CMSIS + HAL 驱动库
├── lab8/                          # 传送带控制器
│   ├── Core/
│   │   ├── Inc/                   # 头文件 (adc.h, tim.h, oled.h ...)
│   │   └── Src/                   # 源文件 (main.c, adc.c, tim.c ...)
│   └── Drivers/
├── LAB9/                          # 分拣器控制器
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/                   # 源文件 (main.c, oled.c ...)
│   └── Drivers/
├── 报告.pdf                       # 综合实验完整报告 (55 页)
└── README.md
```

## 👥 指导教师

> 指导教师：梁燕 · 重庆邮电大学通信与信息工程学院

## 📄 License

本仓库仅用于学习交流。所有代码与文档版权归原作者所有。
