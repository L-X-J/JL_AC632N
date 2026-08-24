#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rider_core_temp.h"

/** Feed one valid fixed-point M601 sample through the estimator. */
static void feed(uint32_t sequence, int16_t temperature_centi)
{
    rider_temperature_sample_t sample = {
        sequence,
        temperature_centi,
        1,
        RIDER_TEMP_STATUS_OK,
    };

    rider_estimator_consume(&sample);
}

/** Verify that a falling filtered skin signal drives the core estimate down. */
static void test_core_estimate_follows_skin_downward(void)
{
    rider_temperature_snapshot_t snapshot;
    int16_t initial_core;
    uint32_t sequence;

    rider_estimator_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_NORMAL_SAMPLES; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.temperature_state == RIDER_TEMP_STATE_STABLE);
    assert(snapshot.skin_valid);
    assert(snapshot.core_estimate_valid);
    initial_core = snapshot.core_temperature_centi;

    for (sequence = RIDER_TEMP_FILTER_NORMAL_SAMPLES + 1;
         sequence <= RIDER_TEMP_FILTER_NORMAL_SAMPLES + 25; ++sequence) {
        feed(sequence, 3500);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.skin_temperature_centi < 3600);
    assert(snapshot.core_estimate_valid);
    assert(snapshot.core_temperature_centi < initial_core);
}

/** Verify a confirmed detach invalidates core output and forces rewarm. */
static void test_detach_does_not_feed_core_model(void)
{
    rider_temperature_snapshot_t snapshot;
    uint8_t saw_hold = 0;
    uint32_t sequence;

    rider_estimator_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_NORMAL_SAMPLES; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.core_estimate_valid);

    for (sequence = RIDER_TEMP_FILTER_NORMAL_SAMPLES + 1;
         sequence <= RIDER_TEMP_FILTER_NORMAL_SAMPLES + 12; ++sequence) {
        feed(sequence, 3300);
        rider_estimator_copy_snapshot(&snapshot);
        if (snapshot.temperature_state == RIDER_TEMP_STATE_STABLE &&
            !snapshot.core_input_valid) {
            saw_hold = 1;
            assert(!snapshot.core_estimate_valid);
        }
    }
    assert(saw_hold);
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.temperature_state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
    assert(!snapshot.skin_valid);
    assert(!snapshot.core_input_valid);
    assert(!snapshot.core_estimate_valid);
    assert(snapshot.core_temperature_centi == 0x7fff);

    for (sequence += 1;
         sequence <= RIDER_TEMP_FILTER_NORMAL_SAMPLES * 2 + 18; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.temperature_state == RIDER_TEMP_STATE_STABLE);
    assert(snapshot.core_estimate_valid);
}

/** Run the estimator directionality contract check. */
int main(void)
{
    test_core_estimate_follows_skin_downward();
    test_detach_does_not_feed_core_model();
    puts("rider_core_estimator host tests: OK");
    return 0;
}
