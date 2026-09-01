# Project Status

Last updated: 2026-09-01

## Current Focus

Rider `0.1.7` normal BLE startup restored after PA0 UART hardware verification.

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
- Enabled the Rider bd19 RCSP BLE OTA channel with `CONFIG_APP_OTA_ENABLE=1`; the profile remains single-bank and the debug mini-program still does not transmit firmware.
- Rebuilt and linked the Rider target on the Windows q32s host; regenerated `sdk.elf`, `app.bin`, `jl_isd.fw` and `update.ufw`, and verified the generated `isd_config.ini` contains `UTTX=PA00` and `UTBD=115200`.
- Set the user-authoritative firmware baseline to `0.1.0`, bumped this change to `0.1.1`, and made startup UART plus BLE `0x2A26` expose the same version macro.
- Fast-forwarded the Windows build checkout to `fd8d380`, completed the q32s link/package, and verified the generated `app.bin` contains both the `RIDER_APP` firmware-version format and `0.1.1`; `isd_config.ini` remains `PA00 / 115200`.
- The post-build downloader reported `Device offline, only package the file`, so this run generated `sdk.elf`, `app.bin` and `jl_isd.fw` but did not flash the board.

## Next

- Build and flash the `0.1.7` image, then verify the Firmware Revision log, M601 initialization and BLE discovery over PA0/115200. PB3 power-key state remains disabled independently.

## Blockers

- Local q32s compiler/linker tools are unavailable.
- The `0.1.7` normal-start image has not yet been built or flashed after the latest source change. `RIDER_POWER_KEY_ENABLE=0` remains set independently.

## Relevant Files

- `apps/rider_core_temp/modules/power/rider_power_key.c`
- `apps/rider_core_temp/modules/diag/rider_board_diag.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider.c`
- `apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h`
