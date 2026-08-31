#include "system/includes.h"
#include "app_config.h"
#include "asm/power_interface.h"
#include "asm/wdt.h"
#include "rider_board_diag.h"
#include "rider_board_power.h"
#include "rider_power_key.h"

#define LOG_TAG_CONST       RIDER_POWER_KEY
#define LOG_TAG             "[RIDER_POWER_KEY]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

#define RIDER_POWER_KEY_SCAN_MS             5
#define RIDER_POWER_KEY_LONG_MS             2000
#define RIDER_POWER_KEY_POWER_ON_MS         2000
#define RIDER_POWER_KEY_FLASH_PHASE_MS      100
#define RIDER_POWER_KEY_FLASH_PHASES        6
#define RIDER_POWER_KEY_DEBOUNCE_SAMPLES    2

enum rider_power_key_state {
    RIDER_POWER_KEY_STOPPED = 0,
    RIDER_POWER_KEY_POWER_ON,
    RIDER_POWER_KEY_IDLE,
    RIDER_POWER_KEY_POWER_OFF,
};

static u16 rider_power_key_timer_id;
static enum rider_power_key_state rider_power_key_state = RIDER_POWER_KEY_STOPPED;
static rider_power_key_prepare_poweroff_cb_t rider_power_key_prepare_poweroff;
static u32 rider_power_key_power_on_started_ms;
static u32 rider_power_key_press_started_ms;
static u32 rider_power_key_power_off_started_ms;
static u8 rider_power_key_raw;
static u8 rider_power_key_stable;
static u8 rider_power_key_filter_samples;
static u8 rider_power_key_press_active;
static u8 rider_power_key_ignore_until_release;
static u8 rider_power_key_poweroff_requested;

/** Return elapsed milliseconds with unsigned wraparound handling. */
static u32 rider_power_key_elapsed(u32 start_ms)
{
    return sys_timer_get_ms() - start_ms;
}

/** Update the debounced PB3 state without turning it into a generic key event. */
static void rider_power_key_update_input(void)
{
    u8 raw = rider_board_power_key_pressed();

    if (raw != rider_power_key_raw) {
        rider_power_key_raw = raw;
        rider_power_key_filter_samples = 0;
        return;
    }
    if (rider_power_key_filter_samples < RIDER_POWER_KEY_DEBOUNCE_SAMPLES) {
        rider_power_key_filter_samples++;
        return;
    }
    rider_power_key_stable = raw;
}

/** Enter the locked six-phase power-off indication. */
static void rider_power_key_enter_power_off(void)
{
    rider_power_key_state = RIDER_POWER_KEY_POWER_OFF;
    rider_power_key_power_off_started_ms = sys_timer_get_ms();
    rider_power_key_press_active = 0;
    rider_board_diag_power_led_claim(0);
    log_info("long press confirmed, starting power-off flash\n");
}

/** Stop product activity and request soft power-off exactly once. */
static void rider_power_key_request_poweroff(void)
{
    if (rider_power_key_poweroff_requested) {
        return;
    }
    rider_power_key_poweroff_requested = 1;
    rider_power_key_stop();
    if (rider_power_key_prepare_poweroff) {
        rider_power_key_prepare_poweroff();
    }
    log_info("request soft power-off\n");
    power_set_soft_poweroff();
}

/** Render the fixed 100 ms off/on phases and finish with a forced-off state. */
static void rider_power_key_render_power_off(void)
{
    u32 elapsed_ms = rider_power_key_elapsed(
        rider_power_key_power_off_started_ms);
    u32 phase;

    if (elapsed_ms >= RIDER_POWER_KEY_FLASH_PHASE_MS *
                       RIDER_POWER_KEY_FLASH_PHASES) {
        rider_board_diag_power_led_claim(0);
        rider_power_key_request_poweroff();
        return;
    }

    phase = elapsed_ms / RIDER_POWER_KEY_FLASH_PHASE_MS;
    rider_board_diag_power_led_claim((phase & 1) != 0);
}

/** Process one 5 ms tick while keeping power-on and power-off modes exclusive. */
static void rider_power_key_timer(void *priv)
{
    u32 now_ms;

    (void)priv;
    rider_power_key_update_input();
    now_ms = sys_timer_get_ms();

    if (rider_power_key_state == RIDER_POWER_KEY_POWER_ON) {
        rider_board_diag_power_led_claim(1);
        if (now_ms - rider_power_key_power_on_started_ms >=
            RIDER_POWER_KEY_POWER_ON_MS) {
            rider_power_key_state = RIDER_POWER_KEY_IDLE;
            rider_power_key_press_started_ms = 0;
            rider_power_key_press_active = 0;
            if (rider_power_key_stable) {
                /* The startup hold is a gesture, not the first shutdown hold. */
                rider_power_key_ignore_until_release = 1;
                rider_board_diag_power_led_claim(0);
            } else {
                rider_power_key_ignore_until_release = 0;
                rider_board_diag_power_led_release();
            }
        }
        return;
    }

    if (rider_power_key_state == RIDER_POWER_KEY_POWER_OFF) {
        rider_power_key_render_power_off();
        return;
    }

    if (rider_power_key_state != RIDER_POWER_KEY_IDLE) {
        return;
    }

    if (rider_power_key_ignore_until_release) {
        rider_board_diag_power_led_claim(0);
        if (!rider_power_key_stable) {
            rider_power_key_ignore_until_release = 0;
            rider_power_key_press_started_ms = 0;
            rider_power_key_press_active = 0;
            rider_board_diag_power_led_release();
        }
        return;
    }

    if (rider_power_key_stable) {
        if (!rider_power_key_press_active) {
            rider_power_key_press_active = 1;
            rider_power_key_press_started_ms = now_ms;
        }
        rider_board_diag_power_led_claim(1);
        if (now_ms - rider_power_key_press_started_ms >=
            RIDER_POWER_KEY_LONG_MS) {
            rider_power_key_enter_power_off();
        }
    } else if (rider_power_key_press_active) {
        rider_power_key_press_active = 0;
        rider_power_key_press_started_ms = 0;
        rider_board_diag_power_led_release();
    }
}

/** Reset state before the board enters the application startup sequence. */
void rider_power_key_init(void)
{
    if (rider_power_key_timer_id) {
        sys_timer_del(rider_power_key_timer_id);
        rider_power_key_timer_id = 0;
    }
    rider_power_key_state = RIDER_POWER_KEY_STOPPED;
    rider_power_key_power_on_started_ms = 0;
    rider_power_key_press_started_ms = 0;
    rider_power_key_power_off_started_ms = 0;
    rider_power_key_raw = 0;
    rider_power_key_stable = 0;
    rider_power_key_filter_samples = 0;
    rider_power_key_press_active = 0;
    rider_power_key_ignore_until_release = 0;
    rider_power_key_poweroff_requested = 0;
}

/** Store the product callback used immediately before a soft power-off request. */
void rider_power_key_register_poweroff_prepare(
    rider_power_key_prepare_poweroff_cb_t callback)
{
    rider_power_key_prepare_poweroff = callback;
}

/** Confirm a PB3 wakeup by requiring a continuous two-second low level. */
uint8_t rider_power_key_startup_check(void)
{
    u32 hold_started_ms = 0;
    u8 hold_started = 0;

    rider_board_diag_power_led_claim(0);
    if (!rider_board_power_key_wakeup()) {
        rider_power_key_state = RIDER_POWER_KEY_POWER_ON;
        rider_power_key_power_on_started_ms = sys_timer_get_ms();
        rider_board_diag_power_led_claim(1);
        log_info("non-key startup, power-on prompt started\n");
        return 1;
    }

    log_info("PB3 wakeup, waiting for continuous hold\n");
    while (1) {
        u32 now_ms = sys_timer_get_ms();

        if (!rider_board_power_key_pressed()) {
            rider_power_key_poweroff_requested = 1;
            rider_power_key_stop();
            rider_board_diag_stop();
            log_info("PB3 released before power-on threshold\n");
            power_set_soft_poweroff();
            return 0;
        }
        if (!hold_started) {
            hold_started_ms = now_ms;
            hold_started = 1;
        }
        if (now_ms - hold_started_ms >= RIDER_POWER_KEY_LONG_MS) {
            rider_power_key_state = RIDER_POWER_KEY_POWER_ON;
            rider_power_key_power_on_started_ms = now_ms;
            /* The wakeup hold must be released before a shutdown hold can arm. */
            rider_power_key_ignore_until_release = 1;
            rider_board_diag_power_led_claim(1);
            log_info("power-on hold confirmed, prompt started\n");
            return 1;
        }
        wdt_clear();
        os_time_dly(1);
    }
}

/** Start the runtime scanner while preserving the startup prompt mode. */
void rider_power_key_start(void)
{
    if (rider_power_key_timer_id || rider_power_key_state == RIDER_POWER_KEY_STOPPED) {
        return;
    }

    rider_power_key_raw = rider_board_power_key_pressed();
    rider_power_key_stable = rider_power_key_raw;
    rider_power_key_filter_samples = 0;
    rider_power_key_press_active = 0;
    /* Keep the startup gesture barrier until the physical key is released. */
    rider_power_key_timer_id = sys_timer_add(NULL, rider_power_key_timer,
                                              RIDER_POWER_KEY_SCAN_MS);
    log_info("runtime scan started: timer=%u interval_ms=%u\n",
             (unsigned)rider_power_key_timer_id,
             (unsigned)RIDER_POWER_KEY_SCAN_MS);
}

/** Stop scanning and leave PB5 forced off while a shutdown is being prepared. */
void rider_power_key_stop(void)
{
    if (rider_power_key_timer_id) {
        sys_timer_del(rider_power_key_timer_id);
        rider_power_key_timer_id = 0;
    }
    rider_power_key_state = RIDER_POWER_KEY_STOPPED;
    rider_power_key_press_active = 0;
    rider_power_key_ignore_until_release = 0;
    rider_board_diag_power_led_claim(0);
    if (!rider_power_key_poweroff_requested) {
        rider_board_diag_power_led_release();
    }
}

#endif
