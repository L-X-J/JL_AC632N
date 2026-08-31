# Project Status

Last updated: 2026-08-31

## Current Focus

Rider PB3 power-key and PB5 power-indicator integration.

## In Progress

None.

## Recently Completed

- Added PB3 GPIO/wakeup adapter and the `modules/power` state machine.
- Added PB5 diagnostic arbitration and shutdown preparation through the existing BLE lifecycle.
- Preserved PB6/PB4, PB0/PB1, PB7 and PA0 mappings and existing BLE debug contracts.
- Updated architecture and Rider documentation.
- CMake code-model build, host regression tests and static port assertions passed.

## Next

- Verify the complete Make build on a host with the q32s toolchain.
- Validate the startup, short-press, long-press, flash and port-ownership cases on AC632N hardware.

## Blockers

Local q32s compiler/linker tools are unavailable.

## Relevant Files

- `apps/rider_core_temp/modules/power/rider_power_key.c`
- `apps/rider_core_temp/modules/diag/rider_board_diag.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h`
