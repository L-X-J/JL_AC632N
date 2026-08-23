#!/usr/bin/env python3
"""Compile and run the host-side Rider temperature filter contract test."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FILTER_SOURCE = ROOT / "apps/rider_core_temp/modules/temp/rider_temp_filter.c"
TEST_SOURCE = ROOT / "tools/test_rider_temp_filter.c"


class RiderTemperatureFilterTests(unittest.TestCase):
    def test_filter_state_machine(self):
        """Compile and execute the C filter state-machine contract test."""
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("host C compiler is not installed")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "rider_temp_filter_test"
            compile_command = [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-DRIDER_TEMP_FILTER_HOST_TEST",
                "-DCONFIG_APP_RIDER_CORE_TEMP=1",
                "-I",
                str(ROOT / "apps/rider_core_temp/include"),
                str(FILTER_SOURCE),
                str(TEST_SOURCE),
                "-o",
                str(executable),
            ]
            subprocess.run(compile_command, check=True, cwd=ROOT)
            completed = subprocess.run([str(executable)], check=True,
                                       capture_output=True, text=True)
            self.assertIn("rider_temp_filter host tests: OK", completed.stdout)


if __name__ == "__main__":
    unittest.main()
