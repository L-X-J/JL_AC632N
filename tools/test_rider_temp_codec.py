#!/usr/bin/env python3
"""Compile and run the production BLE temperature byte codec on the host."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CODEC_SOURCE = ROOT / "apps/rider_core_temp/modules/bt/rider_temp_codec.c"
TEST_SOURCE = ROOT / "tools/test_rider_temp_codec.c"


class RiderTemperatureCodecTests(unittest.TestCase):
    def test_centi_degree_wire_encoding(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("host C compiler is not installed")

        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "rider_temp_codec_test"
            compile_command = [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "apps/rider_core_temp/include"),
                str(CODEC_SOURCE),
                str(TEST_SOURCE),
                "-o",
                str(executable),
            ]
            subprocess.run(compile_command, check=True, cwd=ROOT)
            completed = subprocess.run([str(executable)], check=True,
                                       capture_output=True, text=True)
            self.assertIn("rider_temp_codec host tests: OK", completed.stdout)


if __name__ == "__main__":
    unittest.main()
