#include "rider_temp_codec.h"

/** Encode the variable-length CORE notification frame. */
uint16_t rider_encode_core_temperature_frame(
    uint8_t *frame, uint16_t frame_size, int16_t core_centi,
    const rider_temperature_snapshot_t *snapshot)
{
    int16_t skin = 0x7fff;
    uint8_t flags = 0x04; /* Quality & State is present; core is mandatory. */
    uint8_t quality_state = snapshot ? (snapshot->quality & 0x0f) : 0x07;
    uint16_t length = 0;

    if (!frame || frame_size < 4) {
        return 0;
    }

    /* BLE Quality & State reports HR signal reception (0x10/0x20). Whether
     * Core V1 actually used that signal is model metadata logged separately. */
    quality_state |= 0x10;
    if (snapshot && snapshot->skin_valid) {
        flags |= 0x01;
        skin = snapshot->skin_temperature_centi;
    }
    if (snapshot && snapshot->heart_rate_valid) {
        flags |= 0x10;
        quality_state = (quality_state & (uint8_t)~0x30) | 0x20;
    }

    frame[length++] = flags;
    frame[length++] = (uint8_t)core_centi;
    frame[length++] = (uint8_t)((uint16_t)core_centi >> 8);
    if (flags & 0x01) {
        if (frame_size < length + 2) {
            return 0;
        }
        frame[length++] = (uint8_t)skin;
        frame[length++] = (uint8_t)((uint16_t)skin >> 8);
    }
    if (frame_size < length + 1) {
        return 0;
    }
    frame[length++] = quality_state;
    if (flags & 0x10) {
        if (frame_size < length + 1) {
            return 0;
        }
        frame[length++] = snapshot->heart_rate;
    }
    return length;
}

/** Encode the standard HTS IEEE-11073 FLOAT measurement. */
uint16_t rider_encode_standard_temperature_frame(
    uint8_t *value, uint16_t value_size, uint8_t core_valid,
    int16_t core_centi)
{
    int32_t mantissa;

    if (!value || value_size < RIDER_TEMP_CODEC_STANDARD_FRAME_SIZE) {
        return 0;
    }

    value[0] = 0x04; /* Celsius and Temperature Type present. */
    if (!core_valid) {
        mantissa = 0x007fffff; /* IEEE-11073 FLOAT NaN mantissa. */
        value[1] = (uint8_t)mantissa;
        value[2] = (uint8_t)(mantissa >> 8);
        value[3] = (uint8_t)(mantissa >> 16);
        value[4] = 0;
    } else {
        mantissa = core_centi;
        value[1] = (uint8_t)mantissa;
        value[2] = (uint8_t)(mantissa >> 8);
        value[3] = (uint8_t)(mantissa >> 16);
        value[4] = 0xfe; /* 10^-2 Celsius. */
    }
    return RIDER_TEMP_CODEC_STANDARD_FRAME_SIZE;
}

/** Write an unsigned little-endian 16-bit field without relying on SDK macros. */
static void rider_debug_put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

/** Write an unsigned little-endian 32-bit field without alignment assumptions. */
static void rider_debug_put_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

/** Return a temperature value or the protocol's explicit unavailable sentinel. */
static int16_t rider_debug_temperature(uint8_t valid, int16_t value)
{
    return valid ? value : (int16_t)0x7fff;
}

/** Encode one complete, versioned diagnostic snapshot for host-side logging. */
uint16_t rider_encode_debug_snapshot_frame(
    uint8_t *frame, uint16_t frame_size,
    const rider_temperature_snapshot_t *snapshot,
    int16_t published_core_centi)
{
    uint8_t flags = 0;
    int16_t sensor_centi;
    int16_t contact_centi;
    int16_t skin_centi;
    int16_t core_centi;
    int16_t baseline_centi;
    int16_t delta_1m_centi;
    int16_t delta_5m_centi;
    int16_t slope_centi_per_min;
    int16_t hr_delta_1m;
    uint8_t quality = RIDER_TEMP_QUALITY_NA;
    uint8_t sensor_status = RIDER_TEMP_STATUS_NO_DEVICE;
    uint8_t temperature_state = RIDER_TEMP_STATE_NO_DEVICE;
    uint8_t core_state = RIDER_CORE_STATE_EMPTY;
    uint8_t freshness = RIDER_TEMP_FRESHNESS_UNAVAILABLE;
    uint8_t confidence = 0;
    uint8_t model_mode = RIDER_CORE_MODEL_SKIN_ONLY;
    uint8_t model_version = RIDER_CORE_TEMP_MODEL_VERSION;
    uint8_t heart_rate = 0;
    uint8_t heart_rate_used = 0;
    uint32_t sequence = 0;
    uint16_t core_history_seconds = 0;
    uint16_t contact_samples = 0;
    uint8_t typical_samples = 0;

    if (!frame || frame_size < RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE) {
        return 0;
    }

    sensor_centi = rider_debug_temperature(snapshot && snapshot->sensor_valid,
                                           snapshot ? snapshot->sensor_temperature_centi : 0);
    contact_centi = rider_debug_temperature(snapshot && snapshot->contact_valid,
                                            snapshot ? snapshot->contact_temperature_centi : 0);
    skin_centi = rider_debug_temperature(snapshot && snapshot->skin_valid,
                                         snapshot ? snapshot->skin_temperature_centi : 0);
    core_centi = rider_debug_temperature(snapshot && snapshot->core_estimate_valid,
                                         snapshot ? snapshot->core_temperature_centi : 0);
    baseline_centi = snapshot ? snapshot->skin_baseline_centi : (int16_t)0x7fff;
    delta_1m_centi = snapshot ? snapshot->skin_delta_1m_centi : (int16_t)0x7fff;
    delta_5m_centi = snapshot ? snapshot->skin_delta_5m_centi : (int16_t)0x7fff;
    slope_centi_per_min = snapshot ? snapshot->slope_centi_per_min : (int16_t)0x7fff;
    hr_delta_1m = snapshot ? snapshot->heart_rate_delta_1m : 0;

    if (snapshot) {
        sequence = snapshot->sequence;
        quality = snapshot->quality;
        sensor_status = snapshot->sensor_status;
        temperature_state = snapshot->temperature_state;
        core_state = snapshot->core_state;
        freshness = snapshot->data_freshness;
        confidence = snapshot->confidence;
        model_mode = snapshot->model_mode;
        model_version = snapshot->model_version;
        heart_rate = snapshot->heart_rate_valid ? snapshot->heart_rate : 0;
        heart_rate_used = snapshot->heart_rate_used ? 1 : 0;
        core_history_seconds = snapshot->core_history_seconds;
        contact_samples = snapshot->contact_samples;
        typical_samples = snapshot->typical_samples;
    }

    if (snapshot && snapshot->sensor_valid) {
        flags |= RIDER_TEMP_DEBUG_FLAG_SENSOR_VALID;
    }
    if (snapshot && snapshot->contact_valid) {
        flags |= RIDER_TEMP_DEBUG_FLAG_CONTACT_VALID;
    }
    if (snapshot && snapshot->skin_valid) {
        flags |= RIDER_TEMP_DEBUG_FLAG_SKIN_VALID;
    }
    if (snapshot && snapshot->core_estimate_valid) {
        flags |= RIDER_TEMP_DEBUG_FLAG_CORE_ESTIMATE;
    }
    if (published_core_centi != (int16_t)0x7fff) {
        flags |= RIDER_TEMP_DEBUG_FLAG_PUBLISHED_CORE;
    }
    if (snapshot && snapshot->heart_rate_valid) {
        flags |= RIDER_TEMP_DEBUG_FLAG_HEART_RATE_VALID;
    }
    if (snapshot && snapshot->core_estimate_verified) {
        flags |= RIDER_TEMP_DEBUG_FLAG_CORE_VERIFIED;
    }
    if (snapshot && snapshot->data_freshness == RIDER_TEMP_FRESHNESS_STALE) {
        flags |= RIDER_TEMP_DEBUG_FLAG_DATA_STALE;
    }

    frame[0] = RIDER_TEMP_CODEC_DEBUG_PROTOCOL_VERSION;
    frame[1] = flags;
    rider_debug_put_u32(&frame[2], sequence);
    rider_debug_put_u16(&frame[6], (uint16_t)sensor_centi);
    rider_debug_put_u16(&frame[8], (uint16_t)contact_centi);
    rider_debug_put_u16(&frame[10], (uint16_t)skin_centi);
    rider_debug_put_u16(&frame[12], (uint16_t)core_centi);
    rider_debug_put_u16(&frame[14], (uint16_t)published_core_centi);
    rider_debug_put_u16(&frame[16], (uint16_t)slope_centi_per_min);
    rider_debug_put_u16(&frame[18], (uint16_t)baseline_centi);
    rider_debug_put_u16(&frame[20], (uint16_t)delta_1m_centi);
    rider_debug_put_u16(&frame[22], (uint16_t)delta_5m_centi);
    rider_debug_put_u16(&frame[24], (uint16_t)hr_delta_1m);
    rider_debug_put_u16(&frame[26], core_history_seconds);
    rider_debug_put_u16(&frame[28], contact_samples);
    frame[30] = typical_samples;
    frame[31] = heart_rate;
    frame[32] = quality;
    frame[33] = sensor_status;
    frame[34] = temperature_state;
    frame[35] = core_state;
    /* v2：36..38 改为 M601 总线诊断（无串口时用 App/nRF 抓 hex 排障） */
    {
        rider_m601_diag_t m601_diag = {0};

        rider_temp_copy_m601_diag(&m601_diag);
        frame[36] = m601_diag.bus_flags;
        frame[37] = m601_diag.fail_phase;
        frame[38] = m601_diag.fail_streak;
        (void)freshness;
        (void)confidence;
        (void)model_mode;
    }
    frame[39] = model_version;
    frame[40] = heart_rate_used;
    return RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE;
}
