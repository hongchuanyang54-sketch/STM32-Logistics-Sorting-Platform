import serial
import paho.mqtt.client as mqtt
import threading
import time
import json
import re
import sys

# =========================================================
# MQTT配置
# =========================================================
MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
MQTT_USER = "Motor"
MQTT_PASS = "123456"

MQTT_CLIENT_ID = "Convey_Gateway"
GATEWAY_SOURCE = "Convey_Gateway"


# =========================================================
# MQTT主题
# =========================================================

# 传送带
TOPIC_STATUS = "/convey/status"
TOPIC_SPEED = "/convey/speed"

# 分拣器
TOPIC_SORT_STATUS = "/sort/status"
TOPIC_SORT = "/sort/direction"

# 计数器
TOPIC_COUNTER_STATUS = "/counter/status"
TOPIC_COUNTER_ADD = "/counter/add"
TOPIC_COUNTER_SUB = "/counter/sub"
TOPIC_COUNTER_ZERO = "/counter/zero"

# 急停（新增主题，体现系统扩展性）
TOPIC_ESTOP = "/emergency/stop"
TOPIC_ESTOP_RESET = "/emergency/reset"


# =========================================================
# 全局状态
# =========================================================
running = True
print_lock = threading.Lock()
serial_write_locks = {}

# 传送带速度缓存（用于急停后一键复位恢复）
last_convey_speeds = {}     # device_id -> 最近一次确认的速度值
pre_estop_speeds = {}       # 急停触发时的速度快照，供 RESET 恢复使用


# =========================================================
# 日志函数
# =========================================================
def log(message):
    """多线程安全的日志输出。"""
    with print_lock:
        now = time.strftime("%H:%M:%S")
        print(f"[{now}] {message}", flush=True)


# =========================================================
# 打开串口
# =========================================================
def open_serial_port(port_name):
    try:
        ser = serial.Serial(
            port=port_name,
            baudrate=9600,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1,
            write_timeout=1
        )

        log(
            f"[串口打开成功] "
            f"端口={port_name}，参数=9600-8-N-1"
        )

        return ser

    except Exception as e:
        log(
            f"[串口打开失败] "
            f"端口={port_name}，错误={e}"
        )

        return None


# =========================================================
# 串口映射
#
# COM1  <-> COM2     传送带0
# COM5  <-> COM6     传送带1
#
# COM7  <-> COM8     分拣器0
# COM9  <-> COM10    分拣器1
#
# COM11 <-> COM12    计数器0
# COM13 <-> COM14    计数器1
# =========================================================

# 传送带
device_ports = {
    "0": open_serial_port("COM2"),
    # "1": open_serial_port("COM6")  # 暂未配置
}

# 分拣器
sort_ports = {
    "0": open_serial_port("COM8"),
    "1": open_serial_port("COM10")
}

# 计数器
counter_ports = {
    "0": open_serial_port("COM12"),
    "1": open_serial_port("COM14")
}


# 删除打开失败的串口
device_ports = {
    device_id: ser
    for device_id, ser in device_ports.items()
    if ser is not None
}

sort_ports = {
    device_id: ser
    for device_id, ser in sort_ports.items()
    if ser is not None
}

counter_ports = {
    device_id: ser
    for device_id, ser in counter_ports.items()
    if ser is not None
}


# 为每个已打开的串口创建写锁
for ser in list(device_ports.values()):
    serial_write_locks[ser.port] = threading.Lock()

for ser in list(sort_ports.values()):
    serial_write_locks[ser.port] = threading.Lock()

for ser in list(counter_ports.values()):
    serial_write_locks[ser.port] = threading.Lock()


# =========================================================
# MQTT客户端
# =========================================================
client = mqtt.Client(
    client_id=MQTT_CLIENT_ID,
    protocol=mqtt.MQTTv311
)

client.username_pw_set(
    MQTT_USER,
    MQTT_PASS
)


# =========================================================
# 通用辅助函数
# =========================================================
def convert_to_list(value):
    """将字典或列表统一转换为列表。"""
    if value is None:
        return []

    if isinstance(value, list):
        return value

    if isinstance(value, dict):
        return [value]

    return []


def join_names(value):
    """
    将方向名称统一转换为字符串。

    ["A", "B"] -> "A,B"
    "A"        -> "A"
    None       -> ""
    """
    if value is None:
        return ""

    if isinstance(value, list):
        return ",".join(str(item) for item in value)

    return str(value)


def extract_items(payload, field_names):
    """
    从不同可能的外层字段中提取设备列表。
    """

    for field_name in field_names:
        if field_name in payload:
            return convert_to_list(payload.get(field_name)), field_name

    # 兼容没有外层字段，直接发送单个设备对象
    if "ID" in payload:
        return [payload], "直接对象"

    return [], None


def build_sort_direction_data(data):
    """
    兼容以下分拣方向格式：

    {
        "Direction": {
            "DownNames": [],
            "UpNames": [],
            "LeftNames": [],
            "RightNames": []
        }
    }

    或：

    {
        "Down": "",
        "Up": "",
        "Left": "",
        "Right": ""
    }
    """

    if not isinstance(data, dict):
        return {
            "Down": "",
            "Up": "",
            "Left": "",
            "Right": ""
        }

    direction = data.get("Direction", data)

    if not isinstance(direction, dict):
        direction = {}

    down = join_names(
        direction.get(
            "DownNames",
            direction.get("Down", "")
        )
    )

    up = join_names(
        direction.get(
            "UpNames",
            direction.get("Up", "")
        )
    )

    left = join_names(
        direction.get(
            "LeftNames",
            direction.get("Left", "")
        )
    )

    right = join_names(
        direction.get(
            "RightNames",
            direction.get("Right", "")
        )
    )

    return {
        "Down": down,
        "Up": up,
        "Left": left,
        "Right": right
    }


# =========================================================
# MQTT发布
# =========================================================
def publish_mqtt(device_type, device_id, topic, message):
    try:
        mqtt_text = json.dumps(
            message,
            separators=(",", ":"),
            ensure_ascii=False
        )

        result = client.publish(
            topic=topic,
            payload=mqtt_text,
            qos=0,
            retain=False
        )

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            log(
                f"[MQTT发布成功]"
                f"[{device_type}{device_id}] "
                f"主题={topic}，"
                f"消息ID={result.mid}，"
                f"消息={mqtt_text}"
            )

        else:
            log(
                f"[MQTT发布失败]"
                f"[{device_type}{device_id}] "
                f"主题={topic}，"
                f"结果码={result.rc}，"
                f"错误={mqtt.error_string(result.rc)}"
            )

    except Exception as e:
        log(
            f"[MQTT发布异常]"
            f"[{device_type}{device_id}] "
            f"主题={topic}，"
            f"错误={type(e).__name__}: {e}"
        )


# =========================================================
# 串口发送
# =========================================================
def send_to_serial(device_type, device_id, ser, send_text):
    try:
        send_bytes = send_text.encode("utf-8")

        port_lock = serial_write_locks.get(ser.port)

        if port_lock is None:
            port_lock = threading.Lock()
            serial_write_locks[ser.port] = port_lock

        with port_lock:
            ser.write(send_bytes)
            ser.flush()

        log(
            f"[串口发送]"
            f"[{device_type}{device_id}]"
            f"[{ser.port}] "
            f"文本={send_text!r}，"
            f"原始字节={send_bytes!r}"
        )

    except Exception as e:
        log(
            f"[串口发送失败]"
            f"[{device_type}{device_id}]"
            f"[{ser.port}] "
            f"错误={type(e).__name__}: {e}"
        )


# =========================================================
# MQTT连接回调
# =========================================================
def on_connect(client, userdata, flags, rc):
    if rc != 0:
        log(f"[MQTT] 连接失败，返回码={rc}")
        return

    log("[MQTT] 连接成功")

    subscribe_topics = [
        TOPIC_SPEED,
        TOPIC_STATUS,
        TOPIC_SORT,
        TOPIC_SORT_STATUS,
        TOPIC_COUNTER_STATUS,
        TOPIC_ESTOP,          # 新增：急停主题
        TOPIC_ESTOP_RESET     # 新增：急停复位主题
    ]

    for topic in subscribe_topics:
        result, message_id = client.subscribe(
            topic,
            qos=0
        )

        log(
            f"[MQTT订阅请求] "
            f"主题={topic}，"
            f"结果码={result}，"
            f"消息ID={message_id}"
        )


# =========================================================
# MQTT订阅确认
# =========================================================
def on_subscribe(client, userdata, mid, granted_qos):
    log(
        f"[MQTT订阅确认] "
        f"消息ID={mid}，"
        f"授权QoS={granted_qos}"
    )


# =========================================================
# MQTT断开回调
# =========================================================
def on_disconnect(client, userdata, rc):
    if rc == 0:
        log("[MQTT] 已正常断开连接")
    else:
        log(f"[MQTT] 异常断开，返回码={rc}")


# =========================================================
# 处理 /convey/speed
# =========================================================
def handle_convey_speed(payload):
    """
    传送带速度控制：
    MQTT /convey/speed -> COM2/COM6
    """

    items, source_field = extract_items(
        payload,
        ["ConveyInfo"]
    )

    if not items:
        items = [payload]

    log(
        f"[传送带速度命令] "
        f"数据来源字段={source_field}，"
        f"设备数量={len(items)}"
    )

    for item in items:
        if not isinstance(item, dict):
            log(f"[传送带速度忽略] 数据不是字典：{item!r}")
            continue

        device_id = str(item.get("ID", ""))
        speed = item.get("Speed")

        log(
            f"[传送带速度接收]"
            f"[设备ID={device_id}] "
            f"Speed={speed}"
        )

        if not device_id:
            log("[传送带速度忽略] 消息中没有ID")
            continue

        if speed is None:
            log(
                f"[传送带速度忽略]"
                f"[设备ID={device_id}] "
                f"消息中没有Speed"
            )
            continue

        if device_id not in device_ports:
            log(
                f"[传送带速度忽略]"
                f"[设备ID={device_id}] "
                f"没有配置对应串口，"
                f"已配置ID={list(device_ports.keys())}"
            )
            continue

        send_str = json.dumps(
            {
                "Speed": str(speed)
            },
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "传送带速度",
            device_id,
            device_ports[device_id],
            send_str
        )

        # 缓存本次下发的速度值，供急停后一键恢复使用
        try:
            last_convey_speeds[device_id] = int(speed)
        except (TypeError, ValueError):
            pass


# =========================================================
# 处理 /convey/status
# =========================================================
def handle_convey_status(payload):
    """
    传送带状态：
    MQTT /convey/status -> COM2/COM6
    """

    # 防止脚本接收到自己发布的消息后再次写回串口
    if payload.get("Source") == GATEWAY_SOURCE:
        log(
            "[传送带状态忽略] "
            "这是本脚本自己发布到/convey/status的消息"
        )
        return

    convey_info, source_field = extract_items(
        payload,
        ["ConveyInfo", "ConveyorInfo", "DeviceInfo"]
    )

    if not convey_info:
        log(
            f"[传送带状态字段未知] "
            f"完整JSON={payload}"
        )
        return

    log(
        f"[传送带状态接收] "
        f"来源字段={source_field}，"
        f"共收到{len(convey_info)}个状态"
    )

    for convey in convey_info:
        if not isinstance(convey, dict):
            log(
                f"[传送带状态忽略] "
                f"数据不是字典：{convey!r}"
            )
            continue

        device_id = str(convey.get("ID", ""))
        device_name = convey.get(
            "Name",
            f"Convey_{device_id}"
        )
        speed = convey.get("Speed")
        status = convey.get("Status")

        log(
            f"[传送带状态接收]"
            f"[设备ID={device_id}] "
            f"Name={device_name}，"
            f"Speed={speed}，"
            f"Status={status}"
        )

        if not device_id:
            log("[传送带状态忽略] 消息中没有ID")
            continue

        if device_id not in device_ports:
            log(
                f"[传送带状态忽略]"
                f"[设备ID={device_id}] "
                f"没有配置对应串口，"
                f"已配置ID={list(device_ports.keys())}"
            )
            continue

        if speed is None:
            log(
                f"[传送带状态忽略]"
                f"[设备ID={device_id}] "
                f"状态消息中没有Speed"
            )
            continue

        send_str = json.dumps(
            {
                "Speed": str(speed)
            },
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "传送带状态",
            device_id,
            device_ports[device_id],
            send_str
        )


# =========================================================
# 处理 /sort/direction
# =========================================================
def handle_sort_direction(payload):
    """
    分拣器方向控制：
    MQTT /sort/direction -> COM8/COM10
    """

    sort_items, source_field = extract_items(
        payload,
        [
            "SortInfo",
            "SorterInfo",
            "SortDeviceInfo"
        ]
    )

    if not sort_items:
        sort_items = [payload]

    log(
        f"[分拣方向命令] "
        f"数据来源字段={source_field}，"
        f"设备数量={len(sort_items)}"
    )

    for item in sort_items:
        if not isinstance(item, dict):
            log(f"[分拣方向忽略] 数据不是字典：{item!r}")
            continue

        device_id = str(item.get("ID", ""))

        direction_data = build_sort_direction_data(item)

        log(
            f"[分拣方向接收]"
            f"[设备ID={device_id}] "
            f"Down={direction_data['Down']!r}，"
            f"Up={direction_data['Up']!r}，"
            f"Left={direction_data['Left']!r}，"
            f"Right={direction_data['Right']!r}"
        )

        if not device_id:
            log("[分拣方向忽略] 消息中没有ID")
            continue

        if device_id not in sort_ports:
            log(
                f"[分拣方向忽略]"
                f"[设备ID={device_id}] "
                f"没有配置对应串口，"
                f"已配置ID={list(sort_ports.keys())}"
            )
            continue

        send_str = json.dumps(
            direction_data,
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "分拣方向",
            device_id,
            sort_ports[device_id],
            send_str
        )


# =========================================================
# 处理 /sort/status
# =========================================================
def handle_sort_status(payload):
    """
    分拣器状态：
    MQTT /sort/status -> COM8/COM10
    """

    # 防止脚本自己发布后再次写回串口
    if payload.get("Source") == GATEWAY_SOURCE:
        log(
            "[分拣器状态忽略] "
            "这是本脚本自己发布到/sort/status的消息"
        )
        return

    log(
        f"[分拣器状态接收] "
        f"完整JSON={payload}"
    )

    sort_info, source_field = extract_items(
        payload,
        [
            "SortInfo",
            "SorterInfo",
            "SortDeviceInfo",
            "SortSensorInfo",
            "DirectionInfo"
        ]
    )

    if not sort_info:
        log(
            "[分拣器状态字段未知] "
            "没有找到SortInfo、SorterInfo、"
            "SortDeviceInfo、SortSensorInfo或DirectionInfo"
        )
        return

    log(
        f"[分拣器状态接收] "
        f"来源字段={source_field}，"
        f"共收到{len(sort_info)}个状态"
    )

    for sorter in sort_info:
        if not isinstance(sorter, dict):
            log(
                f"[分拣器状态忽略] "
                f"数据不是字典：{sorter!r}"
            )
            continue

        device_id = str(sorter.get("ID", ""))
        device_name = sorter.get(
            "Name",
            f"Sorter_{device_id}"
        )
        status = sorter.get("Status")

        log(
            f"[分拣器状态接收]"
            f"[设备ID={device_id}] "
            f"Name={device_name}，"
            f"Status={status}，"
            f"完整数据={sorter}"
        )

        if not device_id:
            log("[分拣器状态忽略] 消息中没有ID")
            continue

        if device_id not in sort_ports:
            log(
                f"[分拣器状态忽略]"
                f"[设备ID={device_id}] "
                f"没有配置对应串口，"
                f"已配置ID={list(sort_ports.keys())}"
            )
            continue

        # 如果状态中包含方向字段，转换成统一格式
        has_direction = (
            "Direction" in sorter
            or "DownNames" in sorter
            or "UpNames" in sorter
            or "LeftNames" in sorter
            or "RightNames" in sorter
            or "Down" in sorter
            or "Up" in sorter
            or "Left" in sorter
            or "Right" in sorter
        )

        if has_direction:
            send_data = build_sort_direction_data(sorter)

            # 同时保留状态字段
            if "Status" in sorter:
                send_data["Status"] = sorter["Status"]

            if "State" in sorter:
                send_data["State"] = sorter["State"]

            if "Result" in sorter:
                send_data["Result"] = sorter["Result"]

        else:
            # 未知状态格式时，去掉定位字段后原样发送
            send_data = {
                key: value
                for key, value in sorter.items()
                if key not in ("ID", "Name")
            }

        if not send_data:
            log(
                f"[分拣器状态忽略]"
                f"[设备ID={device_id}] "
                f"没有可发送到串口的状态字段"
            )
            continue

        send_str = json.dumps(
            send_data,
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "分拣器状态",
            device_id,
            sort_ports[device_id],
            send_str
        )


# =========================================================
# 处理 /counter/status
# =========================================================
def handle_counter_status(payload):
    """
    计数器状态：
    MQTT /counter/status -> COM12/COM14
    """

    red_info = payload.get(
        "RedSensorInfo",
        []
    )

    if isinstance(red_info, dict):
        red_info = [red_info]

    if not isinstance(red_info, list):
        log(
            f"[计数器数据错误] "
            f"RedSensorInfo不是列表，"
            f"内容={red_info!r}"
        )
        return

    log(
        f"[计数器接收] "
        f"共收到{len(red_info)}个计数器状态"
    )

    for sensor in red_info:
        if not isinstance(sensor, dict):
            log(
                f"[计数器状态忽略] "
                f"数据不是字典：{sensor!r}"
            )
            continue

        device_id = str(sensor.get("ID", ""))
        goods_number = sensor.get("GoodsNumber", 0)
        device_name = sensor.get(
            "Name",
            f"RedSencor_{device_id}"
        )
        status = sensor.get("Status")

        log(
            f"[计数器接收]"
            f"[设备ID={device_id}] "
            f"Name={device_name}，"
            f"GoodsNumber={goods_number}，"
            f"Status={status}"
        )

        if not device_id:
            log("[计数器状态忽略] 消息中没有ID")
            continue

        if device_id not in counter_ports:
            log(
                f"[计数器忽略]"
                f"[设备ID={device_id}] "
                f"没有配置对应串口，"
                f"已配置ID={list(counter_ports.keys())}"
            )
            continue

        send_str = json.dumps(
            {
                "GoodsNumber": str(goods_number)
            },
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "计数器",
            device_id,
            counter_ports[device_id],
            send_str
        )


# =========================================================
# 处理 /emergency/stop（新增主题：MQTT 远程急停）
# =========================================================
def handle_emergency_stop(payload):
    """
    MQTT 远程触发急停（与硬件按键等效）：
    收到 /emergency/stop 消息后，执行全局急停联动。
    支持两种触发方式：
    1. 硬件按键 KEY0 → 串口 ESTOP → 网关 → 发布到此主题
    2. QT / 其他 MQTT 客户端直接发布到此主题 → 网关执行急停
    """
    # 防止处理自己发布的消息（避免重复执行）
    if payload.get("Source") == GATEWAY_SOURCE:
        log("[远程急停] 这是本网关自己发布的消息，跳过处理")
        return

    trigger_source = payload.get("Source", "MQTT_Client")
    log("=" * 60)
    log(f"[!!! MQTT远程急停 !!!] 触发来源={trigger_source}")

    if not device_ports:
        log("[远程急停] 没有已连接的传送带串口，跳过广播")
        log("=" * 60)
        return

    # 保存急停前速度快照
    global pre_estop_speeds
    pre_estop_speeds = dict(last_convey_speeds)
    log(f"[远程急停] 已保存急停前速度快照: {pre_estop_speeds}")

    stop_command = '{"Speed":"0"}\r\n'

    for device_id, ser in device_ports.items():
        send_to_serial(
            "传送带-远程急停",
            device_id,
            ser,
            stop_command
        )

    log(f"[远程急停] 已向 {len(device_ports)} 个传送带广播停机指令")
    log("=" * 60)


# =========================================================
# 处理 /emergency/reset（新增主题：MQTT 远程急停复位）
# =========================================================
def handle_emergency_reset(payload):
    """
    MQTT 远程触发急停复位（与硬件按键等效）：
    收到 /emergency/reset 消息后，执行全局急停复位。
    """
    # 防止处理自己发布的消息（避免无限循环）
    if payload.get("Source") == GATEWAY_SOURCE:
        log("[远程复位] 这是本网关自己发布的消息，跳过处理")
        return

    trigger_source = payload.get("Source", "MQTT_Client")
    log("=" * 60)
    log(f"[!!! MQTT远程急停复位 !!!] 触发来源={trigger_source}")

    if not device_ports:
        log("[远程复位] 没有已连接的传送带串口，跳过广播")
        log("=" * 60)
        return

    global pre_estop_speeds

    if not pre_estop_speeds:
        log("[远程复位] 没有急停前速度快照，使用默认速度 5")
        pre_estop_speeds = {}

    for device_id, ser in device_ports.items():
        speed = pre_estop_speeds.get(device_id, 5)
        send_str = json.dumps(
            {"Speed": str(speed)},
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "传送带-远程复位",
            device_id,
            ser,
            send_str
        )

    log(f"[远程复位] 已向 {len(device_ports)} 个传送带发送恢复指令")
    log("=" * 60)


# =========================================================
# MQTT消息接收回调
# =========================================================
def on_message(client, userdata, msg):
    try:
        topic = msg.topic

        raw = msg.payload.decode(
            "utf-8",
            errors="replace"
        )

        log("=" * 75)

        log(
            f"[MQTT接收] "
            f"主题={topic}，"
            f"原始消息={raw!r}"
        )

        # 兼容消息正文前面重复携带主题名称
        if raw.startswith(topic):
            raw = raw[len(topic):]

        raw = raw.strip()

        if not raw:
            log(f"[MQTT消息为空] 主题={topic}")
            return

        try:
            payload = json.loads(raw)

        except json.JSONDecodeError as e:
            log(
                f"[MQTT JSON解析失败] "
                f"主题={topic}，"
                f"内容={raw!r}，"
                f"错误={e}"
            )
            return

        if not isinstance(payload, dict):
            log(
                f"[MQTT数据格式错误] "
                f"主题={topic}，"
                f"JSON最外层不是对象，"
                f"内容={payload!r}"
            )
            return

        log(
            f"[MQTT解析成功] "
            f"主题={topic}，"
            f"JSON={payload}"
        )

        # 传送带速度控制
        if topic == TOPIC_SPEED:
            handle_convey_speed(payload)

        # 传送带状态
        elif topic == TOPIC_STATUS:
            handle_convey_status(payload)

        # 分拣器方向控制
        elif topic == TOPIC_SORT:
            handle_sort_direction(payload)

        # 分拣器状态
        elif topic == TOPIC_SORT_STATUS:
            handle_sort_status(payload)

        # 计数器状态
        elif topic == TOPIC_COUNTER_STATUS:
            handle_counter_status(payload)

        # 急停（新增主题）
        elif topic == TOPIC_ESTOP:
            handle_emergency_stop(payload)

        # 急停复位（新增主题）
        elif topic == TOPIC_ESTOP_RESET:
            handle_emergency_reset(payload)

        else:
            log(
                f"[MQTT未处理主题] "
                f"主题={topic}，"
                f"JSON={payload}"
            )

    except Exception as e:
        log(
            f"[MQTT消息处理异常] "
            f"类型={type(e).__name__}，"
            f"错误={e}"
        )


# 绑定MQTT回调
client.on_connect = on_connect
client.on_subscribe = on_subscribe
client.on_disconnect = on_disconnect
client.on_message = on_message


# =========================================================
# 处理传送带串口回传
# =========================================================
def process_convey_serial(device_id, normalized_line):
    """
    支持：

    Speed:7

    或：

    {"Speed":"7"}
    """

    speed_value = None

    match = re.fullmatch(
        r"Speed\s*:\s*(-?\d+)",
        normalized_line,
        flags=re.IGNORECASE
    )

    if match:
        speed_value = int(match.group(1))

    else:
        try:
            serial_json = json.loads(normalized_line)

            if isinstance(serial_json, dict):
                speed_value = serial_json.get("Speed")

        except json.JSONDecodeError:
            pass

    if speed_value is None:
        log(
            f"[传送带{device_id}] "
            f"收到未识别的串口消息={normalized_line!r}"
        )
        return

    try:
        speed_value = int(speed_value)

    except (TypeError, ValueError):
        log(
            f"[传送带{device_id}] "
            f"Speed不是有效整数={speed_value!r}"
        )
        return

    log(
        f"[传送带{device_id}] "
        f"识别到速度={speed_value}"
    )

    # 缓存最新确认速度，供急停后一键恢复使用
    last_convey_speeds[device_id] = speed_value

    status_message = {
        "Source": GATEWAY_SOURCE,
        "ConveyInfo": [
            {
                "ID": device_id,
                "Name": f"Convey_{device_id}",
                "Speed": speed_value,
                "Status": True
            }
        ]
    }

    publish_mqtt(
        "传送带",
        device_id,
        TOPIC_STATUS,
        status_message
    )


# =========================================================
# 全局急停联动（硬件级）
# =========================================================
def global_emergency_stop(source_device_id):
    """
    边缘群控逻辑：
    收到任意计数器的 ESTOP 指令后，
    1. 遍历所有传送带虚拟串口，广播停机指令 {"Speed":"0"}
    2. 同步发布 MQTT 消息，确保 QT 上位机能立即看到急停状态
       （不依赖传送带设备回传，避免因设备离线导致上位机无响应）
    """
    log("=" * 60)
    log(f"[!!! 紧急停止 !!!] 计数器{source_device_id} 触发硬件急停，开始全局广播停机")

    # 保存急停前各传送带的当前速度（供一键复位恢复）
    global pre_estop_speeds
    pre_estop_speeds = dict(last_convey_speeds)
    log(f"[紧急停止] 已保存急停前速度快照: {pre_estop_speeds}")

    stop_command = '{"Speed":"0"}\r\n'

    if not device_ports:
        log("[紧急停止] 没有已连接的传送带串口，跳过广播")

    # 构建传送带急停状态列表，用于 MQTT 发布
    convey_info_list = []

    for device_id, ser in device_ports.items():
        # 1) 串口广播停机指令给物理传送带
        send_to_serial(
            "传送带-急停",
            device_id,
            ser,
            stop_command
        )

        # 2) 收集 MQTT 状态数据
        convey_info_list.append({
            "ID": device_id,
            "Name": f"Convey_{device_id}",
            "Speed": 0,
            "Status": True
        })

    if convey_info_list:
        # ① 发布到新增主题 /emergency/stop（体现系统扩展性，QT 可订阅此主题）
        estop_message = {
            "Source": GATEWAY_SOURCE,
            "Event": "EmergencyStop",
            "TriggerDeviceID": source_device_id,
            "AffectedConveyors": convey_info_list
        }
        publish_mqtt(
            "传送带-急停",
            "ALL",
            TOPIC_ESTOP,
            estop_message
        )

        # ② 同步发布到 /convey/status（兼容现有 QT 上位机，无需修改 QT 订阅）
        status_message = {
            "Source": GATEWAY_SOURCE,
            "ConveyInfo": convey_info_list
        }
        publish_mqtt(
            "传送带-急停",
            "ALL",
            TOPIC_STATUS,
            status_message
        )

        log(f"[紧急停止] 已向 {len(device_ports)} 个传送带广播停机指令，"
            f"已发布至 {TOPIC_ESTOP} 和 {TOPIC_STATUS}")

    log("=" * 60)


# =========================================================
# 全局急停复位（硬件级一键恢复）
# =========================================================
def global_emergency_reset(source_device_id):
    """
    边缘群控逻辑：
    收到任意计数器的 RESET 指令后，
    遍历所有传送带虚拟串口，恢复到急停前的速度。
    """
    global pre_estop_speeds

    log("=" * 60)
    log(f"[!!! 急停复位 !!!] 计数器{source_device_id} 触发复位，恢复急停前速度")

    if not device_ports:
        log("[急停复位] 没有已连接的传送带串口，跳过广播")
        log("=" * 60)
        return

    if not pre_estop_speeds:
        log("[急停复位] 没有急停前速度快照，使用默认速度 5")
        pre_estop_speeds = {}

    convey_info_list = []

    for device_id, ser in device_ports.items():
        # 从快照中恢复速度，无记录时默认 5
        speed = pre_estop_speeds.get(device_id, 5)
        send_str = json.dumps(
            {"Speed": str(speed)},
            separators=(",", ":"),
            ensure_ascii=False
        ) + "\r\n"

        send_to_serial(
            "传送带-复位",
            device_id,
            ser,
            send_str
        )

        convey_info_list.append({
            "ID": device_id,
            "Name": f"Convey_{device_id}",
            "Speed": speed,
            "Status": True
        })

        log(f"[急停复位] 传送带{device_id} 恢复到 Speed={speed}")

    if convey_info_list:
        # ① 发布到新增主题 /emergency/reset（体现系统扩展性）
        reset_message = {
            "Source": GATEWAY_SOURCE,
            "Event": "EmergencyReset",
            "TriggerDeviceID": source_device_id,
            "RestoredConveyors": convey_info_list
        }
        publish_mqtt(
            "传送带-复位",
            "ALL",
            TOPIC_ESTOP_RESET,
            reset_message
        )

        # ② 同步发布到 /convey/status（兼容现有 QT 上位机）
        status_message = {
            "Source": GATEWAY_SOURCE,
            "ConveyInfo": convey_info_list
        }
        publish_mqtt(
            "传送带-复位",
            "ALL",
            TOPIC_STATUS,
            status_message
        )

        log(f"[急停复位] 已恢复 {len(device_ports)} 个传送带速度，"
            f"已发布至 {TOPIC_ESTOP_RESET} 和 {TOPIC_STATUS}")

    log("=" * 60)


# =========================================================
# 处理计数器串口回传
# =========================================================
def process_counter_serial(device_id, normalized_line):
    """
    支持：

    ADD
    SUB
    ZERO
    ESTOP    --> 全局急停，向所有传送带发送 {"Speed":"0"}
    RESET    --> 急停复位，恢复急停前的速度
    """

    command = normalized_line.upper()

    # 硬件级急停：拦截后直接广播，不走单机 MQTT 转发
    if command == "ESTOP":
        global_emergency_stop(device_id)
        return

    # 急停复位：恢复急停前速度
    if command == "RESET":
        global_emergency_reset(device_id)
        return

    if command not in ("ADD", "SUB", "ZERO"):
        log(
            f"[计数器{device_id}] "
            f"收到未识别的串口消息={normalized_line!r}"
        )
        return

    topic_map = {
        "ADD": TOPIC_COUNTER_ADD,
        "SUB": TOPIC_COUNTER_SUB,
        "ZERO": TOPIC_COUNTER_ZERO
    }

    method_map = {
        "ADD": "Set_AddNumber",
        "SUB": "Set_SubNumber",
        "ZERO": "Set_GoodsNumber"
    }

    mqtt_message = {
        "ID": device_id,
        "Method": method_map[command],
        "Name": f"RedSencor_{device_id}",
        "Status": True
    }

    log(
        f"[计数器{device_id}] "
        f"识别到按键={command}"
    )

    publish_mqtt(
        "计数器",
        device_id,
        topic_map[command],
        mqtt_message
    )


# =========================================================
# 处理分拣器串口回传
# =========================================================
def process_sort_serial(device_id, normalized_line):
    """
    分拣器串口回传后，发布到：

    /sort/status
    """

    log(
        f"[分拣器{device_id}] "
        f"收到设备回传={normalized_line!r}"
    )

    try:
        parsed_data = json.loads(normalized_line)

        log(
            f"[分拣器{device_id}] "
            f"回传JSON解析成功={parsed_data}"
        )

        if isinstance(parsed_data, dict):
            sort_item = dict(parsed_data)

            sort_item.setdefault(
                "ID",
                device_id
            )

            sort_item.setdefault(
                "Name",
                f"Sorter_{device_id}"
            )

            sort_item.setdefault(
                "Status",
                True
            )

        else:
            sort_item = {
                "ID": device_id,
                "Name": f"Sorter_{device_id}",
                "Data": parsed_data,
                "Status": True
            }

    except json.JSONDecodeError:
        log(
            f"[分拣器{device_id}] "
            f"回传不是JSON，按普通文本处理"
        )

        sort_item = {
            "ID": device_id,
            "Name": f"Sorter_{device_id}",
            "Message": normalized_line,
            "Status": True
        }

    status_message = {
        "Source": GATEWAY_SOURCE,
        "SortInfo": [
            sort_item
        ]
    }

    publish_mqtt(
        "分拣器",
        device_id,
        TOPIC_SORT_STATUS,
        status_message
    )


# =========================================================
# 处理一条完整串口消息
# =========================================================
def process_serial_line(
    device_type,
    device_id,
    ser,
    raw_line,
    separator
):
    try:
        line_str = raw_line.decode(
            "utf-8",
            errors="replace"
        ).strip()

        log(
            f"[串口完整消息]"
            f"[{device_type}{device_id}]"
            f"[{ser.port}] "
            f"正文原始字节={raw_line!r}，"
            f"结束符={separator!r}，"
            f"解码文本={line_str!r}"
        )

        if not line_str:
            return

        # 兼容XCOM直接输入文字形式的\r\n
        normalized_line = (
            line_str
            .replace("\\r", "")
            .replace("\\n", "")
            .strip()
        )

        if not normalized_line:
            return

        if device_type == "传送带":
            process_convey_serial(
                device_id,
                normalized_line
            )

        elif device_type == "计数器":
            process_counter_serial(
                device_id,
                normalized_line
            )

        elif device_type == "分拣器":
            process_sort_serial(
                device_id,
                normalized_line
            )

    except Exception as e:
        log(
            f"[串口消息处理错误]"
            f"[{device_type}{device_id}]"
            f"[{ser.port}] "
            f"错误={type(e).__name__}: {e}"
        )


# =========================================================
# 串口监听线程
# =========================================================
def serial_receive(device_type, device_id, ser):
    global running

    buffer = b""

    log(
        f"[监听启动]"
        f"[{device_type}{device_id}]"
        f"[{ser.port}]"
    )

    while running:
        try:
            waiting_count = ser.in_waiting

            if waiting_count > 0:
                chunk = ser.read(waiting_count)

                log(
                    f"[串口原始接收]"
                    f"[{device_type}{device_id}]"
                    f"[{ser.port}] "
                    f"字节数={len(chunk)}，"
                    f"数据={chunk!r}"
                )

                buffer += chunk

                # 支持：
                # 真正的CRLF
                # 单独LF
                # 单独CR
                # 文字形式的\r\n
                while True:
                    end_match = re.search(
                        rb"\r\n|\n|\r|\\r\\n",
                        buffer
                    )

                    if not end_match:
                        break

                    raw_line = buffer[:end_match.start()]
                    separator = end_match.group(0)
                    buffer = buffer[end_match.end():]

                    process_serial_line(
                        device_type,
                        device_id,
                        ser,
                        raw_line,
                        separator
                    )

            time.sleep(0.02)

        except serial.SerialException as e:
            log(
                f"[串口异常]"
                f"[{device_type}{device_id}]"
                f"[{ser.port}] "
                f"错误={e}"
            )
            break

        except Exception as e:
            log(
                f"[串口监听错误]"
                f"[{device_type}{device_id}]"
                f"[{ser.port}] "
                f"错误={type(e).__name__}: {e}"
            )

            time.sleep(0.5)

    log(
        f"[监听结束]"
        f"[{device_type}{device_id}]"
        f"[{ser.port}]"
    )


# =========================================================
# 启动串口线程
# =========================================================
def start_serial_threads():
    # 传送带
    for device_id, ser in device_ports.items():
        threading.Thread(
            target=serial_receive,
            args=(
                "传送带",
                device_id,
                ser
            ),
            daemon=True,
            name=f"Convey-{device_id}"
        ).start()

    # 分拣器
    for device_id, ser in sort_ports.items():
        threading.Thread(
            target=serial_receive,
            args=(
                "分拣器",
                device_id,
                ser
            ),
            daemon=True,
            name=f"Sort-{device_id}"
        ).start()

    # 计数器
    for device_id, ser in counter_ports.items():
        threading.Thread(
            target=serial_receive,
            args=(
                "计数器",
                device_id,
                ser
            ),
            daemon=True,
            name=f"Counter-{device_id}"
        ).start()


# =========================================================
# 关闭串口和MQTT
# =========================================================
def close_all_ports():
    log("[系统] 正在释放串口")

    all_groups = [
        ("传送带", device_ports),
        ("分拣器", sort_ports),
        ("计数器", counter_ports)
    ]

    for device_type, ports in all_groups:
        for device_id, ser in ports.items():
            try:
                if ser.is_open:
                    port_name = ser.port
                    ser.close()

                    log(
                        f"[串口已关闭]"
                        f"[{device_type}{device_id}]"
                        f"[{port_name}]"
                    )

            except Exception as e:
                log(
                    f"[串口关闭失败]"
                    f"[{device_type}{device_id}] "
                    f"错误={e}"
                )

    try:
        client.loop_stop()
        client.disconnect()

    except Exception as e:
        log(f"[MQTT关闭错误] 错误={e}")

    log("[系统] 所有COM接口已释放")


# =========================================================
# 主程序
# =========================================================
if __name__ == "__main__":
    try:
        log("[系统] 正在连接MQTT服务器")

        client.connect(
            MQTT_BROKER,
            MQTT_PORT,
            60
        )

        client.loop_start()

        start_serial_threads()

        log("[系统] 网关已启动")

        log(
            f"[系统] 传送带串口="
            f"{[(device_id, ser.port) for device_id, ser in device_ports.items()]}"
        )

        log(
            f"[系统] 分拣器串口="
            f"{[(device_id, ser.port) for device_id, ser in sort_ports.items()]}"
        )

        log(
            f"[系统] 计数器串口="
            f"{[(device_id, ser.port) for device_id, ser in counter_ports.items()]}"
        )

        log(
            "[系统] MQTT接收主题="
            f"{TOPIC_SPEED}，"
            f"{TOPIC_STATUS}，"
            f"{TOPIC_SORT}，"
            f"{TOPIC_SORT_STATUS}，"
            f"{TOPIC_COUNTER_STATUS}，"
            f"{TOPIC_ESTOP}，"
            f"{TOPIC_ESTOP_RESET}"
        )

        log("[系统] 按Ctrl+C停止程序")

        while running:
            time.sleep(1)

    except KeyboardInterrupt:
        log("[系统] 检测到Ctrl+C停止指令")

    except Exception as e:
        log(
            f"[系统错误] "
            f"类型={type(e).__name__}，"
            f"错误={e}"
        )

    finally:
        running = False

        time.sleep(0.5)

        close_all_ports()

        sys.exit(0)