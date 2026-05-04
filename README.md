# SerialParser

SerialParser 是一个 Windows 桌面串口上位机工具，基于 C++17、Qt 6 Widgets、Qt SerialPort 和 CMake 开发。

它既可以作为普通串口助手查看和发送原始数据，也可以根据 JSON 协议配置解析 STM32 发来的自定义二进制数据包。

## 一、拿到发布版后如何使用

如果你是从 GitHub Release 下载，一般会拿到：

```text
SerialParser-Windows-x64.zip
```

请按下面步骤使用：

1. 右键 `SerialParser-Windows-x64.zip`。
2. 选择“全部解压缩”或使用 7-Zip / WinRAR 解压。
3. 解压后进入文件夹：

```text
build_release
```

4. 双击运行：

```text
SerialParser.exe
```

不要直接在压缩包预览窗口里双击运行 exe。必须先完整解压，否则 Qt DLL、平台插件、配置文件和样式文件可能无法被程序找到。

发布版解压后的目录结构如下：

```text
build_release
├── SerialParser.exe        # 启动器，双击这个
├── README.md               # 使用说明
└── app                     # 运行库、配置、样式和真正的 Qt 程序
    ├── SerialParserApp.exe
    ├── Qt6Core.dll
    ├── Qt6Widgets.dll
    ├── Qt6SerialPort.dll
    ├── platforms
    ├── configs
    ├── styles
    └── resources
```

使用时请注意：

- 不要只复制或移动 `SerialParser.exe`。
- 必须保留整个发布文件夹。
- 如果要发给别人，推荐直接发送 `SerialParser-Windows-x64.zip`。
- 如果已经解压，则发送整个 `build_release` 文件夹。
- `app/configs` 目录里放协议配置文件。
- `app/styles` 目录里放界面样式文件。
- `app/resources` 目录里放应用图标等资源。

### 1. 启动软件

双击：

```text
SerialParser.exe
```

如果提示找不到 `SerialParserApp.exe` 或缺少 Qt DLL，说明发布文件夹结构不完整，需要重新运行 `deploy_release.ps1`，或者重新复制整个 `build_release` 文件夹。

### 2. 连接串口设备

1. 将 STM32 或 USB 转串口设备插入电脑。
2. 打开软件。
3. 点击左侧“刷新”按钮。
4. 在串口号下拉框中选择对应 COM 口。
5. 设置串口参数：
   - 默认波特率：`115200`
   - 数据位：`8`
   - 停止位：`1`
   - 校验位：`None`
6. 点击“打开串口”。

打开成功后，左侧状态会显示串口已打开。

### 3. 查看原始数据

下方“原始数据”窗口可以查看 STM32 发来的原始字节流。

支持三种显示模式：

- HEX 显示
- 文本显示
- HEX + 文本双显示

示例：

```text
[12:34:56.789] RX HEX: A5 01 00 00 00 01 9A 99 05 3F 00 00 00 00 00 00 00 00 01 5A
[12:34:57.012] TX TEXT: hello
```

其中：

- `RX` 表示接收到的数据。
- `TX` 表示发送出去的数据。
- 开启“显示时间戳”后，每行前面会显示 `[HH:mm:ss.zzz]`。

原始数据窗口默认最多保留最近 200 行，防止无限增长。

### 4. 查看协议解析结果

软件启动后默认加载：

```text
configs/remote_v1.json
```

默认协议用于解析 20 字节 STM32 遥控器数据帧：

- 包头：`A5`
- 包尾：`5A`
- 字节序：little endian
- 帧模式：搜索包头重同步模式
- 默认不启用 CRC

如果 STM32 发来的数据符合默认协议，中间“实时字段值”表会显示：

- `K1`
- `K2`
- `K3`
- `LB`
- `RB`
- `Vx`
- `Vy`
- `Wz`
- `Mode`

字段正常时状态显示“正常”。字段越界、浮点数 NaN/Inf、超出 min/max 等情况会显示异常颜色。

### 5. 发送数据

底部“发送区”支持两种模式。

文本发送：

1. 选择“文本发送”。
2. 选择编码，默认跟随原始数据显示编码。
3. 输入文本。
4. 选择是否自动追加换行：
   - 无
   - `\r\n`
   - `\n`
   - `\r`
5. 点击“发送”。

HEX 发送：

1. 选择“HEX 发送”。
2. 输入 HEX 字节，例如：

```text
A5 01 00 5A
```

也支持：

```text
0xA5 0x01 0x00 0x5A
```

3. 点击“发送”。

发送成功后，原始数据窗口会显示 `TX HEX:` 或 `TX TEXT:`。

### 6. 修改协议配置

右侧配置区可以修改：

- 基础配置：包长、包头、包尾、字节序、帧模式。
- CRC 配置：是否启用、类型、校验字段位置、计算范围。
- 字段配置：字段名、类型、offset、length、scale、bias、unit、enumMap 等。
- 原始数据设置：显示模式、编码、暂停显示、自动滚动、最大行数、时间戳。

修改字段或协议后，需要点击“应用配置”，解析器才会使用新配置。

对于常规连续结构体协议，通常只需要修改字段名和字段类型，必要时填写 `enumMap`。字段配置页里的“自动计算布局”会根据包头长度、字段顺序、字段类型长度和包尾长度，自动填写 `offset`、修正普通类型的 `length`，并更新基础配置里的 `frameLength`。

需要保存时点击：

- “保存配置”：覆盖当前 JSON。
- “另存为”：保存成新的 JSON。

## 二、默认协议说明

默认配置文件：

```text
configs/remote_v1.json
```

对应 STM32 结构体：

```c
#pragma pack(push, 1)
typedef struct
{
    uint8_t Frame_Header;   // 帧头 0xA5

    uint8_t K1;
    uint8_t K2;
    uint8_t K3;
    uint8_t LB;
    uint8_t RB;

    float Vx;
    float Vy;
    float Wz;

    uint8_t Mode;
    uint8_t Frame_Tail;     // 帧尾 0x5A
} Remote_TX_Frame_t;
#pragma pack(pop)
```

结构体总长度为 20 字节。

默认字段：

| 字段 | 类型 | Offset | 说明 |
|---|---:|---:|---|
| K1 | bool_uint8 | 1 | 按键状态 |
| K2 | bool_uint8 | 2 | 按键状态 |
| K3 | bool_uint8 | 3 | 按键状态 |
| LB | bool_uint8 | 4 | 按键状态 |
| RB | bool_uint8 | 5 | 按键状态 |
| Vx | float32 | 6 | X 方向速度 |
| Vy | float32 | 10 | Y 方向速度 |
| Wz | float32 | 14 | 角速度 |
| Mode | uint8 enum | 18 | 模式 |

`Mode` 默认枚举：

```text
0=自动模式;1=遥控模式;2=调试模式
```

## 三、协议配置字段说明

协议配置使用 JSON。主要字段如下：

```json
{
    "profileName": "STM32_Remote_V1",
    "frameLength": 20,
    "header": "A5",
    "tail": "5A",
    "endian": "little",
    "frameMode": "search_header",
    "serial": {
        "baudrate": 115200,
        "dataBits": 8,
        "stopBits": "1",
        "parity": "None"
    },
    "crc": {
        "enabled": false,
        "type": "none",
        "offset": 0,
        "length": 0,
        "rangeStart": 0,
        "rangeLength": 0
    },
    "fields": []
}
```

### frameLength

固定帧长度，单位 byte。

### header / tail

包头和包尾，使用 HEX 字符串：

```text
A5
AA 55
0D 0A
```

### endian

字节序：

- `little`
- `big`

STM32 常见情况是 little endian。

### frameMode

支持两种模式。

`search_header：搜索包头重同步模式`

- 从连续字节流中搜索包头。
- 丢弃包头前面的错位数据。
- 找到包头后按 `frameLength` 取一包。
- 检查包尾和 CRC。
- 包尾错误时丢弃当前包头第一个字节，继续搜索下一包。
- 适合串口偶尔丢字节、错位、前面有垃圾数据的场景。

`strict_fixed：严格定长缓存模式`

- 不搜索包头。
- 不自动重同步。
- `RxBuffer.length() < frameLength`：等待继续接收。
- `RxBuffer.length() == frameLength`：解析这一包。
- `RxBuffer.length() > frameLength`：记录长度错误并清空缓存。
- 适合用户确认数据已经严格一包一包对齐的场景。

## 四、字段类型说明

支持字段类型：

- `uint8`
- `int8`
- `uint16`
- `int16`
- `uint32`
- `int32`
- `float32`
- `float64`
- `bool_uint8`
- `raw_hex`

字段 `length` 规则：

- `uint8` / `int8` / `bool_uint8` 自动为 1。
- `uint16` / `int16` 自动为 2。
- `uint32` / `int32` / `float32` 自动为 4。
- `float64` 自动为 8。
- `raw_hex` 必须手动填写 length。

`bool_uint8` 本质是 1 字节 `uint8_t`，只是按 bool 显示。推荐 STM32 通信协议中使用 `uint8_t` 表示按键状态，不建议直接使用 C/C++ `bool` 作为通信字段。

### enumMap 编辑方式

UI 中使用一列字符串编辑：

```text
0=自动模式;1=遥控模式;2=调试模式
```

保存 JSON 时会转换为：

```json
{
    "enumMap": {
        "0": "自动模式",
        "1": "遥控模式",
        "2": "调试模式"
    }
}
```

## 五、CRC / 校验配置

支持：

- `sum8`
- `xor8`
- `crc8`
- `crc16_modbus`

配置说明：

- `enabled`：是否启用。
- `type`：校验类型。
- `offset`：校验字段在帧内的偏移。
- `length`：校验字段长度。
- `rangeStart`：参与计算的起始 offset。
- `rangeLength`：参与计算的数据长度。

CRC16-Modbus 参数：

- 多项式：`0xA001`
- 初值：`0xFFFF`
- 帧内 CRC 字段按小端比较。

## 六、文本编码说明

原始数据文本显示和文本发送支持：

- UTF-8
- GBK / GB18030
- Local8Bit / 本地编码
- Latin1

Windows 下 GBK 使用 CP936。GB18030 如果转换失败，会回退到 GBK/CP936，并记录日志。

如果中文乱码，请确认 STM32 发送端实际使用的编码，然后在软件中选择相同编码。

## 七、开发环境编译

如果你是开发者，需要从源码编译，环境要求：

- Windows
- Qt 6.11.0
- MinGW 13.1.0
- CMake
- Ninja
- VS Code 可选

默认 Qt 路径：

```text
D:/Qt/6.11.0/mingw_64
D:/Qt/Tools/mingw1310_64/bin/gcc.exe
D:/Qt/Tools/mingw1310_64/bin/g++.exe
```

### Debug / 本地构建

```powershell
cd D:\AAA_QtProjects\SerialParser
cmake -S . -B build_qt -G "Ninja" -DCMAKE_PREFIX_PATH="D:/Qt/6.11.0/mingw_64" -DCMAKE_C_COMPILER="D:/Qt/Tools/mingw1310_64/bin/gcc.exe" -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1310_64/bin/g++.exe" -DCMAKE_RC_COMPILER="D:/Qt/Tools/mingw1310_64/bin/windres.exe"
cmake --build build_qt
```

运行：

```powershell
$env:PATH="D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:PATH"
.\build_qt\SerialParser.exe
```

### Release 发布构建

如果 PowerShell 允许执行脚本：

```powershell
.\scripts\build_release.ps1
.\scripts\deploy_release.ps1
```

如果提示禁止运行脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_release.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_release.ps1
```

部署完成后，发布整个：

```text
build_release
```

不要只发布 `SerialParser.exe`。部署脚本还会额外生成 GitHub Release 推荐上传的压缩包：

```text
dist/SerialParser-Windows-x64.zip
```

这个 zip 包里包含完整的 `build_release` 文件夹。用户下载后解压，双击 `build_release/SerialParser.exe` 即可运行。

## 八、常见问题

### 检测不到串口

检查 USB 转串口驱动、设备管理器中的 COM 口、线缆和供电，然后点击“刷新”。

### 打不开串口

确认串口没有被其他软件占用，串口号、波特率和权限正确。

### 有 RX 原始数据，但字段表不更新

通常是协议配置不匹配。重点检查：

- `frameLength`
- `header`
- `tail`
- `endian`
- `frameMode`
- 字段 offset

### 数据错位

优先使用 `search_header：搜索包头重同步模式`。同时观察统计区：

- `RxBuffer 长度`
- 丢弃字节
- 包头错误
- 包尾错误

### strict_fixed 模式长度错误

该模式要求缓存严格等于固定包长。超过包长会记录长度错误并清空缓存，不会自动搜索包头。

### CRC 错误

检查 CRC 类型、计算范围、offset、length，以及 STM32 和上位机的 CRC 字节序约定。

### 浮点数异常

如果 `float32` / `float64` 解析为 NaN 或 Inf，通常是 offset、字节序、结构体对齐或包长配置错误。

### 中文乱码

尝试切换 UTF-8、GBK / GB18030、Local8Bit。STM32 发送端和上位机解码必须一致。

### 缺少 Qt DLL

不要只复制 `SerialParser.exe`。重新运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_release.ps1
```

然后复制整个 `build_release` 文件夹。

## 九、后续可扩展功能

- 实时曲线。
- CSV 记录。
- 数据回放。
- 反向发送结构化数据到 STM32。
- 协议配置导入/导出模板。
- 多协议自动识别。
