"""Fit normal feature statistics and calibrate an anomaly threshold."""

import csv
import json
import math
import statistics
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[1]
DATASETS = PYTHON_ROOT / "datasets"
MODELS = PYTHON_ROOT / "models"

FEATURES = ("temperature_c", "vibration_rms_g", "current_rms_a")
CALIBRATION_PERCENTILE = 0.99


def read_normal_records(path):
    records = []

    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if int(row["label"]) != 0:
                raise ValueError(f"Non-normal record in {path.name}")

            values = [float(row[name]) for name in FEATURES]
            if not all(math.isfinite(value) for value in values):
                raise ValueError(f"Non-finite feature in {path.name}")

            records.append(values)

    if len(records) < 2:
        raise ValueError(f"Too few records in {path.name}")

    return records


def anomaly_score(values, means, standard_deviations):
    return sum(
        ((value - mean) / deviation) ** 2
        for value, mean, deviation in zip(
            values, means, standard_deviations
        )
    ) / len(FEATURES)


def main():
    training = read_normal_records(DATASETS / "train.csv")
    calibration = read_normal_records(DATASETS / "calibration.csv")

    means = [
        statistics.mean(record[index] for record in training)
        for index in range(len(FEATURES))
    ]
    deviations = [
        statistics.stdev(record[index] for record in training)
        for index in range(len(FEATURES))
    ]

    if not all(
        math.isfinite(value) and value > 0.0
        for value in deviations
    ):
        raise ValueError("Feature standard deviations must be positive")

    scores = sorted(
        anomaly_score(record, means, deviations)
        for record in calibration
    )

    rank = math.ceil(CALIBRATION_PERCENTILE * len(scores))
    threshold = scores[rank - 1]
    flagged = sum(score > threshold for score in scores)

    model = {
        "schema_version": 1,
        "model_type": "diagonal_standardized_squared_distance",
        "feature_order": list(FEATURES),
        "means": means,
        "standard_deviations": deviations,
        "threshold": threshold,
        "decision_rule": "anomaly if score > threshold",
        "score_definition": "mean of squared standardized features",
        "training_records": len(training),
        "calibration_records": len(calibration),
        "calibration_percentile": CALIBRATION_PERCENTILE,
        "calibration_method": "nearest-rank percentile",
        "calibration_flagged_records": flagged,
        "source": json.loads((DATASETS / "metadata.json").read_text())["source"],
    }

    MODELS.mkdir(parents=True, exist_ok=True)
    output = MODELS / "anomaly_model.json"
    output.write_text(
        json.dumps(model, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )

    print(f"Training records: {len(training)}")
    print(f"Calibration records: {len(calibration)}")

    for name, mean, deviation in zip(FEATURES, means, deviations):
        print(f"{name}: mean={mean:.9f} std={deviation:.9f}")

    print(f"Anomaly threshold: {threshold:.9f}")
    print(
        f"Calibration flagged: {flagged}/{len(calibration)} "
        f"({100.0 * flagged / len(calibration):.2f}%)"
    )
    print("Saved: python/models/anomaly_model.json")
    print("Test dataset was not used.")


if __name__ == "__main__":
    main()
