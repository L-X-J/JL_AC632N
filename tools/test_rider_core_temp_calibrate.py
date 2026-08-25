#!/usr/bin/env python3
"""Host-side regression tests for the Rider Core V1 calibration tool."""

import csv
import importlib.util
import math
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


def synthetic_session(session_id: str, count: int = 760,
                      with_heart_rate: bool = True,
                      start_time: float = 0.0) -> list[MODULE.Sample]:
    """Generate one session from a known V1-style lagged target."""
    skins = [3500.0 + 0.035 * index + 18.0 * math.sin(index / 41.0) +
             4.0 * math.sin(index / 7.0) for index in range(count)]
    heart_rates = ([105.0 + 0.025 * index + 22.0 * math.sin(index / 29.0)
                    for index in range(count)] if with_heart_rate else
                   [None] * count)
    baseline = sum(skins[:MODULE.BASELINE_SAMPLES]) / MODULE.BASELINE_SAMPLES
    previous_core = None
    samples = []
    for index, skin in enumerate(skins):
        core = 3680.0
        if index >= MODULE.HISTORY_SECONDS:
            history_1m = ((index - 60) // MODULE.HISTORY_STEP_SECONDS *
                          MODULE.HISTORY_STEP_SECONDS)
            history_5m = ((index - MODULE.HISTORY_SECONDS) //
                          MODULE.HISTORY_STEP_SECONDS *
                          MODULE.HISTORY_STEP_SECONDS)
            skin_1m = skins[history_1m]
            skin_5m = skins[history_5m]
            heart_rate = heart_rates[index]
            heart_rate_1m = heart_rates[history_1m]
            target = (3680.0 + 0.10 * (skin - MODULE.SKIN_REFERENCE_CENTI) +
                      0.20 * (skin - baseline) +
                      0.10 * (skin - skin_1m) +
                      0.15 * (skin - skin_5m))
            if heart_rate is not None:
                target += 0.40 * (heart_rate - MODULE.HR_REFERENCE_BPM)
                if heart_rate_1m is not None:
                    target += 0.05 * (heart_rate - heart_rate_1m)
            previous_core = (target if previous_core is None else
                             previous_core + 32.0 / 256.0 *
                             (target - previous_core))
            core = previous_core
        samples.append(MODULE.Sample(
            start_time + index, skin, core, skin - 5.0, session_id,
            heart_rates[index]))
    return samples


class CalibrationTests(unittest.TestCase):
    def test_contiguous_holdout_fits_v1_and_writes_versioned_header(self):
        """Fit a known V1 trace and emit the versioned firmware contract."""
        samples = synthetic_session("ride")
        result = MODULE.calibrate(samples, 0.2)

        self.assertTrue(result["gate"]["passed"])
        self.assertEqual(result["model_version"], 1)
        self.assertEqual(result["model"],
                         "rider_core_v1_trusted_skin_history_q8")
        self.assertEqual(result["holdout"]["invalid_predictions"], 0)
        self.assertLessEqual(result["holdout"]["rmse_c"], 0.5)
        self.assertEqual(result["firmware"]["model_version"], 1)
        self.assertIn("reference_skin", result)

        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "calibration.h"
            MODULE.write_header(header, result)
            text = header.read_text(encoding="ascii")
            self.assertIn("RIDER_CORE_TEMP_CAL_MODEL_VERSION 1", text)
            self.assertIn("RIDER_CORE_TEMP_CAL_BASE_CENTI", text)
            self.assertIn("RIDER_CORE_TEMP_CAL_TREND_5M_GAIN_Q8", text)
            self.assertIn("RIDER_CORE_TEMP_CAL_HR_GAIN_Q8", text)
            self.assertNotIn("CAL_OFFSET_CENTI", text)

    def test_read_samples_accepts_trusted_skin_and_optional_hr(self):
        """Accept current column names while preserving missing HR semantics."""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "samples.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(("timestamp", "trusted_skin_c",
                                 "reference_core_c", "skin_state",
                                 "heart_rate", "heart_rate_valid"))
                writer.writerow((0, 35.0, 36.5, "contact_settling", 100, 1))
                for index in range(6):
                    writer.writerow((index + 1, 35.5, 37.0,
                                     "skin_trusted", 120, 1))
                writer.writerow((8, 35.5, 37.0, "skin_trusted", 120, 0))
            samples = MODULE.read_samples(path)
            self.assertEqual(len(samples), 7)
            self.assertAlmostEqual(samples[0].skin_centi, 3550)
            self.assertEqual(samples[0].heart_rate, 120)
            self.assertIsNone(samples[-1].heart_rate)

    def test_features_wait_for_full_five_minute_history(self):
        """Wait five minutes and use the same five-second history as firmware."""
        samples = synthetic_session("ride", count=320, with_heart_rate=False)
        points = MODULE.feature_points(samples)

        self.assertEqual(points[0].timestamp_s, 300)
        self.assertEqual(len(points), 20)
        self.assertIsNone(points[0].heart_rate)
        self.assertEqual(points[0].heart_rate_delta_1m, 0)
        self.assertAlmostEqual(
            points[1].skin_delta_1m_centi,
            samples[301].skin_centi - samples[240].skin_centi,
        )
        self.assertAlmostEqual(
            points[1].skin_delta_5m_centi,
            samples[301].skin_centi - samples[0].skin_centi,
        )

    def test_complete_session_holdout_resets_model_state(self):
        """Keep a complete ride outside fitting and reset recurrence state."""
        samples = (synthetic_session("rest", with_heart_rate=False) +
                   synthetic_session("exercise", with_heart_rate=True,
                                     start_time=1000))
        result = MODULE.calibrate(samples, 0.2, holdout_session="exercise")

        self.assertEqual(result["split"]["strategy"], "complete_session")
        self.assertEqual(result["split"]["holdout_session"], "exercise")
        self.assertEqual(result["split"]["train_sessions"], ["rest"])
        self.assertEqual(result["split"]["holdout_sessions"], ["exercise"])
        self.assertEqual(result["heart_rate"]["train_rows"], 0)
        self.assertGreater(result["heart_rate"]["holdout_rows"], 0)
        self.assertTrue(result["gate"]["passed"])


if __name__ == "__main__":
    unittest.main()
