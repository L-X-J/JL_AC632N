#!/usr/bin/env python3
"""Host-side regression tests for the Rider temperature calibration tool."""

import csv
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("rider_core_temp_calibrate.py")
SPEC = importlib.util.spec_from_file_location("rider_core_temp_calibrate", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules["rider_core_temp_calibrate"] = MODULE
SPEC.loader.exec_module(MODULE)


class CalibrationTests(unittest.TestCase):
    def test_contiguous_holdout_recovers_fixed_point_model(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "samples.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(("timestamp", "contact_centi", "core_centi",
                                 "reference_skin_centi", "state"))
                contacts = [3600 + index * 2 for index in range(20)]
                for index, contact in enumerate(contacts):
                    previous = contacts[index - 1] if index else contact
                    slope = (contact - previous) * 60
                    core = contact + 40 + slope * 64 / 256
                    writer.writerow((index, contact, core, contact - 10, "stable"))
            samples = MODULE.read_samples(path)
            result = MODULE.calibrate(samples, 0.2)
            self.assertTrue(result["gate"]["passed"])
            self.assertLessEqual(result["holdout"]["see_c"],
                                 result["gate"]["see_limit_c"])
            self.assertAlmostEqual(result["firmware"]["offset_centi"], 40, delta=1)
            self.assertAlmostEqual(result["firmware"]["slope_gain_q8"], 64, delta=1)
            self.assertIn("reference_skin", result)
            self.assertEqual(samples[0].reference_skin_centi, 3590)

    def test_warming_and_missing_rows_are_not_fit(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "samples.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(("timestamp", "contact_c", "reference_core_c", "state"))
                writer.writerow((0, 35.0, 36.5, "warming"))
                for index in range(8):
                    writer.writerow((index + 1, 35.5, 37.0, "stable"))
                writer.writerow((10, "", 37.0, "stable"))
            samples = MODULE.read_samples(path)
            self.assertEqual(len(samples), 8)
            self.assertAlmostEqual(samples[0].contact_centi, 3550)

    def test_quantized_metrics_are_reported(self):
        samples = [MODULE.Sample(index, 3600 + index, 3600 + index + 0.49)
                   for index in range(12)]
        result = MODULE.calibrate(samples, 0.25)
        self.assertIn("floating_holdout", result)
        self.assertIn("holdout", result)

    def test_complete_session_holdout_does_not_split_a_session(self):
        """Keep an entire exercise session outside the fitting data."""
        samples = []
        for session_index, session_id in enumerate(("rest", "exercise")):
            for index in range(8):
                contact = 3600 + session_index * 20 + index
                samples.append(MODULE.Sample(
                    session_index * 100 + index, contact, contact + 40,
                    None, session_id))

        result = MODULE.calibrate(samples, 0.2, holdout_session="exercise")

        self.assertEqual(result["split"]["strategy"], "complete_session")
        self.assertEqual(result["split"]["holdout_session"], "exercise")
        self.assertEqual(result["split"]["train_sessions"], ["rest"])
        self.assertEqual(result["split"]["holdout_sessions"], ["exercise"])
        self.assertTrue(result["gate"]["passed"])


if __name__ == "__main__":
    unittest.main()
