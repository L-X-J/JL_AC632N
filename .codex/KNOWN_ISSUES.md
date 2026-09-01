# Known Issues

## q32s toolchain is not installed

Symptom: `make ac632n_rider_core_temp` stops before compilation because `/opt/jieli/q32s/bin/clang` is missing.

Cause: The local environment does not contain the Jieli q32s compiler and linker tools.

Resolution: Run the CMake indexing target and static checks locally; run the full Make build on a host with `clang`, `lto-wrapper`, and `lto-ar` under the configured q32s toolchain path.

Important: A successful CMake code-model build is not evidence that the firmware linked or is flashable. PB3/PB5 behavior still requires hardware verification.

## Remote Windows build host

Symptom: The local macOS checkout cannot run the q32s firmware build because the Jieli toolchain is absent.

Resolution: Use the configured Windows host `xinlei@192.168.110.192`, project directory `C:\Users\pc\Documents\JL_AC632N`, and load the SSH password from `CXL_SUFACE_GO_PWD` without persisting its value.

Important: Check the remote working tree before building. Preserve any remote Git changes and report them before taking actions that would overwrite or clean files.

## Windows Git safe-directory ownership

Symptom: Remote `git status` fails with `detected dubious ownership` because the SSH user differs from the Windows project directory owner.

Resolution: Use the command-local exception `git -c safe.directory=C:/Users/pc/Documents/JL_AC632N status --short`; do not change global Git configuration just for the check.

Important: The Rider build leaves firmware artifacts untracked in the remote checkout. Do not run `clean`, delete them, or revert remote changes as part of a routine build.

## Remote checkout can lag the local Rider commit

Symptom: The remote build may report success while missing newer Rider source files, because the remote checkout is on an older commit and stale objects can satisfy the link.

Resolution: Compare local and remote commit IDs before building. Synchronize only the intended Rider source/configuration files or explicitly update the remote checkout, then force the affected compile when validating.

Important: Do not treat a successful post-build packaging step as proof that the current local source was compiled.

## Rider RCSP include and log configuration

Symptom: A forced Rider build fails on `rcsp_user_update.h` or `log_tag_const_i_RIDER_POWER_KEY`.

Cause: RCSP headers live in `JL_rcsp/rcsp_updata` and `JL_rcsp/bt_trans_data`, while the power module uses a dedicated log tag that must be defined in the Rider log configuration.

Resolution: Keep both RCSP subdirectories in the Rider board Makefile include list and keep the `RIDER_POWER_KEY` constants in `apps/rider_core_temp/config/log_config.c`.

## UART terminal shows continuous garbled bytes after switching to 115200

Symptom: The terminal is configured as `115200 8N1 ASCII`, but the receive pane contains continuous random characters instead of `[Info]`/`[RIDER_*]` lines.

Cause: The board is still running an image built with the former application UART0 setting (`PA0 / 1000000`) or the former boot/OTA default (`PA5 / 1000000`). The terminal's ASCII/HEX mode and line-ending selector do not change the physical receive timing.

Resolution: Keep the Rider board overrides at `CONFIG_UBOOT_DEBUG_PIN=PA00` and `CONFIG_UBOOT_DEBUG_BAUD_RATE=115200`, build and flash the current `ac632n_rider_core_temp` image, connect `USB-UART RX` only to `PA0`, share ground, and keep the terminal at `115200 / 8N1 / no flow control`.

Important: A source-only change is not present on the board until the new image is flashed. If a freshly flashed image still produces random bytes, verify the selected image and PA0 wiring before investigating application log encoding; Rider source string literals and binary dumps are checked by `tools/test_rider_core_temp_serial.py`.

## PB3 wakeup latch can be stale after reset

Symptom: The firmware prints no Rider application logs and appears to power off immediately after reset.

Cause: `get_wakeup_source()` may retain the PB3 wakeup bit after the key has already been released. Treating that bit alone as an active power-on gesture sends `app_main()` down the soft-poweroff path before `start_app()`.

Resolution: The startup gate now requires both the wakeup bit and a live low PB3 level before enforcing the two-second hold. A released PB3 with a stale wakeup bit follows the normal startup prompt. The `0.1.5` isolation image sets `RIDER_UART_HEARTBEAT_ONLY=1`, so it intentionally starts neither BLE nor the PB3 state machine and must only be evaluated by its two-second PA0 heartbeat. Set this macro to `0` before resuming BLE discovery tests; keep `RIDER_POWER_KEY_ENABLE=0` until the application path is stable.
