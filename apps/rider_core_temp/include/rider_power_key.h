#ifndef RIDER_POWER_KEY_H
#define RIDER_POWER_KEY_H

#include <stdint.h>

typedef void (*rider_power_key_prepare_poweroff_cb_t)(void);

/** Reset the application-owned power-key state before startup confirmation. */
void rider_power_key_init(void);

/** Register the callback that stops product activity before soft power-off. */
void rider_power_key_register_poweroff_prepare(
    rider_power_key_prepare_poweroff_cb_t callback);

/** Confirm a PB3 wakeup or accept a non-key startup; returns zero on rejection. */
uint8_t rider_power_key_startup_check(void);

/** Start the 5 ms runtime key scan and the power-on prompt timer. */
void rider_power_key_start(void);

/** Stop the runtime scan and force PB5 off during an application shutdown. */
void rider_power_key_stop(void);

#endif
