#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rider_core_temp.h"

/** Feed one synthetic sample through the production filter. */
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

/** Off-body room temperature belongs only to the sensor timeline. */
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

/** The bounded median must suppress one isolated in-window spike. */
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

/** Five typical samples fill the median but cannot establish trusted skin. */
static void test_five_typical_samples_do_not_shortcut_skin_trust(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TYPICAL_SAMPLES; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.valid);
    assert(!output.skin_trusted);
    assert(!output.core_input_valid);
    assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);
    assert(output.typical_samples == RIDER_TEMP_FILTER_TYPICAL_SAMPLES);
}

/** Exactly thirty contiguous valid samples qualify the trusted-skin timeline. */
static void test_trusted_skin_boundary_is_thirty_samples(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence < RIDER_TEMP_FILTER_TRUSTED_SAMPLES; ++sequence) {
        output = feed(sequence, 3400, 1, RIDER_TEMP_STATUS_OK);
        assert(output.valid);
        assert(!output.skin_trusted);
        assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);
    }
    output = feed(RIDER_TEMP_FILTER_TRUSTED_SAMPLES, 3400, 1,
                  RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.skin_trusted);
    assert(output.core_input_valid);
    assert(output.state == RIDER_TEMP_STATE_SKIN_TRUSTED);
    assert(output.quality == RIDER_TEMP_QUALITY_GOOD);
}

/** Leaving 32~40 C after qualification must not revoke continuous contact. */
static void test_typical_band_is_not_a_latched_validity_range(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.skin_trusted);

    for (; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES + 8; ++sequence) {
        output = feed(sequence, 4100, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.valid);
    assert(output.skin_trusted);
    assert(output.state == RIDER_TEMP_STATE_SKIN_TRUSTED);
    assert(output.typical_samples == 0);
}

/** A small monotonic cooling trend must survive EWMA rounding. */
static void test_small_cooling_trend_is_preserved(void)
{
    rider_temperature_filter_output_t output;
    uint32_t sequence;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    for (; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES + 5; ++sequence) {
        output = feed(sequence, 3599, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.valid);
    assert(output.skin_trusted);
    assert(output.core_input_valid);
    assert(output.filtered_temperature_centi < 3600);
}

/** Fast cooling leaves trusted skin immediately and eventually latches detach. */
static void test_fast_cooling_confirms_detach_and_rewarms(void)
{
    rider_temperature_filter_output_t output;
    uint8_t saw_candidate = 0;
    uint32_t sequence;
    uint32_t index;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES; ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.state == RIDER_TEMP_STATE_SKIN_TRUSTED);

    for (index = 0; index < 12; ++index, ++sequence) {
        output = feed(sequence, 3300, 1, RIDER_TEMP_STATUS_OK);
        if (output.valid &&
            output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED) {
            saw_candidate = 1;
            assert(!output.skin_trusted);
            assert(!output.core_input_valid);
        }
    }
    assert(saw_candidate);
    /* A constant 33 C input eventually creates enough filtered drop to latch;
     * stop at the first confirmed detach instead of consuming later samples. */
    while (output.valid && index < 30) {
        output = feed(sequence++, 3300, 1, RIDER_TEMP_STATUS_OK);
        index++;
    }
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);

    for (index = 0; index < RIDER_TEMP_FILTER_TYPICAL_SAMPLES;
         ++index, ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);

    for (index = 0; index < RIDER_TEMP_FILTER_TRUSTED_SAMPLES;
         ++index, ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.valid);
    assert(output.skin_trusted);
    assert(output.state == RIDER_TEMP_STATE_SKIN_TRUSTED);
}

/** A trusted probe held near 30.3 C must not remain publishable forever. */
static void test_low_temperature_dwell_confirms_off_body(void)
{
    rider_temperature_filter_output_t output;
    uint32_t low_samples = 0;
    uint32_t sequence;
    uint32_t index;

    rider_temp_filter_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES;
         ++sequence) {
        output = feed(sequence, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(output.skin_trusted);

    /* Descend over several minutes with a filtered slope below the fast gate;
     * this models a probe cooling toward the reported 30.3 C ambient plateau. */
    for (; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES + 285; ++sequence) {
        int16_t temperature = (int16_t)(3600 -
            ((sequence - RIDER_TEMP_FILTER_TRUSTED_SAMPLES) * 570) / 285);

        output = feed(sequence, temperature, 1, RIDER_TEMP_STATUS_OK);
        assert(output.valid);
        assert(output.slope_centi_per_min > RIDER_TEMP_FILTER_DETACH_SLOPE_CPM);
        if (output.filtered_temperature_centi <=
            RIDER_TEMP_FILTER_OFF_BODY_MAX_CENTI) {
            low_samples++;
            assert(low_samples < RIDER_TEMP_FILTER_OFF_BODY_CONFIRM_SAMPLES);
            assert(!output.skin_trusted);
            assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
        }
    }

    assert(low_samples > 0);
    assert(output.filtered_temperature_centi <= 3100);
    assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);

    /* The low dwell began during the descent, so only the remaining samples
     * are needed to reach the explicit 60-sample confirmation boundary. */
    for (index = 0; output.valid && index <
         RIDER_TEMP_FILTER_OFF_BODY_CONFIRM_SAMPLES; ++index) {
        output = feed(sequence++, 3030, 1, RIDER_TEMP_STATUS_OK);
        if (output.valid) {
            assert(output.slope_centi_per_min >
                   RIDER_TEMP_FILTER_DETACH_SLOPE_CPM);
            low_samples++;
            assert(low_samples < RIDER_TEMP_FILTER_OFF_BODY_CONFIRM_SAMPLES);
            assert(!output.skin_trusted);
            assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
        }
    }
    assert(!output.valid);
    assert(output.status == RIDER_TEMP_STATUS_NOT_WORN);
    assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
    assert(output.filtered_temperature_centi == 0x7fff);

    /* A 32 C ambient-like plateau is not enough to reattach to the previous
     * 36 C trusted episode; the existing peak-margin gate remains active. */
    for (index = 0; index < RIDER_TEMP_FILTER_TYPICAL_SAMPLES; ++index) {
        output = feed(sequence++, 3200, 1, RIDER_TEMP_STATUS_OK);
        assert(!output.valid);
        assert(output.state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
    }
    for (index = 0; index < RIDER_TEMP_FILTER_TYPICAL_SAMPLES; ++index) {
        output = feed(sequence++, 3600, 1, RIDER_TEMP_STATUS_OK);
    }
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);
}

/** Missing, duplicate, and CRC-invalid samples clear temporal qualification. */
static void test_gap_crc_and_duplicate_force_requalification(void)
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
    assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);

    output = feed(7, 3600, 0, RIDER_TEMP_STATUS_CRC_ERROR);
    assert(!output.valid);
    assert(output.status == RIDER_TEMP_STATUS_CRC_ERROR);
    output = feed(8, 3600, 1, RIDER_TEMP_STATUS_OK);
    assert(output.valid);
    assert(output.state == RIDER_TEMP_STATE_CONTACT_SETTLING);

    output = feed(8, 3610, 1, RIDER_TEMP_STATUS_OK);
    assert(!output.valid);
    assert(output.state == RIDER_TEMP_STATE_STALE);
}

/** Run the production filter contract checks. */
int main(void)
{
    test_not_worn_sample_is_dropped();
    test_median_rejects_an_in_window_spike();
    test_five_typical_samples_do_not_shortcut_skin_trust();
    test_trusted_skin_boundary_is_thirty_samples();
    test_typical_band_is_not_a_latched_validity_range();
    test_small_cooling_trend_is_preserved();
    test_fast_cooling_confirms_detach_and_rewarms();
    test_low_temperature_dwell_confirms_off_body();
    test_gap_crc_and_duplicate_force_requalification();
    puts("rider_temp_filter host tests: OK");
    return 0;
}
