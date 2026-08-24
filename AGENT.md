# 项目架构真相源

## Codex 强制声明

Codex 在任何代码生成前必须读取 AGENT.md 并严格遵守其中的架构约束。

## 系统分层结构

本项目是杰理 AC63 系列嵌入式 SDK，固件构建由根级 Makefile 编排，各应用通过板级 Makefile 选择芯片、宏定义、头文件和源文件。

- **Interface / Application**：`apps/spp_and_le`、`apps/rider_core_temp`、`apps/hid`、`apps/mesh`，负责应用入口、协议业务、用户配置和产品级诊断。
- **Component / Common**：`apps/common`，负责跨应用复用的设备、升级、音频和第三方协议组件。
- **Platform / CPU**：`cpu/<chip>`，负责芯片相关外设、音频、启动和链接脚本输入。
- **Library / SDK**：`include_lib`，提供 SDK、驱动和协议栈的公开头文件及预编译库接口。
- **Infrastructure / Tools**：根 `Makefile`、板级 `Makefile` 和 `tools`，负责构建、预处理、链接和下载。
- **IDE Integration**：根 `CMakeLists.txt` 和 `CMakePresets.json` 只建立 CLion 的代码模型，并调用现有 Makefile；不得承载固件业务逻辑。

## 模块职责边界

- 应用层只负责应用编排、事件处理、协议行为和产品配置。
- 公共组件层只承载多个应用确实共享的稳定能力，不反向依赖具体应用实现。
- CPU 层只负责芯片和板级硬件适配，不放置应用协议逻辑。
- SDK 头文件层只提供接口和平台能力，不复制应用配置。
- Makefile 和 IDE 配置只负责参数、依赖、构建与索引，不实现运行时业务。
- 工具脚本按参数解析、核心逻辑、输入输出职责拆分，禁止将脚本扩展成业务容器。

## 禁止跨层规则

- 禁止在应用入口中直接实现芯片寄存器或工具链构建逻辑。
- 禁止公共组件反向包含具体产品目录中的实现文件。
- 禁止用全局 `utils` 目录承载业务规则、硬件适配或跨域状态。
- 禁止 CMake 复制固件链接流程；固件编译的唯一入口仍是 Makefile。
- 禁止同一目标同时混入不同芯片、板级目录或不同应用的配置宏。
- 禁止循环依赖、上帝文件和为减少代码行数而进行的错误抽象。

## 架构演进规则

1. 新增应用能力先放入对应应用模块；只有存在至少两个稳定使用点时，才提取到 `apps/common`。
2. 新增芯片能力放入对应 `cpu/<chip>` 和板级目录，并同步板级 Makefile 的宏、头文件及源文件列表。
3. 新增 CLion 索引配置时，只扩展 CMake 的 IDE 目标或 Preset，不改变 Makefile 的固件语义。
4. 涉及目录、分层或依赖变化时，必须同步本文件、根 README 和相关模块说明。
5. 每次扩展都应确认依赖方向为应用 → 公共组件 → CPU/SDK，禁止反向依赖。

Rider CoreTemp 的产品协议、温度算法、PB7 传感器适配和 AC632N 板载诊断必须留在 `apps/rider_core_temp`；不得把产品 UUID、温度状态、M601 时序或 J12 LED/按键逻辑移入 `apps/common` 或 `cpu/bd19`。其中 `modules/temp` 按采集、滤波/佩戴状态和估算职责拆分，`modules/bt/core_temp_gatt.c` 负责 GATT 生命周期/通知编排，`modules/bt/rider_temp_codec.c` 只承载可主机验证的纯字节编码，`modules/diag` 只能读取现有 BLE/温度接口并驱动板级配置声明的 GPIO，不得复制协议解析或温度换算。

AC632N 开发板的 J12 是独立的 LED/按键接口，默认跳线映射由 `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h` 声明；PB7 永远归 M601 1-Wire，PA0 永远归 UART0 TX。修改 J12 映射时必须同步模块 README，并确认低功耗 GPIO 清理不会释放诊断端口。

## 代码复用与注释

- 优先使用局部 helper，其次使用同一业务域内的复用；跨模块抽象必须有多个稳定使用点。
- 宁可保留少量语义清晰的重复，也不要用万能 helper 隐藏业务边界。
- 复杂分支、状态、缓存、重试、魔法值和临时兼容方案必须解释设计意图与边界。
- 注释解释为什么、风险和约束，不复述代码。
