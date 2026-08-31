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
- Remote Windows build host and project path recorded; SSH password remains environment-only.
- Remote `ac632n_rider_core_temp` Make build completed successfully (exit code `0`); link/package output reported `app.bin`, `jl_isd.fw` and `jl_isd.ufw` generation.
- Remote tracked Git files remain unchanged; only untracked firmware build artifacts are present.
- Attempted remote script flash after the user pressed `Update`; `isd_download.exe` reported `Device offline, only package the file`, and Windows enumerated only USB hubs plus the host NVMe, so no device write occurred.
- Retried the remote script after the board entered update mode; flash succeeded with `SPI nor flash online`, flash ID `cd7013`, size `512K`, `Write block:0` and download completion reported.
- Fixed the Rider board Makefile RCSP subdirectory include paths and added the missing `RIDER_POWER_KEY` log constants.
- Synchronized the local Rider power/board/configuration files to the remote checkout and forced `LINK_AT=0` compile/link; `sdk.elf` completed successfully, with only q32s stack-size warnings.

## Next

- After the user confirms the board is in Update mode, package and flash this newly linked `sdk.elf`, then validate the startup, short-press, long-press and port-ownership cases.

## Blockers

- Local q32s compiler/linker tools are unavailable.
- Flashing the newly linked image is paused until the user confirms `Update` mode.

## Relevant Files

- `apps/rider_core_temp/modules/power/rider_power_key.c`
- `apps/rider_core_temp/modules/diag/rider_board_diag.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h`
