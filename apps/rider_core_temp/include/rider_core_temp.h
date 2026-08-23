#ifndef RIDER_CORE_TEMP_H
#define RIDER_CORE_TEMP_H

#include <stdint.h>

/* Keep the product header self-contained when host-side module tests include
 * it without the SDK's bt_event definition. */
struct bt_event;

/* A reviewed calibration header may be dropped into this include directory.
 * Keeping it optional leaves the checked-in build in shadow mode. */
#if defined(__has_include)
#if __has_include("rider_core_temp_calibration.h")
#include "rider_core_temp_calibration.h"
#endif
#endif

typedef struct {
    uint8_t same_address;
    uint16_t appearance;
} rider_ble_init_cfg_t;

enum rider_temperature_status {
    RIDER_TEMP_STATUS_OK = 0,
    RIDER_TEMP_STATUS_NO_DEVICE = 1,
    RIDER_TEMP_STATUS_CRC_ERROR = 2,
    RIDER_TEMP_STATUS_RANGE_ERROR = 3,
    RIDER_TEMP_STATUS_NOT_WORN = 4,
};

/* The state describes the signal lifecycle, independently of the 1-Wire
 * transport status.  A stable contact sample is required before it can be
 * published as skin temperature or used by the shadow core estimator. */
enum rider_temperature_state {
    RIDER_TEMP_STATE_NO_DEVICE = 0,
    RIDER_TEMP_STATE_NOT_WORN = 1,
    RIDER_TEMP_STATE_WARMING = 2,
    RIDER_TEMP_STATE_STABLE = 3,
    RIDER_TEMP_STATE_STALE = 4,
};

enum rider_temperature_quality {
    RIDER_TEMP_QUALITY_INVALID = 0,
    RIDER_TEMP_QUALITY_POOR = 1,
    RIDER_TEMP_QUALITY_FAIR = 2,
    RIDER_TEMP_QUALITY_GOOD = 3,
    RIDER_TEMP_QUALITY_EXCELLENT = 4,
    RIDER_TEMP_QUALITY_NA = 7,
};

enum rider_temperature_freshness {
    RIDER_TEMP_FRESHNESS_UNAVAILABLE = 0,
    RIDER_TEMP_FRESHNESS_FRESH = 1,
    RIDER_TEMP_FRESHNESS_STALE = 2,
};

/* M601 also measures ambient temperature when it is off-body.  The driver
 * keeps the electrical range; this product window decides what may be
 * published as a body-temperature sample and can be calibrated per product.
 */
#define RIDER_CORE_TEMP_WEAR_MIN_CENTI       3000
#define RIDER_CORE_TEMP_WEAR_MAX_CENTI       4500

/* Filter parameters are deliberately conservative for the fixed chest-strap
 * prototype.  They remain compile-time product settings until field data is
 * available to calibrate the mechanical design and wear window. */
#define RIDER_TEMP_FILTER_MEDIAN_SAMPLES     5
#define RIDER_TEMP_FILTER_STABLE_SAMPLES     30
#define RIDER_TEMP_FILTER_MAX_GAP_SAMPLES    3
#define RIDER_TEMP_STALE_AFTER_TICKS         3
#define RIDER_TEMP_FILTER_EWMA_ALPHA_Q8      64
#define RIDER_TEMP_FILTER_SLOPE_LIMIT_CPM    150

/* Core estimation remains a shadow feature until a calibration file is
 * installed and passes the held-out-session error gate. A product board may
 * explicitly select CONTACT_PROXY for bring-up compatibility; that mode
 * publishes stable contact temperature but does not validate it as core. */
#define RIDER_CORE_TEMP_PUBLISH_SHADOW       0
#define RIDER_CORE_TEMP_PUBLISH_STRICT       1
#define RIDER_CORE_TEMP_PUBLISH_CONTACT_PROXY 2
#ifndef RIDER_CORE_TEMP_PUBLISH_MODE
#define RIDER_CORE_TEMP_PUBLISH_MODE         RIDER_CORE_TEMP_PUBLISH_SHADOW
#endif

#ifndef RIDER_CORE_TEMP_CALIBRATION_VALID
#define RIDER_CORE_TEMP_CALIBRATION_VALID    0
#endif
#ifndef RIDER_CORE_TEMP_CALIBRATION_AVAILABLE
#define RIDER_CORE_TEMP_CALIBRATION_AVAILABLE RIDER_CORE_TEMP_CALIBRATION_VALID
#endif
#ifndef RIDER_CORE_TEMP_CAL_OFFSET_CENTI
#define RIDER_CORE_TEMP_CAL_OFFSET_CENTI     0
#endif
#ifndef RIDER_CORE_TEMP_CAL_SLOPE_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_SLOPE_GAIN_Q8    0
#endif
#ifndef RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8
#define RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8     32
#endif
#define RIDER_CORE_TEMP_ESTIMATE_MIN_CENTI   3500
#define RIDER_CORE_TEMP_ESTIMATE_MAX_CENTI   4200
/* The scheduler consumes one sample per second.  This is a physiological
 * plausibility guard, not a learned core-temperature offset. */
#define RIDER_CORE_TEMP_ESTIMATE_MAX_STEP_CENTI 25
#define RIDER_CORE_TEMP_MIN_CONFIDENCE        70

typedef struct {
    uint32_t sequence;
    int16_t temperature_centi;
    uint8_t valid;
    uint8_t status;
} rider_temperature_sample_t;

typedef struct {
    uint32_t sequence;
    int16_t filtered_temperature_centi;
    int16_t slope_centi_per_min;
    uint8_t valid;
    uint8_t quality;
    uint8_t status;
    uint8_t state;
    uint8_t freshness;
} rider_temperature_filter_output_t;

typedef struct {
    int16_t offset_centi;
    int16_t slope_gain_q8;
    uint8_t lag_alpha_q8;
    uint8_t available;
    uint8_t valid;
} rider_core_temperature_calibration_t;

typedef struct {
    uint32_t sequence;
    int16_t contact_temperature_centi;
    int16_t skin_temperature_centi;
    int16_t core_temperature_centi;
    int16_t slope_centi_per_min;
    uint8_t valid;
    uint8_t contact_valid;
    uint8_t skin_valid;
    uint8_t core_estimate_valid;     /* Numerical candidate is available. */
    uint8_t core_estimate_verified;  /* Held-out validation gate has passed. */
    uint8_t quality;
    uint8_t confidence;
    uint8_t sensor_status;
    uint8_t temperature_state;
    uint8_t data_freshness;
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

/** Reset the product-side filter and wear-state machine. */
void rider_temp_filter_init(void);

/** Consume one raw sample and produce a filtered contact-temperature state. */
void rider_temp_filter_consume(const rider_temperature_sample_t *sample,
                               rider_temperature_filter_output_t *output);

void rider_estimator_init(void);
void rider_estimator_consume(const rider_temperature_sample_t *sample);
void rider_estimator_copy_snapshot(rider_temperature_snapshot_t *snapshot);
/** Mark the snapshot stale when the sensor sequence stops advancing. */
void rider_estimator_tick(uint32_t latest_sequence);
/** Install or clear the offline-fitted single-sensor core calibration. */
void rider_estimator_set_core_calibration(
    const rider_core_temperature_calibration_t *calibration);
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

#define RIDER_CORE_TEMP_NAME "ICXL-RTemp"
#define RIDER_CORE_TEMP_MANUFACTURER "ICXL"
#define RIDER_CORE_TEMP_MODEL "CoreTemp-Rider"
#define RIDER_CORE_TEMP_FIRMWARE_VERSION "0.1.0"

#endif
