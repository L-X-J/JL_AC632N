# Rider CoreTemp 调试说明

本文档记录 Rider CoreTemp 固件在 AC632N 开发板上的串口调试接线、终端参数、日志格式和常见排障方法。固件构建目标为 `ac632n_rider_core_temp`。

## 1. 调试 UART

当前 UART0 配置如下：

| 项目 | 配置 |
| --- | --- |
| 芯片/板级 | AC632N / bd19 |
| 输出引脚 | `PA0`（应用为 `IO_PORTA_00`，boot 配置为 `PA00`） |
| 输入引脚 | 未启用（`NO_CONFIG_PORT`） |
| 终端波特率 | `115200 baud` |
| 帧格式 | 8 data bits, no parity, 1 stop bit（8N1） |
| 流控 | none |
| 电平 | 使用适配 AC632N 的 3.3 V TTL USB-UART 转接器 |

板级 `debug_uart_init()` 调用预编译 SDK 的 `uart_init()`，不是 M601 的通信接口。M601 在本固件中由 `PB7` 独占 1-Wire 总线，不能把 PB7 当作串口脚位。

Rider 的 boot/OTA 调试和应用日志统一复用 `PA0 / 115200`。应用阶段由 `TCFG_UART0_TX_PORT`、`TCFG_UART0_BAUDRATE` 配置；boot/OTA 阶段由 `CONFIG_UBOOT_DEBUG_PIN`、`CONFIG_UBOOT_DEBUG_BAUD_RATE` 生成 `isd_config.ini` 中的 `UTTX=PA00`、`UTBD=115200`。两个阶段顺序运行，不会同时驱动 PA0。SDK 公共默认值仍可能是 `PA05 / 1000000`，但 Rider 板级配置必须覆盖它。

本目标使用 `DOWNLOAD_MODEL=USB`。`CONFIG_SERIAL_BAUD_RATE`、`CONFIG_LOADER_BAUD_RATE` 等下载通信默认值不属于这根应用日志线，也不需要为了查看日志而修改。

### 烧录版本确认

当前诊断固件版本为 `0.1.6`，唯一真相源是 `include/rider_core_temp.h` 中的 `RIDER_CORE_TEMP_FIRMWARE_VERSION`。板级设置 `RIDER_UART_HEARTBEAT_ONLY=1` 后，`app_main()` 不初始化按钮、J12 诊断或 BLE，只注册 2000 ms 定时器并返回。心跳使用底层 `printf()` 绕过 release 模式下可能产生二进制记录的 `log_print()`；串口会立即输出一次、随后每两秒输出以下 ASCII 日志：

```text
RIDER_HEARTBEAT version=0.1.6
```

`RIDER_HEARTBEAT version=0.1.6` 必须完整且约每两秒重复出现。看到其他版本、只有一次、仍是 `00 00 ...` 二进制流或完全看不到这行，都不能证明板上稳定运行的是本次固件；应重新核对构建产物、烧录结果、PA0 接线和 115200 设置。该诊断镜像故意不启动 BLE，无法扫描设备或读取 `0x2A26` 属于预期行为。

### 接线

```text
USB-UART RX  ->  AC632N PA0 / IO_PORTA_00
USB-UART GND ->  AC632N GND
USB-UART TX  ->  不接（固件未启用 UART0 RX）
```

不要使用 RS-232 电平转接器；确认 USB-UART 转接器支持标准 `115200` 选项，并与开发板共地。

### 波特率分频

SDK 的 UART 初始化使用 `clk_get("uart")` 返回的 24 MHz 时钟，并按以下公式计算分频寄存器：

```text
divider = ((uart_clock + baud / 2) / baud) / 4 - 1
```

对于当前 `115200` 配置：

```text
divider     = 51
actual baud = 24000000 / (4 * (51 + 1)) = 115384 baud
error       = (115384 - 115200) / 115200 = +0.16%
```

这是整数分频带来的正常误差，终端仍选择标准 `115200` 即可。修改 `TCFG_UART0_BAUDRATE` 后必须重新编译并烧录固件。

### 截图中的持续乱码

截图状态栏已经显示 `115200 8N1` 和 `ASCII`，但接收内容仍是随机汉字、方框和符号。这不是 ASCII 开关或结束符设置造成的：ASCII 只决定终端如何显示已经收到的字节，不能修正发送端和接收端的波特率。截图里的 `结束符 None` 和“显示发送数据”也只作用于主机发送方向；当前固件未启用 UART0 RX，不会改变接收结果。

旧镜像存在两种 `1000000 baud` 来源：旧应用从 `PA0` 输出，旧的 boot/OTA 默认配置从 `PA5` 输出。当前 Rider 配置已经把两者统一到 `PA0 / 115200`，因此 USB-UART 的 RX 只能接 `PA0`，不能继续接 `PA5`。用 `115200` 接收任何旧的 1000000-baud 镜像都会把每一帧错误采样成截图中的乱码。

当前源码已经改为 `115200`，但源码修改不会改变板上正在运行的镜像。必须按以下顺序处理：

1. 构建 `ac632n_rider_core_temp`。
2. 将新生成的固件重新烧录到 AC632N。
3. 重新打开串口，选择 `115200 / 8N1 / no flow control / ASCII`。
4. 确认 USB-UART 的 `RX -> PA0`、`GND -> GND`；不要接旧 boot 调试口 `PA5`，不要把 `RX` 接到 USB-UART 自己的 `TX`，也不要把 PB7 当作调试串口。

重新烧录后，boot/OTA 和应用阶段都应在同一个 `115200` 终端中可读；应用日志应出现 `[Info]`、`[SETUP]`、`[RIDER_TEMP]` 或 `[RIDER_BOARD]` 等 ASCII 标签。若仍然是随机字节，先断电重启并确认实际烧录的是本次构建产物，再检查 RX/PA0、共地和 3.3 V TTL 电平；不要通过反复切换终端字符集来排查。

## 2. ASCII 日志约束

Rider 产品日志统一使用 ASCII 文本：

- `log_info()` 的格式字符串、标签和受控的 `%s` 参数只能包含 ASCII 字符。
- 换行使用 `CR/LF`，由现有日志宏追加 `\r\n`。
- M601 scratchpad、BLE payload、广播字节等二进制数据只能调用 `put_buf()`，以十六进制 ASCII 输出；禁止使用 `printf("%s", raw_buffer)`。
- 目标 Makefile 直接编译的 C 源文件由 `tools/test_rider_core_temp_serial.py` 扫描，同时检查原始 UTF-8、`\xNN`、`\uNNNN` 和八进制转义。
- 当前 RCSP/OTA 路径由 `CONFIG_APP_OTA_ENABLE=1` 编译启用；重新构建并烧录后应确认 `AE00/AE01/AE02` 服务可发现。若启用额外 SDK 模块，应重新确认其动态字符串和缓冲区不会直接写入串口。

运行检查：

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools/test_rider_core_temp_serial.py
```

## 3. 日志内容

常用日志标签：

| 标签 | 内容 |
| --- | --- |
| `[RIDER_TEMP]` | M601 presence、转换、scratchpad、CRC、原始温度和采样状态 |
| `[RIDER_ESTIMATOR]` | Sensor/Skin/Core 三条时间线、佩戴状态、历史窗口、模型和心率使用情况 |
| `[RIDER_GATT]` | ATT 读写、CCCD、CORE/HTS 通知、广播和十六进制 payload |
| `[RIDER_BOARD_DIAG]` | J12 按键、LED 诊断和板级状态快照 |
| `[RIDER_BOARD]` | UART/板级启动和 GPIO 清理信息 |

状态快照中的关键字段：

- `ble=0/1/2`：未启动 / 广播 / 已连接。
- `sensor_seq`、`skin_seq`、`core_seq`：分别对应三条数据时间线的来源序号。
- `32767`：对应温度当前无有效值。
- `contact_samples`：可信皮温门控样本数，达到约 30 后才允许可信皮温。
- `typical`：连续 `32~40°C` 的典型贴肤证据计数，不会提前解锁可信皮温。
- `history_s`：Core V1 历史预热秒数，达到约 300 后才进入 `READY`。
- `hr_used=1`：本次 Core 候选实际使用了心率，不仅表示收到过心率。

皮温状态 `0/1/2/3/4/5` 分别为 `NO_DEVICE/NOT_WORN/CONTACT_SETTLING/SKIN_TRUSTED/DETACH_SUSPECTED/STALE`；Core 状态 `0/1/2/3/4` 分别为 `EMPTY/WARMUP/READY/HOLD/INVALID`。`model=v1/0` 是 Skin-only，`model=v1/1` 是 Skin+HR；无效温度不会伪装成 `0°C`。

若持续出现 `status=2`，应优先核对 M601 CRC 多项式和 PB7 上拉/时序；若持续出现 `status=1`，应核对 PB7 连线、外部上拉和传感器供电；若持续出现 `status=4`，应确认传感器贴合位置并按实际佩戴曲线校准区间。

### 标定和模型日志

单 M601 核心估算的离线拟合、连续留出时段验证和 `<=0.5°C` 误差门槛见 [`单M601温度算法研究与验证.md`](../../doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md)。当前板级使用 `EXPERIMENTAL` 是为了采集真实场景数据，不代表模型已经通过验证；完成标定和门槛审查后再切换 `STRICT`，不要把实验候选误称为医疗核心体温。

标定 CSV 至少提供 `timestamp`、可信皮温、参考核心温度和可信状态；可选提供 `heart_rate`、`heart_rate_valid`、参考皮温与 `session_id`。如果包含多次实验，应按完整时段留出，例如：

```sh
python3 tools/rider_core_temp_calibrate.py samples.csv --holdout-session exercise
```

这样可以避免同一运动时段同时出现在拟合集和验证集。滤波状态机、标定工具和串口契约的主机回归统一运行：

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools -p 'test_*.py'
```

典型输出仍是可复制的 ASCII 文本，例如：

```text
[Info]: [RIDER_TEMP] M601 init: port=PB7 convert_delay_ms=15 crc_check=1
[Info]: [RIDER_GATT] Rider connection: handle=0040
```

## 4. 排障顺序

### 输出乱码

1. 终端改为 `115200 / 8N1 / no flow control`。
2. 确认 USB-UART 的 `RX` 接 PA0，而不是旧 boot 口 PA5 或 USB-UART 的 TX。
3. 确认开发板与转接器共地，并确认转接器为 3.3 V TTL 电平。
4. 修改过波特率后重新执行构建和烧录；旧固件仍可能是之前的配置。

### 完全没有输出

1. 检查 `TCFG_UART0_ENABLE` 是否为 `ENABLE_THIS_MOUDLE`。
2. 检查 `TCFG_UART0_TX_PORT` 是否为 `IO_PORTA_00`。
3. 不要等待 UART0 RX 回显；当前固件只启用 TX。
4. 先按 IOKey2 触发一次状态输出，或等待 M601 初始化和 BLE 生命周期日志。

### 看到十六进制行

这是 `put_buf()` 的预期结果，不是串口编码错误。它用于显示原始 scratchpad、BLE 帧和广播字节；每个字节都会被转换为 ASCII 十六进制字符。

## 5. 构建与硬件验证

在仓库根目录执行：

```sh
make ac632n_rider_core_temp
```

烧录后按本说明连接串口，只验收 `RIDER_HEARTBEAT version=0.1.6` 是否约每两秒稳定重复。当前不应出现 `[RIDER_TEMP]`、`[RIDER_GATT]` 或 `[RIDER_BOARD_DIAG]` 运行日志，也不应期待扫描到 BLE；确认应用入口和 UART 正常后再关闭心跳模式并恢复下一层初始化。
