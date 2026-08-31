# Project Memory

## Project Purpose

`JL_AC632N` 是 AC632N/bd19 的 Rider CoreTemp BLE 外设固件工程，负责 M601 温度采集、温度状态/估算、CORE 兼容 GATT 和板级诊断。

## Architecture

- `apps/rider_core_temp` 保有 Rider 产品逻辑；`apps/common` 不反向依赖它。
- `modules/temp` 负责 PB7/M601 采集和温度状态，`modules/bt` 负责 GATT 生命周期/通知，`modules/diag` 负责 J12 诊断和 PB5 输出仲裁，`modules/power` 负责 PB3 电源手势。
- `board/bd19` 只负责 AC632N GPIO、唤醒源、低功耗清理和板级配置；应用通过板级头文件读取 PB3 电平和唤醒结果。

## Important Constraints

- PB3 是低电平有效电源按键，开启内部上拉，使用下降沿唤醒并登记在 `wk_param.port[1]`。
- PB5 是高电平点亮的电源指示灯，也是 J12 LED2 的现有物理端口。电源反馈期间 PB5 覆盖温度诊断，释放后恢复诊断渲染。
- PB6/PB4、PB0/PB1 的 J12 诊断职责、PB7 的 M601 1-Wire 和 PA0 的 UART0 TX 必须保持不变。UART0 既有波特率为 `1000000`。
- `TCFG_LOWPOWER_LOWPOWER_SEL` 保持为 `0`，M601 微秒时序不依赖低功耗切换；关机使用显式软关机和 PB3 唤醒。
- PB3/PB5 逻辑不得改变 BLE UUID、公开特征、调试帧长度或字段。

## Conventions

- 固件入口是根级 Makefile/板级 Makefile；CMake 只用于 CLion 代码模型。
- 新增 Rider 业务能力先放入 `apps/rider_core_temp`，只有多个应用稳定共享时才考虑公共组件。
- 外部 AB202X 文档只作为按键/LED 行为及时序参考，不作为本项目硬件映射真相源。
