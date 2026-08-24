#!/usr/bin/env python3
"""Compile and run the host-side core estimator directionality test."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FILTER_SOURCE = ROOT / "apps/rider_core_temp/modules/temp/rider_temp_filter.c"
ESTIMATOR_SOURCE = ROOT / "apps/rider_core_temp/modules/temp/rider_core_estimator.c"
TEST_SOURCE = ROOT / "tools/test_rider_core_estimator.c"


class RiderCoreEstimatorTests(unittest.TestCase):
    def test_core_estimate_follows_skin_downward(self):
        """Compile the production estimator and verify its signed response."""
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("host C compiler is not installed")

        with tempfile.TemporaryDirectory() as directory:
            temp_root = Path(directory)
            (temp_root / "system").mkdir()
            (temp_root / "system/includes.h").write_text(
                "#include <stdint.h>\n"
                "#include <string.h>\n"
                "typedef uint8_t u8;\n"
                "typedef uint16_t u16;\n"
                "typedef uint32_t u32;\n",
                encoding="ascii",
            )
            (temp_root / "app_config.h").write_text(
                "#define CONFIG_APP_RIDER_CORE_TEMP 1\n",
                encoding="ascii",
            )
            (temp_root / "debug.h").write_text(
                "#define log_info(...) ((void)0)\n",
                encoding="ascii",
            )
            executable = temp_root / "rider_core_estimator_test"
            compile_command = [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-DRIDER_TEMP_FILTER_HOST_TEST",
                "-DCONFIG_APP_RIDER_CORE_TEMP=1",
                "-DRIDER_CORE_TEMP_PUBLISH_MODE=3",
                "-I",
                str(temp_root),
                "-I",
                str(ROOT / "apps/rider_core_temp/include"),
                str(FILTER_SOURCE),
                str(ESTIMATOR_SOURCE),
                str(TEST_SOURCE),
                "-o",
                str(executable),
            ]
            subprocess.run(compile_command, check=True, cwd=ROOT)
            completed = subprocess.run([str(executable)], check=True,
                                       capture_output=True, text=True)
            self.assertIn("rider_core_estimator host tests: OK", completed.stdout)


if __name__ == "__main__":
    unittest.main()
