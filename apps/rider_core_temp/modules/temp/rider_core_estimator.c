#include "system/includes.h"
#include "app_config.h"
#include "rider_core_temp.h"

#define LOG_TAG_CONST       RIDER_ESTIMATOR
#define LOG_TAG             "[RIDER_ESTIMATOR]"
#define LOG_INFO_ENABLE
#include "debug.h"

#if CONFIG_APP_RIDER_CORE_TEMP

/* This is an electrical-sensor adapter, not a physiological model.  The
 * supplied M601 material defines only raw-temperature conversion, so the
 * protocol quality remains N/A until a separately specified confidence source
 * exists. */
static rider_temperature_snapshot_t rider_snapshot;

/** Reset the protocol-facing snapshot to the CORE N/A representation. */
void rider_estimator_init(void)
{
    memset(&rider_snapshot, 0, sizeof(rider_snapshot));
    rider_snapshot.quality = 7; /* CORE Quality N/A */
    rider_snapshot.sensor_status = RIDER_TEMP_STATUS_NO_DEVICE;
    rider_snapshot.core_temperature_centi = 0x7fff;
    log_info("Estimator init: valid=0 status=%u core_centi=32767 skin=NA average=NA\n",
             (unsigned)rider_snapshot.sensor_status);
}

/** Publish only a CRC-checked, range-checked M601 sample. */
void rider_estimator_consume(const rider_temperature_sample_t *sample)
{
    if (!sample) {
        return;
    }

    rider_snapshot.sequence = sample->sequence;
    rider_snapshot.sensor_status = sample->status;
    if (!sample->valid) {
        rider_snapshot.valid = 0;
        rider_snapshot.quality = 7;
        rider_snapshot.core_temperature_centi = 0x7fff;
        log_info("Estimator snapshot: seq=%u valid=0 status=%u core_centi=32767 "
                 "skin=NA average=NA\n",
                 (unsigned)sample->sequence, (unsigned)sample->status);
        return;
    }

    rider_snapshot.core_temperature_centi = sample->temperature_centi;
    rider_snapshot.valid = 1;
    /* A bus-valid reading is not evidence of skin contact or physiology. */
    rider_snapshot.quality = 7;
    log_info("Estimator snapshot: seq=%u valid=1 status=%u core_centi=%d "
             "skin=NA average=NA quality=%u\n",
             (unsigned)sample->sequence, (unsigned)sample->status,
             (int)rider_snapshot.core_temperature_centi,
             (unsigned)rider_snapshot.quality);
}

/** Copy the current snapshot so GATT callbacks never mutate shared storage. */
void rider_estimator_copy_snapshot(rider_temperature_snapshot_t *snapshot)
{
    if (snapshot) {
        memcpy(snapshot, &rider_snapshot, sizeof(*snapshot));
    }
}

/** Store or clear the direct external heart-rate input from Control Point. */
void rider_estimator_set_external_heart_rate(uint8_t heart_rate, uint8_t valid)
{
    rider_snapshot.heart_rate = heart_rate;
    rider_snapshot.heart_rate_valid = valid ? 1 : 0;
}

#endif
