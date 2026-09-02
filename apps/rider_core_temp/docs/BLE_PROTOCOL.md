# Rider CoreTemp BLE 协议（App 交接）

来源：`apps/rider_core_temp`（分支 `dev`），固件设备名固定为 `ICXL-RTemp`。多字节均为小端。Central 必须按 UUID 发现服务/特征，不要依赖句柄序号。

## 连接注意

- 调试快照帧固定 **41 字节**。默认 ATT MTU 不够：连上后先 `requestMtu`（≥44，建议 247），再写 CCCD 打开 Notify。
- 温度无效哨兵：`0x7FFF`（int16）。
- 板级按键（PB3 电源 / J12 IOKey）**目前不走 BLE**，App 状态面板先占位。
- 电量 `0x180F / 0x2A19`：当前为 AC632N `AD_CH_VBAT` 电压估算（非独立 fuel-gauge），可当占位。

## GATT 一览

| 服务 / 特征 | UUID | 属性 |
|---|---|---|
| Core Body Temperature Service | `00002100-5B1E-4347-B07C-97B514DAE121` | Primary |
| Core Body Temperature | `00002101-5B1E-4347-B07C-97B514DAE121` | Read, Notify |
| CoreTemp Control Point | `00002102-5B1E-4347-B07C-97B514DAE121` | Write, Indicate |
| Health Thermometer | `0x1809` | Primary |
| Temperature Measurement | `0x2A1C` | Read, Notify |
| Temperature Type | `0x2A1D` | Read |
| Battery Service | `0x180F` | Primary |
| Battery Level | `0x2A19` | Read, Notify |
| Device Information | `0x180A` | Primary |
| Manufacturer / Model / Firmware | `0x2A29` / `0x2A24` / `0x2A26` | Read |
| Rider Debug Snapshot Service | `00002110-5B1E-4347-B07C-97B514DAE121` | Primary |
| Rider Debug Snapshot | `00002111-5B1E-4347-B07C-97B514DAE121` | Read, Notify |

句柄仅供固件对照（可变）：`0x2101` value=`0x000E` CCCD=`0x000F`；`0x2111` value=`0x0026` CCCD=`0x0027`；Battery value=`0x001B` CCCD=`0x001C`。

## 调试快照 `0x2111`（推荐 App 主通道）

订阅 CCCD 后约 **200 ms** 推送一帧（采样仍约 1 Hz，同秒可能重复 `sequence`）。帧长 **41**。

| 偏移 | 长度 | 字段 |
|---:|---:|---|
| 0 | 1 | `protocol_version`（当前 `1`） |
| 1 | 1 | `flags` |
| 2 | 4 | `sequence` uint32 |
| 6 | 2 | `sensor_temperature_centi` int16（原始 Sensor） |
| 8 | 2 | `contact_temperature_centi` int16 |
| 10 | 2 | `skin_temperature_centi` int16（滤波可信皮温） |
| 12 | 2 | `core_estimate_centi` int16（算法候选） |
| 14 | 2 | `published_core_centi` int16（对外发布核心温） |
| 16 | 2 | `slope_centi_per_min` |
| 18 | 2 | `skin_baseline_centi` |
| 20 | 2 | `skin_delta_1m_centi` |
| 22 | 2 | `skin_delta_5m_centi` |
| 24 | 2 | `heart_rate_delta_1m` |
| 26 | 2 | `core_history_seconds` uint16 |
| 28 | 2 | `contact_samples` uint16 |
| 30 | 1 | `typical_samples` |
| 31 | 1 | `heart_rate` |
| 32 | 1 | `quality` |
| 33 | 1 | `sensor_status` |
| 34 | 1 | `temperature_state` |
| 35 | 1 | `core_state` |
| 36 | 1 | `freshness` |
| 37 | 1 | `confidence` |
| 38 | 1 | `model_mode` |
| 39 | 1 | `model_version` |
| 40 | 1 | `heart_rate_used` |

### flags 位

| bit | 含义 |
|---:|---|
| 0x01 | Sensor 有效 |
| 0x02 | Contact 有效 |
| 0x04 | Skin 有效 |
| 0x08 | Core estimate 有效 |
| 0x10 | Published core 有效 |
| 0x20 | Heart rate 有效 |
| 0x40 | Core verified |
| 0x80 | Data stale |

### 三曲线建议映射

- 原始：`sensor_temperature_centi` @6
- 滤波：`skin_temperature_centi` @10（也可用 contact @8 做对比）
- 核心：`published_core_centi` @14

温度单位：有符号百分之一摄氏度，例 `3675` → `36.75°C`。

### 状态枚举（串口/调试一致）

- 皮温 `temperature_state`：`0` NO_DEVICE / `1` NOT_WORN / `2` CONTACT_SETTLING / `3` SKIN_TRUSTED / `4` DETACH_SUSPECTED / `5` STALE
- Core `core_state`：`0` EMPTY / `1` WARMUP / `2` READY / `3` HOLD / `4` INVALID

## 兼容通道摘要

- **`0x2101`**：CORE 兼容帧，约 1 Hz（Skin 可信后）；Core 预热期可为 `0x7FFF`。
- **HTS `0x2A1C`**：IEEE 11073 FLOAT，`10^-2 °C`；无效 mantissa `0x007FFFFF`；Core READY 后约 10 s 节拍。
- **Control Point `0x2102`**：当前主要实现外部心率 `0x13`；其它操作返回不支持。

更细的算法门控与板级说明见同目录上层 `README.md` / `DEBUG.md`。
