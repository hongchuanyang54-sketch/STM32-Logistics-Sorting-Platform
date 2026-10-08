# 物流分拣平台设计与实现

基于 **STM32F103C8T6** 的智能物流分拣仿真平台，涵盖光电计数器、传送带控制器、分拣器控制器三大功能模块。三个设备固件均已从裸机主循环**移植到 FreeRTOS（CMSIS-RTOS v2）**，通过 **EMQX + Python 网关 + MQTT** 实现物联网全链路接入。

> 📖 重庆邮电大学 · 嵌入式及其应用开发综合实验 · 2026
> ⚠️ 三个设备均为 **Proteus 仿真**，未做实物验证。

---

## 📋 项目概览

```
┌──────────────────────────────────────────────────┐
│                   Qt 上位机                        │
│               (远程监控 & 控制)                     │
└─────────────────┬────────────────────────────────┘
                  │ MQTT
┌─────────────────▼────────────────────────────────┐
│               EMQX Broker 5.3.2                   │
│            (消息中转 & 设备管理)                     │
└─────────────────┬────────────────────────────────┘
                  │ MQTT
┌─────────────────▼────────────────────────────────┐
│            Python 协议网关                         │
│    (串口 ↔ MQTT 双向转换 · 设备 ID 路由)            │
└──────┬──────────┬──────────┬─────────────────────┘
       │ UART     │ UART     │ UART
       │ 9600     │ 9600     │ 9600
┌──────▼───┐ ┌───▼────┐ ┌──▼──────────┐
│ 光电计数器 │ │传送带控制器│ │ 分拣器控制器  │
│  (lab7)  │ │  (lab8) │ │   (LAB9)    │
│ 数码管显示 │ │PWM 调速 │ │ OLED + 分类  │
├──────────┤ ├────────┤ ├─────────────┤
│ 4 任务    │ │ 6 任务  │ │  2 任务      │
│ FreeRTOS │ │FreeRTOS│ │  FreeRTOS   │
└──────────┘ └────────┘ └─────────────┘
```

---

## 🧵 RTOS 架构总览

三个固件共用一套移植思路：

| 设计原则 | 做法 |
|---|---|
| **中断只做最小工作** | ISR 里只 `osMessageQueuePut` 投递原始数据，攒帧/解析全部交给任务 |
| **串口发送单一出口** | 每个设备有且只有一个任务碰 `huart`，其他任务投递到发送队列，避免多任务抢同一个串口 |
| **慢路径压到最低优先级** | OLED 刷新/数码管扫描这类耗时的活放到最低优先级，当作"填缝"任务 |
| **能用 `static` 就不放栈上** | 队列元素缓冲（256B 级）写成任务私有 `static`，不占任务栈 |

| 设备 | 任务数 | 队列数 | 堆 (heap_4) |
|---|---|---|---|
| lab7 光电计数器 | 4 | 2 + 互斥量 1 | 6144 B |
| lab8 传送带控制器 | 6 | 2 | 8192 B |
| LAB9 分拣器控制器 | 2 | 2 | 4096 B |

---

## 🧩 三大功能模块

### 1. 光电计数器 (`lab7/`)

物料数量统计与实时显示，带 **24C02 EEPROM 掉电保存**。

| 功能 | 说明 |
|------|------|
| 数码管显示 | 2 位共阳极动态扫描，PB0~PB7 段选 + PB8/PB9 位选（经 NPN 驱动公共阳极） |
| 按键控制 | KEY1(PA1)加 / KEY2(PA2)减 / KEY3(PA3)清零，上升沿外部中断触发，200ms 消抖 |
| 长按连加连减 | 按下立即响应一次，之后每 200ms 重复（约 5 次/秒），松手即停 |
| 计数范围 | 0~99，到边界后继续长按不再上报（避免上位机计数与设备脱节） |
| 串口远程设定 | 接收 `{"GoodsNumber":"x"}\r\n` 设置 0~99；越界值钳位而非回绕 |
| 事件上报 | `ADD` / `SUB` / `ZERO` / `ESTOP` / `RESET`，均以 `\r\n` 结尾 |
| 紧急停机 | KEY4(PA0) 触发全局急停；KEY_RE(PA6) 触发一键复位 |
| **掉电保存** | 24C02 EEPROM（软件 I²C，PA4=SCL/PA5=SDA，从机地址 0xA0/0xA1），计数值写入即存，上电自动恢复 |

**任务划分**

| 任务 | 优先级 | 栈(words) | 职责 |
|---|---|---|---|
| `TaskDisplay` | High (40) | 128 | 数码管动态扫描（最高优先级保证刷新节拍） |
| `TaskKey` | AboveNormal (32) | 192 | 按键状态机 + 计数 + 事件入队 |
| `TaskUartRx` | Normal (24) | 256 | 串口下行攒帧与解析 |
| `TaskUartTx` | Low (8) | 192 | **唯一**操作 `huart1` 的任务 |

队列 `uartRxQueue`(32×`uint8_t`) · `uartEvtQueue`(8×`UartEvent_t`) · 互斥量 `counterMutex` 保护计数值

**模块**: `Core/Src/main.c`（计数值读写+协议）· `KEY/key.c`（消抖+长短按状态机）· `DISPLAY/display.c`（动态扫描）· `UART_SEND/uart_send.c`（事件投递）· `I2C/soft_i2c.c` + `EEPROM/at24c02.c`（掉电保存）

### 2. 传送带控制器 (`lab8/`)

双模式电机调速系统，支持模拟量与数字指令控制。

| 功能 | 说明 |
|------|------|
| ADC 模拟调速 | PA5(ADC1) 采集电位器电压，12 位分辨率，映射为档位 |
| PWM 输出 | PA0 (TIM2_CH1) → L298N 的 ENA，**PWM 频率 100Hz**（PSC=7200-1, ARR=100-1） |
| 方向控制 | PA1/PA2 → L298N 的 IN1/IN2，PA1 高为 FWD、PA2 高为 REV |
| 档位 | 0~10，每档 10% 占空比。电位器路径满量程到 **9 档**，串口路径可到 10 档 |
| 缓增缓减 | **每 15ms 一拍**，每拍跨 5 档，0→10 档约 30ms |
| 安全换向 | 先减速到 0，停稳后再翻转 IN1/IN2，下一拍才开始升速 |
| UART 数字调速 | `F<n>` / `R<n>` 正反转 n 档；`STOP` 停机；`ADC` 切回电位器模式 |
| JSON 远程调速 | 接收 `{"Speed":"x"}\r\n`（x 为 0~10），收到后锁定为串口模式 |
| OLED 显示 | 软件 I²C (PA3/PA4)，四行显示模式/方向/档位/占空比/ADC 原始值 |
| 状态上报 | 每 100ms 发送 `Speed:<当前档位>\r\n` |
| 防回环 | 自动过滤本机发出的 `Speed:` / `OK:` / `Error:` / `Unknown` / `{` 开头的回显 |

**任务划分**

| 任务 | 优先级 | 栈(words) | 周期 | 职责 |
|---|---|---|---|---|
| `TaskMotor` | High (40) | 128 | 15ms | 电机平滑变速状态机（唯一要求节拍准的） |
| `TaskAdc` | AboveNormal (32) | 160 | 25ms | ADC 采样 + 档位映射 |
| `TaskUartRx` | Normal (24) | 256 | 事件 | 命令解析与执行 |
| `TaskReport` | BelowNormal (16) | 160 | 100ms | 周期状态上报 |
| `TaskUartTx` | BelowNormal (16) | 192 | 事件 | **唯一**操作 `huart1` 的任务 |
| `TaskOled` | Low (8) | 256 | 自定 | OLED 刷新（约 0.13s） |

队列 `uartRxQueue`(32×`uint8_t`) · `uartTxQueue`(8×`UartTxMsg_t`)

**模块**: `Core/Src/main.c`（应用逻辑 `App_*`）· `MOTOR/motor.c`（电机状态机）· `OLED/oled.c`（SSD1306 驱动）· `SOFT_I2C/soft_i2c.c`（位翻转 I²C）· `UART_SEND/uart_send.c`（发送队列）

> 两种控制模式的仲裁规则：**谁最后发指令就听谁的**。收到串口指令则 ADC 不再干预转速；收到 `ADC` 则交还电位器控制。

### 3. 分拣器控制器 (`LAB9/`)

接收上位机下发的分拣指令，OLED 图形化展示分拣信息。

| 功能 | 说明 |
|------|------|
| OLED 汉字显示 | 软件 I²C (PA6/PA7)，SSD1306，16×16 点阵汉字 + 8×16 字符 |
| 双串口通信 | USART1 接收分拣指令；USART2 作调试输出口 |
| 下行报文 | `{"Down":"GoodC,GoodD","Up":"GoodA","Left":"","Right":""}\r\n` |
| JSON 解析 | 自实现字段提取器，按 `Down`/`Up`/`Left`/`Right` 取引号内字符串 |
| Good 前缀去除 | 不区分大小写剥离 `Good` 前缀（`GoodA,GoodB` → `A,B`），逐段处理 |
| 帧完整性判定 | **`}` + `\n` 双标记**：必须同时见到才认为一帧完整，防半帧误判 |

**任务划分**

| 任务 | 优先级 | 栈(words) | 职责 |
|---|---|---|---|
| `TaskUartRx` | AboveNormal (32) | 128 | 字节队列 → 攒帧 → 整帧投进 `frameQueue` |
| `TaskDisplay` | Low (8) | 256 | 取帧 → 解析 → 刷 OLED（约 60ms） |

队列 `uartRxQueue`(64×`uint8_t`) · `frameQueue`(3×`RxFrame_t`)；队列满时**丢最旧的一帧**，保证主机最后发的指令一定能显示

**模块**: `Core/Src/main.c`（帧处理 `App_ProcessFrame`）· `OLED/oled.c`（驱动+汉字字库）· `SOFT_I2C/soft_i2c.c` · `JSON_PARSER/json_parser.c`（字段提取+前缀剥离）· `UART_RX/uart_rx.c`（攒帧状态机）

---

## 🔍 移植过程中发现并修掉的问题

这一节记录裸机版遗留、在移植过程中（借助反汇编、差分测试、HAL 源码逐行核对）定位的真实缺陷。

### 裸机版被阻塞式显示掩盖的时序问题

- **`lab8` 的"15ms 电机节拍"从来没有真正生效过。** 裸机主循环里 `OLED_Refresh()` 是阻塞的，实测一屏要 **~0.5 秒**（软件 I²C，43 个字符 × 9 次 START/STOP 事务）。ADC/电机/上报全被拖成 ~500ms 一拍。移植后电机任务真的按 15ms 跑，斜坡从 ~1 秒缩短到 ~30ms。

### 数值越界类

- **`lab8` 档位回绕**：`motor_gear -= MOTOR_GEAR_STEP` 中 `motor_gear` 是 `uint8_t`，比步长还小时（如 `4 - 5`）会回绕成 255，后面那句 `if (motor_gear < t_gear)` 兜不住（`255 < 1` 为假），于是一路从 250 往下掉 —— 串口就打出 `Speed:230` 这种三位数，PWM 占空比也跟着乱。裸机版拍子慢 30 倍才没暴露。
- **`lab7` 的 `sscanf` 栈溢出**：`char num_str[10]` 配 `%[^\"]` 无宽度限制，上位机发一长串数字即可越界写 ~40 字节。改为 `%9[^\"]`。

### 并发与中断类

- **`lab8` 目标值撕裂读**：`Motor_SetTarget()` 写 `dir`/`gear` 两个 `uint8_t`，移植后调用方（TaskUartRx/TaskAdc）与读取方（TaskMotor）分属不同任务，中途被抢占会读到"新方向 + 旧档位"。打包成单个 `volatile uint16_t`（M3 上对齐 16 位写入是原子的）。
- **`LAB9` 上电后串口永久失聪**：`MX_USART1_UART_Init()` 里 `Mode = TX_RX`，接收器立刻工作，但 `RXNEIE` 要等约 250ms 后（OLED 初始化完）才打开，这期间到达的字节没人读 → ORE 置位。而 HAL 把 ORE 当阻塞性错误：中断里先 `UART_Receive_IT()` 读走 DR、回调里重新武装 `RXNEIE`，紧接着 `UART_EndRxTransfer()` 又把它关掉 —— 串口静默死亡直到复位，无任何报错。实现 `HAL_UART_ErrorCallback()` 重新武装解决。
- **`LAB9` 队列满时丢的是最新帧**：`osMessageQueuePut(..., 0, 0)` 队满时丢新来的那帧，队列里留 3 个旧的 —— 对一块显示"当前状态"的屏，主机最后一条指令永远显示不出来。改成丢最旧。

### 工具链相关

- **`SoftI2C_Delay()` 的循环变量漏加 `volatile`**（lab7/lab8/LAB9 三处同源）。`-O0` 下看着正常，`-Os` 下整个空循环被优化成一条 `bx lr`，延时归零、I²C 时序直接崩。加 `volatile` 后 `-Os` 下实测循环仍在。
- **STM32F1 开漏输出配不出内部上拉**：`.ioc` 里填的 `GPIO_PULLUP` 是空操作 —— F1 没有 PUPDR 寄存器，弱上拉只在输入模式可用，且 HAL 的 `GPIO_MODE_OUTPUT_OD` 分支根本不读 `GPIO_Init->Pull`。软件 I²C 必须外接上拉电阻（4.7kΩ）。
- **Proteus 的 NVIC 仿真不完整**：优先级寄存器 IPR 读回 0xFF（真机 STM32F1 只有 4 个优先级位、应读回 0xF0），导致 FreeRTOS 在 `xPortStartScheduler()` 里的 PRIGROUP 断言必然失败、调度器卡死。仿真期间把 `configASSERT` 改成不致命（**真机需改回致命版本**）。

### CMSIS-RTOS v2 的隐性约束

- `configMAX_PRIORITIES` 必须为 56、`configUSE_PORT_OPTIMISED_TASK_SELECTION` 必须为 0、`INCLUDE_xSemaphoreGetMutexHolder` 必须为 1（CubeMX 不生成最后一项，需手工补）。
- 所有调用 FreeRTOS API 的 ISR，抢占优先级数值必须 ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`（本工程为 5）。三个 lab 的中断优先级都从默认的 0 改成了 5。

---

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
2. **不经云端**，直接向全部虚拟串口广播零速指令
3. 经 MQTT 同步零速状态至 Qt 上位机

`RESET` 一键恢复：从内存快照读取急停前转速并逐设备恢复。

---

## 🔧 技术栈

| 层级 | 技术 |
|------|------|
| 主控 | STM32F103C8T6 (ARM Cortex-M3, 72MHz, 64KB Flash / 20KB SRAM) |
| 固件框架 | **FreeRTOS 10.3.1 + CMSIS-RTOS v2**（heap_4 动态分配） |
| 固件库 | STM32 HAL 库 |
| 工具链 | **GNU Arm Embedded GCC 14.3.1** · 亦可使用 Keil MDK (ARMCC V5.06) |
| 构建 | **CMake + Ninja**（VSCode + STM32 VS Code Extension）· 每 lab 独立工程 |
| 配置 | STM32CubeMX 6.15.0 / FW_F1 V1.8.7 |
| 仿真 | Proteus 8 Professional |
| 串口 | VSPD 虚拟串口对 + COMPIM 组件 |
| 网关 | Python 3.8+ · paho-mqtt · pyserial |
| 消息队列 | EMQX 5.3.2 (本地部署) |
| 上位机 | Qt (console.exe + display.exe) |

---

## 🚀 快速开始

### 前置环境

1. **GNU Arm 工具链 + CMake + Ninja** — 编译固件
   （VSCode 安装 STM32 VS Code Extension 会一并装好）
2. **STM32CubeMX 6.15.0** — 修改外设/RTOS 配置时用
3. **Proteus 8** — 电路仿真运行
4. **Python 3.8+** — 网关脚本
   ```bash
   pip install paho-mqtt pyserial
   ```
5. **EMQX** — MQTT Broker
   ```bash
   # 下载解压后
   .\bin\emqx start
   # 访问 http://localhost:18083 管理控制台
   ```
6. **VSPD** — 创建虚拟串口对（传送带 COM1↔COM2、分拣器 COM7↔COM8、计数器 COM11↔COM12 等）

### 编译固件

三个 lab 都是**独立 CMake 工程**，各自构建：

```bash
cd lab7 && cmake --preset Debug && cmake --build --preset Debug
cd lab8 && cmake --preset Debug && cmake --build --preset Debug
cd LAB9 && cmake --preset Debug && cmake --build --preset Debug
```

或一次编三个：

```bash
./build_all.sh          # 加 -r 强制全量重建
```

产物在 `<lab>/build/Debug/` 下：`<lab>.elf` / `.hex` / `.bin`，**hex 可直接加载进 Proteus**。

> 每个工程的源文件清单分两部分：`cmake/stm32cubemx/` 由 CubeMX 自动维护（HAL + FreeRTOS + Core），顶层 `CMakeLists.txt` 只挂手写模块。在 CubeMX 里重新生成代码不会影响手写模块。

### 运行步骤

1. 编译各 lab 的工程，生成 HEX 文件
2. 在 Proteus 中打开对应电路，加载 HEX 到 STM32（**晶振频率必须设为 72MHz**）
3. 启动 EMQX，运行 Python 网关脚本 `EMQX2.py`
4. 启动 Qt 上位机（console.exe / display.exe）
5. 操作按键或发送串口指令，观察全链路响应

### 工程结构

```
📦 STM32-Logistics-Sorting-Platform
├── lab7/                          # 光电计数器（独立 CMake 工程）
│   ├── Core/                      # CubeMX 生成：初始化、FreeRTOS 任务、中断向量
│   ├── DISPLAY/                   # 数码管动态扫描
│   ├── KEY/                       # 按键消抖 + 长短按状态机
│   ├── UART_SEND/                 # 事件投递
│   ├── I2C/  EEPROM/              # 软件 I²C + 24C02 驱动
│   ├── cmake/stm32cubemx/         # CubeMX 维护的构建子工程
│   ├── Middlewares/               # FreeRTOS 源码
│   └── EMQX2.py                   # Python 网关（含急停快照/恢复）
├── lab8/                          # 传送带控制器（独立 CMake 工程）
│   ├── Core/
│   ├── MOTOR/                     # 电机状态机（平滑变速、安全换向）
│   ├── OLED/                      # SSD1306 驱动 + 字库
│   ├── SOFT_I2C/                  # 位翻转 I²C
│   ├── UART_SEND/                 # 发送队列
│   └── cmake/stm32cubemx/  Middlewares/
├── LAB9/                          # 分拣器控制器（独立 CMake 工程）
│   ├── Core/
│   ├── OLED/                      # SSD1306 驱动 + 汉字字库
│   ├── SOFT_I2C/
│   ├── JSON_PARSER/               # JSON 字段提取 + Good 前缀剥离
│   ├── UART_RX/                   # 攒帧状态机
│   └── cmake/stm32cubemx/  Middlewares/
├── build_all.sh                   # 一次编译三个固件
├── CMakeLists.txt                 # 顶层占位（三个 lab 已各自独立）
└── README.md
```

---

## ⚠️ 已知限制

- 三个设备均为 **Proteus 仿真**，未在实物上验证；PWM 负载、电机惯性与真实电路有差异
- 软件 I²C 依赖**外接上拉电阻**（STM32F1 开环输出配不出内部上拉），当前未在原理图中补齐
- `configASSERT` 被改成不致命以绕过 Proteus 的 NVIC 仿真缺陷，**真机上应改回致命版本**，否则中断优先级配错时不会报警
- 串口与 MQTT 均为 **明文传输**，未做加密与认证加固
- Python 网关缺少断线重连、消息缓存与异常恢复机制
- 分拣器的执行机构（电机/舵机）未接入，仅做显示
- 摄像头识别软件与 Qt 上位机由指导教师提供，非本组开发

---

## 👥 团队成员

| 姓名 | 学号 |
|------|------|
| 张健豪 | 2024210241 |
| 黄浩睿 | 2024210232 |
| 杨洪川 | 2024210236 |
| 姚家宝 | 2024210220 |

> 指导教师：梁燕 · 重庆邮电大学通信与信息工程学院

## 📄 License

本仓库仅用于学习交流。所有代码与文档版权归原作者所有。
