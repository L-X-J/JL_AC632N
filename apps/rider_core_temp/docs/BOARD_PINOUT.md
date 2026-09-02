# Rider CoreTemp 板级脚位说明

本文记录 `apps/rider_core_temp` 在 **新生产板** 与 **旧 AC632 DevKit（J12）** 上的 GPIO 对照。量产改脚只修改 `board/bd19/board_ac632n_rider_cfg.h` 中的板级宏，不要在业务模块里写死寄存器。

BLE 协议帧布局见 [BLE_PROTOCOL.md](./BLE_PROTOCOL.md)；**本轮脚位适配不改动任何 BLE 字段、UUID 或上报路径。按键不上报 BLE。**

## 新旧脚位对照

| 功能 | 旧板（DevKit / 代码原值） | 新板（产品已定） | 板级宏 |
|---|---|---|---|
| 温感 DQ（M601 / GTM601 1-Wire） | PB7 `IO_PORTB_07` | **PB7**（保持） | `m601_1wire.c` 内 `RIDER_M601_DQ_PORT` |
| KEY1 / 电源键业务 | PB3 `IO_PORTB_03` | **PA1** | `RIDER_BOARD_POWER_KEY_PORT` + `wk_param.port[1]` |
| KEY2 | 旧诊断 IOKey2=PB1；产品 KEY2 原无独立定义 | **PA2**（仅输入初始化，无业务回调） | `RIDER_BOARD_DIAG_IOKEY2_PORT` |
| 红灯 RED | 诊断 LED1=PB6；电源灯=PB5 | **PA7**（红灯兼电源指示） | `RIDER_BOARD_DIAG_LED1_PORT` / `RIDER_BOARD_POWER_LED_PORT` |
| 蓝灯 BLUE | 诊断 LED3=PB4 | **PA8**（传感器诊断） | `RIDER_BOARD_DIAG_LED3_PORT` |
| 独立绿灯 | 诊断 LED2=PB5（与电源灯同脚） | **无**（`NO_CONFIG_PORT`，温度状态挂蓝灯） | `RIDER_BOARD_DIAG_LED2_PORT` |
| 诊断 IOKey1 | PB0 | **禁用**（与 KEY1 同脚冲突，改 `NO_CONFIG_PORT`） | `RIDER_BOARD_DIAG_IOKEY1_PORT` |
| PA0 | 旧板 UART0 TX | **悬空，禁止占用** | 不再作 UART；见下行 |
| 调试 UART0 TX | PA0 | **PB5**（新板未引出，仅开发板看 log） | `TCFG_UART0_TX_PORT = IO_PORTB_05` |
| USB DP/DN | 高阻（USB 关闭） | **不改动** | `board_ac632n_rider.c` 中 USB 清理路径 |

## 冲突与处理

| 冲突项 | 旧配置 | 处理 |
|---|---|---|
| `UART_DB_TX/RX` | PA1 / PA2 | 已改为 `NO_CONFIG_PORT`，避免占 KEY1/KEY2 |
| `TCFG_UART0_TX` | PA0 | 产品要求 PA0 悬空；迁到新板未引出的 **PB5**，开发板排针接 log |
| 硬件 I2C 组 `'C'` | PA7 / PA8 | 保持 `'B'`（PA9/PA10）；**禁止**改 `'C'` 抢灯脚 |
| FLASH SPI CS | PB6 | 新板未用 PB6，可保留 |
| DIAG_IOKEY1 vs KEY1 | 同为 PA1 会双驱动 | 诊断侧禁用 IOKey1；电源业务只走 `RIDER_POWER_KEY` |

## 行为摘要（新板）

- **KEY1=PA1**：承接旧 PB3 电源键路径（2 s 开机确认、运行态短按反馈、长按软关机）；唤醒源仍为 `wk_param.port[1]`。
- **KEY2=PA2**：板级与诊断仅做输入上拉初始化；本轮无打印、无闪灯、不上报 BLE。
- **RED=PA7**：BLE 状态（广播慢闪 / 连接常亮）与电源键仲裁共用；电源键占用期间诊断不改写该脚。
- **BLUE=PA8**：传感器/温度状态（无独立绿灯时挂此灯）；CRC/超范围故障优先。
- **TEMP=PB7**：1-Wire 独占，与旧板一致。
- **PA0**：不驱动、不挂业务。
- **调试串口**：TX=`PB5`（115200），新板未引出，只在开发板上看 log。

## 烧录验证建议

1. 编译烧录 `ac632n_rider_core_temp`。
2. **KEY1**：关机态按住 ≥2 s 应开机，红灯开机提示；运行态长按 ≥2 s 红灯快闪后软关机。
3. **红/蓝灯**：广播时红灯慢闪；连上常亮；贴肤预热/就绪时观察蓝灯节奏；无传感器时蓝灯快闪。
4. **温感**：PB7 接 M601/GTM601，确认采样序号递增、CRC 正常。
5. **KEY2**：确认仅为输入，按下不改变 BLE/灯语/日志业务。
6. **串口 / PA0 / USB**：开发板从 **PB5** 接 USB-TTL（115200）看 log；确认 PA0 无驱动。USB DP/DN 保持关闭配置下的高阻清理。
7. **BLE**：用 Central 核对 UUID/帧长度/字段与改前一致；按键不上报。
