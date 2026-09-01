
[tag download]:https://github.com/Jieli-Tech/fw-AC63_BT_SDK/tags
[tag_badgen]:https://img.shields.io/github/v/tag/Jieli-Tech/fw-AC63_BT_SDK?style=plastic&logo=bluetooth&labelColor=ffffff&color=informational&label=Tag&logoColor=blue

# fw-AC63_BT_SDK   [![tag][tag_badgen]][tag download]

中文 | [EN](./README-en.md)

AC63 系列通用蓝牙SDK 固件程序

本仓库包含SDK release 版本代码，线下线上支持同步发布，并且引用了其他开源项目（如Zephyr RTOS）.

本工程提供的例子，需要结合对应命名规则的库文件(lib.a) 和对应的子仓库进行编译.

快速开始
------------

欢迎使用杰理开源项目，在开始进入项目之前，请详细阅读SDK 介绍文档，
从而获得对杰理系列芯片和SDK 的大概认识，并且可以通过快速开始介绍来进行开发.


工具链
------------

关于如何获取`杰理工具链` 和 如何进行环境搭建，请阅读以下内容：

* 编译工具 ：请安装杰理编译工具来搭建起编译环境, [下载链接](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/index.html) 

* USB 升级工具 : 在开发完成后，需要使用杰理烧写工具将对应的`hex`文件烧录到目标板，进行开发调试, 关于如何获取工具请进入申请 [链接](https://item.taobao.com/item.htm?spm=a1z10.1-c-s.w4004-22883854875.5.504d246bXKwyeH&id=620295020803) 并详细阅读对应的[文档](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/forced_upgrade/index.html)，以及相关下载脚本[配置](https://doc.zh-jieli.com/AC63/zh-cn/master/getting_started/project_download/INI_config.html)

介绍文档
------------

* 芯片简介 : [SoC 数据手册扼要](https://doc.zh-jieli.com/vue/#/docs/ac63), [下载链接](./doc/datasheet)

* SDK 版本信息 : [SDK 历史版本](https://doc.zh-jieli.com/AC63/zh-cn/master/other/version/index.html)

* SDK 介绍文档 : [SDK 快速开始简介](https://doc.zh-jieli.com/AC63/zh-cn/master/index.html)

* SDK 结构文档 : [SDK 模块结构](./doc/architure)

编译工程
-------------
请选择以下一个工程进行编译，下列目录包含了便于开发的工程文件：

* 蓝牙应用 : [SPP_LE](./apps/spp_and_le), 适用领域：透传, 数传, 扫描设备, 广播设备, 信标, FindMy应用, 多机连接. Dongle(usb / bt). [文档链接](https://doc.zh-jieli.com/AC63/zh-cn/master/module_demo/spple/index.html)

* 蓝牙应用 : [Rider CoreTemp](./apps/rider_core_temp)，AC632N 单 BLE 外设，用于 PB7 上的 M601 温度采集和 CORE 兼容协议广播；UART0 调试输出固定为 `PA0 / 115200 8N1`，接线和 ASCII 日志约束见 [Rider CoreTemp 调试说明](./apps/rider_core_temp/DEBUG.md)。

  Rider 板级电源接口固定为 `PB3` 低电平有效按键和 `PB5` 高电平点亮指示灯；PB5 的电源反馈优先于温度诊断显示，反馈结束后恢复诊断状态。`PB7` 继续独占 M601 1-Wire，`PA0` 继续作为 UART0 TX，J12 的 PB6/PB4 与 PB0/PB1 映射不变。

* 微信小程序调试台 : [Rider CoreTemp Debug](./wechat_miniprogram/rider_core_temp_debug)，连接 Rider 固件的 `0x2110/0x2111` 调试服务，按 200 ms 接收最新温度快照并导出 CSV。

* 蓝牙应用 : [HID](./apps/hid), 适用领域：遥控器, 自拍器, 键盘, 鼠标, 吃鸡王座, 语音遥控器. [文档链接](https://doc.zh-jieli.com/AC63/zh-cn/master/module_demo/hid/index.html)

* 蓝牙应用 : [Mesh](./apps/mesh), 适用领域：物联网节点, 天猫精灵接入, 自组网应用. [文档链接](https://doc.zh-jieli.com/AC63/zh-cn/master/module_demo/mesh/index.html)

已发布版本详见 标签(Tags)。

即将发布：

* 蓝牙应用 ：`IoT (ipv6 / 6lowpan)`

* 2.4G 应用 : `Vendor Wireless`

SDK 同时支持Codeblock 和 Make 编译环境，请确保编译前已经搭建好编译环境，

* Codeblock 编译 : 进入对应的工程目录并找到后缀为 `.cbp` 的文件, 双击打开便可进行编译.

* Makefile 编译 : 双击`tools/make_prompt.bat`，输入 `make target`（具体`target`的名字，参考`Makefile`开头的注释）

  `在编译下载代码前，请确保USB 升级工具正确连接并且进入编程模式`

* 蓝牙OTA : [OTA](https://doc.zh-jieli.com/AC63/zh-cn/master/module_demo/ota/index.html) , 适用领域：单备份，双备份蓝牙升级

## CLion 开发与代码索引

### 模块职责

当前工程由根级 Makefile 编排固件构建，`apps/spp_and_le` 提供 SPP/LE 应用入口与协议业务，`apps/common` 提供公共组件，`cpu/bd19` 提供 AC632N 的芯片适配，`include_lib` 提供 SDK 接口。根目录 `CMakeLists.txt` 仅用于让 CLion 建立代码索引，不替代固件 Makefile。

### 目录结构

```text
apps/
  spp_and_le/       SPP/LE 应用、配置、模块和示例
  rider_core_temp/  Rider 核心温度 BLE 外设应用
  common/           跨应用公共组件
cpu/bd19/           AC632N 芯片与外设实现
include_lib/        SDK、驱动和协议栈头文件
tools/              构建后处理与下载工具
Makefile            固件目标编排
CMakeLists.txt      CLion 索引入口
CMakePresets.json   CLion CMake 配置预设
AGENT.md            架构约束真相源
```

### 数据流与模块边界

源码由对应应用的 `app_main.c` 进入流程：SPP_LE 使用 `apps/spp_and_le`，Rider CoreTemp 使用 `apps/rider_core_temp`。应用通过 `apps/common` 调用 SDK 接口与 `cpu/bd19` 硬件能力；板级 Makefile 注入 AC632N 宏、头文件路径和源文件列表，随后完成编译、链接和下载。CMake 索引目标只读取同一目标的源码、宏和头文件路径，固件产物仍分别由 `make ac632n_spp_and_le` 或 `make ac632n_rider_core_temp` 生成。

### 在 CLion 中打开

1. 关闭当前 Makefile Project，用 CLion 打开仓库根目录，不要单独打开 `apps/spp_and_le/app_main.c`。
2. 在 CMake Profiles 中选择 `clion-ac632n-index`；如果当前窗口仍显示 Makefile Project，关闭项目后重新打开根目录，让 CLion 重新加载 `CMakePresets.json`。
3. SPP_LE 索引目标名称为 `ac632n_spp_and_le_indexing`，Rider 索引目标名称为 `ac632n_rider_core_temp_indexing`；如果本地已有旧的 Makefile 工程元数据，只需删除 `.idea/misc.xml` 后重新打开项目，不要删除源码或构建产物。
4. 如果本机安装了杰理工具链，将 `JL_EXTERNAL_INCLUDE_DIR` 指向 q32s 的 include 目录；Linux 默认探测 `/opt/jieli/q32s/include`，Windows 可使用 `C:/JL/pi32/q32s-include`。
5. 代码索引完成后，函数跳转、头文件解析和宏展开按 AC632N bd19 配置工作；固件构建使用 CMake 目标 `firmware_ac632n_spp_and_le` 或根级 Makefile。

### 扩展方式与禁止事项

- 新增应用逻辑放入对应 `apps/<application>`，新增芯片逻辑放入对应 `cpu/<chip>`，并同步板级 Makefile。
- 只有稳定且确实跨应用复用的能力才可放入 `apps/common`。
- 禁止在 CMake 中复制链接脚本、库列表或固件业务逻辑。
- 禁止混用不同芯片的 include 目录和配置宏，禁止循环依赖、上帝文件和万能 utils。
- 详细架构边界与演进规则见 [AGENT.md](./AGENT.md)。

### Rider CoreTemp 固件

Rider 的电源手势由 `PB3` 按键和 `PB5` 指示灯状态机负责：关机唤醒后须持续按住 2 秒，运行中长按 2 秒后执行 3 次“灭 100 ms、亮 100 ms”再软关机；PB5 反馈期间暂时覆盖温度诊断，结束后恢复。该行为及时序参考附加 AB202X 文档，但本项目不采用其硬件映射，也不修改现有 BLE 调试帧。

进入 `apps/rider_core_temp` 后，可使用 `make ac632n_rider_core_temp` 构建独立固件。应用只启用 BLE 外设角色，设备名固定为 `ICXL-RTemp`；PB7 由 `modules/temp/m601_1wire.c` 独占，`modules/temp/rider_temp_filter.c` 负责滤波和佩戴状态，`modules/bt/core_temp_gatt.c` 编排 GATT、`modules/bt/rider_temp_codec.c` 负责可测试的温度字节编码，J12 LED/按键诊断由 `modules/diag/rider_board_diag.c` 提供。温度链路明确保留 Sensor、Trusted Skin、Experimental Core 三条时间线：`30~45°C` 是宽泛佩戴保护窗，5 个样本只填满中值窗口，连续约 30 秒有效接触后才建立可信皮温；`32~40°C` 仅作为典型胸部贴肤和脱落恢复证据，不能提前跳过 30 秒门控。可信后，自定义 CORE 约 1 Hz 稳定上报 Skin，核心字段先保持 `0x7FFF`；Core V1 再积累约 5 分钟可信皮温历史，随后约 1 Hz 更新实验性核心候选，HTS 约每 10 秒上报一次 Core。可信皮温建立后若滤波值持续低于 `31.50°C` 约 60 秒，也会判为离体并清空当前 episode；重新贴肤须回到脱落前峰值附近并重新完成可信与模型预热。通用头文件默认使用 `SHADOW`；当前 AC632N 板级显式使用 `EXPERIMENTAL`，用于把可追溯但尚未验证的核心候选送到码表导出，不能宣称为医疗或已验证核心体温。完成完整骑行 Session 留出标定后才可切换 `STRICT`。平均温度仍由码表对有效样本统计。开发板 J12 的脚位和日志格式见 [Rider CoreTemp 模块说明](./apps/rider_core_temp/README.md)，公式、参数和验证记录见 [单 M601 温度算法研究与验证](./doc/ICXL-CoreTemp-Ride/单M601温度算法研究与验证.md)，协议字段见 [CORE2 BLE 协议说明](./doc/ICXL-CoreTemp-Ride/CORE2_BLE_协议说明.md)。

蓝牙官方认证
-------------

经典蓝牙LMP / 低功耗蓝牙Link Layer 层和Host 协议栈均支持蓝牙5.0 、5.1和5.4版本实现

* Core v5.0 [QDID 134104](https://launchstudio.bluetooth.com/ListingDetails/88799)

* Core v5.1 [QDID 136145](https://launchstudio.bluetooth.com/ListingDetails/91371)

* Core v5.4 [QDID 222830](https://launchstudio.bluetooth.com/ListingDetails/193923)


硬件环境
-------------

* 开发评估板 ：开发板申请入口[链接](https://shop321455197.taobao.com/?spm=a230r.7195193.1997079397.2.2a6d391d3n5udo)

* AC632N 开发板资料：[AC632_DevKitBoard V2.0 板卡说明](./doc/datasheet/AC632N/AC632N开发板/AC632_DevKitBoard_V2.0_板卡说明.md)，包含 J12 位置、LED/按键引脚和当前 Rider 固件跳线映射。

* 生产烧写工具 : 为量产和裸片烧写而设计, 申请入口 [链接](https://item.taobao.com/item.htm?spm=a1z10.1-c-s.w4004-22883854875.8.504d246bXKwyeH&id=620941819219) 并仔细阅读相关 [文档](https://doc.zh-jieli.com/Tools/zh-cn/mass_prod_tools/burner_1tuo2/index.html)

* 无线测试盒 : 为空中升级/射频标定/快速产品测试而设计, 申请入口 [链接](https://item.taobao.com/item.htm?spm=a1z10.1-c-s.w4004-22883854875.10.504d246bXKwyeH&id=620942507511), 阅读[文档](https://doc.zh-jieli.com/Tools/zh-cn/mass_prod_tools/testbox_1tuo2/index.html) 获取更多详细信息.


社区
--------------

* 技术交流群，钉钉群

  1群 : `31691148`（满）

  2群 : `3375034077`（满）

  3群 : `107855006323`

* 常见问题集合[链接](./doc/FAQ)

免责声明
------------

AC63_BT_SDK 支持AC63 系列芯片开发.
AC63 系列芯片支持了通用蓝牙常见应用，可以作为开发，评估，样品，甚至量产使用，对应SDK 版本见tag 和 release
