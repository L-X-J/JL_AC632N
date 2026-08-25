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

/** Run the protocol codec contract checks. */
int main(void)
{
    test_core_frame_keeps_centi_degree_precision();
    test_core_frame_reports_received_heart_rate();
    test_core_frame_keeps_skin_during_core_warmup();
    test_standard_frame_uses_exponent_minus_two();
    puts("rider_temp_codec host tests: OK");
    return 0;
}
