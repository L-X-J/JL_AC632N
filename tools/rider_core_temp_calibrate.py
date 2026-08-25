#!/usr/bin/env python3
"""Fit and validate Rider Core V1 against synchronized reference data.

The firmware consumes trusted single-site chest skin temperature at 1 Hz and
uses a five-minute feature history. External heart rate is optional: rows
without fresh HR train and validate the skin-only path instead of inventing a
zero BPM observation. A complete session or a contiguous suffix is held out,
and the quantized Q8 recurrence is evaluated before a calibration header can
be marked valid.
"""

from __future__ import annotations

import argparse
import bisect
import csv
import json
import math
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Sequence


MODEL_VERSION = 1
HISTORY_SECONDS = 300
HISTORY_STEP_SECONDS = 5
BASELINE_SAMPLES = 30
SKIN_REFERENCE_CENTI = 3500
HR_REFERENCE_BPM = 80
CORE_MIN_CENTI = 3500
CORE_MAX_CENTI = 4200
CORE_MAX_STEP_CENTI = 25
FEATURE_NAMES = (
    "intercept",
    "skin_from_reference",
    "skin_from_wear_baseline",
    "skin_delta_1m",
    "skin_delta_5m",
    "heart_rate_from_reference",
    "heart_rate_delta_1m",
)


@dataclass(frozen=True)
class Sample:
    """One synchronized trusted-skin/reference-core observation."""

    timestamp_s: float
    skin_centi: float
    core_centi: float
    reference_skin_centi: float | None = None
    session_id: str = ""
    heart_rate: float | None = None

    @property
    def contact_centi(self) -> float:
        """Retain the old attribute name for analysis notebooks."""
        return self.skin_centi


@dataclass(frozen=True)
class FeaturePoint:
    """One model-ready row with the exact V1 feature semantics."""

    timestamp_s: float
    session_id: str
    core_centi: float
    skin_centi: float
    skin_baseline_centi: float
    skin_delta_1m_centi: float
    skin_delta_5m_centi: float
    heart_rate: float | None
    heart_rate_delta_1m: float

    def vector(self) -> tuple[float, ...]:
        """Return the affine target vector in firmware coefficient order."""
        return (
            1.0,
            self.skin_centi - SKIN_REFERENCE_CENTI,
            self.skin_centi - self.skin_baseline_centi,
            self.skin_delta_1m_centi,
            self.skin_delta_5m_centi,
            0.0 if self.heart_rate is None else self.heart_rate - HR_REFERENCE_BPM,
            self.heart_rate_delta_1m,
        )


def _number(row: dict[str, str], names: Sequence[str]) -> float | None:
    """Read the first non-empty numeric alias from one CSV row."""
    for name in names:
        value = (row.get(name) or "").strip()
        if value:
            return float(value)
    return None


def _boolean(row: dict[str, str], names: Sequence[str]) -> bool | None:
    """Read an optional CSV boolean without treating an empty cell as false."""
    for name in names:
        value = (row.get(name) or "").strip().lower()
        if not value:
            continue
        if value in {"1", "true", "yes", "valid", "fresh"}:
            return True
        if value in {"0", "false", "no", "invalid", "stale"}:
            return False
        raise ValueError(f"invalid boolean value for {name}: {value}")
    return None


def _timestamp(value: str) -> float:
    """Parse seconds since epoch or an ISO-8601 timestamp."""
    value = value.strip()
    try:
        return float(value)
    except ValueError:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


def _trusted_state(value: str) -> bool:
    """Accept current trusted-skin labels and the legacy stable aliases."""
    return value.strip().lower() in {
        "skin_trusted", "core_ready", "trusted", "stable", "3", "ok", "valid",
    }


def _group_samples(samples: Sequence[Sample]) -> list[list[Sample]]:
    """Group sessions in first-seen order and sort each session by time."""
    groups: dict[str, list[Sample]] = {}
    for sample in samples:
        groups.setdefault(sample.session_id, []).append(sample)
    return [sorted(group, key=lambda item: item.timestamp_s)
            for group in groups.values()]


def read_samples(path: Path) -> list[Sample]:
    """Read trusted skin, reference core, optional HR, and session metadata."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = csv.DictReader(handle)
        if not rows.fieldnames:
            raise ValueError("CSV has no header")
        samples: list[Sample] = []
        for line_number, row in enumerate(rows, start=2):
            state = row.get("skin_state") or row.get("state") or ""
            if not _trusted_state(state):
                continue
            raw_time = row.get("timestamp", "") or row.get("time", "")
            raw_skin = _number(row, (
                "trusted_skin_centi", "skin_centi", "contact_centi",
                "m601_centi", "temperature_centi",
            ))
            if raw_skin is None:
                raw_skin_c = _number(row, (
                    "trusted_skin_c", "skin_c", "contact_c", "m601_c",
                    "temperature_c",
                ))
                if raw_skin_c is not None:
                    raw_skin = raw_skin_c * 100.0
                else:
                    raw_m601 = _number(row, ("m601_raw", "raw_m601", "raw"))
                    raw_skin = (None if raw_m601 is None else
                                raw_m601 * 100.0 / 256.0 + 4000.0)
            raw_core = _number(row, (
                "reference_core_centi", "core_reference_centi", "core_centi",
            ))
            if raw_core is None:
                raw_core_c = _number(row, (
                    "reference_core_c", "core_reference_c", "core_c",
                ))
                raw_core = None if raw_core_c is None else raw_core_c * 100.0
            reference_skin = _number(row, (
                "reference_skin_centi", "skin_reference_centi",
            ))
            if reference_skin is None:
                reference_skin_c = _number(row, (
                    "reference_skin_c", "skin_reference_c",
                ))
                reference_skin = (None if reference_skin_c is None else
                                  reference_skin_c * 100.0)
            heart_rate = _number(row, ("heart_rate", "hr", "heart_rate_bpm"))
            try:
                heart_rate_valid = _boolean(
                    row, ("heart_rate_valid", "hr_valid", "heart_rate_fresh"))
            except ValueError as exc:
                raise ValueError(f"invalid row {line_number}: {exc}") from exc
            if heart_rate_valid is False or heart_rate is not None and heart_rate <= 0:
                heart_rate = None
            if not raw_time or raw_skin is None or raw_core is None:
                continue
            try:
                samples.append(Sample(
                    _timestamp(raw_time), raw_skin, raw_core, reference_skin,
                    (row.get("session_id") or row.get("session") or "").strip(),
                    heart_rate,
                ))
            except ValueError as exc:
                raise ValueError(f"invalid row {line_number}: {exc}") from exc
    if len(samples) < 6:
        raise ValueError("at least six trusted-skin/reference-core rows are required")
    return [sample for group in _group_samples(samples) for sample in group]


def _sample_at_or_before(group: Sequence[Sample], timestamps: Sequence[float],
                         target_s: float) -> Sample | None:
    """Return the newest same-session observation at or before a target."""
    index = bisect.bisect_right(timestamps, target_s) - 1
    return None if index < 0 else group[index]


def feature_points(samples: Sequence[Sample]) -> list[FeaturePoint]:
    """Build baseline and firmware-cadence history features per session."""
    points: list[FeaturePoint] = []
    for group in _group_samples(samples):
        if len(group) < BASELINE_SAMPLES:
            continue
        baseline = (sum(sample.skin_centi
                        for sample in group[:BASELINE_SAMPLES]) /
                    BASELINE_SAMPLES)
        history: list[Sample] = []
        history_timestamps: list[float] = []
        last_history_time: float | None = None
        start_s = group[0].timestamp_s
        for sample in group:
            if (last_history_time is None or
                    sample.timestamp_s - last_history_time >=
                    HISTORY_STEP_SECONDS):
                history.append(sample)
                history_timestamps.append(sample.timestamp_s)
                last_history_time = sample.timestamp_s
            if sample.timestamp_s - start_s < HISTORY_SECONDS:
                continue
            one_minute = _sample_at_or_before(
                history, history_timestamps, sample.timestamp_s - 60.0)
            five_minutes = _sample_at_or_before(
                history, history_timestamps,
                sample.timestamp_s - HISTORY_SECONDS)
            if one_minute is None or five_minutes is None:
                continue
            hr_delta = 0.0
            if sample.heart_rate is not None and one_minute.heart_rate is not None:
                hr_delta = sample.heart_rate - one_minute.heart_rate
            points.append(FeaturePoint(
                sample.timestamp_s,
                sample.session_id,
                sample.core_centi,
                sample.skin_centi,
                baseline,
                sample.skin_centi - one_minute.skin_centi,
                sample.skin_centi - five_minutes.skin_centi,
                sample.heart_rate,
                hr_delta,
            ))
    if len(points) < 12:
        raise ValueError(
            "at least twelve model-ready rows after five-minute history are required")
    return points


def _solve_linear_system(matrix: list[list[float]],
                         vector: list[float]) -> list[float]:
    """Solve a small dense system with partial-pivot Gaussian elimination."""
    size = len(vector)
    augmented = [matrix[row][:] + [vector[row]] for row in range(size)]
    for column in range(size):
        pivot = max(range(column, size),
                    key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1e-12:
            augmented[pivot][column] = 1e-12
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        divisor = augmented[column][column]
        augmented[column] = [value / divisor for value in augmented[column]]
        for row in range(size):
            if row == column:
                continue
            factor = augmented[row][column]
            if factor:
                augmented[row] = [
                    augmented[row][item] - factor * augmented[column][item]
                    for item in range(size + 1)
                ]
    return [augmented[row][-1] for row in range(size)]


def _least_squares(rows: Sequence[Sequence[float]],
                   values: Sequence[float]) -> list[float]:
    """Fit a scaled, lightly regularized normal equation without NumPy."""
    columns = len(rows[0])
    scales = []
    for column in range(columns):
        rms = math.sqrt(sum(row[column] ** 2 for row in rows) / len(rows))
        scales.append(rms if rms > 1e-9 else 1.0)
    scaled = [[row[column] / scales[column] for column in range(columns)]
              for row in rows]
    matrix = [[sum(row[left] * row[right] for row in scaled)
               for right in range(columns)] for left in range(columns)]
    vector = [sum(row[column] * value for row, value in zip(scaled, values))
              for column in range(columns)]
    ridge = max(1.0, len(rows)) * 1e-8
    for column in range(columns):
        matrix[column][column] += ridge
    scaled_coefficients = _solve_linear_system(matrix, vector)
    return [scaled_coefficients[index] / scales[index]
            for index in range(columns)]


def _lagged_rows(points: Sequence[FeaturePoint], alpha_q8: int,
                 initial_basis: Sequence[float] | None = None,
                 initial_session: str | None = None,
                 ) -> tuple[list[list[float]], list[float], str | None]:
    """Apply the first-order recurrence to feature bases for one alpha."""
    alpha = max(1, min(255, int(alpha_q8))) / 256.0
    previous = None if initial_basis is None else list(initial_basis)
    current_session = initial_session
    rows: list[list[float]] = []
    for point in points:
        features = list(point.vector())
        if previous is None or point.session_id != current_session:
            previous = features
        else:
            previous = [(1.0 - alpha) * old + alpha * new
                        for old, new in zip(previous, features)]
        rows.append(previous[:])
        current_session = point.session_id
    return rows, ([] if previous is None else previous), current_session


def fit_lagged(points: Sequence[FeaturePoint]) -> tuple[list[float], int]:
    """Grid-search the firmware lag and fit all V1 target coefficients."""
    references = [point.core_centi for point in points]
    best: tuple[list[float], int, float] | None = None
    for alpha_q8 in range(1, 256):
        rows, _, _ = _lagged_rows(points, alpha_q8)
        coefficients = _least_squares(rows, references)
        predictions = [sum(value * coefficient for value, coefficient
                           in zip(row, coefficients)) for row in rows]
        error = sum((prediction - reference) ** 2
                    for prediction, reference in zip(predictions, references))
        if best is None or error < best[2]:
            best = (coefficients, alpha_q8, error)
    assert best is not None
    return best[0], best[1]


def predict_floating(points: Sequence[FeaturePoint], coefficients: Sequence[float],
                     alpha_q8: int, initial_basis: Sequence[float] | None = None,
                     initial_session: str | None = None,
                     ) -> tuple[list[float], list[float], str | None]:
    """Predict the unconstrained floating recurrence for comparison."""
    rows, last_basis, last_session = _lagged_rows(
        points, alpha_q8, initial_basis, initial_session)
    return ([sum(value * coefficient for value, coefficient
                 in zip(row, coefficients)) for row in rows],
            last_basis, last_session)


def _q8(value: float) -> int:
    """Quantize a signed floating gain into an int16 Q8 coefficient."""
    return max(-32768, min(32767, int(round(value * 256.0))))


def _apply_gain(value: int, gain_q8: int) -> int:
    """Mirror the firmware's signed round-to-nearest Q8 multiplication."""
    product = value * gain_q8
    if product >= 0:
        return (product + 128) // 256
    return -(((-product) + 128) // 256)


def _truncate_division(numerator: int, denominator: int) -> int:
    """Match C99 signed integer division, which truncates toward zero."""
    if numerator >= 0:
        return numerator // denominator
    return -((-numerator) // denominator)


def quantize(coefficients: Sequence[float], alpha_q8: int) -> dict[str, int]:
    """Convert fitted coefficients into the compile-time firmware layout."""
    return {
        "model_version": MODEL_VERSION,
        "base_core_centi": int(round(coefficients[0])),
        "skin_gain_q8": _q8(coefficients[1]),
        "skin_delta_gain_q8": _q8(coefficients[2]),
        "trend_1m_gain_q8": _q8(coefficients[3]),
        "trend_5m_gain_q8": _q8(coefficients[4]),
        "heart_rate_gain_q8": _q8(coefficients[5]),
        "heart_rate_trend_gain_q8": _q8(coefficients[6]),
        "lag_alpha_q8": max(1, min(255, int(alpha_q8))),
    }


def _firmware_target(point: FeaturePoint, firmware: dict[str, int]) -> int:
    """Calculate the same integer V1 target used by the MCU."""
    skin = int(round(point.skin_centi))
    baseline = int(round(point.skin_baseline_centi))
    target = firmware["base_core_centi"]
    target += _apply_gain(skin - SKIN_REFERENCE_CENTI,
                          firmware["skin_gain_q8"])
    target += _apply_gain(skin - baseline,
                          firmware["skin_delta_gain_q8"])
    target += _apply_gain(int(round(point.skin_delta_1m_centi)),
                          firmware["trend_1m_gain_q8"])
    target += _apply_gain(int(round(point.skin_delta_5m_centi)),
                          firmware["trend_5m_gain_q8"])
    if point.heart_rate is not None:
        target += _apply_gain(int(round(point.heart_rate)) - HR_REFERENCE_BPM,
                              firmware["heart_rate_gain_q8"])
        target += _apply_gain(int(round(point.heart_rate_delta_1m)),
                              firmware["heart_rate_trend_gain_q8"])
    return target


def predict_firmware(points: Sequence[FeaturePoint], firmware: dict[str, int],
                     initial_q8: int | None = None,
                     initial_session: str | None = None,
                     ) -> tuple[list[float | None], int | None, str | None]:
    """Run the exact Q8 lag, rate limit, and invalid-not-clamped range gate."""
    predictions: list[float | None] = []
    previous_q8 = initial_q8
    current_session = initial_session
    alpha_q8 = firmware["lag_alpha_q8"]
    for point in points:
        if point.session_id != current_session:
            previous_q8 = None
        current_session = point.session_id
        target = _firmware_target(point, firmware)
        if target < CORE_MIN_CENTI or target > CORE_MAX_CENTI:
            predictions.append(None)
            continue
        if previous_q8 is None:
            previous_q8 = target * 256
        else:
            delta_q8 = target * 256 - previous_q8
            step_q8 = _truncate_division(delta_q8 * alpha_q8, 256)
            if not step_q8 and delta_q8:
                step_q8 = 1 if delta_q8 > 0 else -1
            maximum = CORE_MAX_STEP_CENTI * 256
            step_q8 = max(-maximum, min(maximum, step_q8))
            previous_q8 += step_q8
        predictions.append(float((previous_q8 + 128) // 256))
    return predictions, previous_q8, current_session


def metrics(points: Sequence[FeaturePoint],
            predictions: Sequence[float | None]) -> dict[str, float | int | None]:
    """Report error, bias, agreement limits, coverage, and invalid outputs."""
    residuals = [prediction - point.core_centi
                 for point, prediction in zip(points, predictions)
                 if prediction is not None]
    invalid = len(points) - len(residuals)
    if not residuals:
        return {
            "samples": len(points), "valid_predictions": 0,
            "invalid_predictions": invalid, "rmse_c": None, "mae_c": None,
            "see_c": None, "bias_c": None, "loa_lower_c": None,
            "loa_upper_c": None, "within_0_3_fraction": 0.0,
            "within_0_5_fraction": 0.0,
        }
    count = len(residuals)
    bias = sum(residuals) / count
    squared = sum(value * value for value in residuals)
    standard_deviation = math.sqrt(
        sum((value - bias) ** 2 for value in residuals) / max(1, count - 1))
    return {
        "samples": len(points),
        "valid_predictions": count,
        "invalid_predictions": invalid,
        "rmse_c": math.sqrt(squared / count) / 100.0,
        "mae_c": sum(abs(value) for value in residuals) / count / 100.0,
        "see_c": math.sqrt(squared / max(1, count - len(FEATURE_NAMES))) / 100.0,
        "bias_c": bias / 100.0,
        "loa_lower_c": (bias - 1.96 * standard_deviation) / 100.0,
        "loa_upper_c": (bias + 1.96 * standard_deviation) / 100.0,
        "within_0_3_fraction": sum(abs(value) <= 30 for value in residuals) / count,
        "within_0_5_fraction": sum(abs(value) <= 50 for value in residuals) / count,
    }


def reference_skin_metrics(samples: Sequence[Sample]) -> dict[str, float] | None:
    """Summarize trusted-skin agreement with an optional reference sensor."""
    paired = [sample for sample in samples
              if sample.reference_skin_centi is not None]
    if not paired:
        return None
    residuals = [sample.skin_centi - sample.reference_skin_centi
                 for sample in paired]
    squared = sum(value * value for value in residuals)
    return {
        "samples": float(len(residuals)),
        "rmse_c": math.sqrt(squared / len(residuals)) / 100.0,
        "bias_c": sum(residuals) / len(residuals) / 100.0,
    }


def _split_points(points: Sequence[FeaturePoint], holdout_fraction: float,
                  holdout_session: str | None,
                  ) -> tuple[list[FeaturePoint], list[FeaturePoint], str, str | None]:
    """Create a contiguous suffix or leave one complete ride session out."""
    if holdout_session is not None:
        holdout = [point for point in points if point.session_id == holdout_session]
        train = [point for point in points if point.session_id != holdout_session]
        if not holdout:
            raise ValueError(f"holdout session not found: {holdout_session}")
        if len(train) < 8 or len(holdout) < 3:
            raise ValueError("session holdout needs at least eight train and three holdout rows")
        return train, holdout, "complete_session", holdout_session
    split = max(8, min(len(points) - 3,
                       int(len(points) * (1.0 - holdout_fraction))))
    return list(points[:split]), list(points[split:]), "contiguous_suffix", None


def _gate_passed(metric: dict[str, float | int | None]) -> bool:
    """Apply the provisional held-out experimental publication gate."""
    rmse = metric["rmse_c"]
    see = metric["see_c"]
    bias = metric["bias_c"]
    return (metric["invalid_predictions"] == 0 and rmse is not None and
            see is not None and bias is not None and rmse <= 0.5 and
            see <= 0.5 and abs(bias) <= 0.5)


def calibrate(samples: Sequence[Sample], holdout_fraction: float,
              holdout_session: str | None = None) -> dict[str, object]:
    """Fit Core V1, quantize it, and evaluate the untouched time segment."""
    points = feature_points(samples)
    train, holdout, split_strategy, selected_session = _split_points(
        points, holdout_fraction, holdout_session)
    coefficients, alpha_q8 = fit_lagged(train)
    floating_train, train_basis, train_session = predict_floating(
        train, coefficients, alpha_q8)
    carry_float = split_strategy == "contiguous_suffix"
    floating_holdout, _, _ = predict_floating(
        holdout, coefficients, alpha_q8,
        train_basis if carry_float else None,
        train_session if carry_float else None)
    firmware = quantize(coefficients, alpha_q8)
    firmware_train, train_q8, firmware_train_session = predict_firmware(
        train, firmware)
    carry_firmware = split_strategy == "contiguous_suffix"
    firmware_holdout, _, _ = predict_firmware(
        holdout, firmware,
        train_q8 if carry_firmware else None,
        firmware_train_session if carry_firmware else None)
    train_metrics = metrics(train, firmware_train)
    holdout_metrics = metrics(holdout, firmware_holdout)
    gate_passed = _gate_passed(holdout_metrics)
    result: dict[str, object] = {
        "model": "rider_core_v1_trusted_skin_history_q8",
        "model_version": MODEL_VERSION,
        "input_samples": len(samples),
        "model_ready_samples": len(points),
        "train_samples": len(train),
        "holdout_samples": len(holdout),
        "holdout_fraction": holdout_fraction,
        "split": {
            "strategy": split_strategy,
            "holdout_session": selected_session,
            "train_sessions": sorted({point.session_id for point in train
                                      if point.session_id}),
            "holdout_sessions": sorted({point.session_id for point in holdout
                                        if point.session_id}),
        },
        "features": list(FEATURE_NAMES),
        "heart_rate": {
            "train_rows": sum(point.heart_rate is not None for point in train),
            "holdout_rows": sum(point.heart_rate is not None for point in holdout),
        },
        "floating_point": {
            name: coefficients[index] for index, name in enumerate(FEATURE_NAMES)
        } | {"lag_alpha_q8": alpha_q8},
        "firmware": firmware | {"available": True, "valid": gate_passed},
        "train": train_metrics,
        "holdout": holdout_metrics,
        "floating_train": metrics(train, floating_train),
        "floating_holdout": metrics(holdout, floating_holdout),
        "gate": {
            "rmse_limit_c": 0.5,
            "see_limit_c": 0.5,
            "absolute_bias_limit_c": 0.5,
            "requires_zero_invalid_predictions": True,
            "passed": gate_passed,
        },
    }
    skin_metrics = reference_skin_metrics(samples)
    if skin_metrics is not None:
        result["reference_skin"] = skin_metrics
    return result


def write_header(path: Path, result: dict[str, object]) -> None:
    """Write a versioned compile-time calibration fragment for review."""
    firmware = result["firmware"]
    gate = result["gate"]
    assert isinstance(firmware, dict) and isinstance(gate, dict)
    with path.open("w", encoding="ascii") as handle:
        handle.write("/* Generated by rider_core_temp_calibrate.py; review before use. */\n")
        handle.write(f"#define RIDER_CORE_TEMP_CAL_MODEL_VERSION {MODEL_VERSION}\n")
        handle.write("#define RIDER_CORE_TEMP_CALIBRATION_AVAILABLE 1\n")
        handle.write(f"#define RIDER_CORE_TEMP_CALIBRATION_VALID {1 if gate['passed'] else 0}\n")
        for macro, key in (
            ("RIDER_CORE_TEMP_CAL_BASE_CENTI", "base_core_centi"),
            ("RIDER_CORE_TEMP_CAL_SKIN_GAIN_Q8", "skin_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_SKIN_DELTA_GAIN_Q8", "skin_delta_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_TREND_1M_GAIN_Q8", "trend_1m_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_TREND_5M_GAIN_Q8", "trend_5m_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_HR_GAIN_Q8", "heart_rate_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_HR_TREND_GAIN_Q8", "heart_rate_trend_gain_q8"),
            ("RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8", "lag_alpha_q8"),
        ):
            handle.write(f"#define {macro} {firmware[key]}\n")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse calibration input, holdout strategy, and report destinations."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path,
                        help="time-ordered trusted-skin/reference-core CSV")
    parser.add_argument("--holdout-fraction", type=float, default=0.2)
    parser.add_argument("--holdout-session",
                        help="leave out this complete session_id instead of a suffix")
    parser.add_argument("--output", type=Path, help="write JSON calibration report")
    parser.add_argument("--header", type=Path, help="write generated firmware defines")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Run calibration, emit JSON evidence, and optionally write defines."""
    args = parse_args(argv or sys.argv[1:])
    if not 0.1 <= args.holdout_fraction <= 0.5:
        raise SystemExit("--holdout-fraction must be between 0.1 and 0.5")
    result = calibrate(read_samples(args.csv), args.holdout_fraction,
                       args.holdout_session)
    encoded = json.dumps(result, ensure_ascii=True, indent=2) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    else:
        sys.stdout.write(encoded)
    if args.header:
        write_header(args.header, result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
