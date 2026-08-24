#if defined(RIDER_TEMP_FILTER_HOST_TEST)
#include <string.h>
#else
#include "system/includes.h"
#include "app_config.h"
#endif
#include "rider_core_temp.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/* The filter owns temporal state; the M601 driver remains responsible only
 * for bus timing, CRC validation and electrical-range conversion. */
typedef struct {
    int16_t history[RIDER_TEMP_FILTER_MEDIAN_SAMPLES];
    uint8_t history_count;
    uint8_t history_write;
    uint8_t have_sequence;
    uint8_t have_filtered;
    uint32_t last_sequence;
    uint16_t valid_samples;
    uint8_t normal_samples;
    uint8_t stable_latched;
    int16_t filtered_temperature_centi;
    int16_t previous_filtered_centi;
} rider_temperature_filter_state_t;

static rider_temperature_filter_state_t rider_filter;

/** Reset the short history while retaining the transport sequence marker. */
static void rider_filter_reset_history(void)
{
    memset(rider_filter.history, 0, sizeof(rider_filter.history));
    rider_filter.history_count = 0;
    rider_filter.history_write = 0;
    rider_filter.valid_samples = 0;
    rider_filter.normal_samples = 0;
    rider_filter.stable_latched = 0;
    rider_filter.have_filtered = 0;
    rider_filter.filtered_temperature_centi = 0;
    rider_filter.previous_filtered_centi = 0;
}

/** Fill an unavailable filter result without inventing a temperature value. */
static void rider_filter_set_invalid(rider_temperature_filter_output_t *output,
                                     uint8_t status, uint8_t state)
{
    memset(output, 0, sizeof(*output));
    output->sequence = rider_filter.last_sequence;
    output->filtered_temperature_centi = 0x7fff;
    output->slope_centi_per_min = 0;
    output->quality = RIDER_TEMP_QUALITY_INVALID;
    output->status = status;
    output->state = state;
    output->freshness = state == RIDER_TEMP_STATE_STALE
                            ? RIDER_TEMP_FRESHNESS_STALE
                            : RIDER_TEMP_FRESHNESS_UNAVAILABLE;
}

/** Map a transport failure to the product state without inventing a value. */
static uint8_t rider_filter_invalid_state(uint8_t status)
{
    if (status == RIDER_TEMP_STATUS_NO_DEVICE) {
        return RIDER_TEMP_STATE_NO_DEVICE;
    }
    if (status == RIDER_TEMP_STATUS_NOT_WORN) {
        return RIDER_TEMP_STATE_NOT_WORN;
    }
    return RIDER_TEMP_STATE_STALE;
}

/** Return the median of the bounded five-sample history. */
static int16_t rider_filter_median(void)
{
    int16_t sorted[RIDER_TEMP_FILTER_MEDIAN_SAMPLES];
    uint8_t count = rider_filter.history_count;
    uint8_t i;

    for (i = 0; i < count; ++i) {
        int16_t value = rider_filter.history[i];
        uint8_t j = i;

        while (j > 0 && sorted[j - 1] > value) {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = value;
    }
    return sorted[count / 2];
}

/** Apply a signed Q8 EWMA step without relying on implementation-defined shifts. */
static int16_t rider_filter_ewma(int16_t previous, int16_t input)
{
    int32_t delta = (int32_t)input - previous;
    int32_t step = delta * RIDER_TEMP_FILTER_EWMA_ALPHA_Q8;

    if (step >= 0) {
        step = (step + 128) / 256;
    } else {
        step = -(((-step) + 128) / 256);
    }
    return (int16_t)(previous + step);
}

/** Return whether a raw reading is in the accelerated skin-temperature band. */
static uint8_t rider_filter_in_normal_band(int16_t temperature_centi)
{
    return temperature_centi >= RIDER_TEMP_FILTER_NORMAL_MIN_CENTI &&
           temperature_centi <= RIDER_TEMP_FILTER_NORMAL_MAX_CENTI;
}

/** Reset the filter and start a new temporal contact episode. */
void rider_temp_filter_init(void)
{
    memset(&rider_filter, 0, sizeof(rider_filter));
}

/**
 * Consume one M601 sample and classify its filtered contact signal.
 *
 * A valid 1-Wire frame is not sufficient evidence of skin contact.  The
 * product window, a contiguous sample run and the short robust filter are
 * intentionally kept here so the estimator and GATT layers cannot bypass
 * missing/stale data handling.
 */
void rider_temp_filter_consume(const rider_temperature_sample_t *sample,
                               rider_temperature_filter_output_t *output)
{
    uint32_t sequence_gap = 0;
    int16_t median;
    int16_t slope = 0;

    if (!output) {
        return;
    }
    if (!sample) {
        rider_filter_set_invalid(output, RIDER_TEMP_STATUS_NO_DEVICE,
                                 RIDER_TEMP_STATE_STALE);
        return;
    }

    if (rider_filter.have_sequence) {
        if (sample->sequence <= rider_filter.last_sequence) {
            rider_filter_reset_history();
            rider_filter_set_invalid(output, sample->status,
                                     RIDER_TEMP_STATE_STALE);
            return;
        }
        sequence_gap = sample->sequence - rider_filter.last_sequence;
    }
    rider_filter.last_sequence = sample->sequence;
    rider_filter.have_sequence = 1;

    if (!sample->valid || sample->status != RIDER_TEMP_STATUS_OK) {
        rider_filter_reset_history();
        rider_filter_set_invalid(output, sample->status,
                                 rider_filter_invalid_state(sample->status));
        return;
    }

    /* A long gap must never be bridged by the filter or the core model. */
    if (sequence_gap > RIDER_TEMP_FILTER_MAX_GAP_SAMPLES) {
        rider_filter_reset_history();
        rider_filter_set_invalid(output, sample->status,
                                 RIDER_TEMP_STATE_STALE);
        return;
    }

    /* The electrical range belongs to the driver; this narrower product
     * window rejects ordinary off-body room temperatures such as 23 C. */
    if (sample->temperature_centi < RIDER_CORE_TEMP_WEAR_MIN_CENTI ||
        sample->temperature_centi > RIDER_CORE_TEMP_WEAR_MAX_CENTI) {
        rider_filter_reset_history();
        rider_filter_set_invalid(output, RIDER_TEMP_STATUS_NOT_WORN,
                                 RIDER_TEMP_STATE_NOT_WORN);
        return;
    }

    rider_filter.history[rider_filter.history_write] = sample->temperature_centi;
    rider_filter.history_write = (uint8_t)((rider_filter.history_write + 1) %
                                           RIDER_TEMP_FILTER_MEDIAN_SAMPLES);
    if (rider_filter.history_count < RIDER_TEMP_FILTER_MEDIAN_SAMPLES) {
        rider_filter.history_count++;
    }
    median = rider_filter_median();

    if (!rider_filter.have_filtered) {
        rider_filter.filtered_temperature_centi = median;
        rider_filter.previous_filtered_centi = median;
        rider_filter.have_filtered = 1;
    } else {
        rider_filter.previous_filtered_centi =
            rider_filter.filtered_temperature_centi;
        rider_filter.filtered_temperature_centi = rider_filter_ewma(
            rider_filter.filtered_temperature_centi, median);
        if (sequence_gap) {
            slope = (int16_t)(((int32_t)rider_filter.filtered_temperature_centi -
                               rider_filter.previous_filtered_centi) * 60 /
                              (int32_t)sequence_gap);
        }
    }

    if (rider_filter.valid_samples < RIDER_TEMP_FILTER_STABLE_SAMPLES) {
        rider_filter.valid_samples++;
    }
    if (!rider_filter.stable_latched) {
        if (rider_filter_in_normal_band(sample->temperature_centi)) {
            if (rider_filter.normal_samples < RIDER_TEMP_FILTER_NORMAL_SAMPLES) {
                rider_filter.normal_samples++;
            }
        } else {
            rider_filter.normal_samples = 0;
        }
        if (rider_filter.valid_samples >= RIDER_TEMP_FILTER_STABLE_SAMPLES ||
            rider_filter.normal_samples >= RIDER_TEMP_FILTER_NORMAL_SAMPLES) {
            /* Qualification is latched for this continuous wear episode. A
             * later in-window fluctuation must not make BLE state oscillate. */
            rider_filter.stable_latched = 1;
        }
    }
    output->sequence = sample->sequence;
    output->filtered_temperature_centi = rider_filter.filtered_temperature_centi;
    output->slope_centi_per_min = slope;
    output->valid = 1;
    output->status = RIDER_TEMP_STATUS_OK;
    output->state = rider_filter.stable_latched ? RIDER_TEMP_STATE_STABLE
                                                : RIDER_TEMP_STATE_WARMING;
    if (output->state == RIDER_TEMP_STATE_STABLE) {
        output->quality = (slope < -RIDER_TEMP_FILTER_SLOPE_LIMIT_CPM ||
                           slope > RIDER_TEMP_FILTER_SLOPE_LIMIT_CPM)
                              ? RIDER_TEMP_QUALITY_FAIR
                              : RIDER_TEMP_QUALITY_GOOD;
    } else {
        output->quality = RIDER_TEMP_QUALITY_POOR;
    }
    output->freshness = RIDER_TEMP_FRESHNESS_FRESH;
}

#endif
