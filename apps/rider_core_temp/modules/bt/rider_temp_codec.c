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
