# Known Issues

## q32s toolchain is not installed

Symptom: `make ac632n_rider_core_temp` stops before compilation because `/opt/jieli/q32s/bin/clang` is missing.

Cause: The local environment does not contain the Jieli q32s compiler and linker tools.

Resolution: Run the CMake indexing target and static checks locally; run the full Make build on a host with `clang`, `lto-wrapper`, and `lto-ar` under the configured q32s toolchain path.

Important: A successful CMake code-model build is not evidence that the firmware linked or is flashable. PB3/PB5 behavior still requires hardware verification.
