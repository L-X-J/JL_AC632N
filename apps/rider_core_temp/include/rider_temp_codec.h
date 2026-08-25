#ifndef RIDER_TEMP_CODEC_H
#define RIDER_TEMP_CODEC_H

#include <stdint.h>

#include "rider_core_temp.h"

#define RIDER_TEMP_CODEC_CORE_FRAME_MAX 9
#define RIDER_TEMP_CODEC_STANDARD_FRAME_SIZE 5
#define RIDER_TEMP_CODEC_DEBUG_PROTOCOL_VERSION 1
#define RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE 41

/* Fixed debug snapshot flags.  The value fields remain present in every
 * frame; a clear bit means that the corresponding temperature is 0x7fff. */
#define RIDER_TEMP_DEBUG_FLAG_SENSOR_VALID       0x01
#define RIDER_TEMP_DEBUG_FLAG_CONTACT_VALID      0x02
#define RIDER_TEMP_DEBUG_FLAG_SKIN_VALID         0x04
#define RIDER_TEMP_DEBUG_FLAG_CORE_ESTIMATE      0x08
#define RIDER_TEMP_DEBUG_FLAG_PUBLISHED_CORE     0x10
#define RIDER_TEMP_DEBUG_FLAG_HEART_RATE_VALID   0x20
#define RIDER_TEMP_DEBUG_FLAG_CORE_VERIFIED      0x40
#define RIDER_TEMP_DEBUG_FLAG_DATA_STALE         0x80

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

/** Encode the fixed-width debug snapshot used by the Rider collector.
 *
 * `published_core_centi` is selected by the GATT publication policy.  Passing
 * 0x7fff keeps an experimental candidate visible in its own field while
 * explicitly marking the compatibility-published value unavailable.
 */
uint16_t rider_encode_debug_snapshot_frame(
    uint8_t *frame, uint16_t frame_size,
    const rider_temperature_snapshot_t *snapshot,
    int16_t published_core_centi);

#endif
