#include "app_config.h"

#ifdef CONFIG_BOARD_AC632N_RIDER

#include "system/includes.h"
#include "asm/power/p33.h"
#include "norflash.h"
#include "user_cfg.h"
#include "usb/otg.h"

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

/** Rider has no physical power key; boot is controlled by the product board. */
u8 get_power_on_status(void)
{
    return 0;
}

/** Mark a port as owned by an active peripheral before high-impedance cleanup. */
static void rider_protect_port(u16 *groups, u32 port)
{
    if (port == NO_CONFIG_PORT) {
        return;
    }
    groups[port / IO_GROUP_NUM] &= (u16)~BIT(port % IO_GROUP_NUM);
}

/** Put unused GPIOs in a quiet state while preserving the PB7 sensor bus. */
static void rider_close_gpio(void)
{
    u16 groups[3] = {0x1ff, 0x3ff, 0x3ff};

    /* PB7 is deliberately excluded from the high-impedance mask. */
    rider_protect_port(groups, IO_PORTB_07);
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

/** No GPIO wake source is enabled for this always-on BLE sensor target. */
const struct wakeup_param wk_param = {
};

/** Prepare the board for software power-off without touching PB7 ownership. */
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
