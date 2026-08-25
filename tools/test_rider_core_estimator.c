#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rider_core_temp.h"
#include "rider_temp_codec.h"

/** Encode the current experimental projection using the production codec. */
static uint16_t encode_snapshot(uint8_t *frame, uint16_t frame_size,
                                const rider_temperature_snapshot_t *snapshot)
{
    int16_t core = snapshot->core_estimate_valid
                       ? snapshot->core_temperature_centi : 0x7fff;

    return rider_encode_core_temperature_frame(frame, frame_size, core, snapshot);
}

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

/** Feed a constant skin value through the complete five-minute warm-up. */
static uint32_t feed_until_core_ready(int16_t temperature_centi)
{
    uint32_t ready_sequence = RIDER_TEMP_FILTER_TRUSTED_SAMPLES +
                              RIDER_CORE_TEMP_HISTORY_SECONDS;
    uint32_t sequence;

    for (sequence = 1; sequence <= ready_sequence; ++sequence) {
        feed(sequence, temperature_centi);
    }
    return ready_sequence;
}

/** Trusted skin and Core readiness must be separate timeline gates. */
static void test_skin_and_core_warmup_are_independent(void)
{
    rider_temperature_snapshot_t snapshot;
    uint8_t frame[RIDER_TEMP_CODEC_CORE_FRAME_MAX];
    uint16_t frame_length;
    uint32_t ready_sequence = RIDER_TEMP_FILTER_TRUSTED_SAMPLES +
                              RIDER_CORE_TEMP_HISTORY_SECONDS;
    uint32_t sequence;

    rider_estimator_init();
    for (sequence = 1; sequence <= RIDER_TEMP_FILTER_TYPICAL_SAMPLES; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.sensor_valid);
    assert(snapshot.contact_valid);
    assert(!snapshot.skin_valid);
    assert(snapshot.core_state == RIDER_CORE_STATE_EMPTY);
    assert(!snapshot.core_estimate_valid);

    for (; sequence <= RIDER_TEMP_FILTER_TRUSTED_SAMPLES; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.temperature_state == RIDER_TEMP_STATE_SKIN_TRUSTED);
    assert(snapshot.skin_valid);
    assert(snapshot.skin_source_sequence == RIDER_TEMP_FILTER_TRUSTED_SAMPLES);
    assert(snapshot.core_state == RIDER_CORE_STATE_WARMUP);
    assert(!snapshot.core_estimate_valid);
    frame_length = encode_snapshot(frame, sizeof(frame), &snapshot);
    assert(frame_length == 6);
    assert(frame[0] == 0x05);
    assert(frame[1] == 0xff && frame[2] == 0x7f);
    assert(frame[3] == 0x10 && frame[4] == 0x0e); /* Skin 36.00 C. */

    for (; sequence < ready_sequence; ++sequence) {
        feed(sequence, 3600);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.skin_valid);
    assert(snapshot.core_history_seconds < RIDER_CORE_TEMP_HISTORY_SECONDS);
    assert(!snapshot.core_estimate_valid);

    feed(ready_sequence, 3600);
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.core_history_seconds == RIDER_CORE_TEMP_HISTORY_SECONDS);
    assert(snapshot.core_state == RIDER_CORE_STATE_READY);
    assert(snapshot.core_estimate_valid);
    assert(snapshot.valid);
    assert(snapshot.core_source_sequence == ready_sequence);
    assert(snapshot.core_temperature_centi >= 3680);
    assert(snapshot.core_temperature_centi <= 3700);
    assert(snapshot.core_temperature_centi != snapshot.skin_temperature_centi);
    frame_length = encode_snapshot(frame, sizeof(frame), &snapshot);
    assert(frame_length == 6);
    assert(!(frame[1] == 0xff && frame[2] == 0x7f));
    assert(frame[3] == 0x10 && frame[4] == 0x0e);
}

/** A gradual falling trusted-skin signal must drive Core V1 downward. */
static void test_core_estimate_follows_skin_downward(void)
{
    rider_temperature_snapshot_t snapshot;
    int16_t initial_core;
    uint32_t sequence;
    uint32_t index;

    rider_estimator_init();
    sequence = feed_until_core_ready(3600);
    rider_estimator_copy_snapshot(&snapshot);
    initial_core = snapshot.core_temperature_centi;

    for (index = 1; index <= 80; ++index) {
        int16_t skin = index <= 50 ? (int16_t)(3600 - index) : 3550;
        feed(++sequence, skin);
    }
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.skin_valid);
    assert(snapshot.core_state == RIDER_CORE_STATE_READY);
    assert(snapshot.skin_temperature_centi < 3600);
    assert(snapshot.core_temperature_centi < initial_core);
}

/** Fresh external HR selects the enhanced model and clears without a jump. */
static void test_heart_rate_model_and_fallback_are_observable(void)
{
    rider_temperature_snapshot_t snapshot;
    int16_t skin_only_core;
    int16_t hr_core;
    uint32_t sequence;
    uint32_t index;

    rider_estimator_init();
    sequence = feed_until_core_ready(3500);
    rider_estimator_copy_snapshot(&snapshot);
    skin_only_core = snapshot.core_temperature_centi;
    assert(snapshot.model_mode == RIDER_CORE_MODEL_SKIN_ONLY);
    assert(!snapshot.heart_rate_used);

    rider_estimator_set_external_heart_rate(160, 1);
    for (index = 0; index < 90; ++index) {
        feed(++sequence, 3500);
    }
    rider_estimator_copy_snapshot(&snapshot);
    hr_core = snapshot.core_temperature_centi;
    assert(snapshot.model_mode == RIDER_CORE_MODEL_SKIN_AND_HR);
    assert(snapshot.heart_rate_valid);
    assert(snapshot.heart_rate_used);
    assert(hr_core > skin_only_core);

    rider_estimator_set_external_heart_rate(0, 0);
    rider_estimator_copy_snapshot(&snapshot);
    assert(!snapshot.heart_rate_valid);
    assert(snapshot.heart_rate_used);
    assert(snapshot.model_mode == RIDER_CORE_MODEL_SKIN_AND_HR);
    feed(++sequence, 3500);
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.model_mode == RIDER_CORE_MODEL_SKIN_ONLY);
    assert(!snapshot.heart_rate_valid);
    assert(!snapshot.heart_rate_used);
    assert(snapshot.core_estimate_valid);
    assert(snapshot.core_temperature_centi < hr_core);
}

/** 35~42 C is an invalidity gate and must never clamp to a fake boundary. */
static void test_out_of_range_target_is_not_clamped(void)
{
    rider_core_temperature_calibration_t calibration = {
        5000, 0, 0, 0, 0, 0, 0, 8, 1, 0,
    };
    rider_temperature_snapshot_t snapshot;
    uint32_t sequence;

    rider_estimator_init();
    sequence = feed_until_core_ready(3600);
    rider_estimator_set_core_calibration(&calibration);
    feed(++sequence, 3600);
    rider_estimator_copy_snapshot(&snapshot);
    assert(snapshot.skin_valid);
    assert(snapshot.core_state == RIDER_CORE_STATE_INVALID);
    assert(!snapshot.core_estimate_valid);
    assert(snapshot.core_temperature_centi == 0x7fff);
    assert(snapshot.core_temperature_centi !=
           RIDER_CORE_TEMP_ESTIMATE_MAX_CENTI);
}

/** A detach candidate must create a real export gap, never a held sample. */
static void test_detach_holds_then_invalidates_core(void)
{
    rider_temperature_snapshot_t snapshot;
    uint8_t saw_hold = 0;
    uint32_t sequence;
    uint32_t index;

    rider_estimator_init();
    sequence = feed_until_core_ready(3600);
    for (index = 0; index < 12; ++index) {
        feed(++sequence, 3300);
        rider_estimator_copy_snapshot(&snapshot);
        if (snapshot.core_state == RIDER_CORE_STATE_HOLD) {
            saw_hold = 1;
            assert(!snapshot.skin_valid);
            assert(!snapshot.core_estimate_valid);
            assert(snapshot.core_temperature_centi == 0x7fff);
        }
    }
    assert(saw_hold);
    assert(snapshot.temperature_state == RIDER_TEMP_STATE_DETACH_SUSPECTED);
    assert(!snapshot.skin_valid);
    assert(!snapshot.core_estimate_valid);
}

/** Run the production estimator contract checks. */
int main(void)
{
    test_skin_and_core_warmup_are_independent();
    test_core_estimate_follows_skin_downward();
    test_heart_rate_model_and_fallback_are_observable();
    test_out_of_range_target_is_not_clamped();
    test_detach_holds_then_invalidates_core();
    puts("rider_core_estimator host tests: OK");
    return 0;
}
