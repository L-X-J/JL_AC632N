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

int main(void)
{
    test_core_frame_keeps_centi_degree_precision();
    test_standard_frame_uses_exponent_minus_two();
    puts("rider_temp_codec host tests: OK");
    return 0;
}
