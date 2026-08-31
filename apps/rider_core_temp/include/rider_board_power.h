#ifndef RIDER_BOARD_POWER_H
#define RIDER_BOARD_POWER_H

#include <stdint.h>

/** Return the current active-low Rider power-key state. */
uint8_t rider_board_power_key_pressed(void);

/** Return whether the most recent wakeup was caused by the Rider power key. */
uint8_t rider_board_power_key_wakeup(void);

#endif
