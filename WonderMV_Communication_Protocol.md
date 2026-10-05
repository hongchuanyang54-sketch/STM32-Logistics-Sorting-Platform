# WonderMV 视觉模块通信协议

## 1. 概述

本文档定义了 STM32 单片机与 WonderMV（K210）视觉模块之间的通信协议。协议基于 **I²C** 总线，采用 **主-从机模式**（STM32 为主机，WonderMV 为从机），通过 **寄存器映射** 方式读取视觉识别结果。

适用场景：颜色识别、人脸检测、标签（AprilTag）识别、物体检测。

---

## 2. 硬件接口

| 项目 | 说明 |
|------|------|
| **通信总线** | I²C（I2C1） |
| **SCL 引脚** | PB6 |
| **SDA 引脚** | PB7 |
| **时钟频率** | 100 kHz（标准模式） |
| **从机地址** | **0x32**（7 位地址） |
| **地址模式** | 7 位寻址 |

> **注意**：STM32 HAL 库的 I²C 地址参数要求将 7 位地址左移 1 位后再传入，即 `0x32 << 1 = 0x64`。但在协议层面从机地址固定为 0x32，左移操作是主机驱动的实现细节，不影响协议本身。

---

## 3. 寄存器地址映射

WonderMV 将不同类型的视觉识别结果映射到不同的寄存器地址。主机读取某个寄存器，即获取对应类型的识别数据。

| 寄存器地址 | 宏定义 | 功能 |
|-----------|--------|------|
| **0x00** | `COLOR_REG` | 颜色识别结果 |
| **0x10** | `FACE_REG` | 人脸检测结果 |
| **0x20** | `TAG_REG` | 标签（AprilTag）识别结果 |
| **0x30** | `OBJECT_REG` | 物体检测结果 |

---

## 4. 数据格式

每个寄存器存储 **9 字节** 的结果数据，格式统一：

| 字节偏移 | 数据项 | 说明 |
|---------|--------|------|
| [0]     | **id** | 识别结果编号（如颜色编号：1=红，2=绿，3=蓝） |
| [1]     | **x_L** | 中心点 X 坐标，低 8 位 |
| [2]     | **x_H** | 中心点 X 坐标，高 8 位 |
| [3]     | **y_L** | 中心点 Y 坐标，低 8 位 |
| [4]     | **y_H** | 中心点 Y 坐标，高 8 位 |
| [5]     | **w_L** | 识别框宽度，低 8 位 |
| [6]     | **w_H** | 识别框宽度，高 8 位 |
| [7]     | **h_L** | 识别框高度，低 8 位 |
| [8]     | **h_H** | 识别框高度，高 8 位 |

**坐标拼接规则（小端格式）：**

```
x = (x_H << 8) | x_L
y = (y_H << 8) | y_L
w = (w_H << 8) | w_L
h = (h_H << 8) | h_L
```

---

## 5. 通信时序

读取识别结果的完整 I²C 通信流程如下：

```
步骤1：主机发送 —— 写入寄存器地址（1 字节）
步骤2：主机接收 —— 读取结果数据（9 字节）
```

### 详细流程（主机视角）

```
┌─────────────────────────────────────────────────────┐
│                    主机 (STM32)                       │
│                                                      │
│  ┌─────────────┐         ┌──────────────────────┐   │
│  │ 写寄存器地址  │  ───→  │ 从机 ACK              │   │
│  │ (1 字节)     │         │                      │   │
│  └─────────────┘         └──────────────────────┘   │
│                                                      │
│  ┌─────────────────────────────────────────┐         │
│  │ 读取数据 (9 字节)     ←──  从机发送     │         │
│  │ [0] id                                  │         │
│  │ [1] x_L                                 │         │
│  │ [2] x_H                                 │         │
│  │ [3] y_L                                 │         │
│  │ [4] y_H                                 │         │
│  │ [5] w_L                                 │         │
│  │ [6] w_H                                 │         │
│  │ [7] h_L                                 │         │
│  │ [8] h_H                                 │         │
│  └─────────────────────────────────────────┘         │
└─────────────────────────────────────────────────────┘
```

### 伪代码描述

```
// 读取指定寄存器的识别结果
function read_recognition(register_address) -> 9 bytes

    1. 主机通过 I²C 向从机地址 0x32 发送 1 字节：register_address
    2. 等待从机 ACK
    3. 主机通过 I²C 从从机地址 0x32 读取 9 字节数据
    4. 返回这 9 字节数据
```

### 超时处理

- 写操作建议设置超时时间（如 100 ms），超时后返回失败状态
- 读操作可采用轮询方式等待 I²C 总线空闲，或在 DMA 完成中断中处理

---

## 6. 数据结构定义

学生需要实现以下 C 语言数据结构：

### PositionObjectTypeDef

描述识别结果的位置和尺寸信息。

```c
typedef struct
{
    uint16_t w;     // 识别框宽度
    uint16_t h;     // 识别框高度
    uint16_t x;     // 识别框中心点 X 坐标
    uint16_t y;     // 识别框中心点 Y 坐标
} PositionObjectTypeDef;
```

### RecognitionHanleTypeDef

描述完整的识别结果。

```c
typedef struct
{
    uint8_t id;                     // 识别结果编号
    PositionObjectTypeDef position; // 识别框位置和尺寸
} RecognitionHanleTypeDef;
```

---

## 7. API 函数声明

学生需要实现以下 API 函数：

### 初始化

```c
/**
 * @brief 初始化 WonderMV 视觉模块
 *        配置 I²C 从机地址，重置通信状态
 */
void wonder_mv_init(void);
```

### 颜色识别

```c
/**
 * @brief 执行颜色识别
 * @param color 指向 RecognitionHanleTypeDef 结构体的指针，
 *              用于接收识别结果（id + 位置）
 * @return true  识别成功，结果已写入 color
 *         false 识别失败或通信超时
 */
bool wonder_mv_color_recognition(RecognitionHanleTypeDef* color);
```

**颜色编号定义（参考值）：**

| id 值 | 颜色 |
|-------|------|
| 1     | 红色 |
| 2     | 绿色 |
| 3     | 蓝色 |
| 其他  | 未识别或未知 |

### 人脸检测

```c
/**
 * @brief 执行人脸检测
 * @param face 指向 RecognitionHanleTypeDef 结构体的指针
 * @return true 检测成功；false 检测失败
 */
bool wonder_mv_face_detection(RecognitionHanleTypeDef* face);
```

### 标签识别

```c
/**
 * @brief 执行标签（AprilTag）识别
 * @param tag 指向 RecognitionHanleTypeDef 结构体的指针
 * @return true 识别成功；false 识别失败
 */
bool wonder_mv_tag_detection(RecognitionHanleTypeDef* tag);
```

### 物体识别

```c
/**
 * @brief 执行物体检测
 * @param obj 指向 RecognitionHanleTypeDef 结构体的指针
 * @return true 检测成功；false 检测失败
 */
bool wonder_mv_object_detection(RecognitionHanleTypeDef* obj);
```

---

## 8. 头文件组织建议

`wonder_mv.h` 中应包含：

- 宏定义（寄存器地址、设备地址）
- 类型定义（`PositionObjectTypeDef`、`RecognitionHanleTypeDef`）
- 函数声明（`wonder_mv_init`、各识别函数）

`wonder_mv.c` 中应包含：

- I²C 读写操作的封装
- 写寄存器地址 + 读取结果数据的组合操作
- 结果数据解析（9 字节 → 结构体成员赋值）

---

## 9. 编程注意事项

1. **I²C 地址处理**：协议层从机地址为 `0x32`，与具体 MCU 平台无关。各平台驱动库对地址的处理方式可能不同（如 STM32 HAL 需左移 1 位），学生需根据所用平台调整。

2. **数据解析**：读取到的 9 字节数据中，x、y、w、h 均为 **小端格式**，拼接时注意高低字节顺序。

3. **通信可靠性**：
   - 每次读取前先确认 I²C 总线状态就绪
   - 建议在读取循环中加入适当延时（如 10 ms），避免连续高频轮询
   - 可加入多次读取确认机制（如连续两次读到相同有效 ID 才采用结果）来提高稳定性

4. **结果清零**：处理完一次识别结果后，建议将 `RecognitionHanleTypeDef` 结构体清空（`memset`），避免残留数据影响下一次判断。

---

## 10. 使用示例（伪代码）

```c
// 初始化
wonder_mv_init();

RecognitionHanleTypeDef color;

while (1)
{
    // 读取颜色识别结果
    if (wonder_mv_color_recognition(&color))
    {
        if (color.id != 0)
        {
            // 根据 color.id 判断颜色
            switch (color.id)
            {
                case 1:  // 红色
                    // 执行红色对应的动作
                    break;
                case 2:  // 绿色
                    // 执行绿色对应的动作
                    break;
                case 3:  // 蓝色
                    // 执行蓝色对应的动作
                    break;
            }
            
            // 获取物体中心位置
            uint16_t obj_x = color.position.x;
            uint16_t obj_y = color.position.y;
        }
    }
    
    delay(10);  // 适当延时
}
```

---

## 11. 附录：寄存器地址宏定义参考

建议在头文件中定义的寄存器地址宏：

| 宏名称 | 值 | 用途 |
|--------|-----|------|
| `WONDERMV_ADDR` | `0x32` | WonderMV 从机 I²C 地址 |
| `COLOR_REG` | `0x00` | 颜色识别结果寄存器 |
| `FACE_REG` | `0x10` | 人脸检测结果寄存器 |
| `TAG_REG` | `0x20` | 标签识别结果寄存器 |
| `OBJECT_REG` | `0x30` | 物体检测结果寄存器 |

---

> 版本：v1.0  
> 适用平台：STM32 F1 系列 + WonderMV（K210）视觉模块  
> 协议类型：I²C（标准模式，100 kHz）
