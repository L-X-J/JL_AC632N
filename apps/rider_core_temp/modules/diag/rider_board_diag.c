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
static u8 rider_diag_last_temp_state = 0xff;
static u8 rider_diag_last_core_state = 0xff;
static u8 rider_diag_last_temp_freshness = 0xff;
static u8 rider_diag_power_led_override;
static u8 rider_diag_gpio_ready;

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

/** 电源灯与红灯同脚时，仲裁占用期间不改写该脚。 */
static u8 rider_diag_led1_is_power_led(void)
{
    return rider_diag_port_valid(RIDER_BOARD_DIAG_LED1_PORT) &&
           RIDER_BOARD_DIAG_LED1_PORT == RIDER_BOARD_POWER_LED_PORT;
}

/** 新图案或关机前熄灭诊断灯；电源仲裁占用时跳过电源/红灯脚。 */
static void rider_diag_leds_off(void)
{
    if (!(rider_diag_power_led_override && rider_diag_led1_is_power_led())) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 0);
    }
    if (!rider_diag_power_led_override) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT, 0);
        if (!rider_diag_port_valid(RIDER_BOARD_DIAG_LED2_PORT)) {
            /* 无独立绿灯时，温度状态挂在蓝灯，熄灭一并处理 */
            rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 0);
            return;
        }
    }
    rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 0);
}

/** Force every LED off when the diagnostic owner itself is being stopped. */
static void rider_diag_leds_off_force(void)
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

/** 将按键脚配置为低电平有效输入并开启内部上拉。 */
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
        rider_diag_last_temp_valid == snapshot->contact_valid &&
        rider_diag_last_temp_status == snapshot->sensor_status &&
        rider_diag_last_temp_state == snapshot->temperature_state &&
        rider_diag_last_core_state == snapshot->core_state &&
        rider_diag_last_temp_freshness == snapshot->data_freshness) {
        return;
    }
    rider_diag_last_ble_state = (u8)ble_state;
    rider_diag_last_temp_valid = snapshot->contact_valid;
    rider_diag_last_temp_status = snapshot->sensor_status;
    rider_diag_last_temp_state = snapshot->temperature_state;
    rider_diag_last_core_state = snapshot->core_state;
    rider_diag_last_temp_freshness = snapshot->data_freshness;
    log_info("state: ble=%u sensor_valid=%u contact_valid=%u skin_valid=%u "
             "core_valid=%u core_verified=%u status=%u skin_state=%u "
             "core_state=%u freshness=%u sensor_seq=%u skin_seq=%u "
             "core_seq=%u sensor_centi=%d skin_centi=%d core_centi=%d "
             "history_s=%u contact_samples=%u typical=%u model=v%u/%u "
             "hr=%u hr_used=%u\n",
             (unsigned)ble_state, (unsigned)snapshot->sensor_valid,
             (unsigned)snapshot->contact_valid,
             (unsigned)snapshot->skin_valid,
             (unsigned)snapshot->core_estimate_valid,
             (unsigned)snapshot->core_estimate_verified,
             (unsigned)snapshot->sensor_status,
             (unsigned)snapshot->temperature_state,
             (unsigned)snapshot->core_state,
             (unsigned)snapshot->data_freshness,
             (unsigned)snapshot->sequence,
             (unsigned)snapshot->skin_source_sequence,
             (unsigned)snapshot->core_source_sequence,
             snapshot->sensor_valid ? (int)snapshot->sensor_temperature_centi : 32767,
             snapshot->skin_valid ? (int)snapshot->skin_temperature_centi : 32767,
             snapshot->core_estimate_valid ? (int)snapshot->core_temperature_centi : 32767,
             (unsigned)snapshot->core_history_seconds,
             (unsigned)snapshot->contact_samples,
             (unsigned)snapshot->typical_samples,
             (unsigned)snapshot->model_version,
             (unsigned)snapshot->model_mode,
             (unsigned)snapshot->heart_rate,
             (unsigned)snapshot->heart_rate_used);
}

/** LED 自检：红→（可选绿）→蓝；无绿灯时第二拍仍亮红灯脚。 */
static void rider_diag_render_led_test(void)
{
    u8 step;

    rider_diag_leds_off();
    if (!rider_diag_led_test_ticks) {
        return;
    }
    step = rider_diag_led_test_step % 3;
    if (step == 0) {
        if (!(rider_diag_power_led_override && rider_diag_led1_is_power_led())) {
            rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 1);
        }
    } else if (step == 1) {
        if (rider_diag_port_valid(RIDER_BOARD_DIAG_LED2_PORT) &&
            !rider_diag_power_led_override) {
            rider_diag_led_set(RIDER_BOARD_DIAG_LED2_PORT, 1);
        } else if (!(rider_diag_power_led_override &&
                     rider_diag_led1_is_power_led())) {
            rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 1);
        }
    } else {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 1);
    }
}

/** 在蓝灯上渲染传感器/温度状态（新板无独立绿灯）。 */
static void rider_diag_render_temp_on_port(u32 port,
                                           const rider_temperature_snapshot_t *snapshot,
                                           u8 sensor_fault)
{
    if (!rider_diag_port_valid(port) || !snapshot) {
        return;
    }
    if (snapshot->skin_valid &&
        snapshot->sensor_status == RIDER_TEMP_STATUS_OK &&
        snapshot->data_freshness == RIDER_TEMP_FRESHNESS_FRESH &&
        snapshot->core_state == RIDER_CORE_STATE_READY &&
        snapshot->core_estimate_valid) {
        rider_diag_led_set(port, 1);
    } else if (snapshot->contact_valid &&
               snapshot->sensor_status == RIDER_TEMP_STATUS_OK &&
               snapshot->data_freshness == RIDER_TEMP_FRESHNESS_FRESH &&
               (snapshot->temperature_state ==
                    RIDER_TEMP_STATE_CONTACT_SETTLING ||
                snapshot->temperature_state ==
                    RIDER_TEMP_STATE_SKIN_TRUSTED ||
                snapshot->temperature_state ==
                    RIDER_TEMP_STATE_DETACH_SUSPECTED)) {
        rider_diag_led_set(port, (rider_diag_tick_count % 10) < 5);
    } else if (sensor_fault &&
               snapshot->sensor_status == RIDER_TEMP_STATUS_NO_DEVICE) {
        rider_diag_led_set(port, (rider_diag_tick_count % 4) < 2);
    }
}

/** 渲染 BLE 红灯、传感器蓝灯状态。 */
static void rider_diag_render_status(const rider_temperature_snapshot_t *snapshot,
                                     enum rider_ble_state ble_state)
{
    u8 sensor_fault = snapshot && !snapshot->contact_valid;
    u8 has_green = rider_diag_port_valid(RIDER_BOARD_DIAG_LED2_PORT);

    rider_diag_leds_off();

    /* 红灯：广播慢闪，连接常亮；与电源灯同脚时，仲裁占用期间不改写 */
    if (!(rider_diag_power_led_override && rider_diag_led1_is_power_led())) {
        if (ble_state == RIDER_BLE_STATE_ADVERTISING) {
            rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT,
                               (rider_diag_tick_count % 10) < 5);
        } else if (ble_state == RIDER_BLE_STATE_CONNECTED) {
            rider_diag_led_set(RIDER_BOARD_DIAG_LED1_PORT, 1);
        }
    }

    /* 有独立绿灯时仍走 LED2；新板无绿灯则温度状态挂蓝灯 */
    if (!rider_diag_power_led_override && has_green) {
        rider_diag_render_temp_on_port(RIDER_BOARD_DIAG_LED2_PORT, snapshot,
                                       sensor_fault);
    }

    /* 蓝灯：CRC/超范围优先；否则挂温度状态（无绿灯时）或 KEY2 短闪（已关闭业务） */
    if (snapshot && sensor_fault &&
        snapshot->sensor_status == RIDER_TEMP_STATUS_CRC_ERROR) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT,
                           (rider_diag_tick_count % 10) < 5);
    } else if (snapshot && sensor_fault &&
               (snapshot->sensor_status == RIDER_TEMP_STATUS_RANGE_ERROR ||
                snapshot->sensor_status == RIDER_TEMP_STATUS_NOT_WORN)) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT,
                           (rider_diag_tick_count % 10 == 0) ||
                           (rider_diag_tick_count % 10 == 2));
    } else if (!has_green) {
        rider_diag_render_temp_on_port(RIDER_BOARD_DIAG_LED3_PORT, snapshot,
                                       sensor_fault);
    } else if (rider_diag_button_flash_ticks &&
               (rider_diag_tick_count & 1)) {
        rider_diag_led_set(RIDER_BOARD_DIAG_LED3_PORT, 1);
    }
}

/** 将电源/红灯交由电源键状态机独占；其它诊断灯仍可刷新。 */
void rider_board_diag_power_led_claim(uint8_t on)
{
    rider_diag_power_led_override = 1;
    if (!rider_diag_gpio_ready) {
        rider_diag_led_init(RIDER_BOARD_POWER_LED_PORT);
        rider_diag_gpio_ready = 1;
    }
    rider_diag_led_set(RIDER_BOARD_POWER_LED_PORT, on);
}

/** 释放电源/红灯，交还诊断渲染并刷新当前温度状态。 */
void rider_board_diag_power_led_release(void)
{
    rider_temperature_snapshot_t snapshot;

    rider_diag_power_led_override = 0;
    if (!rider_diag_gpio_ready) {
        rider_diag_led_set(RIDER_BOARD_POWER_LED_PORT, 0);
        return;
    }
    rider_estimator_copy_snapshot(&snapshot);
    rider_diag_render_status(&snapshot, rider_core_temp_ble_state());
}

/** Print the current sensor and BLE status on the requested button action. */
static void rider_diag_dump_state(void)
{
    rider_temperature_snapshot_t snapshot;
    enum rider_ble_state ble_state = rider_core_temp_ble_state();

    rider_estimator_copy_snapshot(&snapshot);
    log_info("button dump: ble=%u sensor_valid=%u contact_valid=%u "
             "skin_valid=%u core_input=%u core_valid=%u core_verified=%u "
             "status=%u skin_state=%u core_state=%u freshness=%u "
             "sensor_seq=%u skin_seq=%u core_seq=%u sensor_centi=%d "
             "skin_centi=%d core_centi=%d confidence=%u history_s=%u "
             "contact_samples=%u typical=%u model=v%u/%u hr=%u "
             "hr_valid=%u hr_used=%u\n",
             (unsigned)ble_state, (unsigned)snapshot.sensor_valid,
             (unsigned)snapshot.contact_valid,
             (unsigned)snapshot.skin_valid,
             (unsigned)snapshot.core_input_valid,
             (unsigned)snapshot.core_estimate_valid,
             (unsigned)snapshot.core_estimate_verified,
             (unsigned)snapshot.sensor_status,
             (unsigned)snapshot.temperature_state,
             (unsigned)snapshot.core_state,
             (unsigned)snapshot.data_freshness,
             (unsigned)snapshot.sequence,
             (unsigned)snapshot.skin_source_sequence,
             (unsigned)snapshot.core_source_sequence,
             snapshot.sensor_valid ? (int)snapshot.sensor_temperature_centi : 32767,
             snapshot.skin_valid ? (int)snapshot.skin_temperature_centi : 32767,
             snapshot.core_estimate_valid ? (int)snapshot.core_temperature_centi : 32767,
             (unsigned)snapshot.confidence,
             (unsigned)snapshot.core_history_seconds,
             (unsigned)snapshot.contact_samples,
             (unsigned)snapshot.typical_samples,
             (unsigned)snapshot.model_version,
             (unsigned)snapshot.model_mode,
             (unsigned)snapshot.heart_rate,
             (unsigned)snapshot.heart_rate_valid,
             (unsigned)snapshot.heart_rate_used);
}

/** 诊断按键回调：KEY1 已移交电源状态机；KEY2 本轮仅输入、无业务。 */
static void rider_diag_button_pressed(enum rider_diag_key_index index)
{
    if (index == RIDER_DIAG_KEY_1) {
        /* DIAG_IOKEY1 已置 NO_CONFIG，正常不会进入；保留空实现防误扫 */
        log_info("button: KEY1 由电源状态机处理，诊断忽略\n");
        return;
    }
    /* KEY2：只初始化为输入，本轮不上报 BLE、不打印、不闪灯 */
    (void)index;
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

/** 在估算器与 BLE 就绪后启动板级 GPIO 诊断。 */
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
    rider_diag_last_temp_state = 0xff;
    rider_diag_last_core_state = 0xff;
    rider_diag_last_temp_freshness = 0xff;
    rider_diag_power_led_override = 0;
    rider_diag_gpio_ready = 0;

    rider_diag_led_init(RIDER_BOARD_DIAG_LED1_PORT);
    rider_diag_led_init(RIDER_BOARD_DIAG_LED2_PORT);
    rider_diag_led_init(RIDER_BOARD_DIAG_LED3_PORT);
    rider_diag_gpio_ready = 1;
    rider_diag_key_init(RIDER_BOARD_DIAG_IOKEY1_PORT);
    rider_diag_key_init(RIDER_BOARD_DIAG_IOKEY2_PORT);
    log_info("init: RED=%u GREEN=%u BLUE=%u KEY1_diag=%u KEY2=%u power_led=%u\n",
             (unsigned)RIDER_BOARD_DIAG_LED1_PORT,
             (unsigned)RIDER_BOARD_DIAG_LED2_PORT,
             (unsigned)RIDER_BOARD_DIAG_LED3_PORT,
             (unsigned)RIDER_BOARD_DIAG_IOKEY1_PORT,
             (unsigned)RIDER_BOARD_DIAG_IOKEY2_PORT,
             (unsigned)RIDER_BOARD_POWER_LED_PORT);
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
    rider_diag_power_led_override = 0;
    rider_diag_leds_off_force();
    rider_diag_gpio_ready = 0;
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

/** Keep the power-key LED contract link-safe when diagnostics are disabled. */
void rider_board_diag_power_led_claim(uint8_t on)
{
    (void)on;
}

/** Keep the power-key LED contract link-safe when diagnostics are disabled. */
void rider_board_diag_power_led_release(void)
{
}

#endif
