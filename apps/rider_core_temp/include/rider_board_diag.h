#ifndef RIDER_BOARD_DIAG_H
#define RIDER_BOARD_DIAG_H

#include <stdint.h>

/** Start the application-level LED and button diagnostics for the AC632N board. */
void rider_board_diag_init(void);

/** Stop the diagnostics timer and leave all board LEDs un-driven. */
void rider_board_diag_stop(void);

/** Claim PB5 for power-key feedback without stopping LED1/LED3 diagnostics. */
void rider_board_diag_power_led_claim(uint8_t on);

/** Release PB5 so the next diagnostic render owns the temperature LED again. */
void rider_board_diag_power_led_release(void);

#endif
