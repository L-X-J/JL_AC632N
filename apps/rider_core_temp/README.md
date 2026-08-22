# Rider CoreTemp 固件

## 模块职责

`apps/rider_core_temp` 是 AC632N（bd19）上的独立 BLE 外设应用。它只负责 Rider CoreTemp 产品的应用编排、CORE 兼容 GATT 协议、M601 温度传感器适配和产品级板卡配置；不复用 `apps/spp_and_le` 的产品源文件，也不把产品协议放入 `apps/common` 或 `cpu/bd19`。

设备名固定为 `ICXL-CoreTemp-Rider`，只启用 BLE 外设角色：经典蓝牙、SPP、BLE Client、安全配对、USB、音频、充电和演示按键逻辑均关闭。

## 目录结构

```text
apps/rider_core_temp/
  app_main.c                         应用入口和系统事件转发
  version.c                          版本段兼容入口
  include/                           产品类型、配置和 GATT 句柄
  modules/bt/                        BLE 生命周期和 GATT profile
  modules/main/                      温度采样调度与应用编排
  modules/system/                    产品身份和用户配置钩子
  modules/temp/                      PB7/M601 1-Wire 驱动与快照转换
  board/bd19/                        AC632N 板级配置和启动适配
  config/                            SDK 库配置入口
```

## 数据流

```text
PB7(IO_PORTB_07)
  -> m601_1wire.c: reset / CONVERT T / READ SCRATCHPAD
  -> rider_core_estimator.c: CRC、范围检查后的协议快照
  -> core_temp_gatt.c: CORE 帧、标准体温帧、广播和通知
  -> BLE Central
```

应用启动后先初始化 BLE common 和静态 GATT profile，再启动温度调度。M601 每个采样周期先发 `0xCC 0x44`，等待 15 ms，再发 `0xCC 0xBE` 读取 9 字节 scratchpad。一次转换或读取失败只发布无效快照，不阻塞 BLE 任务。

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

CORE 温度通知的核心温度是有符号百分之一摄氏度；当前实现只提供核心温度和 Quality & State 字段。没有外部心率时，Quality 使用 `7 (N/A)`，状态表示支持心率但未收到信号；Control Point 只实现协议文档中约定的外部心率输入 `0x13`，其他操作返回“不支持”。无效温度编码为 `0x7FFF`。

标准 Health Thermometer 使用 IEEE 11073 FLOAT，分辨率为 `10^-2 °C`；无效值使用 NaN mantissa `0x007FFFFF`。标准通知按约 10 秒节拍发送，CORE 自定义帧随采样调度发送。广播主包包含 CORE 128-bit 服务 UUID，扫描响应包含 `0x1809` 和完整设备名；只有快照有效时才附带 Manufacturer Specific Data 温度字段。广播温度单位为千分之一摄氏度。

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

驱动按示例使用 `raw / 256 + 40` 的换算，保存为摄氏百分之一度，并拒绝超出 `-40.00°C` 到 `125.00°C` 的读数。该换算和传感器读数不能直接宣称为真实人体核心体温、皮温或医疗精度数据；当前协议质量始终为 N/A，直到产品定义独立的接触/置信度来源。

## 构建和验证

在仓库根目录执行：

```sh
make ac632n_rider_core_temp
```

CLion 代码索引目标为 `ac632n_rider_core_temp_indexing`，固件链接仍由 `apps/rider_core_temp/board/bd19/Makefile` 负责。当前开发机若未安装杰理 q32s 工具链（`clang`、`lto-wrapper`、`lto-ar`），只能完成 Make dry-run、CMake 配置和主机侧语法/索引检查，不能声称固件已完成链接或可烧录。

硬件验收必须至少覆盖：广播服务 UUID 和名称、连接/断连、温度与标准体温 CCCD、Control Point indication（连续写入应返回 busy）、无设备/CRC 错误、PB7 上拉和长线时序，以及 VBAT ADC/电池百分比阈值校准。

## 扩展方式和禁止事项

新增产品协议字段先修改 `core_temp_profile.h` 与 `core_temp_gatt.c`，新增传感器状态先扩展 `rider_core_temp.h` 和 `modules/temp`；只有稳定地被多个应用使用时才考虑提取公共组件。任何结构变化都要同步根 `AGENT.md` 和 README。

禁止：

- 引入 `apps/spp_and_le` 的产品实现来“借用” BLE 生命周期或协议逻辑。
- 在 `apps/common`、`cpu/bd19` 或 CMake 中实现 Rider 业务规则。
- 用未验证的温度值伪造质量、电量或医疗结论。
- 在没有 M601 正式 CRC 资料和真实硬件测试的情况下删除校验或宣称时序可靠。
