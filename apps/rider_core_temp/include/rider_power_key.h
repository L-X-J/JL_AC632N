#ifndef RIDER_POWER_KEY_H
#define RIDER_POWER_KEY_H

#include <stdint.h>

typedef void (*rider_power_key_prepare_poweroff_cb_t)(void);

/** 开机确认前复位电源键状态机。 */
void rider_power_key_init(void);

/** 注册软关机前停止产品活动的回调。 */
void rider_power_key_register_poweroff_prepare(
    rider_power_key_prepare_poweroff_cb_t callback);

/** 确认 KEY1(PA1) 唤醒或接受非按键上电；拒绝时返回 0。 */
uint8_t rider_power_key_startup_check(void);

/** 启动 5 ms 运行态按键扫描与开机提示定时器。 */
void rider_power_key_start(void);

/** 停止运行态扫描，关机准备期间强制关闭电源/红灯。 */
void rider_power_key_stop(void);

#endif
