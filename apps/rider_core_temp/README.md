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
  modules/power/                     PB3 电源按键、PB5 电源灯状态机
  modules/main/                      温度采样调度与应用编排
  modules/system/                    产品身份和用户配置钩子
  modules/temp/                      PB7/M601 1-Wire 驱动、滤波与快照转换
  board/bd19/                        AC632N 板级配置和启动适配
  config/                            SDK 库配置入口
  docs/BLE_PROTOCOL.md               App/Central BLE 协议交接
  docs/BOARD_PINOUT.md               新旧板脚位对照与验证
```

## 新板脚位（产品已定）

完整新旧对照、冲突处理与烧录验证见 [`docs/BOARD_PINOUT.md`](docs/BOARD_PINOUT.md)。摘要：

| 功能 | 新板脚位 | 说明 |
|---|---|---|
| 温感 M601/GTM601 | **PB7** | 与旧板相同，1-Wire 独占 |
| KEY1（电源键业务） | **PA1** | 承接旧 PB3；`wk_param.port[1]` 同步 |
| KEY2 | **PA2** | 仅输入初始化，本轮无业务回调、不上报 BLE |
| 红灯 / 电源指示 | **PA7** | 诊断红灯与电源灯同脚，继续电源仲裁 |
| 蓝灯 | **PA8** | 传感器/温度诊断（无独立绿灯） |
| PA0 | **悬空** | 禁止占用；`TCFG_UART0_TX` 已迁到 `PB5`（新板未引出） |
| USB DP/DN | 不改 | 保持关闭配置下的原有处理 |

旧 DevKit J12 跳线说明见下文「AC632N 开发板初步诊断」，仅作历史参考；量产以本表与 `board_ac632n_rider_cfg.h` 为准。

## 数据流

```text
PB7(IO_PORTB_07)
  -> m601_1wire.c: reset / CONVERT T / READ SCRATCHPAD
  -> rider_temp_filter.c: 中值/EWMA、斜率、接触资格和可信皮温
  -> rider_core_estimator.c: Sensor / Trusted Skin / Experimental Core 三条时间线
       -> 30 秒可信皮温门控 -> 5 分钟历史特征 -> Core V1 Q8 估算
  -> core_temp_gatt.c + rider_temp_codec.c: CORE/HTS 帧、广播和通知
  -> BLE Central

J12（跳线接到 MCU GPIO）
  -> rider_board_diag.c: 按键去抖、BLE/温度状态采样
  -> LED 状态显示 + UART0 串口诊断

KEY1=PA1（低电平有效，内部上拉；旧板为 PB3）
  -> board_ac632n_rider.c: GPIO 电平、wk_param.port[1] 唤醒结果
  -> rider_power_key.c: 开机确认、运行按键、软关机前置清理
  -> PA7 红灯（高电平点亮）: 电源提示/按键反馈，结束后交还 BLE/诊断
KEY2=PA2：仅输入初始化，本轮无业务回调
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

PB3/PB5 属于产品电源接口；若生产板需要变更这两个端口，必须同时修改 `RIDER_BOARD_POWER_KEY_*`、`RIDER_BOARD_POWER_LED_PORT` 和 `wk_param.port[1]` 的板级契约，并重新核对低功耗 GPIO 保护。不要用 J12 诊断宏单独覆盖电源映射。

如果实物没有装配 J12 但能找到 LED2 信号焊盘，可直接使用当前固件的既有映射：`PB6 -> LED2` 会让物理绿灯显示逻辑 LED1 的 BLE 状态，`PB5 -> LED2` 会让它显示逻辑 LED2 的 M601 状态。LED2 的 `510R` 限流电阻必须保留；不要把 PB7 或 PA0 接到 LED。具体替代接法和单灯配置见 [AC632_DevKitBoard V2.0 板卡说明](../../doc/datasheet/AC632N/AC632N开发板/AC632_DevKitBoard_V2.0_板卡说明.md)。

LED 行为如下：

| 指示灯 | 含义 |
|---|---|
| LED1 红 | BLE 未启动熄灭；广播时慢闪；连接后常亮 |
| LED2 绿（PB5） | 电源开机提示、运行按键反馈和关机快闪优先；空闲时显示接触/Core 状态，未检测到器件时快闪 |
| LED3 蓝 | CRC 错误慢闪；物理范围/未佩戴双脉冲；IOKey2 打印状态时短暂闪烁 |

IOKey1 按下会依次点亮三色灯完成约 1.2 秒自检；IOKey2 按下会输出一次 BLE/温度快照，不改变 BLE 协议状态。未接跳线的按键应保持释放状态。

## 电源键与指示灯（新板 KEY1=PA1，红灯=PA7）

> 旧文档曾写 PB3/PB5；代码宏已迁到 PA1/PA7，详见 [BOARD_PINOUT.md](docs/BOARD_PINOUT.md)。

Rider 的 PB3/PB5 是产品电源接口，不是从附加 AB202X 文档复制的硬件映射。PB3 由外部按键接地，按下为低电平；板级代码开启内部上拉，并把它登记为 BD19 `wk_param.port[1]` 的下降沿唤醒源。PB5 为高电平点亮，当前也对应 J12 的 LED2，因此温度诊断仍可使用同一物理灯，但必须经过电源状态机的仲裁。

| 信号 | MCU 端口 | 所有权和行为 |
|---|---|---|
| Rider 电源按键 | PB3 | 低电平有效；关机时下降沿唤醒，运行时由 `modules/power/rider_power_key.c` 每 5 ms 扫描 |
| Rider 电源指示灯 | PB5 | 高电平点亮；开机提示、按键反馈和关机快闪期间由电源状态机独占 |
| J12 LED1/LED3 | PB6/PB4 | 继续显示 BLE 或传感器诊断，不受 PB5 电源灯仲裁影响 |
| J12 IOKey1/IOKey2 | PB0/PB1 | 继续执行 LED 自检和状态输出 |
| M601 1-Wire | PB7 | 继续由 `modules/temp/m601_1wire.c` 独占，不参与电源逻辑 |
| UART0 TX | PA0 | 继续输出串口诊断，不参与电源逻辑 |

行为和状态边界如下：

- 关机唤醒后，PB3 必须持续按下 2 秒；不足 2 秒松开时 PB5 保持熄灭并回到软关机。
- 若 SDK 的唤醒寄存器在复位后仍保留 PB3 标志、但 PB3 实际已经释放，固件按普通上电继续启动，不会被陈旧标志再次关机。
- 确认开机后 PB5 长亮 2 秒；复位、上电和其他非 PB3 唤醒也执行一次同样的 2 秒开机提示。
- 触发开机确认的原始长按会被隔离，提示结束前后都不会直接转成关机；只有松开 PB3 后，下一次持续按住 2 秒才会进入关机模式。
- 运行态短按只在按下期间点亮 PB5，松开后释放覆盖并立即恢复当前温度诊断显示。
- 运行态持续按住 2 秒后锁定关机模式，PB5 执行 3 次“灭 100 ms、亮 100 ms”，共 6 个阶段；快闪期间按键不能重新点灯或重复触发，随后先停止 BLE/GATT、M601 调度和诊断定时器，再调用 `power_set_soft_poweroff()`。

PB5 的覆盖是临时优先级规则：`rider_board_diag_power_led_claim()` 申请后，诊断定时器仍可更新 PB6/PB4，但不得写 PB5；`rider_board_diag_power_led_release()` 释放后由诊断模块重新渲染温度状态。现有 GATT UUID、帧长度和字段不因按键/指示灯逻辑改变。

该节的行为及时序参考附加 AB202X 文档；附加文档不属于本项目板级配置，不能改变 PB7、PA0 或 J12 的现有映射。

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
| Rider Debug Snapshot Service | `00002110-5B1E-4347-B07C-97B514DAE121` | service | - | - |
| Rider Debug Snapshot | `00002111-5B1E-4347-B07C-97B514DAE121` | Read, Notify | `0x0026` | `0x0027` |

### 调试快照帧（0x2111）

订阅 `0x0027` 后，固件启动独立的 200 ms 定时器，向 `0x0026` 发送固定 41 字节帧；退订、断开或 GATT 退出会停止定时器。200 ms 是最新快照的发送节拍，不会把 M601 的 1 秒采样任务改成 5 Hz，因此同一秒内可能出现相同 `sequence`。小端字段和偏移如下：

| 偏移 | 长度 | 字段 |
|---:|---:|---|
| 0 | 1 | protocol_version，当前为 `1` |
| 1 | 1 | flags：Sensor/Contact/Skin/CoreCandidate/PublishedCore/HR/Verified/Stale 位 |
| 2 | 4 | sequence，uint32 LE |
| 6 | 2 | sensor_temperature_centi，int16 LE |
| 8 | 2 | contact_temperature_centi，int16 LE |
| 10 | 2 | skin_temperature_centi，int16 LE |
| 12 | 2 | core_estimate_centi，int16 LE |
| 14 | 2 | published_core_centi，int16 LE |
| 16 | 2 | slope_centi_per_min，int16 LE |
| 18 | 2 | skin_baseline_centi，int16 LE |
| 20 | 2 | skin_delta_1m_centi，int16 LE |
| 22 | 2 | skin_delta_5m_centi，int16 LE |
| 24 | 2 | heart_rate_delta_1m，int16 LE |
| 26 | 2 | core_history_seconds，uint16 LE |
| 28 | 2 | contact_samples，uint16 LE |
| 30 | 1 | typical_samples |
| 31 | 1 | heart_rate |
| 32..40 | 9 | quality、sensor_status、temperature_state、core_state、freshness、confidence、model_mode、model_version、heart_rate_used |

温度字段当前值无效时统一为 `0x7FFF`；`core_estimate_centi` 保留算法候选，`published_core_centi` 由板级 `RIDER_CORE_TEMP_PUBLISH_MODE` 决定，便于记录实验值而不污染兼容 CORE 特征。服务 UUID 不放进现有 31 字节广播，Central 应按设备名连接后按 UUID 发现服务。

## Flutter 配套 / 调试 App

配套 App 工程位于 [`flutter/icxl_rtemp_companion`](../../flutter/icxl_rtemp_companion/)。扫描设备名 `ICXL-RTemp`，连上后先抬 MTU 再订阅 `0x2111`，展示传感器/按键/电量状态、三条温度曲线（原始 / 滤波皮温 / 核心）并支持 CSV 导出。协议细节以 [`docs/BLE_PROTOCOL.md`](docs/BLE_PROTOCOL.md) 为准。App 只解码协议字段，不重新实现温度滤波或 Core 算法。

### OTA 边界

当前板级 `board_ac632n_rider_global_build_cfg.h` 将 `CONFIG_APP_OTA_ENABLE` 设为 `1`，双备份仍为 `0`。因此 Rider profile 会编译 RCSP `AE00/AE01/AE02` 服务，并通过 `RCSP_BTMATE_EN`、`RCSP_UPDATE_EN` 打开 BLE OTA 命令路径；单备份模式的 `EXIF` 区域由后处理配置生成。升级前仍需使用兼容的 RCSP OTA 客户端，并在硬件上验证认证、文件信息、分块 ACK、校验和重启流程。Flutter 配套 App 当前不做 RCSP OTA；AE00 通道探测与 E1-E7 传输状态机尚未认证，不会发送固件。

本轮固件的 Firmware Revision 为 `0.1.8`，Core 模型版本为 `1`。产品固件版本的唯一真相源是 `include/rider_core_temp.h` 中的 `RIDER_CORE_TEMP_FIRMWARE_VERSION`。`app_main()` 初始化应用状态、J12 板级诊断并启动 BLE；当前 `RIDER_POWER_KEY_ENABLE=1`，PB3 电源键会执行两秒开机确认、运行态扫描和长按软关机。串口调试使用 PB5 / 115200（新板 PA0 悬空），启动日志直接引用同一版本宏。每次人体/骑行实验都应把设备地址、固件版本、模型版本和标定 header 的来源与码表导出文件一起登记；仅凭温度列无法判断数据使用了哪套门控和系数。

CORE 温度通知的 Core 和 Skin 都是有符号百分之一摄氏度。当前实现把三个阶段分开：原始 Sensor 样本先通过 CRC、物理范围和 `30~45°C` 佩戴保护窗；5 点中值和 EWMA 形成接触温度；连续约 30 秒有效接触后，才把该固定胸带位置的滤波值标记为 Trusted Skin。`32~40°C` 是典型贴肤证据和重新佩戴辅助条件，不是可信皮温捷径，也不是人体核心温度范围。可信皮温建立后，若滤波值持续低于 `31.50°C` 约 60 秒，则锁存为脱落并清空 Skin/Core 资格，防止离体后稳定在 `30.3°C` 一类环境温度时继续上报旧值；重新回到典型贴肤温度后仍须重新完成预热。可信皮温建立后，自定义 CORE 约 1 Hz 附带 Skin，Core 在模型预热阶段明确为 `0x7FFF`。

当前 AC632N 板级显式选择 `EXPERIMENTAL`。Core V1 保存 5 秒间隔、覆盖约 5 分钟的可信皮温历史，并使用本次佩戴基线、1/5 分钟皮温变化和可选外部心率计算 Q8 实验候选；约 5 分钟历史满足后才进入独立的 `READY` 状态，随后按采样节拍更新 Core。输出超出 `35~42°C` 时整帧 Core 判为无效而不是钳制到边界，每秒变化另受 `0.25°C` 保护。滤波皮温快速下降时，Trusted Skin 和 Core 立即暂停；确认脱落后必须连续 5 个 `32~40°C` 样本且回到脱落前峰值 `0.75°C` 范围内，才开始新的 30 秒可信皮温与 5 分钟 Core 预热。标准 HTS 和广播只承载 Core，不承载 Skin；完成完整骑行 Session 留出验证后才可切换 `STRICT`，`CONTACT_PROXY` 只保留用于旧版本对比。

Quality 低 4 位反映当前皮温信号质量。Quality & State 高半字节按 CORE V2.2 正式取值编码：`0x10` 表示支持心率但当前未接收，`0x20` 表示正在接收；截图资料中“加 16 表示使用心率”按旧版/简化说明记录，不用来覆盖 V2.2 位值。诊断元数据中的 `heart_rate_used` 另行表示本次 Core V1 计算是否实际采用心率，避免把“已收到”和“已参与模型”混为一谈。Control Point 只实现外部心率输入 `0x13`，其他操作返回“不支持”。Core 无效值统一编码为 `0x7FFF`。

标准 Health Thermometer 使用 IEEE 11073 FLOAT，分辨率为 `10^-2 °C`；无效值使用 NaN mantissa `0x007FFFFF`。`2A1C` 提供 **Read + Notify**，兼容 DURA 在订阅前主动读取当前值的流程；Temperature Type 由独立的 `2A1D` 读取。CCCD 写响应完成后，固件在 ATT 可发送窗口推送首个可用值。自定义 CORE `0x2101` 在 `SKIN_TRUSTED` 后约 1 Hz 发送：Core V1 `WARMUP` 时带可信 Skin 和无效 Core，`READY` 时同时带可信 Skin 和实验性 Core。HTS 只在 Core `READY` 后按约 10 秒节拍发送。profile 保持 CORE 自定义服务在 HTS 之前，但 Central 必须按 UUID 发现服务和特征，不能依赖句柄或服务序号。当前板级 `EXPERIMENTAL` 和经过验证的 `STRICT` 模式都可附带 Manufacturer Specific Data 的 Core 字段；`SHADOW` 模式省略无效 Core，广播温度单位为千分之一摄氏度。为避免旧式广播数据更新产生的停播/重启窗口，未连接期间只在 Core 可用状态改变时刷新广播；每次断开连接后会在 SDK 自动恢复广播前装入最新温度。平均温度属于码表侧历史统计，未知字段不会被编码为 0 值伪造。算法研究与标定流程见 [`doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md`](../../doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md)。

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

无设备、CRC 错误、物理范围错误、低温持续脱落、未佩戴或连续约 3 秒没有新序号的样本都会形成无效快照，清空当前皮温/Core episode；ATT Read 返回协议规定的无效哨兵值。`CONTACT_SETTLING` 期间只保留内部接触诊断，不发送 Skin Notification；约 30 秒后进入 `SKIN_TRUSTED`，自定义 CORE 才开始发送可信 Skin。Core V1 继续预热约 5 分钟，此时自定义帧的 Core 为 `0x7FFF`，HTS 和温度广播仍等待 Core 发布门控；进入 `READY` 后，板级 `EXPERIMENTAL` 把 Skin 与 Core 候选同时送入自定义 CORE，并把 Core 候选送入 HTS/广播。这样码表不会把断报、未确认接触、持续 `30.3°C` 或 `23°C` 当成低温样本；Core 必须保留“实验值”语义，平均值仍由码表只对相应有效历史样本统计。

## 串口诊断日志

boot/OTA 调试和应用 UART0 统一从 `PB5` 输出（新板未引出该脚，仅开发板接线），终端固定使用 `115200 / 8N1 / no flow control / ASCII`；源码或配置修改后必须重新编译并烧录，旧镜像不会自动改变。串口脚位、波特率分频、ASCII 日志约束、字段说明和排障顺序见独立文档：[Rider CoreTemp 调试说明](./DEBUG.md)。

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

硬件验收必须至少覆盖：广播服务 UUID 和名称、连接/断连、温度与标准体温 CCCD、Control Point indication（连续写入应返回 busy）、无设备/CRC 错误、`23°C` 脱落、快速下降后 `DETACH_SUSPECTED`、重新贴肤、5 样本不能提前解锁、30 秒可信皮温边界、5 分钟 Core `READY` 边界、单点尖峰、连续断报、CORE Flags/皮温字段、Skin-only 与 Skin+HR 模式、心率超时回退、EXPERIMENTAL 下 Skin 与算法 Core 同帧、越界 Core 失效而非钳制、SHADOW 下的核心 `0x7FFF` 和 HTS NaN 编码、PB7 上拉和长线时序，以及 VBAT ADC/电池百分比阈值校准。主机 codec 回归还验证 `36.75°C` 的 `3675` 小端 mantissa 和 HTS `0xFE` exponent。

## 扩展方式和禁止事项

新增产品协议字段先修改 `core_temp_profile.h` 与 `core_temp_gatt.c`，新增传感器状态先扩展 `rider_core_temp.h` 和 `modules/temp`；只有稳定地被多个应用使用时才考虑提取公共组件。任何结构变化都要同步根 `AGENT.md` 和 README。

禁止：

- 引入 `apps/spp_and_le` 的产品实现来“借用” BLE 生命周期或协议逻辑。
- 在 `apps/common`、`cpu/bd19` 或 CMake 中实现 Rider 业务规则。
- 用未验证的温度值伪造质量、电量或医疗结论。
- 在没有 M601 正式 CRC 资料和真实硬件测试的情况下删除校验或宣称时序可靠。
