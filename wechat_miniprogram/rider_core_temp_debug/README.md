# Rider CoreTemp 微信小程序调试台

## 用途

这是一个面向固件 bring-up 和骑行实验记录的微信小程序，不是面向普通用户的产品 App。它扫描广播名包含 `RTemp` 的设备，连接后按 UUID 发现 `00002110-5B1E-4347-B07C-97B514DAE121`，订阅 `00002111-5B1E-4347-B07C-97B514DAE121`，接收固件每 200 ms 发送的 41 字节固定调试帧。

页面直接进入设备操作工作台：扫描/连接、Sensor/Contact/Skin/Core 指标、质量与状态、序号和模型元数据、实时记录、原始十六进制和 CSV 导出。记录最多保留 5000 条，CSV 带 UTF-8 BOM，适合在 Excel 中继续分析。

## 目录结构

```text
rider_core_temp_debug/
  app.js / app.json / app.wxss       小程序入口和全局样式
  pages/index/                       BLE 操作、快照面板和记录工作区
  services/ble.js                    扫描、连接、按 UUID 发现和订阅
  services/rcspOta.js                RCSP AE00/AE01/AE02 探测与待认证 framing helper
  types/protocol.js                  UUID、版本和 flags 常量
  utils/debugCodec.js                41 字节帧解码和十六进制
  utils/csv.js                       记录 CSV 序列化
```

## 运行

1. 用微信开发者工具导入本目录，真机调试时打开蓝牙和定位权限。
2. 烧录包含 `0x2110/0x2111` 的 Rider 固件后点击“扫描设备”。服务 UUID 没有塞进旧版 31 字节广播，页面先按名称筛选，再连接和发现服务。
3. 点击设备连接；订阅成功后固件会每 200 ms 发一帧，传感器本身仍按 1 秒采样。
4. 点击“开始记录”后保存快照，使用“导出 CSV”打开文件或分享给分析工具。

## OTA 说明

当前 Rider 默认 `CONFIG_APP_OTA_ENABLE=0`，不会出现 RCSP 服务。小程序可以探测 AE00 通道并展示能力状态，但所有写入路径（包括 `sendPacket()` 和 `upload()`）都保持拒绝，避免在没有认证、ACK、文件信息和回滚验证的情况下误烧固件。只有打开 OTA 宏并完成硬件验收后，才应在该适配器中补齐 E1-E7 命令状态机。
