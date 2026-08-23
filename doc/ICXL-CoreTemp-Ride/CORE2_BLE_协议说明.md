# CORE 2 核心温度传感器 BLE 协议说明

> 适用对象：CORE / CORE 2 核心体温传感器。本文依据 CORE 官方公开的 BLE GATT 服务规范（CoreTemp Service Specification V2.2）及连接实现说明（Connectivity Implementation Notes V3.3）整理。

## 1. 接入结论

CORE 2 通过 BLE 提供三种获取核心体温的方式：

1. **CORE 自定义 Core Body Temperature Service（推荐）**：可获取核心温度、皮肤温度、数据质量和外部心率；本 Rider 固件暂不输出未经验证的 Heat Strain Index（HSI）。
2. **标准 Health Thermometer Service**：仅需兼容标准体温计协议或仅显示核心温度的码表可使用。
3. **广播 Manufacturer Specific Data**：无需连接，适合低功耗快速显示；仅能取得广播中的核心温度和设备状态。

若码表需要最完整、最稳定的数据，应连接设备并订阅自定义温度特征 `00002101-5B1E-4347-B07C-97B514DAE121`。

## 2. BLE 扫描与设备识别

### 2.1 推荐扫描筛选条件

优先在广播或主动扫描响应（Scan Response）中筛选下列 128-bit Service UUID：

```text
00002100-5B1E-4347-B07C-97B514DAE121
```

这是公开的 **Core Body Temperature Service**。

为兼容旧版本固件，也可同时接受以下旧私有服务 UUID：

```text
00004200-F366-40B2-AC37-70CCE0AA83B1
```

官方 CORE 设备名通常为 `CORE`（可能随设备状态附带后缀），但不建议只通过设备名识别。本项目固件使用产品名 `ICXL-RTemp`，连接端仍应按服务 UUID 识别。

### 2.2 广播内容

官方 CORE 设备常见 Advertisement Data：

| AD Type | 内容 |
|---|---|
| `0x01` | GAP Flags，示例 `0x06` |
| `0x03` | 16-bit UUID 列表，包含 Health Thermometer Service `0x1809` |
| `0x09` | Complete Local Name，官方设备通常为 `CORE` |
| `0xFF` | Manufacturer Specific Data，携带 beacon 核心温度 |

官方设备的自定义 CoreTemp Service 128-bit UUID 可能只出现在主动扫描响应中，因此通用扫描端仍建议采用 **active scan**；本项目固件则将该 UUID 放在主广播包中以兼容扫描后立即连接的 Central。

本项目为兼容 DURA/COROS 的扫描后连接流程，主广播包含 Flags、完整 Core 128-bit Service UUID，以及有效温度时的 Manufacturer Data；主动扫描响应包含 `0x1809` 和完整产品名 `ICXL-RTemp`。扫描端仍应按 `...2100` 服务 UUID 识别，不能依赖广播名称。

## 3. GATT 服务总览

| 服务 | UUID | 用途 |
|---|---|---|
| Core Body Temperature Service | `00002100-5B1E-4347-B07C-97B514DAE121` | CORE 自定义完整温度数据服务 |
| Health Thermometer Service | `0x1809` | 标准体温计服务，仅核心温度 |
| Battery Service | `0x180F` | 电池电量 |
| Device Information Service | `0x180A` | 型号、厂商等设备信息 |
| Generic Access / Attribute | `0x1800` / `0x1801` | BLE 基础服务 |

所有多字节数值均按 **Little Endian（小端序）**传输。

## 4. 自定义 Core Body Temperature Service

### 4.1 特征（Characteristics）

| 特征 | UUID | 属性 | 用途 |
|---|---|---|---|
| Core Body Temperature | `00002101-5B1E-4347-B07C-97B514DAE121` | Read, Notify | 实时温度及附加指标 |
| CoreTemp Control Point | `00002102-5B1E-4347-B07C-97B514DAE121` | Write, Indicate | 心率设备管理、外部心率输入 |

### 4.2 订阅实时温度

向温度特征的 CCCD（UUID `0x2902`）写入：

```text
01 00
```

表示启用 Notification。随后设备会通过 `...2101` 推送温度帧。

关闭通知则写：

```text
00 00
```

### 4.3 温度通知帧格式

温度帧是**变长帧**。核心温度字段始终存在；其余字段是否存在由 Flags 指定。

```text
Offset  长度  字段
0       1     Flags
1       2     Core Body Temperature（SINT16，始终存在）
3       2     Skin Temperature（Flags.bit0 = 1 时有效）
5       2     Core Reserved（Flags.bit1 = 1 时有效）
7       1     Quality & State（Flags.bit2 = 1 时有效）
8       1     Heart Rate（Flags.bit4 = 1 时有效）
9       1     Heat Strain Index（Flags.bit5 = 1 时有效）
```

> 上表为所有可选字段均存在时的偏移。解析时必须按 Flags 从前向后移动游标，不能假设固定长度。

#### Flags 定义

| Bit | 名称 | 0 | 1 |
|---:|---|---|---|
| 0 | Skin Temperature | 无有效值 | 皮肤温度有效 |
| 1 | Core Reserved | 无有效值 | 保留内部数值有效 |
| 2 | Quality and State | 无有效值 | 质量与状态有效 |
| 3 | Temperature Unit | °C | °F |
| 4 | Heart Rate | 无有效值 | 心率有效 |
| 5 | Heat Strain Index | 无有效值 | HSI 有效 |
| 6–7 | RFU | 必须为 0 | — |

#### 字段换算

| 字段 | 类型 | 解析方式 |
|---|---|---|
| Core Body Temperature | `SINT16` LE | 数值 ÷ 100；单位由 Flags.bit3 决定 |
| Skin Temperature | `SINT16` LE | 数值 ÷ 100；单位同核心温度 |
| Core Reserved | `SINT16` LE | 内部保留字段；码表一般可忽略 |
| Heart Rate | `UINT8` | BPM；值 `0` 表示当前没有心率信号 |
| Heat Strain Index | `UINT8` | 数值 ÷ 10，范围约 `0.0`–`25.4` |

核心温度为 `0x7FFF`（十进制 `32767`）时，表示 **Data not available**，不得将其换算为正常温度。通用 Rider 温度模块默认处于影子模式：稳定接触读数可作为皮肤附近温度附在 Skin Temperature 字段，但核心字段保持 `0x7FFF`。当前 AC632N bring-up 板级为兼容旧码表显式选择 `CONTACT_PROXY`，因此稳定接触后核心字段会带滤波后的接触温度；该字段是代理值，不是经过参考数据验证的核心体温。完成留出时段验证后才可启用 `STRICT`。

#### Quality & State 字段

低 4 位是 Data Quality：

| 值 | 含义 |
|---:|---|
| `0` | Invalid |
| `1` | Poor |
| `2` | Fair |
| `3` | Good |
| `4` | Excellent |
| `7` | N/A |

高半字节中，bits 4–5 是心率关联状态：

| bits 5–4 | 含义 |
|---:|---|
| `00` | 不支持心率配对 |
| `01` | 支持心率，但未接收到心率信号 |
| `10` | 支持心率，正在接收心率信号 |
| `11` | N/A |

bits 3、6–7 为保留位，应为 0。

### 4.4 通知示例

完整字段示例帧：

```text
37 19 0F C2 0D 2F 00 11 00 27
```

解析结果：

| 字段 | 值 |
|---|---|
| Flags | `0x37`：皮温、保留值、质量状态、心率、HSI 均有效；单位 °C |
| 核心温度 | `0x0F19` = `3865` → **38.65 °C** |
| 皮肤温度 | `0x0DC2` = `3522` → **35.22 °C** |
| Core Reserved | `0x002F` = `47` |
| Quality & State | `0x11`：Quality = Poor；支持心率但未收到信号 |
| 心率 | `0` BPM |
| HSI | `0x27` = `39` → **3.9** |

## 5. 标准 Health Thermometer Service

如果码表只实现 BLE SIG 标准 Health Thermometer Profile，可使用此服务。平均温度不是 CORE BLE 广播或 `0x2101` 的字段，而是码表基于历史样本自行统计的汇总值；Rider 的单 M601 在稳定接触阶段可作为皮肤附近温度填入自定义 CORE 的 Skin Temperature 字段，但标准 HTS 不承载皮温。通用 `SHADOW`/严格未验证状态下，HTS 核心值保持 NaN；当前板级 `CONTACT_PROXY` 为兼容旧码表发送滤波接触温度代理，码表侧应将其标注为接触温度趋势，不应宣称为已验证核心体温。

| 项目 | UUID | 说明 |
|---|---|---|
| Health Thermometer Service | `0x1809` | 标准服务 |
| Temperature Measurement | `0x2A1C` | Read, Notify；核心温度 |
| Temperature Type | `0x2A1D` | CORE 值为 `0x02`（General） |

### 5.1 Temperature Measurement 数据

`0x2A1C` 使用标准 IEEE 11073-20601 32-bit FLOAT，兼容实现固定为 **5 字节**：

```text
Flags (1 byte) + Temperature value (4 bytes)
```

CORE 的行为：

- Flags.bit0：`0` 表示 Celsius；
- Flags.bit1：`0`，不带时间戳；
- Flags.bit2：`1`，携带 Temperature Type；
- 无有效值时发送 IEEE 11073 NaN：`0x007FFFFF`；
- CORE 官方实现和 Wear OS 示例使用 Notification CCCD；本 Rider 固件在此基础上保留 `Read`，兼容 DURA 在订阅前主动读取当前值的流程。Rider 在 `STRICT` 模式下仅发送已验证核心估算；`SHADOW` 返回 IEEE 11073 NaN；当前板级 `CONTACT_PROXY` 在稳定接触后发送滤波接触温度代理。`2A1D` 单独返回 `0x02`，HTS 当前发送节拍约为 **10 秒**。自定义 `0x2101` 温度特征按采样节拍约 1 Hz 发送。

### 5.2 连接时序和认证

连接后应先完成服务发现，再读取 Battery Level（如果需要），最后向 `2A1C` 的 `0x2902` 写入 `01 00`。每个 GATT 请求都必须先收到响应，再发送下一条请求或写 CCCD；这是 CORE 官方连接说明特别强调的时序要求。本 Rider 的公开特征权限为 None，不主动要求链路加密或绑定；Security Manager 仅被动响应 Central 发起的无输入/无输出 Just Works 请求，因此不需要 PIN 或人工确认。

对于只需在码表显示当前核心温度的场景，此服务通常更容易接入；但它不带皮温、质量、HSI 等信息。

广播布局保持 CORE 兼容形式，连接阶段是否成功取决于 GATT 服务发现、Battery 读取和 CCCD 写入时序，而不是设备名。

## 6. Battery Service

| 服务 | 特征 | 值格式 |
|---|---|---|
| `0x180F` Battery Service | `0x2A19` Battery Level | `UINT8`，范围 `0`–`100`，表示电量百分比 |

本 Rider 固件按上述范围提供 Battery Level。由于目标板没有独立 fuel-gauge，默认以 AC632N `AD_CH_VBAT` 的电压做线性估算（`3.30V=0%`、`4.22V=100%`，超出范围饱和到 0/100）；量产前必须按实际电池、分压网络和校准数据调整阈值。不得把该估算当作精确剩余容量。

## 7. 广播 Beacon 温度格式

无需连接时，可从 AD Type `0xFF` 的 Manufacturer Specific Data 读取核心温度或板级 CONTACT_PROXY 温度代理。

```text
Offset  长度  字段
0       2     Manufacturer ID（uint16 LE，值 0xF60B）
2       1     Manufacturer Data Version
3       1     Status
4       2     Beacon Temperature（uint16 LE，单位 0.001 °C）
```

示例：

```text
0B F6 00 04 B3 91
```

解析：

- Manufacturer ID：`0xF60B`
- Version：`0x00`
- Status：`0x04`（正常测量/擦除状态；低 4 位为传感器状态机状态）
- Beacon Temperature：`0x91B3` = `37299` → **37.299 °C**

广播温度适合快速、低功耗的只读显示；需要注意它没有质量、皮温、HSI 等辅助信息，也不应替代连接后订阅的数据。

## 8. Control Point：心率管理与外部心率输入

Control Point UUID：

```text
00002102-5B1E-4347-B07C-97B514DAE121
```

使用前，先向其 CCCD 写入：

```text
02 00
```

以启用 **Indication**。客户端将请求写入该特征；设备完成操作后用 Indication 返回结果。一次操作未收到返回前，不应发送下一条命令。

结果指示格式：

```text
80 <request_opcode> <result_code> [response_parameters...]
```

| Result Code | 含义 |
|---:|---|
| `0x01` | Success |
| `0x02` | Op Code Not Supported |
| `0x03` | Invalid Parameter |
| `0x04` | Operation Failed |

常用 OpCode：

| OpCode | 命令 | 参数 |
|---:|---|---|
| `0x01` | 清空已配对 ANT+ 心率带 | 无 |
| `0x02` / `0x03` | 添加 / 删除 ANT+ 心率带 | ANT+ ID + Tx type，共 3 字节 |
| `0x04` | 获取已配对 ANT+ 心率带数量 | 无 |
| `0x05` | 获取指定已配对 ANT+ 心率带 | index（1 字节） |
| `0x06` / `0x07` | 添加 / 删除 BLE 心率带 | BLE 地址（6 字节） |
| `0x08` | 获取已配对 BLE 心率带数量 | 无 |
| `0x09` | 获取已配对 BLE 心率带名称和状态 | index（1 字节） |
| `0x0A` | 扫描 ANT+ 心率带 | `FF` 扫描；`FE` 就近配对 |
| `0x0B` / `0x0C` | 获取扫描到的 ANT+ 心率带数量 / 指定 ID | 无 / index |
| `0x0D` | 扫描 BLE 心率带 | `FF` 扫描；`FE` 就近配对 |
| `0x0E` | 获取扫描到的 BLE 心率带数量 | 无 |
| `0x0F` / `0x10` | 获取扫描到的 BLE 心率带名称 / MAC | index |
| `0x11` | 清空已配对 BLE 心率带 | 无 |
| `0x12` | 获取已配对 BLE 心率带 MAC 和状态 | index |
| `0x13` | 直接输入外部心率 | 1 字节 BPM；空参数表示停止外部输入 |

外部心率输入例子：向 Control Point 写入 `13 A8`，表示将外部心率设为 `168 BPM`。若同一值持续超过约 15 秒未更新，设备可能降低核心温度输出的质量等级。

## 9. 码表实现建议

1. 主动扫描，按 `...2100` 服务 UUID 识别设备；同时兼容旧 UUID `...4200`。
2. 连接后发现服务与特征，优先订阅 `...2101` 的 Notification。
3. 每帧按 Flags 动态解析，特别处理 `0x7FFF` 核心温度无效值。
4. 显示温度时优先采用核心温度；皮温、HSI 和质量字段仅在对应 flag 有效时显示。
5. 如果只支持 BLE 标准体温服务，订阅 `0x1809 / 0x2A1C` 即可。
6. 仅需要粗略实时数值、且不希望保持连接时，解析厂商广播数据中的 `Beacon Temperature`。
7. 断连、空帧、RFU bits 非零或长度不足的帧应丢弃并记录诊断信息，不应将异常数据展示为体温。

本项目 Rider 固件另有产品侧约束：M601 通过 CRC 和 `-40~125°C` 物理范围后，还必须落在默认 `30~45°C` 佩戴区间；脱离人体产生的 `23°C` 等环境读数会按未佩戴处理。稳定接触后，自定义 CORE 可以带皮肤字段；通用影子模式的核心字段为 `0x7FFF`，当前板级 `CONTACT_PROXY` 则把滤波接触温度映射到 CORE/HTS/广播，严格模式才会发送通过标定验证的核心估算。无效样本在 ATT Read 中使用 CORE/HTS 协议规定的 `0x7FFF`/IEEE FLOAT NaN 哨兵，未稳定接触时不会通过已订阅的温度 Notification 反复发送。因此码表侧应对断报保持上一有效值或暂停平均统计，不能把缺失窗口按 `0°C` 或低温样本参与平均。平均温度不是 Rider 固件上报字段，仍由码表基于有效历史样本自行计算；CONTACT_PROXY 的平均只代表接触温度趋势。

## 10. 参考资料

- CORE 官方 GitHub：<https://github.com/CoreBodyTemp/CoreBodyTemp>
- `CoreTemp BLE Service Specification.pdf`，V2.2，2024-07-04
- `CORE Connectivity Implementation Notes.pdf`，V3.3，仓库最新公开版本
