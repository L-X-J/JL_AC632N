#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"

#if CONFIG_APP_RIDER_CORE_TEMP

static u16 rider_sample_timer_id;
static uint32_t rider_consumed_sequence;

/** Complete the previous conversion, queue the next one, then publish BLE. */
static void rider_core_temp_scheduler(void *priv)
{
    rider_temperature_sample_t sample;
    uint32_t sequence;

    (void)priv;
    rider_temp_start_conversion();
    sequence = rider_temp_sequence();
    if (sequence != rider_consumed_sequence && rider_temp_copy_latest(&sample)) {
        rider_estimator_consume(&sample);
        rider_consumed_sequence = sequence;
    }
    rider_estimator_tick(sequence);
    rider_core_temp_ble_tick();
}

/** Start one conversion immediately and then maintain the one-second cadence. */
void rider_core_temp_start_scheduler(void)
{
    if (rider_sample_timer_id) {
        return;
    }

    /* The application owns the sensor/estimator lifetime so a restart cannot
     * publish a snapshot left over from a previous BLE session. */
    rider_temp_init();
    rider_estimator_init();
    rider_consumed_sequence = rider_temp_sequence();
    rider_temp_start_conversion();
    rider_sample_timer_id = sys_timer_add(NULL, rider_core_temp_scheduler, 1000);
}

/** Stop periodic sampling before the BLE stack is torn down. */
void rider_core_temp_stop_scheduler(void)
{
    if (rider_sample_timer_id) {
        sys_timer_del(rider_sample_timer_id);
        rider_sample_timer_id = 0;
    }
    rider_temp_stop();
}

#endif
