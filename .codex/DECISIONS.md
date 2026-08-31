## ADR-001: Rider 电源按键和指示灯归属

Status: Accepted

Context: Rider 需要按键开关机，但附加 AB202X 文档的硬件映射不能覆盖 AC632N 板级现有端口职责。

Decision: 在 `board/bd19` 仅声明 PB3 低有效输入、PB3 的 `wk_param.port[1]` 下降沿唤醒和 PB5 高有效输出；在 `modules/power` 实现 2 秒确认/关机手势，在 `modules/diag` 提供 PB5 申请/释放接口。

Reason: 板级适配与产品行为分层，保留 PB7、PA0 和 J12 既有映射，同时允许电源反馈优先于温度诊断。

Alternatives: 复用通用按键驱动或把 PB5 直接写入多个模块；这些方案会引入重复状态机或 GPIO 写入冲突。

Reconsider when: 板卡更换 PB3/PB5，或 SDK 提供可表达相同隔离和优先级语义的统一电源按键生命周期。
