#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"

#define LOG_TAG_CONST       RIDER_ESTIMATOR
#define LOG_TAG             "[RIDER_ESTIMATOR]"
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/** Five-second history retains minute-scale features in about 512 bytes. */
typedef struct {
    int16_t skin_centi[RIDER_CORE_TEMP_HISTORY_SLOTS];
    uint32_t sequence[RIDER_CORE_TEMP_HISTORY_SLOTS];
    uint8_t heart_rate[RIDER_CORE_TEMP_HISTORY_SLOTS];
    uint8_t heart_rate_valid[RIDER_CORE_TEMP_HISTORY_SLOTS];
    uint8_t count;
    uint8_t write_index;
    uint8_t have_store_sequence;
    uint8_t baseline_samples;
    uint8_t baseline_ready;
    uint32_t last_store_sequence;
    int32_t baseline_sum_centi;
    int16_t baseline_centi;
} rider_core_feature_history_t;

static rider_temperature_snapshot_t rider_snapshot;
static rider_core_temperature_calibration_t rider_calibration;
static rider_core_feature_history_t rider_core_history;
/* Q8 state retains sub-centi-degree movement in both directions. */
static int32_t rider_core_model_q8;
static u8 rider_core_model_initialized;
static uint32_t rider_estimator_last_tick_sequence;
static u8 rider_estimator_missing_ticks;

/** Reset the dynamic candidate without discarding qualified skin history. */
static void rider_estimator_reset_core_model(void)
{
    rider_core_model_q8 = 0;
    rider_core_model_initialized = 0;
}

/** Start a new wear episode and discard all time-dependent features. */
static void rider_estimator_reset_core_tracking(void)
{
    memset(&rider_core_history, 0, sizeof(rider_core_history));
    rider_estimator_reset_core_model();
}

/** Apply a signed Q8 gain with symmetric round-to-nearest behavior. */
static int32_t rider_estimator_apply_gain(int32_t value, int16_t gain_q8)
{
    int32_t product = value * gain_q8;

    if (product >= 0) {
        return (product + 128) / 256;
    }
    return -(((-product) + 128) / 256);
}

/** Move a Q8 model state toward a centi-degree target. */
static int32_t rider_estimator_q8_step(int32_t previous_q8,
                                       int32_t target_centi,
                                       uint8_t alpha_q8)
{
    int32_t target_q8 = target_centi * 256;
    int32_t delta_q8 = target_q8 - previous_q8;
    int32_t step_q8 = (delta_q8 * alpha_q8) / 256;

    if (!step_q8 && delta_q8) {
        step_q8 = delta_q8 > 0 ? 1 : -1;
    }
    return previous_q8 + step_q8;
}

/** Limit a one-second candidate change without changing its target value. */
static int32_t rider_estimator_limit_rate_q8(int32_t previous_q8,
                                             int32_t candidate_q8)
{
    int32_t delta_q8 = candidate_q8 - previous_q8;
    int32_t maximum_step_q8 =
        (int32_t)RIDER_CORE_TEMP_ESTIMATE_MAX_STEP_CENTI * 256;

    if (delta_q8 > maximum_step_q8) {
        return previous_q8 + maximum_step_q8;
    }
    if (delta_q8 < -maximum_step_q8) {
        return previous_q8 - maximum_step_q8;
    }
    return candidate_q8;
}

/** Convert positive physiological Q8 values back to centi-degrees. */
static int16_t rider_estimator_q8_to_centi(int32_t value_q8)
{
    return (int16_t)((value_q8 + 128) / 256);
}

/** Reject anomalous model output instead of manufacturing a boundary value. */
static uint8_t rider_estimator_target_in_range(int32_t target_centi)
{
    return target_centi >= RIDER_CORE_TEMP_ESTIMATE_MIN_CENTI &&
           target_centi <= RIDER_CORE_TEMP_ESTIMATE_MAX_CENTI;
}

/** Load reviewed compile-time coefficients or the transparent V1 defaults. */
static void rider_estimator_load_default_calibration(void)
{
    rider_calibration.base_core_centi = RIDER_CORE_TEMP_CAL_BASE_CENTI;
    rider_calibration.skin_gain_q8 = RIDER_CORE_TEMP_CAL_SKIN_GAIN_Q8;
    rider_calibration.skin_delta_gain_q8 =
        RIDER_CORE_TEMP_CAL_SKIN_DELTA_GAIN_Q8;
    rider_calibration.trend_1m_gain_q8 = RIDER_CORE_TEMP_CAL_TREND_1M_GAIN_Q8;
    rider_calibration.trend_5m_gain_q8 = RIDER_CORE_TEMP_CAL_TREND_5M_GAIN_Q8;
    rider_calibration.heart_rate_gain_q8 = RIDER_CORE_TEMP_CAL_HR_GAIN_Q8;
    rider_calibration.heart_rate_trend_gain_q8 =
        RIDER_CORE_TEMP_CAL_HR_TREND_GAIN_Q8;
    rider_calibration.lag_alpha_q8 = RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8;
    rider_calibration.available = RIDER_CORE_TEMP_CALIBRATION_AVAILABLE ? 1 : 0;
    rider_calibration.valid = RIDER_CORE_TEMP_CALIBRATION_VALID ? 1 : 0;
#if RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_EXPERIMENTAL
    /* Experimental collection must emit a deterministic candidate. These
     * defaults are traceable engineering priors, not validated coefficients. */
    rider_calibration.available = 1;
#endif
}

/** Return the chronological array index for one bounded history position. */
static uint8_t rider_estimator_history_index(uint8_t position)
{
    uint8_t start = rider_core_history.count == RIDER_CORE_TEMP_HISTORY_SLOTS
                        ? rider_core_history.write_index
                        : 0;

    return (uint8_t)((start + position) % RIDER_CORE_TEMP_HISTORY_SLOTS);
}

/** Record a trusted-skin feature point at a five-second cadence. */
static void rider_estimator_store_history(uint32_t sequence,
                                          int16_t skin_centi)
{
    uint8_t index;

    if (rider_core_history.have_store_sequence &&
        sequence - rider_core_history.last_store_sequence <
            RIDER_CORE_TEMP_HISTORY_STEP_SECONDS) {
        return;
    }

    index = rider_core_history.write_index;
    rider_core_history.skin_centi[index] = skin_centi;
    rider_core_history.sequence[index] = sequence;
    rider_core_history.heart_rate[index] = rider_snapshot.heart_rate;
    rider_core_history.heart_rate_valid[index] =
        rider_snapshot.heart_rate_valid ? 1 : 0;
    rider_core_history.write_index =
        (uint8_t)((index + 1) % RIDER_CORE_TEMP_HISTORY_SLOTS);
    if (rider_core_history.count < RIDER_CORE_TEMP_HISTORY_SLOTS) {
        rider_core_history.count++;
    }
    rider_core_history.last_store_sequence = sequence;
    rider_core_history.have_store_sequence = 1;
}

/** Find the newest feature point at or before a target sequence. */
static uint8_t rider_estimator_find_history(uint32_t target_sequence,
                                            int16_t *skin_centi,
                                            uint8_t *heart_rate,
                                            uint8_t *heart_rate_valid)
{
    uint8_t position;
    uint8_t found = 0;

    for (position = 0; position < rider_core_history.count; ++position) {
        uint8_t index = rider_estimator_history_index(position);

        if (rider_core_history.sequence[index] > target_sequence) {
            break;
        }
        if (skin_centi) {
            *skin_centi = rider_core_history.skin_centi[index];
        }
        if (heart_rate) {
            *heart_rate = rider_core_history.heart_rate[index];
        }
        if (heart_rate_valid) {
            *heart_rate_valid = rider_core_history.heart_rate_valid[index];
        }
        found = 1;
    }
    return found;
}

/** Return the time coverage of the current trusted-skin feature history. */
static uint16_t rider_estimator_history_seconds(uint32_t current_sequence)
{
    uint8_t oldest_index;
    uint32_t span;

    if (!rider_core_history.count) {
        return 0;
    }
    oldest_index = rider_estimator_history_index(0);
    if (current_sequence <= rider_core_history.sequence[oldest_index]) {
        return 0;
    }
    span = current_sequence - rider_core_history.sequence[oldest_index];
    return span > 0xffff ? 0xffff : (uint16_t)span;
}

/** Build the per-wear skin baseline from the first trusted samples. */
static void rider_estimator_update_baseline(int16_t skin_centi)
{
    if (rider_core_history.baseline_ready) {
        return;
    }
    rider_core_history.baseline_sum_centi += skin_centi;
    rider_core_history.baseline_samples++;
    if (rider_core_history.baseline_samples >=
        RIDER_CORE_TEMP_BASELINE_SAMPLES) {
        rider_core_history.baseline_centi =
            (int16_t)((rider_core_history.baseline_sum_centi +
                       RIDER_CORE_TEMP_BASELINE_SAMPLES / 2) /
                      RIDER_CORE_TEMP_BASELINE_SAMPLES);
        rider_core_history.baseline_ready = 1;
    }
}

/** Clear derived timelines while preserving the current sensor sample and HR. */
static void rider_estimator_clear_measurement(uint8_t status, uint8_t state)
{
    rider_snapshot.valid = 0;
    rider_snapshot.contact_valid = 0;
    rider_snapshot.skin_valid = 0;
    rider_snapshot.core_input_valid = 0;
    rider_snapshot.core_estimate_valid = 0;
    rider_snapshot.core_estimate_verified = 0;
    rider_snapshot.heart_rate_used = 0;
    rider_snapshot.contact_temperature_centi = 0x7fff;
    rider_snapshot.skin_temperature_centi = 0x7fff;
    rider_snapshot.core_temperature_centi = 0x7fff;
    rider_snapshot.skin_baseline_centi = 0x7fff;
    rider_snapshot.slope_centi_per_min = 0;
    rider_snapshot.skin_delta_1m_centi = 0;
    rider_snapshot.skin_delta_5m_centi = 0;
    rider_snapshot.heart_rate_delta_1m = 0;
    rider_snapshot.skin_source_sequence = 0;
    rider_snapshot.core_source_sequence = 0;
    rider_snapshot.core_history_seconds = 0;
    rider_snapshot.contact_samples = 0;
    rider_snapshot.typical_samples = 0;
    rider_snapshot.quality = RIDER_TEMP_QUALITY_NA;
    rider_snapshot.confidence = 0;
    rider_snapshot.sensor_status = status;
    rider_snapshot.temperature_state = state;
    rider_snapshot.core_state = RIDER_CORE_STATE_INVALID;
    rider_snapshot.model_mode = RIDER_CORE_MODEL_SKIN_ONLY;
    rider_snapshot.data_freshness = state == RIDER_TEMP_STATE_STALE
                                        ? RIDER_TEMP_FRESHNESS_STALE
                                        : RIDER_TEMP_FRESHNESS_UNAVAILABLE;
    rider_estimator_reset_core_tracking();
}

/** Set signal confidence as readiness metadata, never as an accuracy claim. */
static void rider_estimator_set_skin_quality(
    const rider_temperature_filter_output_t *filtered)
{
    rider_snapshot.quality = filtered->quality;
    if (filtered->skin_trusted) {
        rider_snapshot.confidence = filtered->quality == RIDER_TEMP_QUALITY_GOOD
                                         ? 60
                                         : 50;
    } else if (filtered->state == RIDER_TEMP_STATE_CONTACT_SETTLING) {
        rider_snapshot.confidence = 20;
    } else {
        rider_snapshot.confidence = 0;
    }
}

/** Calculate the V1 target from trusted skin history and optional fresh HR. */
static int32_t rider_estimator_core_target(int16_t skin_centi)
{
    int32_t target = rider_calibration.base_core_centi;

    target += rider_estimator_apply_gain(
        skin_centi - RIDER_CORE_TEMP_SKIN_REFERENCE_CENTI,
        rider_calibration.skin_gain_q8);
    target += rider_estimator_apply_gain(
        skin_centi - rider_core_history.baseline_centi,
        rider_calibration.skin_delta_gain_q8);
    target += rider_estimator_apply_gain(
        rider_snapshot.skin_delta_1m_centi,
        rider_calibration.trend_1m_gain_q8);
    target += rider_estimator_apply_gain(
        rider_snapshot.skin_delta_5m_centi,
        rider_calibration.trend_5m_gain_q8);

    rider_snapshot.model_mode = RIDER_CORE_MODEL_SKIN_ONLY;
    if (rider_snapshot.heart_rate_valid) {
        target += rider_estimator_apply_gain(
            (int16_t)rider_snapshot.heart_rate -
                RIDER_CORE_TEMP_HR_REFERENCE_BPM,
            rider_calibration.heart_rate_gain_q8);
        target += rider_estimator_apply_gain(
            rider_snapshot.heart_rate_delta_1m,
            rider_calibration.heart_rate_trend_gain_q8);
        rider_snapshot.model_mode = RIDER_CORE_MODEL_SKIN_AND_HR;
    }
    return target;
}

/** Update minute features and publish one experimental core candidate. */
static void rider_estimator_update_core_model(
    const rider_temperature_filter_output_t *filtered)
{
    int16_t skin_1m = 0;
    int16_t skin_5m = 0;
    uint8_t heart_rate_1m = 0;
    uint8_t heart_rate_1m_valid = 0;
    uint8_t have_1m;
    uint8_t have_5m;
    uint8_t alpha;
    int32_t target;

    rider_snapshot.core_estimate_valid = 0;
    rider_snapshot.core_estimate_verified = 0;
    rider_snapshot.heart_rate_used = 0;
    rider_snapshot.core_temperature_centi = 0x7fff;
    rider_snapshot.core_source_sequence = 0;

    if (!filtered || !filtered->valid || !filtered->skin_trusted ||
        !filtered->core_input_valid) {
        /* A suspected detach holds history/model state for a short recovery,
         * but neither held state is presented as a fresh measurement. */
        if (filtered &&
            filtered->state == RIDER_TEMP_STATE_DETACH_SUSPECTED) {
            rider_snapshot.core_state = RIDER_CORE_STATE_HOLD;
        } else if (filtered && filtered->skin_trusted) {
            rider_snapshot.core_state = RIDER_CORE_STATE_WARMUP;
        } else {
            rider_snapshot.core_state = RIDER_CORE_STATE_EMPTY;
        }
        return;
    }

    rider_estimator_update_baseline(filtered->filtered_temperature_centi);
    rider_estimator_store_history(filtered->sequence,
                                  filtered->filtered_temperature_centi);
    rider_snapshot.skin_baseline_centi = rider_core_history.baseline_ready
                                             ? rider_core_history.baseline_centi
                                             : 0x7fff;
    rider_snapshot.core_history_seconds =
        rider_estimator_history_seconds(filtered->sequence);

    have_1m = filtered->sequence >= 60 && rider_estimator_find_history(
        filtered->sequence - 60, &skin_1m, &heart_rate_1m,
        &heart_rate_1m_valid);
    have_5m = filtered->sequence >= RIDER_CORE_TEMP_HISTORY_SECONDS &&
              rider_estimator_find_history(
                  filtered->sequence - RIDER_CORE_TEMP_HISTORY_SECONDS,
                  &skin_5m, NULL, NULL);
    rider_snapshot.skin_delta_1m_centi = have_1m
        ? (int16_t)(filtered->filtered_temperature_centi - skin_1m) : 0;
    rider_snapshot.skin_delta_5m_centi = have_5m
        ? (int16_t)(filtered->filtered_temperature_centi - skin_5m) : 0;
    rider_snapshot.heart_rate_delta_1m =
        have_1m && heart_rate_1m_valid && rider_snapshot.heart_rate_valid
            ? (int16_t)rider_snapshot.heart_rate - heart_rate_1m
            : 0;

    if (!rider_calibration.available || !rider_core_history.baseline_ready ||
        !have_5m ||
        rider_snapshot.core_history_seconds < RIDER_CORE_TEMP_HISTORY_SECONDS ||
        filtered->quality < RIDER_TEMP_QUALITY_FAIR ||
        (RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_STRICT &&
         filtered->quality < RIDER_TEMP_QUALITY_GOOD)) {
        rider_snapshot.core_state = RIDER_CORE_STATE_WARMUP;
        return;
    }

    target = rider_estimator_core_target(filtered->filtered_temperature_centi);
    if (!rider_estimator_target_in_range(target)) {
        /* 35~42 C is an anomaly gate. Clamping would create a plausible-looking
         * boundary sample and contaminate the validation export. */
        rider_snapshot.core_state = RIDER_CORE_STATE_INVALID;
        return;
    }

    alpha = rider_calibration.lag_alpha_q8;
    if (!alpha) {
        alpha = 1;
    }
    if (!rider_core_model_initialized) {
        rider_core_model_q8 = target * 256;
        rider_core_model_initialized = 1;
    } else {
        rider_core_model_q8 = rider_estimator_limit_rate_q8(
            rider_core_model_q8,
            rider_estimator_q8_step(rider_core_model_q8, target, alpha));
    }
    if (!rider_estimator_target_in_range(
            rider_estimator_q8_to_centi(rider_core_model_q8))) {
        rider_snapshot.core_state = RIDER_CORE_STATE_INVALID;
        return;
    }

    rider_snapshot.core_temperature_centi =
        rider_estimator_q8_to_centi(rider_core_model_q8);
    rider_snapshot.core_state = RIDER_CORE_STATE_READY;
    rider_snapshot.core_source_sequence = filtered->sequence;
    rider_snapshot.heart_rate_used =
        rider_snapshot.model_mode == RIDER_CORE_MODEL_SKIN_AND_HR;
    rider_snapshot.confidence = filtered->quality == RIDER_TEMP_QUALITY_GOOD
                                    ? (rider_snapshot.heart_rate_used ? 75 : 70)
                                    : (rider_snapshot.heart_rate_used ? 65 : 60);
    rider_snapshot.core_estimate_valid =
#if RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_EXPERIMENTAL
        1;
#else
        rider_snapshot.confidence >= RIDER_CORE_TEMP_MIN_CONFIDENCE;
#endif
    rider_snapshot.core_estimate_verified = rider_snapshot.core_estimate_valid &&
                                            rider_calibration.valid;
    if (!rider_snapshot.core_estimate_valid) {
        rider_snapshot.core_temperature_centi = 0x7fff;
        rider_snapshot.core_source_sequence = 0;
        rider_snapshot.heart_rate_used = 0;
    }
}

/** Reset all three timelines and start the product filter. */
void rider_estimator_init(void)
{
    memset(&rider_snapshot, 0, sizeof(rider_snapshot));
    rider_snapshot.sensor_temperature_centi = 0x7fff;
    rider_snapshot.contact_temperature_centi = 0x7fff;
    rider_snapshot.skin_temperature_centi = 0x7fff;
    rider_snapshot.core_temperature_centi = 0x7fff;
    rider_snapshot.skin_baseline_centi = 0x7fff;
    rider_snapshot.quality = RIDER_TEMP_QUALITY_NA;
    rider_snapshot.sensor_status = RIDER_TEMP_STATUS_NO_DEVICE;
    rider_snapshot.temperature_state = RIDER_TEMP_STATE_NO_DEVICE;
    rider_snapshot.core_state = RIDER_CORE_STATE_EMPTY;
    rider_snapshot.data_freshness = RIDER_TEMP_FRESHNESS_UNAVAILABLE;
    rider_snapshot.model_mode = RIDER_CORE_MODEL_SKIN_ONLY;
    rider_snapshot.model_version = RIDER_CORE_TEMP_MODEL_VERSION;
    rider_estimator_load_default_calibration();
    rider_estimator_reset_core_tracking();
    rider_estimator_last_tick_sequence = 0;
    rider_estimator_missing_ticks = 0;
    rider_temp_filter_init();
    log_info("Estimator init: model=v%u calibration_available=%u "
             "calibration_verified=%u publish_mode=%u core=NA skin=NA\n",
             (unsigned)rider_snapshot.model_version,
             (unsigned)rider_calibration.available,
             (unsigned)rider_calibration.valid,
             (unsigned)RIDER_CORE_TEMP_PUBLISH_MODE);
}

/** Consume one sensor sample and advance the aligned sensor/skin/core lines. */
void rider_estimator_consume(const rider_temperature_sample_t *sample)
{
    rider_temperature_filter_output_t filtered;

    if (sample) {
        rider_snapshot.sequence = sample->sequence;
        rider_snapshot.sensor_valid =
            sample->valid && sample->status == RIDER_TEMP_STATUS_OK;
        rider_snapshot.sensor_temperature_centi = rider_snapshot.sensor_valid
                                                       ? sample->temperature_centi
                                                       : 0x7fff;
    } else {
        rider_snapshot.sensor_valid = 0;
        rider_snapshot.sensor_temperature_centi = 0x7fff;
    }

    rider_temp_filter_consume(sample, &filtered);
    rider_snapshot.sequence = filtered.sequence;
    rider_snapshot.sensor_status = filtered.status;
    rider_snapshot.temperature_state = filtered.state;
    rider_snapshot.data_freshness = filtered.freshness;
    rider_snapshot.core_input_valid = filtered.core_input_valid;
    rider_snapshot.contact_samples = filtered.contact_samples;
    rider_snapshot.typical_samples = filtered.typical_samples;
    rider_snapshot.slope_centi_per_min = filtered.slope_centi_per_min;

    if (!filtered.valid) {
        rider_estimator_clear_measurement(filtered.status, filtered.state);
        rider_snapshot.sequence = filtered.sequence;
        log_info("Estimator timeline: sensor_seq=%u sensor=%s sensor_centi=%d "
                 "skin=NA core=NA status=%u skin_state=%u core_state=%u\n",
                 (unsigned)rider_snapshot.sequence,
                 rider_snapshot.sensor_valid ? "valid" : "invalid",
                 rider_snapshot.sensor_valid
                     ? (int)rider_snapshot.sensor_temperature_centi : 32767,
                 (unsigned)filtered.status, (unsigned)filtered.state,
                 (unsigned)rider_snapshot.core_state);
        return;
    }

    rider_snapshot.contact_temperature_centi =
        filtered.filtered_temperature_centi;
    rider_snapshot.contact_valid = 1;
    rider_snapshot.skin_valid = filtered.skin_trusted;
    rider_snapshot.skin_source_sequence = rider_snapshot.skin_valid
                                               ? filtered.sequence : 0;
    rider_snapshot.skin_temperature_centi = rider_snapshot.skin_valid
                                                ? filtered.filtered_temperature_centi
                                                : 0x7fff;
    rider_snapshot.valid = 0;
    rider_estimator_set_skin_quality(&filtered);
    rider_estimator_update_core_model(&filtered);
#if RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_STRICT
    rider_snapshot.valid = rider_snapshot.core_estimate_valid &&
                           rider_snapshot.core_estimate_verified &&
                           rider_snapshot.core_state == RIDER_CORE_STATE_READY;
#elif RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_EXPERIMENTAL
    rider_snapshot.valid = rider_snapshot.core_estimate_valid &&
                           rider_snapshot.core_state == RIDER_CORE_STATE_READY &&
                           rider_snapshot.data_freshness ==
                               RIDER_TEMP_FRESHNESS_FRESH;
#elif RIDER_CORE_TEMP_PUBLISH_MODE == RIDER_CORE_TEMP_PUBLISH_CONTACT_PROXY
    rider_snapshot.valid = rider_snapshot.skin_valid &&
                           rider_snapshot.data_freshness ==
                               RIDER_TEMP_FRESHNESS_FRESH;
#else
    rider_snapshot.valid = 0;
#endif

    log_info("Estimator timeline: sensor_seq=%u sensor_centi=%d "
             "skin_seq=%u skin_centi=%d skin_state=%u quality=%u "
             "core_seq=%u core_centi=%d core_state=%u model=v%u/%u "
             "hr=%u hr_valid=%u hr_used=%u baseline=%d d1m=%d d5m=%d "
             "history_s=%u contact_samples=%u typical=%u publish=%u\n",
             (unsigned)rider_snapshot.sequence,
             rider_snapshot.sensor_valid
                 ? (int)rider_snapshot.sensor_temperature_centi : 32767,
             (unsigned)rider_snapshot.skin_source_sequence,
             rider_snapshot.skin_valid
                 ? (int)rider_snapshot.skin_temperature_centi : 32767,
             (unsigned)rider_snapshot.temperature_state,
             (unsigned)rider_snapshot.quality,
             (unsigned)rider_snapshot.core_source_sequence,
             rider_snapshot.core_estimate_valid
                 ? (int)rider_snapshot.core_temperature_centi : 32767,
             (unsigned)rider_snapshot.core_state,
             (unsigned)rider_snapshot.model_version,
             (unsigned)rider_snapshot.model_mode,
             (unsigned)rider_snapshot.heart_rate,
             (unsigned)rider_snapshot.heart_rate_valid,
             (unsigned)rider_snapshot.heart_rate_used,
             rider_snapshot.skin_baseline_centi == 0x7fff
                 ? 32767 : (int)rider_snapshot.skin_baseline_centi,
             (int)rider_snapshot.skin_delta_1m_centi,
             (int)rider_snapshot.skin_delta_5m_centi,
             (unsigned)rider_snapshot.core_history_seconds,
             (unsigned)rider_snapshot.contact_samples,
             (unsigned)rider_snapshot.typical_samples,
             (unsigned)rider_snapshot.valid);
}

/** Copy the current snapshot so GATT callbacks never mutate shared storage. */
void rider_estimator_copy_snapshot(rider_temperature_snapshot_t *snapshot)
{
    if (snapshot) {
        memcpy(snapshot, &rider_snapshot, sizeof(*snapshot));
    }
}

/** Expire all derived timelines when the sensor sequence stops advancing. */
void rider_estimator_tick(uint32_t latest_sequence)
{
    if (latest_sequence != rider_estimator_last_tick_sequence) {
        rider_estimator_last_tick_sequence = latest_sequence;
        rider_estimator_missing_ticks = 0;
        return;
    }

    if (rider_estimator_missing_ticks < 0xff) {
        rider_estimator_missing_ticks++;
    }
    if (rider_estimator_missing_ticks < RIDER_TEMP_STALE_AFTER_TICKS ||
        rider_snapshot.temperature_state == RIDER_TEMP_STATE_STALE ||
        (!rider_snapshot.contact_valid && !rider_snapshot.valid)) {
        return;
    }

    rider_temp_filter_init();
    rider_snapshot.sensor_valid = 0;
    rider_snapshot.sensor_temperature_centi = 0x7fff;
    rider_estimator_clear_measurement(RIDER_TEMP_STATUS_NO_DEVICE,
                                      RIDER_TEMP_STATE_STALE);
    rider_snapshot.sequence = latest_sequence;
    log_info("Estimator stale timeout: seq=%u ticks=%u skin_state=%u "
             "core_state=%u\n", (unsigned)latest_sequence,
             (unsigned)rider_estimator_missing_ticks,
             (unsigned)rider_snapshot.temperature_state,
             (unsigned)rider_snapshot.core_state);
}

/** Install or clear a host-fitted V1 calibration. */
void rider_estimator_set_core_calibration(
    const rider_core_temperature_calibration_t *calibration)
{
    if (!calibration) {
        rider_estimator_load_default_calibration();
    } else {
        rider_calibration = *calibration;
        rider_calibration.available = calibration->available || calibration->valid;
        rider_calibration.valid = calibration->valid ? 1 : 0;
    }
    rider_estimator_reset_core_model();
    rider_snapshot.valid = 0;
    rider_snapshot.core_estimate_valid = 0;
    rider_snapshot.core_estimate_verified = 0;
    rider_snapshot.core_temperature_centi = 0x7fff;
    rider_snapshot.core_source_sequence = 0;
    rider_snapshot.heart_rate_used = 0;
    rider_snapshot.core_state = rider_snapshot.skin_valid
                                    ? RIDER_CORE_STATE_WARMUP
                                    : RIDER_CORE_STATE_EMPTY;
    log_info("Estimator calibration: model=v%u available=%u valid=%u base=%d "
             "skin_q8=%d skin_delta_q8=%d trend_1m_q8=%d trend_5m_q8=%d "
             "hr_q8=%d hr_trend_q8=%d lag_alpha_q8=%u\n",
             (unsigned)RIDER_CORE_TEMP_MODEL_VERSION,
             (unsigned)rider_calibration.available,
             (unsigned)rider_calibration.valid,
             (int)rider_calibration.base_core_centi,
             (int)rider_calibration.skin_gain_q8,
             (int)rider_calibration.skin_delta_gain_q8,
             (int)rider_calibration.trend_1m_gain_q8,
             (int)rider_calibration.trend_5m_gain_q8,
             (int)rider_calibration.heart_rate_gain_q8,
             (int)rider_calibration.heart_rate_trend_gain_q8,
             (unsigned)rider_calibration.lag_alpha_q8);
}

/** Store or clear the direct external heart-rate input from Control Point. */
void rider_estimator_set_external_heart_rate(uint8_t heart_rate, uint8_t valid)
{
    rider_snapshot.heart_rate = valid ? heart_rate : 0;
    rider_snapshot.heart_rate_valid = valid ? 1 : 0;
    /* model_mode/heart_rate_used describe the current Core candidate, not the
     * next input. They change only when the next sensor sample is evaluated. */
}

#endif
