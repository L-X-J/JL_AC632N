#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"
#include "rider_board_diag.h"

#define LOG_TAG_CONST       RIDER_BOARD_DIAG
#define LOG_TAG             "[RIDER_BOARD_DIAG]"
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP && RIDER_BOARD_DIAG_ENABLE

#define RIDER_BOARD_DIAG_SCAN_MS             100
#define RIDER_BOARD_DIAG_KEY_FILTER_SAMPLES  3
#define RIDER_BOARD_DIAG_LED_TEST_TICKS      12
#define RIDER_BOARD_DIAG_BUTTON_FLASH_TICKS  6

enum rider_diag_key_index {
    RIDER_DIAG_KEY_1 = 0,
    RIDER_DIAG_KEY_2,
    RIDER_DIAG_KEY_COUNT,
};

static u16 rider_diag_timer_id;
static u32 rider_diag_tick_count;
static u8 rider_diag_key_raw[RIDER_DIAG_KEY_COUNT];
static u8 rider_diag_key_stable[RIDER_DIAG_KEY_COUNT];
static u8 rider_diag_key_filter[RIDER_DIAG_KEY_COUNT];
static u8 rider_diag_led_test_ticks;
static u8 rider_diag_led_test_step;
static u8 rider_diag_button_flash_ticks;
static u8 rider_diag_last_ble_state = 0xff;
static u8 rider_diag_last_temp_valid = 0xff;
static u8 rider_diag_last_temp_status = 0xff;

/** Return whether a board mapping names a real AC632N GPIO. */
static u8 rider_diag_port_valid(u32 port)
{
    return port != (u32)NO_CONFIG_PORT && port < IO_MAX_NUM;
}

/** Drive one active-high/active-low LED without changing its board mapping. */
static void rider_diag_led_set(u32 port, u8 on)
{
    u8 value;

    if (!rider_diag_port_valid(port)) {
        return;
    }
    value = on ? RIDER_BOARD_DIAG_LED_ACTIVE_LEVEL
               : !RIDER_BOARD_DIAG_LED_ACTIVE_LEVEL;
    gpio_set_output_value(port, value);
    gpio_set_direction(port, 0);
}

/** Turn off all diagnostic LEDs before a new pattern or shutdown. */
static void rider_diag_leds_off(void)
{
    rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 0);
    rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT, 0);
    rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 0);
}

/** Configure one LED output and keep its pull resistors disabled. */
static void rider_diag_led_init(u32 port)
{
    if (!rider_diag_port_valid(port)) {
        return;
    }
    gpio_set_die(port, 1);
    gpio_set_pull_up(port, 0);
    gpio_set_pull_down(port, 0);
    rider_diag_led_set(port, 0);
}

/** Configure one J12 button input as active-low with an internal pull-up. */
static void rider_diag_key_init(u32 port)
{
    if (!rider_diag_port_valid(port)) {
        return;
    }
    gpio_set_die(port, 1);
    gpio_set_pull_down(port, 0);
    gpio_set_pull_up(port, 1);
    gpio_set_direction(port, 1);
}

/** Read a configured J12 button, treating an absent mapping as released. */
static u8 rider_diag_key_pressed(u32 port)
{
    if (!rider_diag_port_valid(port)) {
        return 0;
    }
    return gpio_read(port) == RIDER_BOARD_DIAG_KEY_ACTIVE_LEVEL;
}

/** Log a full diagnostic snapshot when BLE or sensor state changes. */
static void rider_diag_log_state(const rider_temperature_snapshot_t *snapshot,
                                 enum rider_ble_state ble_state)
{
    if (!snapshot) {
        return;
    }
    if (rider_diag_last_ble_state == (u8)ble_state &&
        rider_diag_last_temp_valid == snapshot->valid &&
        rider_diag_last_temp_status == snapshot->sensor_status) {
        return;
    }
    rider_diag_last_ble_state = (u8)ble_state;
    rider_diag_last_temp_valid = snapshot->valid;
    rider_diag_last_temp_status = snapshot->sensor_status;
    log_info("state: ble=%u temp_valid=%u temp_status=%u seq=%u core_centi=%d "
             "skin=NA average=NA\n",
             (unsigned)ble_state, (unsigned)snapshot->valid,
             (unsigned)snapshot->sensor_status, (unsigned)snapshot->sequence,
             snapshot->valid ? (int)snapshot->core_temperature_centi : 32767);
}

/** Render the one-button LED self-test sequence. */
static void rider_diag_render_led_test(void)
{
    u8 step;

    rider_diag_leds_off();
    if (!rider_diag_led_test_ticks) {
        return;
    }
    step = rider_diag_led_test_step % 3;
    if (step == 0) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 1);
    } else if (step == 1) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT, 1);
    } else {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 1);
    }
}

/** Render BLE state, M601 state, and the short button acknowledgement. */
static void rider_diag_render_status(const rider_temperature_snapshot_t *snapshot,
                                     enum rider_ble_state ble_state)
{
    u8 sensor_fault = snapshot && !snapshot->valid;

    rider_diag_leds_off();

    /* Red LED1 is the BLE state: slow blink while advertising, solid when linked. */
    if (ble_state == RIDER_BLE_STATE_ADVERTISING) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT,
                           (rider_diag_tick_count % 10) < 5);
    } else if (ble_state == RIDER_BLE_STATE_CONNECTED) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 1);
    }

    /* Green LED2 is the sensor state; a valid sample is steady, no device is fast. */
    if (snapshot && snapshot->valid) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT, 1);
    } else if (sensor_fault && snapshot->sensor_status == RIDER_TEMP_STATUS_NO_DEVICE) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT,
                           (rider_diag_tick_count % 4) < 2);
    }

    /* Blue LED3 distinguishes CRC/range faults and button-2 state dumps. */
    if (snapshot && sensor_fault &&
        snapshot->sensor_status == RIDER_TEMP_STATUS_CRC_ERROR) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT,
                           (rider_diag_tick_count % 10) < 5);
    } else if (snapshot && sensor_fault &&
               snapshot->sensor_status == RIDER_TEMP_STATUS_RANGE_ERROR) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT,
                           (rider_diag_tick_count % 10 == 0) ||
                           (rider_diag_tick_count % 10 == 2));
    } else if (rider_diag_button_flash_ticks &&
               (rider_diag_tick_count & 1)) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 1);
    }
}

/** Print the current sensor and BLE status on the requested button action. */
static void rider_diag_dump_state(void)
{
    rider_temperature_snapshot_t snapshot;
    enum rider_ble_state ble_state = rider_core_temp_ble_state();

    rider_estimator_copy_snapshot(&snapshot);
    log_info("button dump: ble=%u temp_valid=%u status=%u seq=%u core_centi=%d "
             "skin=NA average=NA\n",
             (unsigned)ble_state, (unsigned)snapshot.valid,
             (unsigned)snapshot.sensor_status, (unsigned)snapshot.sequence,
             snapshot.valid ? (int)snapshot.core_temperature_centi : 32767);
}

/** Handle a debounced button press without changing product protocol state. */
static void rider_diag_button_pressed(enum rider_diag_key_index index)
{
    if (index == RIDER_DIAG_KEY_1) {
        rider_diag_led_test_ticks = RIDER_BOARD_DIAG_LED_TEST_TICKS;
        rider_diag_led_test_step = 0;
        log_info("button: IOKey1 -> LED self-test\n");
    } else {
        rider_diag_button_flash_ticks = RIDER_BOARD_DIAG_BUTTON_FLASH_TICKS;
        rider_diag_dump_state();
        log_info("button: IOKey2 -> state dump\n");
    }
}

/** Debounce both active-low J12 buttons and emit only press transitions. */
static void rider_diag_scan_keys(void)
{
    static const u32 key_ports[RIDER_DIAG_KEY_COUNT] = {
        RIDER_BOARD_DIAG_IOKEY1_PORT,
        RIDER_BOARD_DIAG_IOKEY2_PORT,
    };
    u8 index;

    for (index = 0; index < RIDER_DIAG_KEY_COUNT; ++index) {
        u8 raw = rider_diag_key_pressed(key_ports[index]);
        if (raw != rider_diag_key_raw[index]) {
            rider_diag_key_raw[index] = raw;
            rider_diag_key_filter[index] = 0;
            continue;
        }
        if (rider_diag_key_filter[index] < RIDER_BOARD_DIAG_KEY_FILTER_SAMPLES) {
            rider_diag_key_filter[index]++;
            continue;
        }
        if (rider_diag_key_stable[index] != raw) {
            rider_diag_key_stable[index] = raw;
            if (raw) {
                rider_diag_button_pressed((enum rider_diag_key_index)index);
            }
        }
    }
}

/** Poll buttons and update the board pattern at a human-visible cadence. */
static void rider_diag_timer(void *priv)
{
    rider_temperature_snapshot_t snapshot;
    enum rider_ble_state ble_state;

    (void)priv;
    rider_diag_tick_count++;
    rider_diag_scan_keys();

    if (rider_diag_led_test_ticks) {
        rider_diag_render_led_test();
        rider_diag_led_test_ticks--;
        rider_diag_led_test_step++;
        return;
    }

    rider_estimator_copy_snapshot(&snapshot);
    ble_state = rider_core_temp_ble_state();
    rider_diag_log_state(&snapshot, ble_state);
    rider_diag_render_status(&snapshot, ble_state);
    if (rider_diag_button_flash_ticks) {
        rider_diag_button_flash_ticks--;
    }
}

/** Start GPIO diagnostics after the estimator and BLE stack are available. */
void rider_board_diag_init(void)
{
    if (rider_diag_timer_id) {
        return;
    }

    rider_diag_tick_count = 0;
    rider_diag_led_test_ticks = 0;
    rider_diag_led_test_step = 0;
    rider_diag_button_flash_ticks = 0;
    memset(rider_diag_key_raw, 0, sizeof(rider_diag_key_raw));
    memset(rider_diag_key_stable, 0, sizeof(rider_diag_key_stable));
    memset(rider_diag_key_filter, 0, sizeof(rider_diag_key_filter));
    rider_diag_last_ble_state = 0xff;
    rider_diag_last_temp_valid = 0xff;
    rider_diag_last_temp_status = 0xff;

    rider_diag_led_init(RIDER_BOARD_DIAG_LED1_PORT);
    rider_diag_led_init(RIDER_BOARD_DIAG_LED2_PORT);
    rider_diag_led_init(RIDER_BOARD_DIAG_LED3_PORT);
    rider_diag_key_init(RIDER_BOARD_DIAG_IOKEY1_PORT);
    rider_diag_key_init(RIDER_BOARD_DIAG_IOKEY2_PORT);
    log_info("init: J12 LED1=%u LED2=%u LED3=%u IOKey1=%u IOKey2=%u\n",
             (unsigned)RIDER_BOARD_DIAG_LED1_PORT,
             (unsigned)RIDER_BOARD_DIAG_LED2_PORT,
             (unsigned)RIDER_BOARD_DIAG_LED3_PORT,
             (unsigned)RIDER_BOARD_DIAG_IOKEY1_PORT,
             (unsigned)RIDER_BOARD_DIAG_IOKEY2_PORT);
    rider_diag_timer(NULL);
    rider_diag_timer_id = sys_timer_add(NULL, rider_diag_timer,
                                        RIDER_BOARD_DIAG_SCAN_MS);
}

/** Stop the diagnostic timer and make all LED GPIOs quiet inputs. */
void rider_board_diag_stop(void)
{
    u32 ports[] = {
        RIDER_BOARD_DIAG_LED1_PORT,
        RIDER_BOARD_DIAG_LED2_PORT,
        RIDER_BOARD_DIAG_LED3_PORT,
    };
    u8 index;

    if (rider_diag_timer_id) {
        sys_timer_del(rider_diag_timer_id);
        rider_diag_timer_id = 0;
    }
    rider_diag_leds_off();
    for (index = 0; index < ARRAY_SIZE(ports); ++index) {
        if (rider_diag_port_valid(ports[index])) {
            gpio_set_pull_up(ports[index], 0);
            gpio_set_pull_down(ports[index], 0);
            gpio_set_direction(ports[index], 1);
        }
    }
}

#elif CONFIG_APP_RIDER_CORE_TEMP

/** Keep the lifecycle contract link-safe when board diagnostics are disabled. */
void rider_board_diag_init(void)
{
}

/** Keep the lifecycle contract link-safe when board diagnostics are disabled. */
void rider_board_diag_stop(void)
{
}

#endif
