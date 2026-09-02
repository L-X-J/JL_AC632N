#ifndef RIDER_BOARD_DIAG_H
#define RIDER_BOARD_DIAG_H

#include <stdint.h>

/** 启动 AC632N 板级 LED/按键诊断。 */
void rider_board_diag_init(void);

/** 停止诊断定时器并释放全部板级 LED。 */
void rider_board_diag_stop(void);

/** 将电源/红灯交给电源键反馈，不停止蓝灯等其它诊断。 */
void rider_board_diag_power_led_claim(uint8_t on);

/** 释放电源/红灯，交还诊断渲染（温度状态挂蓝灯）。 */
void rider_board_diag_power_led_release(void);

#endif
