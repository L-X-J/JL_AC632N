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

/* Contact qualification is independent of both the 1-Wire transport and the
 * core model.  CONTACT_SETTLING may expose a filtered contact diagnostic, but
 * only SKIN_TRUSTED is allowed onto the trusted-skin timeline. */
enum rider_temperature_state {
    RIDER_TEMP_STATE_NO_DEVICE = 0,
    RIDER_TEMP_STATE_NOT_WORN = 1,
    RIDER_TEMP_STATE_CONTACT_SETTLING = 2,
    RIDER_TEMP_STATE_SKIN_TRUSTED = 3,
    RIDER_TEMP_STATE_DETACH_SUSPECTED = 4,
    RIDER_TEMP_STATE_STALE = 5,
};

/* Source compatibility for board diagnostics and older host integrations.
 * New code must use the names above because skin trust is not thermal
 * steadiness and does not imply that the core model is ready. */
#define RIDER_TEMP_STATE_WARMING RIDER_TEMP_STATE_CONTACT_SETTLING
#define RIDER_TEMP_STATE_STABLE  RIDER_TEMP_STATE_SKIN_TRUSTED

/** Lifecycle of the experimental core estimate, separate from skin trust. */
enum rider_core_temperature_state {
    RIDER_CORE_STATE_EMPTY = 0,
    RIDER_CORE_STATE_WARMUP = 1,
    RIDER_CORE_STATE_READY = 2,
    RIDER_CORE_STATE_HOLD = 3,
    RIDER_CORE_STATE_INVALID = 4,
};

/** Identify which feature set produced the current experimental candidate. */
enum rider_core_model_mode {
    RIDER_CORE_MODEL_SKIN_ONLY = 0,
    RIDER_CORE_MODEL_SKIN_AND_HR = 1,
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

/* Filter parameters are bring-up defaults for the fixed chest-strap probe.
 * Five samples fill the robust median window; they never qualify skin or core
 * by themselves.  Thirty contiguous valid samples establish trusted skin.
 * The 32~40 C band is only typical-contact evidence and detach recovery, not
 * a physiological validity range. */
#define RIDER_TEMP_FILTER_MEDIAN_SAMPLES     5
#define RIDER_TEMP_FILTER_TRUSTED_SAMPLES    30
#define RIDER_TEMP_FILTER_TYPICAL_SAMPLES    5
#define RIDER_TEMP_FILTER_TYPICAL_MIN_CENTI  3200
#define RIDER_TEMP_FILTER_TYPICAL_MAX_CENTI  4000
/* Deprecated parameter aliases retained for out-of-tree host tests. */
#define RIDER_TEMP_FILTER_STABLE_SAMPLES RIDER_TEMP_FILTER_TRUSTED_SAMPLES
#define RIDER_TEMP_FILTER_NORMAL_SAMPLES RIDER_TEMP_FILTER_TYPICAL_SAMPLES
#define RIDER_TEMP_FILTER_NORMAL_MIN_CENTI RIDER_TEMP_FILTER_TYPICAL_MIN_CENTI
#define RIDER_TEMP_FILTER_NORMAL_MAX_CENTI RIDER_TEMP_FILTER_TYPICAL_MAX_CENTI
#define RIDER_TEMP_FILTER_MAX_GAP_SAMPLES    3
#define RIDER_TEMP_STALE_AFTER_TICKS         3
#define RIDER_TEMP_FILTER_EWMA_ALPHA_Q8      64
#define RIDER_TEMP_FILTER_SLOPE_LIMIT_CPM    150
/* A fast fall starts a hold-off before the core model sees the sample. The
 * values are product bring-up defaults, not physiological constants; they
 * must be calibrated with real detach/re-attach traces before release. */
#define RIDER_TEMP_FILTER_DETACH_SLOPE_CPM   (-180)
#define RIDER_TEMP_FILTER_DETACH_CONFIRM_SAMPLES 5
#define RIDER_TEMP_FILTER_DETACH_MIN_DROP_CENTI 50
#define RIDER_TEMP_FILTER_DETACH_RECOVERY_SLOPE_CPM 30
#define RIDER_TEMP_FILTER_REATTACH_MARGIN_CENTI 75

/* Publication is intentionally separate from model validity. EXPERIMENTAL
 * publishes the unverified candidate so real rides can be recorded; STRICT
 * additionally requires the offline held-out-session gate. CONTACT_PROXY is
 * retained only for old firmware comparisons and publishes contact twice. */
#define RIDER_CORE_TEMP_PUBLISH_SHADOW       0
#define RIDER_CORE_TEMP_PUBLISH_STRICT       1
#define RIDER_CORE_TEMP_PUBLISH_CONTACT_PROXY 2
#define RIDER_CORE_TEMP_PUBLISH_EXPERIMENTAL 3
#ifndef RIDER_CORE_TEMP_PUBLISH_MODE
#define RIDER_CORE_TEMP_PUBLISH_MODE         RIDER_CORE_TEMP_PUBLISH_SHADOW
#endif

#ifndef RIDER_CORE_TEMP_CALIBRATION_VALID
#define RIDER_CORE_TEMP_CALIBRATION_VALID    0
#endif
#ifndef RIDER_CORE_TEMP_CALIBRATION_AVAILABLE
#define RIDER_CORE_TEMP_CALIBRATION_AVAILABLE RIDER_CORE_TEMP_CALIBRATION_VALID
#endif
#ifndef RIDER_CORE_TEMP_CAL_BASE_CENTI
#define RIDER_CORE_TEMP_CAL_BASE_CENTI       3680
#endif
#ifndef RIDER_CORE_TEMP_CAL_SKIN_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_SKIN_GAIN_Q8     21
#endif
#ifndef RIDER_CORE_TEMP_CAL_SKIN_DELTA_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_SKIN_DELTA_GAIN_Q8 64
#endif
#ifndef RIDER_CORE_TEMP_CAL_TREND_1M_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_TREND_1M_GAIN_Q8 16
#endif
#ifndef RIDER_CORE_TEMP_CAL_TREND_5M_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_TREND_5M_GAIN_Q8 32
#endif
#ifndef RIDER_CORE_TEMP_CAL_HR_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_HR_GAIN_Q8       256
#endif
#ifndef RIDER_CORE_TEMP_CAL_HR_TREND_GAIN_Q8
#define RIDER_CORE_TEMP_CAL_HR_TREND_GAIN_Q8 32
#endif
#ifndef RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8
#define RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8     8
#endif
/* V1 uses five-second history points for minute-scale features. Sixty-one
 * points cover both endpoints of a full five-minute interval; skin, sequence,
 * optional HR and bookkeeping consume about 512 bytes. */
#define RIDER_CORE_TEMP_MODEL_VERSION         1
#ifndef RIDER_CORE_TEMP_CAL_MODEL_VERSION
#define RIDER_CORE_TEMP_CAL_MODEL_VERSION     0
#endif
#if RIDER_CORE_TEMP_CALIBRATION_AVAILABLE && \
    RIDER_CORE_TEMP_CAL_MODEL_VERSION != RIDER_CORE_TEMP_MODEL_VERSION
#error "Rider core calibration model version does not match firmware"
#endif
#define RIDER_CORE_TEMP_HISTORY_STEP_SECONDS  5
#define RIDER_CORE_TEMP_HISTORY_SECONDS       300
#define RIDER_CORE_TEMP_HISTORY_SLOTS         61
#define RIDER_CORE_TEMP_BASELINE_SAMPLES      30
#define RIDER_CORE_TEMP_SKIN_REFERENCE_CENTI  3500
#define RIDER_CORE_TEMP_HR_REFERENCE_BPM      80
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
    uint16_t contact_samples; /* Contiguous valid samples in this wear episode. */
    uint8_t typical_samples; /* Consecutive samples in the 32~40 C evidence band. */
    uint8_t valid;
    uint8_t skin_trusted;
    uint8_t core_input_valid; /* Trusted and not under detach evaluation. */
    uint8_t quality;
    uint8_t status;
    uint8_t state;
    uint8_t freshness;
} rider_temperature_filter_output_t;

typedef struct {
    int16_t base_core_centi;
    int16_t skin_gain_q8;
    int16_t skin_delta_gain_q8;
    int16_t trend_1m_gain_q8;
    int16_t trend_5m_gain_q8;
    int16_t heart_rate_gain_q8;
    int16_t heart_rate_trend_gain_q8;
    uint8_t lag_alpha_q8;
    uint8_t available;
    uint8_t valid;
} rider_core_temperature_calibration_t;

typedef struct {
    uint32_t sequence;
    uint32_t skin_source_sequence;
    uint32_t core_source_sequence;
    int16_t sensor_temperature_centi;
    int16_t contact_temperature_centi;
    int16_t skin_temperature_centi;
    int16_t core_temperature_centi;
    int16_t slope_centi_per_min;
    int16_t skin_baseline_centi;
    int16_t skin_delta_1m_centi;
    int16_t skin_delta_5m_centi;
    int16_t heart_rate_delta_1m;
    uint16_t core_history_seconds;
    uint16_t contact_samples;
    uint8_t valid;
    uint8_t sensor_valid;
    uint8_t contact_valid;
    uint8_t skin_valid;              /* Qualified single-site trusted skin value. */
    uint8_t core_input_valid;         /* Current sample may update the model. */
    uint8_t core_estimate_valid;     /* Numerical candidate is available. */
    uint8_t core_estimate_verified;  /* Held-out validation gate has passed. */
    uint8_t quality;
    uint8_t confidence;
    uint8_t sensor_status;
    uint8_t temperature_state;
    uint8_t core_state;
    uint8_t data_freshness;
    uint8_t typical_samples;
    uint8_t heart_rate;
    uint8_t heart_rate_valid;
    uint8_t heart_rate_used;
    uint8_t model_mode;
    uint8_t model_version;
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
#define RIDER_CORE_TEMP_FIRMWARE_VERSION "0.2.0"

#endif
