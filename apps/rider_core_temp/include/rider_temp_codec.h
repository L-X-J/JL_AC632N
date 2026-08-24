#ifndef RIDER_TEMP_CODEC_H
#define RIDER_TEMP_CODEC_H

#include <stdint.h>

#include "rider_core_temp.h"

#define RIDER_TEMP_CODEC_CORE_FRAME_MAX 9
#define RIDER_TEMP_CODEC_STANDARD_FRAME_SIZE 5

/** Encode the custom CORE temperature frame from protocol-facing fields.
 *
 * Core is always present and uses a signed centi-degree value. The caller
 * supplies the already-gated value so this module remains a pure byte codec.
 */
uint16_t rider_encode_core_temperature_frame(
    uint8_t *frame, uint16_t frame_size, int16_t core_centi,
    const rider_temperature_snapshot_t *snapshot);

/** Encode the SIG Health Thermometer FLOAT frame at 0.01 degree resolution. */
uint16_t rider_encode_standard_temperature_frame(
    uint8_t *value, uint16_t value_size, uint8_t core_valid,
    int16_t core_centi);

#endif
