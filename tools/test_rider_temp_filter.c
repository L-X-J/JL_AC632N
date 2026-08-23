#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rider_core_temp.h"

/** Feed one synthetic sample through the product filter. */
static rider_temperature_filter_output_t feed(uint32_t sequence,
                                              int16_t temperature_centi,
                                              uint8_t valid,
                                              uint8_t status)
{
    rider_temperature_sample_t sample = {
        sequence,
        temperature_centi,
        valid,
        status,
    };
    rider_temperature_filter_output_t output;

    rider_temp_filter_consume(&sample, &output);
    return output;
}

/** Verify that an off-body room-temperature sample is never accepted. */
static void test_not_worn_sample_is_dropped(void)
{
    rider_temperature_filter_output_t output;

    rider_temp_filter_init();
    output = feed(1, 2300, 1, RIDER_TEMP_STATUS_OK);
    assert(!output.valid);
    assert(output.status == RIDER_TEMP_STATUS_NOT_WORN);
    assert(output.state == RIDER_TEMP_STATE_NOT_WORN);
    assert(output.filtered_temperature_centi == 0x7fff);
}

/** Verify that the bounded median suppresses one isolated spike. */
static void test_median_rejects_an_in_window_spike(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= 5; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
        assert(output.valid);
    }
    output = feed(6, 4400, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.filtered_temperature_centi == 3600);
}

/** Verify the configured warming count and stable quality transition. */
static void test_warming_and_stable_boundaries(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence < RIDER_TEMP_FILTER_STABLE_SAMPLES; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
        assert(output.valid);
        assert(output.state == RIDER_TEMP_STATE_WARMING);
        assert(output.quality == RIDER_TEMP_QUALITY_POOR);
    }
    output = feed(RIDER_TEMP_FILTER_STABLE_SAMPLES, 3600, 1,
                  RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.state == RIDER_TEMP_STATE_STABLE);
    assert(output.quality == RIDER_TEMP_QUALITY_GOOD);
    assert(output.freshness == RIDER_TEMP_FRESHNESS_FRESH);
}

/** Verify that missing or CRC-invalid samples clear temporal history. */
static void test_gap_and_crc_error_force_rewarm(void)
{
    rider_temperature_filter_output_t output;

    rider_temp_filter_init();
    output = feed(1, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    output = feed(5, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_STALE);
    output = feed(6, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.state == RIDER_TEMP_STATE_WARMING);

    output = feed(7, 3600, 0, RIDER_TEMP_STATUS_CRC_ERROR);
    assert(!output.valid);
    assert(output.status == RIDER_TEMP_STATUS_CRC_ERROR);
    assert(output.state == RIDER_TEMP_STATE_STALE);
    output = feed(8, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.state == RIDER_TEMP_STATE_WARMING);
}

/** Verify that duplicate transport sequence numbers cannot be reused. */
static void test_duplicate_sequence_is_not_reused(void)
{
    rider_temperature_filter_output_t output;

    rider_temp_filter_init();
    output = feed(1, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    output = feed(1, 3610, 1, RIDER_TEMP_STATUS_OK);
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_STALE);
}

/** Run the host-only filter contract checks. */
int main(void)
{
    test_not_worn_sample_is_dropped();
    test_median_rejects_an_in_window_spike();
    test_warming_and_stable_boundaries();
    test_gap_and_crc_error_force_rewarm();
    test_duplicate_sequence_is_not_reused();
    puts("rider_temp_filter host tests: OK");
    return 0;
}
