#!/usr/bin/env python3
"""Fit and validate the single-M601 core-temperature shadow model.

The input is a time-ordered CSV containing a contact temperature and a
reference core temperature.  The final contiguous portion is held out so
that a slowly drifting time series cannot leak future samples into the fit.
The output parameters match the fixed-point model in Rider CoreTemp:

    target = contact_centi + offset_centi
                     + slope_centi_per_min * slope_gain_q8 / 256

The tool intentionally produces calibration evidence even when the error
gate fails; callers must not enable the firmware calibration flag in that
case.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Sequence


@dataclass(frozen=True)
class Sample:
    timestamp_s: float
    contact_centi: float
    core_centi: float
    reference_skin_centi: float | None = None
    session_id: str = ""


def _number(row: dict[str, str], names: Sequence[str]) -> float | None:
    """Read the first non-empty numeric column alias from one CSV row."""
    for name in names:
        value = (row.get(name) or "").strip()
        if value:
            return float(value)
    return None


def _timestamp(value: str) -> float:
    """Parse either seconds since epoch or an ISO-8601 timestamp."""
    value = value.strip()
    try:
        return float(value)
    except ValueError:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


def read_samples(path: Path) -> list[Sample]:
    """Read supported CSV aliases and return samples sorted by timestamp."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = csv.DictReader(handle)
        if not rows.fieldnames:
            raise ValueError("CSV has no header")
        samples: list[Sample] = []
        for line_number, row in enumerate(rows, start=2):
            raw_time = row.get("timestamp", "") or row.get("time", "")
            raw_contact = _number(row, ("contact_centi", "m601_centi", "temperature_centi",
                                        "skin_centi"))
            if raw_contact is None:
                raw_contact_c = _number(row, ("contact_c", "skin_c", "m601_c",
                                              "temperature_c"))
                if raw_contact_c is not None:
                    raw_contact = raw_contact_c * 100.0
                else:
                    raw_m601 = _number(row, ("m601_raw", "raw_m601", "raw"))
                    raw_contact = (None if raw_m601 is None else
                                   raw_m601 * 100.0 / 256.0 + 4000.0)
            raw_core = _number(row, ("core_centi", "reference_core_centi"))
            if raw_core is None:
                raw_core_c = _number(row, ("core_c", "reference_core_c"))
                raw_core = None if raw_core_c is None else raw_core_c * 100.0
            raw_skin = _number(row, ("reference_skin_centi", "skin_reference_centi"))
            if raw_skin is None:
                raw_skin_c = _number(row, ("reference_skin_c", "skin_reference_c"))
                raw_skin = None if raw_skin_c is None else raw_skin_c * 100.0
            state = (row.get("state") or "").strip().lower()
            if state not in {"stable", "3", "ok", "valid"}:
                continue
            if not raw_time or raw_contact is None or raw_core is None:
                continue
            try:
                session_id = (row.get("session_id") or row.get("session") or "").strip()
                samples.append(Sample(_timestamp(raw_time), raw_contact, raw_core,
                                      raw_skin, session_id))
            except ValueError as exc:
                raise ValueError(f"invalid row {line_number}: {exc}") from exc
    samples.sort(key=lambda sample: sample.timestamp_s)
    if len(samples) < 6:
        raise ValueError("at least six stable samples with contact/core values are required")
    return samples


def _same_session(previous: Sample, current: Sample) -> bool:
    """Treat explicit session changes as a filter/model reset boundary."""
    return (not previous.session_id or not current.session_id or
            previous.session_id == current.session_id)


def slopes(samples: Sequence[Sample]) -> list[float]:
    """Calculate contact-temperature slope in centi-degrees per minute."""
    result = [0.0]
    for previous, current in zip(samples, samples[1:]):
        delta_s = current.timestamp_s - previous.timestamp_s
        if delta_s <= 0 or not _same_session(previous, current):
            result.append(0.0)
        else:
            result.append((current.contact_centi - previous.contact_centi) * 60.0 / delta_s)
    return result


def fit(samples: Sequence[Sample], sample_slopes: Sequence[float] | None = None) -> tuple[float, float]:
    """Fit offset and slope coefficient by ordinary least squares."""
    sample_slopes = list(sample_slopes) if sample_slopes is not None else slopes(samples)
    x = [sample_slopes[index] for index in range(len(samples))]
    y = [sample.core_centi - sample.contact_centi for sample in samples]
    mean_x = sum(x) / len(x)
    mean_y = sum(y) / len(y)
    denominator = sum((value - mean_x) ** 2 for value in x)
    beta = 0.0 if denominator < 1e-9 else sum(
        (x[index] - mean_x) * (y[index] - mean_y) for index in range(len(x))
    ) / denominator
    return mean_y - beta * mean_x, beta


def predict(samples: Sequence[Sample], offset: float, beta: float,
            sample_slopes: Sequence[float] | None = None) -> list[float]:
    """Apply the fitted affine contact-plus-slope model to samples."""
    sample_slopes = list(sample_slopes) if sample_slopes is not None else slopes(samples)
    return [
        sample.contact_centi + offset + beta * sample_slopes[index]
        for index, sample in enumerate(samples)
    ]


def predict_lagged(samples: Sequence[Sample], offset: float, beta: float,
                   alpha_q8: int, sample_slopes: Sequence[float] | None = None,
                   initial: float | None = None) -> list[float]:
    """Apply the same first-order lag recurrence used by the firmware."""
    sample_slopes = list(sample_slopes) if sample_slopes is not None else slopes(samples)
    predictions: list[float] = []
    previous = initial
    alpha = max(1, min(255, int(alpha_q8))) / 256.0
    for index, sample in enumerate(samples):
        if index and not _same_session(samples[index - 1], sample):
            previous = None
        target = sample.contact_centi + offset + beta * sample_slopes[index]
        previous = target if previous is None else previous + alpha * (target - previous)
        predictions.append(previous)
    return predictions


def _signed_q8_step(delta: int, alpha_q8: int) -> int:
    """Mirror the firmware's signed round-to-nearest Q8 division."""
    step = delta * alpha_q8
    if step >= 0:
        return (step + 128) // 256
    return -(((-step) + 128) // 256)


def _firmware_slopes(samples: Sequence[Sample]) -> list[int]:
    """Convert CSV slopes to the integer centi-degree slopes used by C."""
    result = [0]
    for previous, current in zip(samples, samples[1:]):
        delta_s = current.timestamp_s - previous.timestamp_s
        if delta_s <= 0:
            result.append(0)
        else:
            result.append(int(math.trunc(
                (current.contact_centi - previous.contact_centi) * 60.0 / delta_s)))
    return result


def predict_firmware(samples: Sequence[Sample], offset_centi: int,
                     slope_gain_q8: int, lag_alpha_q8: int,
                     sample_slopes: Sequence[int] | None = None,
                     initial: int | None = None) -> list[float]:
    """Run the integer model, including range and one-second rate guards."""
    sample_slopes = (list(sample_slopes) if sample_slopes is not None else
                     _firmware_slopes(samples))
    predictions: list[float] = []
    previous = initial
    alpha = max(1, min(255, int(lag_alpha_q8)))
    for index, sample in enumerate(samples):
        if index and not _same_session(samples[index - 1], sample):
            previous = None
        contact = int(round(sample.contact_centi))
        slope_term = int(math.trunc(sample_slopes[index] * slope_gain_q8 / 256.0))
        target = max(3500, min(4200, contact + offset_centi + slope_term))
        if previous is None:
            previous = target
        else:
            candidate = max(3500, min(4200,
                                       previous + _signed_q8_step(target - previous,
                                                                  alpha)))
            delta = max(-25, min(25, candidate - previous))
            previous = previous + delta
        predictions.append(float(previous))
    return predictions


def fit_lagged(samples: Sequence[Sample], sample_slopes: Sequence[float]) -> tuple[float, float, int]:
    """Fit offset, slope gain and lag with a small fixed-point grid search.

    For each lag candidate, the recurrence is linear in offset and slope gain;
    solving its two-variable normal equation avoids importing a numerical
    dependency into the host-side calibration tool.
    """
    best: tuple[float, float, int, float] | None = None
    core_values = [sample.core_centi for sample in samples]
    for alpha_q8 in range(1, 256):
        alpha = alpha_q8 / 256.0
        offset_basis = 0.0
        slope_basis = 0.0
        constant = 0.0
        rows: list[tuple[float, float, float]] = []
        for index, sample in enumerate(samples):
            if index == 0 or not _same_session(samples[index - 1], sample):
                offset_basis = 1.0
                slope_basis = sample_slopes[index]
                constant = sample.contact_centi
            else:
                offset_basis = (1.0 - alpha) * offset_basis + alpha
                slope_basis = ((1.0 - alpha) * slope_basis +
                               alpha * sample_slopes[index])
                constant = (1.0 - alpha) * constant + alpha * sample.contact_centi
            rows.append((offset_basis, slope_basis, constant))

        s00 = sum(row[0] * row[0] for row in rows)
        s01 = sum(row[0] * row[1] for row in rows)
        s11 = sum(row[1] * row[1] for row in rows)
        t0 = sum(row[0] * (core_values[i] - row[2]) for i, row in enumerate(rows))
        t1 = sum(row[1] * (core_values[i] - row[2]) for i, row in enumerate(rows))
        determinant = s00 * s11 - s01 * s01
        if abs(determinant) < 1e-9:
            offset, beta = fit(samples, sample_slopes)
        else:
            offset = (t0 * s11 - t1 * s01) / determinant
            beta = (s00 * t1 - s01 * t0) / determinant
        predictions = predict_lagged(samples, offset, beta, alpha_q8, sample_slopes)
        error = sum((predictions[i] - core_values[i]) ** 2
                    for i in range(len(samples)))
        if best is None or error < best[3]:
            best = (offset, beta, alpha_q8, error)
    assert best is not None
    return best[:3]


def metrics(samples: Sequence[Sample], predictions: Sequence[float]) -> dict[str, float]:
    """Return RMSE, standard error and signed bias for a contiguous split."""
    residuals = [predictions[index] - sample.core_centi for index, sample in enumerate(samples)]
    count = len(residuals)
    squared = sum(value * value for value in residuals)
    return {
        "samples": float(count),
        "rmse_centi": math.sqrt(squared / count),
        "rmse_c": math.sqrt(squared / count) / 100.0,
        "see_centi": math.sqrt(squared / max(1, count - 2)),
        "see_c": math.sqrt(squared / max(1, count - 2)) / 100.0,
        "bias_centi": sum(residuals) / count,
        "bias_c": sum(residuals) / count / 100.0,
    }


def reference_skin_metrics(samples: Sequence[Sample]) -> dict[str, float] | None:
    """Summarize contact-versus-reference-skin agreement when supplied."""
    paired = [sample for sample in samples if sample.reference_skin_centi is not None]
    if not paired:
        return None
    residuals = [sample.contact_centi - sample.reference_skin_centi for sample in paired]
    squared = sum(value * value for value in residuals)
    return {
        "samples": float(len(residuals)),
        "rmse_centi": math.sqrt(squared / len(residuals)),
        "rmse_c": math.sqrt(squared / len(residuals)) / 100.0,
        "bias_centi": sum(residuals) / len(residuals),
        "bias_c": sum(residuals) / len(residuals) / 100.0,
    }


def q8(value: float) -> int:
    """Round a floating coefficient into the firmware's signed Q8 format."""
    rounded = int(round(value * 256.0))
    return max(-32768, min(32767, rounded))


def _split_samples(samples: Sequence[Sample], holdout_fraction: float,
                   holdout_session: str | None) -> tuple[list[Sample], list[Sample],
                                                          str, str | None]:
    """Create either a contiguous split or a complete-session split."""
    if holdout_session is not None:
        holdout = [sample for sample in samples
                   if sample.session_id == holdout_session]
        train = [sample for sample in samples
                 if sample.session_id != holdout_session]
        if not holdout:
            raise ValueError(f"holdout session not found: {holdout_session}")
        if any(not sample.session_id for sample in holdout):
            raise ValueError("--holdout-session requires a session_id column")
        if len(train) < 3 or len(holdout) < 3:
            raise ValueError("session holdout requires at least three samples per side")
        return train, holdout, "complete_session", holdout_session

    split = max(3, min(len(samples) - 3,
                       int(len(samples) * (1.0 - holdout_fraction))))
    return list(samples[:split]), list(samples[split:]), "contiguous_suffix", None


def calibrate(samples: Sequence[Sample], holdout_fraction: float,
              holdout_session: str | None = None) -> dict[str, object]:
    """Fit without the holdout session and evaluate untouched samples."""
    train, holdout, split_strategy, selected_session = _split_samples(
        samples, holdout_fraction, holdout_session)
    train_slopes = slopes(train)
    holdout_slopes = slopes(holdout)
    offset_centi, beta, lag_alpha_q8 = fit_lagged(train, train_slopes)
    train_predictions = predict_lagged(train, offset_centi, beta, lag_alpha_q8,
                                       train_slopes)
    holdout_initial = (None if split_strategy == "complete_session"
                       else train_predictions[-1])
    holdout_predictions = predict_lagged(holdout, offset_centi, beta, lag_alpha_q8,
                                         holdout_slopes, holdout_initial)
    floating_train_metrics = metrics(train, train_predictions)
    floating_holdout_metrics = metrics(holdout, holdout_predictions)
    offset_q = int(round(offset_centi))
    slope_gain_q8 = q8(beta)
    firmware_train_slopes = _firmware_slopes(train)
    firmware_holdout_slopes = _firmware_slopes(holdout)
    firmware_train_predictions = predict_firmware(
        train, offset_q, slope_gain_q8, lag_alpha_q8, firmware_train_slopes)
    firmware_holdout_predictions = predict_firmware(
        holdout, offset_q, slope_gain_q8, lag_alpha_q8, firmware_holdout_slopes,
        (None if split_strategy == "complete_session"
         else int(firmware_train_predictions[-1])))
    firmware_train_metrics = metrics(train, firmware_train_predictions)
    firmware_holdout_metrics = metrics(holdout, firmware_holdout_predictions)
    gate_passed = (firmware_holdout_metrics["rmse_c"] <= 0.5 and
                   firmware_holdout_metrics["see_c"] <= 0.5 and
                   abs(firmware_holdout_metrics["bias_c"]) <= 0.5)
    result: dict[str, object] = {
        "model": "single_m601_contact_plus_slope_lag_q8",
        "samples": len(samples),
        "train_samples": len(train),
        "holdout_samples": len(holdout),
        "holdout_fraction": holdout_fraction,
        "split": {
            "strategy": split_strategy,
            "holdout_session": selected_session,
            "train_sessions": sorted({sample.session_id for sample in train
                                        if sample.session_id}),
            "holdout_sessions": sorted({sample.session_id for sample in holdout
                                          if sample.session_id}),
        },
        "floating_point": {
            "offset_centi": offset_centi,
            "slope_gain_centi_per_min": beta,
        },
        "firmware": {
            "offset_centi": offset_q,
            "slope_gain_q8": slope_gain_q8,
            "lag_alpha_q8": lag_alpha_q8,
            "available": True,
            "valid": bool(gate_passed),
        },
        "train": firmware_train_metrics,
        "holdout": firmware_holdout_metrics,
        "floating_train": floating_train_metrics,
        "floating_holdout": floating_holdout_metrics,
        "gate": {
            "rmse_limit_c": 0.5,
            "see_limit_c": 0.5,
            "absolute_bias_limit_c": 0.5,
            "passed": gate_passed,
        },
    }
    skin_metrics = reference_skin_metrics(samples)
    if skin_metrics is not None:
        result["reference_skin"] = skin_metrics
    return result


def write_header(path: Path, result: dict[str, object]) -> None:
    """Write a reviewable compile-time calibration fragment."""
    firmware = result["firmware"]
    gate = result["gate"]
    with path.open("w", encoding="ascii") as handle:
        handle.write("/* Generated by rider_core_temp_calibrate.py; review gate before use. */\n")
        handle.write("#define RIDER_CORE_TEMP_CALIBRATION_AVAILABLE 1\n")
        handle.write(f"#define RIDER_CORE_TEMP_CALIBRATION_VALID {1 if gate['passed'] else 0}\n")
        handle.write(f"#define RIDER_CORE_TEMP_CAL_OFFSET_CENTI {firmware['offset_centi']}\n")
        handle.write(f"#define RIDER_CORE_TEMP_CAL_SLOPE_GAIN_Q8 {firmware['slope_gain_q8']}\n")
        handle.write(f"#define RIDER_CORE_TEMP_CAL_LAG_ALPHA_Q8 {firmware['lag_alpha_q8']}\n")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse the command-line interface for calibration and report output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="time-ordered M601/reference CSV")
    parser.add_argument("--holdout-fraction", type=float, default=0.2)
    parser.add_argument("--holdout-session",
                        help="leave out this complete session_id instead of a suffix")
    parser.add_argument("--output", type=Path, help="write JSON calibration report")
    parser.add_argument("--header", type=Path, help="write generated firmware defines")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Run calibration, emit the JSON report, and optionally write defines."""
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
