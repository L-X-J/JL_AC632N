#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rider_temp_codec.h"

/** Verify that custom CORE fields preserve hundredths of a degree. */
static void test_core_frame_keeps_centi_degree_precision(void)
{
    rider_temperature_snapshot_t snapshot = {0};
    uint8_t frame[RIDER_TEMP_CODEC_CORE_FRAME_MAX];
    uint16_t length;

    snapshot.skin_valid = 1;
    snapshot.skin_temperature_centi = 3650;
    snapshot.quality = RIDER_TEMP_QUALITY_GOOD;
    length = rider_encode_core_temperature_frame(frame, sizeof(frame),
                                                  3675, &snapshot);

    assert(length == 6);
    assert(frame[0] == 0x05);
    assert(frame[1] == 0x5b && frame[2] == 0x0e); /* 36.75 C */
    assert(frame[3] == 0x42 && frame[4] == 0x0e); /* 36.50 C */
    assert(frame[5] == 0x13); /* Good + HR supported, no HR signal. */
}

/** Verify official HR reception state and the optional HR payload byte. */
static void test_core_frame_reports_received_heart_rate(void)
{
    rider_temperature_snapshot_t snapshot = {0};
    uint8_t frame[RIDER_TEMP_CODEC_CORE_FRAME_MAX];
    uint16_t length;

    snapshot.skin_valid = 1;
    snapshot.skin_temperature_centi = 3500;
    snapshot.quality = RIDER_TEMP_QUALITY_GOOD;
    snapshot.heart_rate = 160;
    snapshot.heart_rate_valid = 1;
    snapshot.heart_rate_used = 1;
    length = rider_encode_core_temperature_frame(frame, sizeof(frame),
                                                  3750, &snapshot);

    assert(length == 7);
    assert(frame[0] == 0x15); /* Skin, Quality & State, and HR are present. */
    assert(frame[5] == 0x23); /* Good + receiving HR signal. */
    assert(frame[6] == 160);
}

/** Trusted skin remains exportable while Core is explicitly unavailable. */
static void test_core_frame_keeps_skin_during_core_warmup(void)
{
    rider_temperature_snapshot_t snapshot = {0};
    uint8_t frame[RIDER_TEMP_CODEC_CORE_FRAME_MAX];

    snapshot.skin_valid = 1;
    snapshot.skin_temperature_centi = 3450;
    snapshot.quality = RIDER_TEMP_QUALITY_GOOD;
    assert(rider_encode_core_temperature_frame(frame, sizeof(frame), 0x7fff,
                                               &snapshot) == 6);
    assert(frame[1] == 0xff && frame[2] == 0x7f);
    assert(frame[3] == 0x7a && frame[4] == 0x0d);
}

/** Verify HTS uses a centi-degree mantissa and -2 exponent. */
static void test_standard_frame_uses_exponent_minus_two(void)
{
    uint8_t value[RIDER_TEMP_CODEC_STANDARD_FRAME_SIZE];

    assert(rider_encode_standard_temperature_frame(value, sizeof(value), 1,
                                                   3675) == 5);
    assert(value[0] == 0x04);
    assert(value[1] == 0x5b && value[2] == 0x0e && value[3] == 0x00);
    assert(value[4] == 0xfe);

    rider_encode_standard_temperature_frame(value, sizeof(value), 0, 3675);
    assert(value[1] == 0xff && value[2] == 0xff && value[3] == 0x7f);
    assert(value[4] == 0x00);
}

/** Verify the fixed debug frame keeps all three temperature timelines aligned. */
static void test_debug_frame_has_stable_offsets(void)
{
    rider_temperature_snapshot_t snapshot = {0};
    uint8_t frame[RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE];
    uint16_t length;

    snapshot.sequence = 0x78563412;
    snapshot.sensor_valid = 1;
    snapshot.sensor_temperature_centi = 2312;
    snapshot.contact_valid = 1;
    snapshot.contact_temperature_centi = 3412;
    snapshot.skin_valid = 1;
    snapshot.skin_temperature_centi = 3525;
    snapshot.core_estimate_valid = 1;
    snapshot.core_temperature_centi = 3675;
    snapshot.core_estimate_verified = 1;
    snapshot.quality = RIDER_TEMP_QUALITY_GOOD;
    snapshot.sensor_status = RIDER_TEMP_STATUS_OK;
    snapshot.temperature_state = RIDER_TEMP_STATE_SKIN_TRUSTED;
    snapshot.core_state = RIDER_CORE_STATE_READY;
    snapshot.data_freshness = RIDER_TEMP_FRESHNESS_FRESH;
    snapshot.confidence = 88;
    snapshot.model_mode = RIDER_CORE_MODEL_SKIN_AND_HR;
    snapshot.model_version = 1;
    snapshot.heart_rate = 144;
    snapshot.heart_rate_valid = 1;
    snapshot.heart_rate_used = 1;
    snapshot.core_history_seconds = 300;
    snapshot.contact_samples = 42;
    snapshot.typical_samples = 12;

    length = rider_encode_debug_snapshot_frame(frame, sizeof(frame), &snapshot,
                                                3660);
    assert(length == RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE);
    assert(frame[0] == RIDER_TEMP_CODEC_DEBUG_PROTOCOL_VERSION);
    assert((frame[1] & (RIDER_TEMP_DEBUG_FLAG_SENSOR_VALID |
                        RIDER_TEMP_DEBUG_FLAG_SKIN_VALID |
                        RIDER_TEMP_DEBUG_FLAG_PUBLISHED_CORE)) ==
           (RIDER_TEMP_DEBUG_FLAG_SENSOR_VALID |
            RIDER_TEMP_DEBUG_FLAG_SKIN_VALID |
            RIDER_TEMP_DEBUG_FLAG_PUBLISHED_CORE));
    assert(frame[2] == 0x12 && frame[3] == 0x34 &&
           frame[4] == 0x56 && frame[5] == 0x78);
    assert(frame[6] == 0x08 && frame[7] == 0x09); /* Sensor 23.12 C. */
    assert(frame[10] == 0xc5 && frame[11] == 0x0d); /* Skin 35.25 C. */
    assert(frame[12] == 0x5b && frame[13] == 0x0e); /* Core 36.75 C. */
    assert(frame[14] == 0x4c && frame[15] == 0x0e); /* Published 36.60 C. */
    assert(frame[31] == 144 && frame[37] == 88 && frame[40] == 1);
}

/** Invalid fields must remain explicit and must not become a zero reading. */
static void test_debug_frame_uses_unavailable_sentinel(void)
{
    rider_temperature_snapshot_t snapshot = {0};
    uint8_t frame[RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE];

    snapshot.quality = RIDER_TEMP_QUALITY_NA;
    assert(rider_encode_debug_snapshot_frame(frame, sizeof(frame), &snapshot,
                                             (int16_t)0x7fff) ==
           RIDER_TEMP_CODEC_DEBUG_FRAME_SIZE);
    assert(frame[6] == 0xff && frame[7] == 0x7f);
    assert(frame[10] == 0xff && frame[11] == 0x7f);
    assert(frame[12] == 0xff && frame[13] == 0x7f);
    assert(frame[14] == 0xff && frame[15] == 0x7f);
    assert((frame[1] & (RIDER_TEMP_DEBUG_FLAG_SENSOR_VALID |
                        RIDER_TEMP_DEBUG_FLAG_PUBLISHED_CORE)) == 0);
}

/** Run the protocol codec contract checks. */
int main(void)
{
    test_core_frame_keeps_centi_degree_precision();
    test_core_frame_reports_received_heart_rate();
    test_core_frame_keeps_skin_during_core_warmup();
    test_standard_frame_uses_exponent_minus_two();
    test_debug_frame_has_stable_offsets();
    test_debug_frame_uses_unavailable_sentinel();
    puts("rider_temp_codec host tests: OK");
    return 0;
}
