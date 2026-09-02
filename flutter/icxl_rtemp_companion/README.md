# ICXL RTemp Companion (Flutter)

BLE companion for the JL AC632N **ICXL-RTemp** core-body-temperature patch: scan/connect, parse 41-byte LE notify frames, live charts, CSV export, and offline mock mode.

**Platforms:** Android minSdk 29 · iOS 18.0+

**How to run**

```bash
cd flutter/icxl_rtemp_companion
flutter pub get && flutter run
# Offline UI: enable Mock toggle in the app bar
```

**UUID map:** advertise name `ICXL-RTemp`; notify `00002111-5B1E-4347-B07C-97B514DAE121` (CCCD Notify); Battery `180F` / Level `2A19`. Always match by UUID, never GATT handle ordinals. Android: `requestMtu(247)` **before** `setNotifyValue(true)`.

**CSV:** `timestamp_iso,sensor_raw,skin_filtered,core` (UTC ISO-8601; empty cell for `0x7FFF` nulls).

**Permissions:** Android `BLUETOOTH_SCAN`/`CONNECT` (+ location on some OEMs); iOS `NSBluetoothAlwaysUsageDescription`.

**Frame layout (firmware-confirmed):** offset0 `protocol_version`, 1 `flags`, 2..5 `sequence` uint32 LE; temps 6/10/14; tail 32 `quality`, 33–35 status. Keys: UI「未上报」。

---

# ICXL-RTemp 伴侣 App（Flutter）

杰理 AC632N 核心体温贴 **ICXL-RTemp** 的 BLE 调试伴侣：扫描连接、41 字节 Notify 解析、体温曲线、CSV 导出，以及离线模拟模式。

## 运行步骤

```bash
# 准备 Flutter SDK（本仓库约定路径）

cd flutter/icxl_rtemp_companion
flutter pub get

# Android
flutter run

# 仅分析 / 单测
flutter analyze
flutter test
```

- **Android**：`minSdk = 29`；需授予蓝牙扫描/连接（及部分机型的定位）权限。
- **iOS**：部署目标 **18.0**；首次启动会请求蓝牙权限（`NSBluetoothAlwaysUsageDescription`）。

## UUID 对照表

| 用途 | UUID | 说明 |
|------|------|------|
| 广播名 | `ICXL-RTemp` | 扫描过滤（精确匹配） |
| 温度 Notify 特征 | `00002111-5B1E-4347-B07C-97B514DAE121` | 41 字节 LE 调试快照 |
| Battery Service | `0000180F-0000-1000-8000-00805F9B34FB` | 标准电量服务 |
| Battery Level | `00002A19-0000-1000-8000-00805F9B34FB` | 0–100，连接后读取（并周期性刷新） |

**务必按 UUID 匹配特征，不要依赖 handle 序号。**

## 关键：MTU 247 必须在 CCCD 之前

Android 上 41 字节 Notify 可能因默认 ATT MTU 被拆包。连接流程约定：

1. `connect`（本工程显式传入 `mtu: null`，避免默认 512）
2. `discoverServices`
3. **`requestMtu(247)`**（Android；iOS 系统自行协商）
4. 再对 `0x2111` 调用 **`setNotifyValue(true)`**（写 CCCD）

顺序写在 `lib/ble/ble_controller.dart`，切勿颠倒。

## 帧格式（41 字节，小端）

| 偏移 | 字段 | 类型 | 备注 |
|------|------|------|------|
| 0 | protocol_version | uint8 | 固件确认 |
| 1 | flags | uint8 | 固件确认 |
| 2–5 | sequence | uint32 LE | 固件确认 |
| 6–7 | sensor_raw | int16 LE | 单位 0.01 °C |
| 10–11 | skin_filtered | int16 LE | 单位 0.01 °C |
| 14–15 | published_core | int16 LE | 单位 0.01 °C |
| 32 | quality | uint8 | 固件确认 |
| 33 | sensor_status | uint8 | 固件确认 |
| 34 | temperature_state | uint8 | 固件确认 |
| 35 | core_state | uint8 | 固件确认 |

无效温度哨兵：**`0x7FFF`**（有符号 int16 = **32767**）→ 解析为 `null`，曲线跳过、CSV 留空。

## CSV 格式

表头：

```text
timestamp_iso,sensor_raw,skin_filtered,core
```

- `timestamp_iso`：UTC ISO-8601
- 温度列：保留两位小数；哨兵/`null` 为空字段
- 导出路径：应用文档目录，并通过 `share_plus` 拉起系统分享

## 模拟模式

首页开关「模拟模式」后：

- 每 **200 ms** 用 `FrameParser.buildMockFrame` 生成帧（约 5 Hz）
- 体温在体核附近做正弦 + 轻微噪声
- 约 2% 概率注入 `0x7FFF` 缺口，用于验证曲线断点与 CSV 空值
- 不发起真实 BLE；电量等为演示值

## 按键

按键通道当前 BLE 未上报，UI 固定显示「未上报」。状态字段布局已按固件确认写入 `FrameParser`。

## 工程结构（lib）

```text
lib/
  main.dart
  ble/
    uuids.dart
    frame_parser.dart
    ble_controller.dart
    ble_service.dart          # 兼容 re-export
  models/
    temp_sample.dart
    device_status.dart
  export/
    csv_exporter.dart
  ui/
    home_page.dart
    scan_page.dart
    status_panel.dart
    temp_chart.dart
```
