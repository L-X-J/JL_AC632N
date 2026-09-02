#include "app_config.h"

#ifdef CONFIG_BOARD_AC632N_RIDER

#include "system/includes.h"
#include "asm/power/p33.h"
#include "norflash.h"
#include "user_cfg.h"
#include "usb/otg.h"
#include "rider_board_power.h"

void usb1_iomode(u32 enable);

#define LOG_TAG_CONST       BOARD
#define LOG_TAG             "[RIDER_BOARD]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

/** Keep the board in the clock/power mode required by BLE and 1-Wire timing. */
const struct low_power_param power_param = {
    .config = TCFG_LOWPOWER_LOWPOWER_SEL,
    .btosc_hz = TCFG_CLOCK_OSC_HZ,
    .delay_us = TCFG_CLOCK_SYS_HZ / 1000000L,
    .btosc_disable = TCFG_LOWPOWER_BTOSC_DISABLE,
    .vddiom_lev = TCFG_LOWPOWER_VDDIOM_LEVEL,
    .vddiow_lev = TCFG_LOWPOWER_VDDIOW_LEVEL,
    .osc_type = TCFG_LOWPOWER_OSC_TYPE,
    .lpctmu_en = 0,
    .vd13_cap_en = TCFG_VD13_CAP_EN,
};

#if TCFG_UART0_ENABLE
UART0_PLATFORM_DATA_BEGIN(uart0_data)
    .tx_pin = TCFG_UART0_TX_PORT,
    .rx_pin = TCFG_UART0_RX_PORT,
    .baudrate = TCFG_UART0_BAUDRATE,
    .flags = UART_DEBUG,
UART0_PLATFORM_DATA_END()
#endif

/* The target has no runtime device peripheral, but the SDK still expects the
 * board device section to exist. */
REGISTER_DEVICES(device_table) = {
};

/** Initialise the optional debug UART without enabling any other peripheral. */
void debug_uart_init(const struct uart_platform_data *data)
{
#if TCFG_UART0_ENABLE
    uart_init(data ? data : &uart0_data);
#else
    (void)data;
#endif
}

/** 将 KEY1(PA1) 配置为低电平有效输入，供唤醒与运行态扫描共用。 */
static void rider_power_key_gpio_init(void)
{
    gpio_set_die(RIDER_BOARD_POWER_KEY_PORT, 1);
    gpio_set_pull_down(RIDER_BOARD_POWER_KEY_PORT, 0);
    gpio_set_pull_up(RIDER_BOARD_POWER_KEY_PORT, 1);
    gpio_set_direction(RIDER_BOARD_POWER_KEY_PORT, 1);
}

/** 将 KEY2(PA2) 配置为输入；本轮无业务回调，仅初始化脚位。 */
static void rider_key2_gpio_init(void)
{
    if (RIDER_BOARD_DIAG_IOKEY2_PORT == NO_CONFIG_PORT) {
        return;
    }
    gpio_set_die(RIDER_BOARD_DIAG_IOKEY2_PORT, 1);
    gpio_set_pull_down(RIDER_BOARD_DIAG_IOKEY2_PORT, 0);
    gpio_set_pull_up(RIDER_BOARD_DIAG_IOKEY2_PORT, 1);
    gpio_set_direction(RIDER_BOARD_DIAG_IOKEY2_PORT, 1);
}

/** 通过板级适配边界返回 KEY1 当前物理电平是否按下。 */
uint8_t rider_board_power_key_pressed(void)
{
    return gpio_read(RIDER_BOARD_POWER_KEY_PORT) ==
           RIDER_BOARD_POWER_KEY_ACTIVE_LEVEL;
}

/** 判断 P33 唤醒源是否指向 wk_param.port[1]（KEY1=PA1）。 */
uint8_t rider_board_power_key_wakeup(void)
{
    return (get_wakeup_source() & TCFG_WAKEUP_PORT_POWER_SRC) != 0;
}

/** Compatibility hook used by the generic SDK power-on path. */
u8 get_power_on_status(void)
{
    return rider_board_power_key_pressed();
}

/** Mark a port as owned by an active peripheral before high-impedance cleanup. */
static void rider_protect_port(u16 *groups, u32 port)
{
    if (port == NO_CONFIG_PORT) {
        return;
    }
    groups[port / IO_GROUP_NUM] &= (u16)~BIT(port % IO_GROUP_NUM);
}

/** 将未用 GPIO 置安静态；保留 PB7 温感总线与产品 KEY/灯脚。 */
static void rider_close_gpio(void)
{
    u16 groups[3] = {0x1ff, 0x3ff, 0x3ff};

    rider_protect_port(groups, RIDER_BOARD_POWER_KEY_PORT);
    /* 红灯兼电源指示，勿被通用 GPIO 清理改写 */
    rider_protect_port(groups, RIDER_BOARD_POWER_LED_PORT);
    /* PB7 温感 1-Wire 独占，刻意排除在高阻清理之外 */
    rider_protect_port(groups, IO_PORTB_07);
#if RIDER_BOARD_DIAG_ENABLE
    rider_protect_port(groups, RIDER_BOARD_DIAG_LED1_PORT);
    rider_protect_port(groups, RIDER_BOARD_DIAG_LED2_PORT);
    rider_protect_port(groups, RIDER_BOARD_DIAG_LED3_PORT);
    rider_protect_port(groups, RIDER_BOARD_DIAG_IOKEY1_PORT);
    rider_protect_port(groups, RIDER_BOARD_DIAG_IOKEY2_PORT);
#endif
#if TCFG_UART0_ENABLE
    rider_protect_port(groups, TCFG_UART0_TX_PORT);
    rider_protect_port(groups, TCFG_UART0_RX_PORT);
#endif

    gpio_dir(GPIOA, 0, 9, groups[0], GPIO_OR);
    gpio_set_pu(GPIOA, 0, 9, (u16)~groups[0], GPIO_AND);
    gpio_set_pd(GPIOA, 0, 9, (u16)~groups[0], GPIO_AND);
    gpio_die(GPIOA, 0, 9, (u16)~groups[0], GPIO_AND);
    gpio_dieh(GPIOA, 0, 9, (u16)~groups[0], GPIO_AND);

    gpio_dir(GPIOB, 0, 10, groups[1], GPIO_OR);
    gpio_set_pu(GPIOB, 0, 10, (u16)~groups[1], GPIO_AND);
    gpio_set_pd(GPIOB, 0, 10, (u16)~groups[1], GPIO_AND);
    gpio_die(GPIOB, 0, 10, (u16)~groups[1], GPIO_AND);
    gpio_dieh(GPIOB, 0, 10, (u16)~groups[1], GPIO_AND);

    /* USB is disabled by configuration; force both USB pairs high impedance. */
    usb_iomode(1);
    gpio_set_pull_up(IO_PORT_DP, 0);
    gpio_set_pull_down(IO_PORT_DP, 0);
    gpio_set_direction(IO_PORT_DP, 1);
    gpio_set_die(IO_PORT_DP, 0);
    gpio_set_dieh(IO_PORT_DP, 0);
    gpio_set_pull_up(IO_PORT_DM, 0);
    gpio_set_pull_down(IO_PORT_DM, 0);
    gpio_set_direction(IO_PORT_DM, 1);
    gpio_set_die(IO_PORT_DM, 0);
    gpio_set_dieh(IO_PORT_DM, 0);
    usb1_iomode(1);
    gpio_set_pull_up(IO_PORT_DP1, 0);
    gpio_set_pull_down(IO_PORT_DP1, 0);
    gpio_set_direction(IO_PORT_DP1, 1);
    gpio_set_die(IO_PORT_DP1, 0);
    gpio_set_dieh(IO_PORT_DP1, 0);
    gpio_set_pull_up(IO_PORT_DM1, 0);
    gpio_set_pull_down(IO_PORT_DM1, 0);
    gpio_set_direction(IO_PORT_DM1, 1);
    gpio_set_die(IO_PORT_DM1, 0);
    gpio_set_dieh(IO_PORT_DM1, 0);
}

/** KEY1(PA1) 为唯一数字唤醒源；其余产品脚位保持各自职责。 */
static const struct port_wakeup rider_power_key_wakeup = {
    .pullup_down_enable = ENABLE,
    .edge               = FALLING_EDGE,
    .both_edge          = 0,
    .filter             = PORT_FLT_2ms,
    .iomap              = RIDER_BOARD_POWER_KEY_PORT,
};

const struct wakeup_param wk_param = {
    .port[RIDER_BOARD_POWER_KEY_WAKEUP_INDEX] = &rider_power_key_wakeup,
};

/** 软关机前整理 GPIO，不触碰 PB7 温感所有权。 */
void board_set_soft_poweroff(void)
{
    rider_close_gpio();
}

/** Restore the debug pin after a low-power transition. */
void sleep_exit_callback(u32 usec)
{
    (void)usec;
}

/** Apply the board GPIO state requested by the power manager. */
void sleep_enter_callback(u8 step)
{
    if (step != 1) {
        rider_close_gpio();
    }
}

/** Initialise power callbacks and ADC services before the application starts. */
void board_power_init(void)
{
    power_init(&power_param);
    rider_power_key_gpio_init();
    rider_key2_gpio_init();
    gpio_longpress_pin0_reset_config(IO_PORTA_09, 0, 0);
    gpio_shortpress_reset_config(0);
    power_set_callback(TCFG_LOWPOWER_LOWPOWER_SEL,
                       sleep_enter_callback, sleep_exit_callback,
                       board_set_soft_poweroff);
    power_keep_dacvdd_en(0);
    power_wakeup_init(&wk_param);
}

/** Run the minimum AC632N board bring-up sequence and load Rider identity. */
void board_init(void)
{
    board_power_init();
    adc_vbg_init();
    adc_init();
    cfg_file_parse(0);
    devices_init();
    power_set_mode(TCFG_LOWPOWER_POWER_SEL);
#if TCFG_UART0_ENABLE
    if (uart0_data.rx_pin < IO_MAX_NUM) {
        gpio_set_die(uart0_data.rx_pin, 1);
    }
#endif
}

#endif
