"""Evaluate the fixed model on the held-out test dataset."""

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    model_path = ROOT / "models" / "anomaly_model.json"
    test_path = ROOT / "datasets" / "test.csv"
    model_bytes = model_path.read_bytes()
    model = json.loads(model_bytes)

    expected = ["temperature_c", "vibration_rms_g", "current_rms_a"]
    if model["feature_order"] != expected:
        raise ValueError("Unexpected model feature order")

    means = model["means"]
    deviations = model["standard_deviations"]
    threshold = model["threshold"]

    if len(means) != 3 or len(deviations) != 3:
        raise ValueError("Expected three model features")
    if not all(math.isfinite(value) for value in means):
        raise ValueError("Invalid model means")
    if not all(math.isfinite(value) and value > 0 for value in deviations):
        raise ValueError("Invalid model deviations")
    if not math.isfinite(threshold) or threshold < 0:
        raise ValueError("Invalid model threshold")

    tp = tn = fp = fn = 0
    scenarios = {}

    with test_path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            label = int(row["label"])
            if label not in (0, 1):
                raise ValueError("Unexpected test label")

            values = [float(row[name]) for name in expected]
            if not all(math.isfinite(value) for value in values):
                raise ValueError("Non-finite test feature")

            score = sum(
                ((value - mean) / deviation) ** 2
                for value, mean, deviation in zip(values, means, deviations)
            ) / 3.0
            predicted = score > threshold

            if label == 1:
                tp += int(predicted)
                fn += int(not predicted)
            else:
                fp += int(predicted)
                tn += int(not predicted)

            entry = scenarios.setdefault(
                row["scenario"], {"total": 0, "flagged": 0}
            )
            entry["total"] += 1
            entry["flagged"] += int(predicted)

    if tp + fn == 0 or tn + fp == 0:
        raise ValueError("Test set must contain both classes")

    metrics = {
        "precision": tp / (tp + fp) if tp + fp else 0.0,
        "recall": tp / (tp + fn),
        "false_positive_rate": fp / (fp + tn),
        "accuracy": (tp + tn) / (tp + tn + fp + fn),
    }

    report = {
        "source": json.loads((ROOT / "datasets" / "metadata.json").read_text())["source"],
        "model_sha256": hashlib.sha256(model_bytes).hexdigest(),
        "test_sha256": hashlib.sha256(test_path.read_bytes()).hexdigest(),
        "threshold": threshold,
        "true_positive": tp,
        "true_negative": tn,
        "false_positive": fp,
        "false_negative": fn,
        "metrics": metrics,
        "scenarios": scenarios,
        "limitation": "Only two small simulated test scenarios were evaluated; this does not establish field accuracy or current/vibration fault detection.",
    }

    output = ROOT / "models" / "evaluation_report.json"
    output.write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )

    print(f"Test records: {tp + tn + fp + fn}")
    print(f"True positives:  {tp}")
    print(f"True negatives:  {tn}")
    print(f"False positives: {fp}")
    print(f"False negatives: {fn}")

    for name, value in metrics.items():
        print(f"{name}: {value:.2%}")

    for name, counts in sorted(scenarios.items()):
        print(f"{name}: flagged {counts['flagged']}/{counts['total']}")

    print("Saved: python/models/evaluation_report.json")
    print("Model parameters and threshold were not changed.")


if __name__ == "__main__":
    main()
