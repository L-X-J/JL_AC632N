# Project Status

Last updated: 2026-09-01

## Current Focus

Rider firmware traceability: identify the flashed image through the PA0/115200 startup version log and BLE Firmware Revision.

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
- Set the user-authoritative firmware baseline to `0.1.0`, bumped this change to `0.1.1`, and made startup UART plus BLE `0x2A26` expose the same version macro.

## Next

- Build and flash the `0.1.1` image, then verify `Firmware version: 0.1.1` over PA0/115200 and read the same value from BLE `0x2A26`.

## Blockers

- Local q32s compiler/linker tools are unavailable.
- The `0.1.1` change passes the host code-model build, but has not yet been built with q32s or flashed on hardware.

## Relevant Files

- `apps/rider_core_temp/modules/power/rider_power_key.c`
- `apps/rider_core_temp/modules/diag/rider_board_diag.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h`
