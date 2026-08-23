#ifndef RIDER_BOARD_DIAG_H
#define RIDER_BOARD_DIAG_H

/** Start the application-level LED and button diagnostics for the AC632N board. */
void rider_board_diag_init(void);

/** Stop the diagnostics timer and leave all board LEDs un-driven. */
void rider_board_diag_stop(void);

#endif
