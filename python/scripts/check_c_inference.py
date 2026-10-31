"""Compare compiled C inference with the original Python model."""

import csv
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: check_c_inference.py HOST_EXECUTABLE")

    model = json.loads(
        (ROOT / "models" / "anomaly_model.json").read_text(encoding="utf-8")
    )

    with (ROOT / "datasets" / "test.csv").open(
        newline="", encoding="utf-8"
    ) as stream:
        rows = list(csv.DictReader(stream))

    if not rows:
        raise ValueError("Empty test dataset")

    features = [
        [float(row[name]) for name in model["feature_order"]]
        for row in rows
    ]
    payload = "".join(
        " ".join(repr(value) for value in record) + "\n"
        for record in features
    )

    result = subprocess.run(
        [sys.argv[1]], input=payload, text=True,
        capture_output=True, check=True
    )
    print(result.stderr.strip())

    lines = result.stdout.splitlines()
    if len(lines) != len(features):
        raise ValueError("C output record count mismatch")

    maximum_error = 0.0
    score_failures = 0
    decision_mismatches = 0
    c_false_positives = 0
    c_false_negatives = 0

    for row, values, line in zip(rows, features, lines):
        python_score = sum(
            ((value - mean) / deviation) ** 2
            for value, mean, deviation in zip(
                values, model["means"], model["standard_deviations"]
            )
        ) / 3.0

        score_text, decision_text = line.split()
        c_score = float(score_text)
        c_decision = int(decision_text)
        if not math.isfinite(c_score) or c_decision not in (0, 1):
            raise ValueError("Invalid C output")

        error = abs(c_score - python_score)
        maximum_error = max(maximum_error, error)

        # Allow float32 rounding in inputs, parameters and arithmetic.
        tolerance = 1e-4 * (1.0 + abs(python_score))
        score_failures += int(error > tolerance)
        decision_mismatches += int(
            bool(c_decision) != (python_score > model["threshold"])
        )

        label = int(row["label"])
        c_false_positives += int(label == 0 and c_decision == 1)
        c_false_negatives += int(label == 1 and c_decision == 0)

    print(f"Compared records: {len(features)}")
    print(f"Maximum absolute score difference: {maximum_error:.9g}")
    print(f"Scores outside tolerance: {score_failures}")
    print(f"Decision mismatches: {decision_mismatches}")
    print(f"C false positives: {c_false_positives}")
    print(f"C false negatives: {c_false_negatives}")

    if score_failures or decision_mismatches:
        raise SystemExit("FAIL: investigate C/Python differences")

    print("PASS: C inference agrees with Python on this test dataset.")


if __name__ == "__main__":
    main()
