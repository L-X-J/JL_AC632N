# Rider CoreTemp 固件

## 模块职责

`apps/rider_core_temp` 是 AC632N（bd19）上的独立 BLE 外设应用。它只负责 Rider CoreTemp 产品的应用编排、CORE 兼容 GATT 协议、M601 温度传感器适配和产品级板卡配置；不复用 `apps/spp_and_le` 的产品源文件，也不把产品协议放入 `apps/common` 或 `cpu/bd19`。

设备名固定为 `ICXL-RTemp`，只启用 BLE 外设角色：经典蓝牙、SPP、BLE Client、USB、音频、充电和演示按键逻辑均关闭。为兼容标准体温码表和 DURA/COROS，主广播同时包含 Flags、Core 128-bit Service UUID 和 Health Thermometer `0x1809`；主动扫描响应包含 `0x1809`、完整设备名和有效时的 Manufacturer Data。码表作为 BLE Central 会在扫描后自动连接、发现其支持的 GATT UUID，并按标准流程读取/打开 CCCD；固件不要求用户手工操作。CORE-aware 码表可从 `0x2101` 一次取得 Skin 与 Core，标准-only 码表可从 `0x1809/0x2A1C` 取得 Core。连接阶段必须按 GATT UUID 发现服务，不依赖广播名称或服务序号。CORE 的公开 GATT 特征安全权限为 None；Security Manager 只被动响应 Central 发起的无输入/无输出 Just Works，不主动发起配对、不要求 PIN 或人工确认。

## 目录结构

```text
apps/rider_core_temp/
  app_main.c                         应用入口和系统事件转发
  version.c                          版本段兼容入口
  include/                           产品类型、配置和 GATT 句柄
  modules/bt/                        BLE 生命周期、GATT profile 和温度字节编码
  modules/diag/                      AC632N 板载 LED/按键诊断
  modules/main/                      温度采样调度与应用编排
  modules/system/                    产品身份和用户配置钩子
  modules/temp/                      PB7/M601 1-Wire 驱动、滤波与快照转换
  board/bd19/                        AC632N 板级配置和启动适配
  config/                            SDK 库配置入口
```

## 数据流

```text
PB7(IO_PORTB_07)
  -> m601_1wire.c: reset / CONVERT T / READ SCRATCHPAD
  -> rider_temp_filter.c: 中值/EWMA、斜率和佩戴状态
  -> rider_core_estimator.c: 接触/皮肤字段与校准核心影子估算
  -> core_temp_gatt.c + rider_temp_codec.c: CORE/HTS 帧、广播和通知
  -> BLE Central

J12（跳线接到 MCU GPIO）
  -> rider_board_diag.c: 按键去抖、BLE/温度状态采样
  -> LED 状态显示 + UART0 串口诊断
```

应用启动后先清空传感器/估算器状态，再初始化 BLE common 和静态 GATT profile，避免重启时用上一会话快照构造广播；随后维持温度调度。M601 每个采样周期先发 `0xCC 0x44`，等待 15 ms，再发 `0xCC 0xBE` 读取 9 字节 scratchpad。一次转换或读取失败只发布无效快照，不阻塞 BLE 任务。

## AC632N 开发板初步诊断

开发板资料为 `doc/datasheet/AC632N/AC632N开发板/AC632_DevKitBoard V2.0原理图.pdf`；完整板卡和 J12 位置见 [AC632_DevKitBoard V2.0 板卡说明](../../doc/datasheet/AC632N/AC632N开发板/AC632_DevKitBoard_V2.0_板卡说明.md)。原理图中的 J12 是 LED/按键接口，不是已经连接到 AC632N 的固定 GPIO；烧录前必须按下表插跳线。下面以 MCU 排针 J1 为例，J3 上具有相同名称的 GPIO 也可以使用。

| J12 脚位 | 信号 | 默认 MCU 映射 | J1 示例脚位 |
|---:|---|---|---:|
| 1 | ADKEY | 不使用 | 不接 |
| 2 | IOKey2 | PB1 | 9 |
| 3 | IOKey1 | PB0 | 8 |
| 4 | LED1（红） | PB6 | 14 |
| 5 | LED2（绿） | PB5 | 13 |
| 6 | LED3（蓝） | PB4 | 12 |

按键为低电平有效，固件开启内部上拉；LED 由 GPIO 高电平点亮。默认映射刻意避开 PB7（M601 1-Wire）和 PA0（UART0 TX），不要把 J12 任一信号接到 PB7，也不要占用 PA0。若跳线改接其他 GPIO，只修改 `board/bd19/board_ac632n_rider_cfg.h` 中的 `RIDER_BOARD_DIAG_*_PORT` 宏，并重新烧录。

如果实物没有装配 J12 但能找到 LED2 信号焊盘，可直接使用当前固件的既有映射：`PB6 -> LED2` 会让物理绿灯显示逻辑 LED1 的 BLE 状态，`PB5 -> LED2` 会让它显示逻辑 LED2 的 M601 状态。LED2 的 `510R` 限流电阻必须保留；不要把 PB7 或 PA0 接到 LED。具体替代接法和单灯配置见 [AC632_DevKitBoard V2.0 板卡说明](../../doc/datasheet/AC632N/AC632N开发板/AC632_DevKitBoard_V2.0_板卡说明.md)。

LED 行为如下：

| 指示灯 | 含义 |
|---|---|
| LED1 红 | BLE 未启动熄灭；广播时慢闪；连接后常亮 |
| LED2 绿 | M601 预热或快速下降候选时慢闪；稳定且核心模型接收当前样本后常亮；未检测到器件时快闪 |
| LED3 蓝 | CRC 错误慢闪；物理范围/未佩戴双脉冲；IOKey2 打印状态时短暂闪烁 |

IOKey1 按下会依次点亮三色灯完成约 1.2 秒自检；IOKey2 按下会输出一次 BLE/温度快照，不改变 BLE 协议状态。未接跳线的按键应保持释放状态。

## BLE 服务和句柄

profile 使用 `include/core_temp_profile.h` 中的静态 ATT 数据，所有多字节数值按小端传输。

| 服务/特征 | UUID | 属性 | Value handle | CCCD handle |
|---|---|---|---:|---:|
| Core Body Temperature Service | `00002100-5B1E-4347-B07C-97B514DAE121` | service | - | - |
| Core Body Temperature | `00002101-5B1E-4347-B07C-97B514DAE121` | Read, Notify | `0x000E` | `0x000F` |
| CoreTemp Control Point | `00002102-5B1E-4347-B07C-97B514DAE121` | Write, Indicate | `0x0011` | `0x0012` |
| Health Thermometer / Temperature Measurement | `0x1809` / `0x2A1C` | Read, Notify | `0x0015` | `0x0016` |
| Health Thermometer / Temperature Type | `0x1809` / `0x2A1D` | Read | `0x0018` | - |
| Battery / Battery Level | `0x180F` / `0x2A19` | Read, Notify | `0x001B` | `0x001C` |
| Device Information / Manufacturer Name | `0x180A` / `0x2A29` | Read | `0x001F` | - |
| Device Information / Model Number | `0x180A` / `0x2A24` | Read | `0x0021` | - |
| Device Information / Firmware Revision | `0x180A` / `0x2A26` | Read | `0x0023` | - |

CORE 温度通知的核心温度字段是有符号百分之一摄氏度；当前实现同时支持 M601 固定贴肤点的合法 Skin Temperature、算法 Core Temperature、Quality & State 和外部心率字段。通过 `30~45°C` 佩戴窗口和 M601 校验的有效样本，在 `WARMING` 阶段先附带 Skin Temperature，核心字段保持 `0x7FFF`；连续 5 个原始样本落在正常 `35~38°C` 区间，或累计 30 个佩戴窗口内有效样本后进入 `STABLE`。当前 AC632N 板级显式选择 `EXPERIMENTAL`：稳定后将多个有效皮肤样本经过中值/EWMA、斜率、滞后和限速模型得到的 Core 候选，与 Skin 字段同时发送到自定义 CORE，供码表记录和现实场景校准；该候选尚未通过参考核心温度验证，不能当作医疗核心体温。核心滞后状态使用 Q8 小数累积，正向和负向变化使用同一条路径，避免皮温下降时被整数舍入吞掉。若滤波后的皮温快速下降并持续约 5 秒没有回升，进入 `DETACH_SUSPECTED`：核心模型冻结，确认后 Skin/Core 均发送无效值；重新连续 5 个 `35~38°C` 样本后才清空历史并重新预热。标准 HTS 和广播只发送 Core 候选，不承载 Skin 字段。完成留出时段验证后才切换 `STRICT`；`CONTACT_PROXY` 仅保留作旧版本对比。Quality 低 4 位反映滤波后的温度信号质量；没有外部心率时，仅心率关联状态表示“支持但未收到信号”。Control Point 只实现协议文档中约定的外部心率输入 `0x13`，其他操作返回“不支持”。核心无效温度编码为 `0x7FFF`。

标准 Health Thermometer 使用 IEEE 11073 FLOAT，分辨率为 `10^-2 °C`；无效值使用 NaN mantissa `0x007FFFFF`。`2A1C` 提供 **Read + Notify**，兼容 DURA 在订阅前主动读取当前值的流程；Temperature Type 由独立的 `2A1D` 读取。CCCD 写响应完成后，固件在 ATT 可发送窗口推送首帧，后续按约 10 秒节拍发送。自定义 CORE `0x2101` 按采样节拍约 1 Hz 发送：WARMING 带合法 Skin、Core 无效；STABLE 同时带合法 Skin 和实验性算法 Core。profile 保持 CORE 自定义服务在 HTS 之前，但 Central 必须按 UUID 发现服务和特征，不能依赖句柄或服务序号。当前板级 `EXPERIMENTAL` 和经过验证的 `STRICT` 模式都可附带 Manufacturer Specific Data 的 Core 字段；`SHADOW` 模式省略无效 Core，广播温度单位为千分之一摄氏度。为避免旧式广播数据更新产生的停播/重启窗口，未连接期间只在测量可用状态改变时刷新广播；每次断开连接后会在 SDK 自动恢复广播前装入最新温度。当前 M601 直接测量固定贴肤点，Skin 字段是该位置的滤波值；Core 是多个有效皮肤样本经过模型得到的实验候选。平均温度属于码表侧历史统计，未知字段不会被编码为 0 值伪造。算法研究与标定流程见 [`doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md`](../../doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md)。

Battery Level 始终返回协议规定的 `0-100`。当前板级没有独立 fuel-gauge，固件使用 AC632N `AD_CH_VBAT` 的电压估算：默认 `3.30V=0%`、`4.22V=100%`，阈值位于 `board/bd19/board_ac632n_rider_cfg.h`，必须按实际电池和分压校准。该值是电压估算，不代表精确剩余容量。

## PB7 电气和所有权要求

PB7 (`IO_PORTB_07`) 由 M601 1-Wire 总线独占：

- 传感器 DQ 必须连接 PB7，并按 M601 电气要求提供外部上拉；内部上拉只作为默认释放状态，不能替代硬件验证。
- 总线采用开漏式时序，低电平由芯片驱动，释放阶段切换为输入。
- 板级配置已关闭 ADKEY、IOKEY、触摸键、握手、充电和其他可能占用 PB7 的功能。
- 禁止在新模块、板级回调或调试代码中再次把 PB7 配置为触摸、握手、按键、LED 或普通输出。
- 1-Wire 的微秒时序依赖当前关闭低功耗切换；真实硬件上仍需验证上拉电阻、线长、噪声和采样窗口。

## M601 CRC 和温度换算边界

随项目提供的 `doc/ICXL-CoreTemp-Ride/temp_sample` 示例只说明 scratchpad 的 CRC 字段，没有给出多项式。当前驱动采用 Dallas/Maxim CRC-8（多项式反射值 `0x8C`）作为明确的集成假设，并默认开启校验。拿到 M601 正式数据手册后必须确认多项式、初值和温度编码；若不一致，应在本模块内调整并补充测试，不能把当前假设当成已验证事实。

驱动按示例使用 `raw / 256 + 40` 的换算，保存为摄氏百分之一度，并拒绝超出 `-40.00°C` 到 `125.00°C` 的读数。该换算和传感器读数不能直接宣称为真实人体核心体温、皮温或医疗精度数据；协议 Quality 由滤波后的连续性、斜率和佩戴状态给出，核心估算仍必须通过独立参考数据的置信度门控。

### 佩戴有效区间和断报处理

`-40.00°C` 到 `125.00°C` 是 M601 的电气/物理读数范围，不是佩戴判定。应用估算层另用默认 `30.00°C` 到 `45.00°C` 的产品区间过滤环境读数；例如脱离人体后常见的 `23.00°C` 会被标记为 `RIDER_TEMP_STATUS_NOT_WORN`（status=4），不会成为有效核心温度。该区间定义在 `include/rider_core_temp.h`，量产时可按结构和实测校准。

无设备、CRC 错误、物理范围错误、未佩戴或连续约 3 秒没有新序号的样本都会保留为无效快照并进入 `STALE`，并清空当前预热资格；ATT Read 返回协议规定的无效哨兵值。通过佩戴窗口但尚未完成预热的样本只允许进入自定义 CORE 的 Skin Temperature 字段，核心字段仍为 `0x7FFF`，HTS 和温度广播仍等待核心发布门控；板级 `EXPERIMENTAL` 在稳定接触后把滤波后的 M601 Skin 与算法 Core 候选同时送入自定义 CORE，并把 Core 候选送入 HTS/广播。这样码表不会把断报或 `23°C` 当成低温样本参与平均；Skin 和实验 Core 的历史平均都应保留字段语义，Core 不应标注为已验证核心体温。平均温度仍由码表对有效历史样本自行统计。

## 串口诊断日志

板级 UART0 调试输出为 `IO_PORTA_00`（PA0，TX）、`1000000 baud`、8N1。使用 USB-UART 转接器时，将转接器 RX 接 PA0、GND 共地。日志通过现有 `log_info`/`put_buf` 路径输出，重点前缀如下：

- `[RIDER_TEMP]`：M601 presence、转换启动、9 字节 scratchpad、CRC 期望值/收到值、原始温度和状态。
- `[RIDER_ESTIMATOR]`：采样序号、接触/皮肤温度、核心估算有效性、发布模式、质量、置信度、佩戴状态和新鲜度；EXPERIMENTAL 会记录并发送算法 Core 候选，STRICT 还要求核心标定验证通过，CONTACT_PROXY 只记录旧的接触温度代理。
- `[RIDER_GATT]`：ATT 读/写、CCCD、CORE/HTS 读值和通知帧、广播主包与扫描响应原始字节。

串口中 `ble=0/1/2` 分别表示未启动、广播、已连接；`contact_centi=3650` 和 `skin_centi=3650` 表示滤波后的合法贴肤温度 `36.50°C`，`core_est_centi=3650` 表示实验性算法候选，`publish_mode=3 publish_core_centi=3650` 表示当前板级正在发送该 Core 候选。`warmup=有效样本数/35~38°C样本数` 用于判断 LED2 一直慢闪的原因；估算器在温度状态或新鲜度变化时会打印完整快照。M601 原始采样的 `status=0` 表示通过 CRC 和物理范围；估算器快照的 `state=0/1/2/3/4/5` 分别表示无设备、未佩戴、预热、稳定、疑似脱离、陈旧，`freshness=0/1/2` 分别表示不可用、最新、陈旧。无效核心温度统一打印为 `NA(32767)`，不会把未知字段伪装成 `0°C`。若持续出现 `status=2`，应优先核对 M601 CRC 多项式和 PB7 上拉/时序；若持续出现 `status=1`，应核对 PB7 连线、外部上拉和传感器供电；若持续出现 `status=4`，应确认传感器贴合位置并按实际佩戴曲线校准区间。

单 M601 核心估算的离线拟合、连续留出时段验证和 `<=0.5°C` 误差门槛见 [`单M601温度算法研究与验证.md`](../../doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md)。当前板级使用 `EXPERIMENTAL` 是为了采集真实场景数据，不代表模型已经通过验证；完成标定和门槛审查后再切换 `STRICT`，不要把实验候选误称为医疗核心体温。

标定 CSV 如果包含多次实验，应使用 `session_id` 并按完整时段留出，例如
`python3 tools/rider_core_temp_calibrate.py samples.csv --holdout-session exercise`；这会避免同一运动时段同时出现在拟合集和验证集。滤波状态机和标定工具的主机回归统一运行：

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools -p 'test_*.py'
```

## 构建和验证

在仓库根目录执行：

```sh
make ac632n_rider_core_temp
```

### Windows Workbench 构建和烧录

Rider CoreTemp 的 bd19 Makefile 必须保留 `CONFIG_DATA_TRANS_CASE_ENABLE`。该宏让
`cpu/bd19/tools/download.c` 生成标准的 `download/data_trans/download.bat` 调用；如果缺少它，
构建仍可能成功并生成 `app.bin`，但 Windows 后处理脚本只会打包文件，不会真正写入开发板，
Workbench 会显示“设备离线”。

建议使用 Workbench CLI 验证完整链路：

```sh
ac632n-workbench-cli.exe build --project <project-dir> --target ac632n_rider_core_temp
ac632n-workbench-cli.exe flash --project <project-dir>
```

烧录成功的日志应同时包含 `Online flash id`、`Write sector` 或 `Write block`，不能只看“已复制文件”。
如果下载器在一次失败尝试后消失，重新插拔开发板或在 Workbench 点击 Update，确认出现 `USB 大容量存储设备`
后再执行 `flash`。

CLion 代码索引目标为 `ac632n_rider_core_temp_indexing`，固件链接仍由 `apps/rider_core_temp/board/bd19/Makefile` 负责。当前开发机若未安装杰理 q32s 工具链（`clang`、`lto-wrapper`、`lto-ar`），只能完成 Make dry-run、CMake 配置和主机侧语法/索引检查，不能声称固件已完成链接或可烧录。

硬件验收必须至少覆盖：广播服务 UUID 和名称、连接/断连、温度与标准体温 CCCD、Control Point indication（连续写入应返回 busy）、无设备/CRC 错误、`23°C` 脱落、快速下降后 `DETACH_SUSPECTED`、重新贴肤预热、单点尖峰、连续断报/重新佩戴、预热到稳定状态、CORE Flags/皮温字段、EXPERIMENTAL 下 Skin 与算法 Core 同帧、SHADOW 下的核心 `0x7FFF` 和 HTS NaN 编码、PB7 上拉和长线时序，以及 VBAT ADC/电池百分比阈值校准。主机 codec 回归还验证 `36.75°C` 的 `3675` 小端 mantissa 和 HTS `0xFE` exponent。

## 扩展方式和禁止事项

新增产品协议字段先修改 `core_temp_profile.h` 与 `core_temp_gatt.c`，新增传感器状态先扩展 `rider_core_temp.h` 和 `modules/temp`；只有稳定地被多个应用使用时才考虑提取公共组件。任何结构变化都要同步根 `AGENT.md` 和 README。

禁止：

- 引入 `apps/spp_and_le` 的产品实现来“借用” BLE 生命周期或协议逻辑。
- 在 `apps/common`、`cpu/bd19` 或 CMake 中实现 Rider 业务规则。
- 用未验证的温度值伪造质量、电量或医疗结论。
- 在没有 M601 正式 CRC 资料和真实硬件测试的情况下删除校验或宣称时序可靠。
