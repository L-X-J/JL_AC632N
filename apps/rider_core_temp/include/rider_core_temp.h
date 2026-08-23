#ifndef RIDER_CORE_TEMP_H
#define RIDER_CORE_TEMP_H

#include <stdint.h>

typedef struct {
    uint8_t same_address;
    uint16_t appearance;
} rider_ble_init_cfg_t;

enum rider_temperature_status {
    RIDER_TEMP_STATUS_OK = 0,
    RIDER_TEMP_STATUS_NO_DEVICE = 1,
    RIDER_TEMP_STATUS_CRC_ERROR = 2,
    RIDER_TEMP_STATUS_RANGE_ERROR = 3,
};

typedef struct {
    uint32_t sequence;
    int16_t temperature_centi;
    uint8_t valid;
    uint8_t status;
} rider_temperature_sample_t;

typedef struct {
    uint32_t sequence;
    int16_t core_temperature_centi;
    uint8_t valid;
    uint8_t quality;
    uint8_t sensor_status;
    uint8_t heart_rate;
    uint8_t heart_rate_valid;
} rider_temperature_snapshot_t;

/** BLE state used by the board diagnostic indicator, independent of GATT data. */
enum rider_ble_state {
    RIDER_BLE_STATE_OFF = 0,
    RIDER_BLE_STATE_ADVERTISING = 1,
    RIDER_BLE_STATE_CONNECTED = 2,
};

void rider_temp_init(void);
void rider_temp_stop(void);
void rider_temp_start_conversion(void);
uint32_t rider_temp_sequence(void);
int rider_temp_copy_latest(rider_temperature_sample_t *sample);

void rider_estimator_init(void);
void rider_estimator_consume(const rider_temperature_sample_t *sample);
void rider_estimator_copy_snapshot(rider_temperature_snapshot_t *snapshot);
void rider_estimator_set_external_heart_rate(uint8_t heart_rate, uint8_t valid);

void rider_core_temp_start_scheduler(void);
void rider_core_temp_stop_scheduler(void);

void rider_core_temp_gatt_before_init(void);
void rider_core_temp_gatt_init(void);
void rider_core_temp_gatt_exit(void);
void rider_core_temp_ble_tick(void);
enum rider_ble_state rider_core_temp_ble_state(void);

void bt_ble_before_start_init(void);
void bt_ble_init(void);
void bt_ble_exit(void);
void ble_module_enable(uint8_t enable);
void btstack_ble_start_before_init(const rider_ble_init_cfg_t *cfg, int param);
void btstack_ble_exit(int param);
int bt_comm_ble_status_event_handler(struct bt_event *bt);
int bt_comm_ble_hci_event_handler(struct bt_event *bt);

#define RIDER_CORE_TEMP_NAME "ICXL-CoreTemp-Rider"
#define RIDER_CORE_TEMP_MANUFACTURER "ICXL"
#define RIDER_CORE_TEMP_MODEL "CoreTemp-Rider"
#define RIDER_CORE_TEMP_FIRMWARE_VERSION "0.1.0"

#endif
