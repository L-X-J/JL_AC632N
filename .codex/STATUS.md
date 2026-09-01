# Project Status

Last updated: 2026-09-01

## Current Focus

Rider UART serial diagnostics: unify boot/OTA and application output on PA0 at 115200, then verify the flashed image.

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
- Changed the Rider UART0 debug contract from `1000000` to the standard `115200` baud (8N1); documented the screenshot garbled-byte failure mode and the required rebuild/flash step.
- Found the remaining boot/OTA post-build default at `PA5 / 1000000` and overrode it for Rider so every diagnostic stage uses `PA0 / 115200`.
- Rebuilt and linked the Rider target on the Windows q32s host; regenerated `sdk.elf`, `app.bin`, `jl_isd.fw` and `update.ufw`, and verified the generated `isd_config.ini` contains `UTTX=PA00` and `UTBD=115200`.

## Next

- After the user confirms the board is in Update mode, flash the already-built image containing the unified PA0/115200 settings, then verify readable boot and ASCII application logs.

## Blockers

- Local q32s compiler/linker tools are unavailable.
- Flashing the newly linked image is paused until the user confirms `Update` mode.

## Relevant Files

- `apps/rider_core_temp/modules/power/rider_power_key.c`
- `apps/rider_core_temp/modules/diag/rider_board_diag.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h`
