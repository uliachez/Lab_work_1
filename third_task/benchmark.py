#!/usr/bin/env python3

import csv
import re
import subprocess
import statistics
import random
from pathlib import Path

THREADS = (1, 2, 4, 8)
RUNS = 5
INITIAL_KEYS = 1000
TOTAL_OPS = 60000
WORKLOADS = {
    "read_heavy_90_5_5": ("0.90", "0.05"),
    "balanced_50_25_25": ("0.50", "0.25"),
}
TIME_RE = re.compile(r"^Elapsed time = ([0-9.eE+-]+) seconds$", re.M)
COUNTS = ("member ops", "insert ops", "delete ops", "member hits")


def run(variant, threads, fractions):
    cmd = [f"./list_{variant}", str(threads)]
    input_text = f"{INITIAL_KEYS}\n{TOTAL_OPS}\n{fractions[0]}\n{fractions[1]}\n"
    output = subprocess.check_output(cmd, input=input_text, text=True)
    match = TIME_RE.search(output)
    if match is None:
        raise RuntimeError(f"No timing found in {output}")
    counters = {key: int(re.search(rf"^{re.escape(key)} = (\d+)$", output, re.M).group(1))
                for key in COUNTS}
    if sum(counters[k] for k in COUNTS[:3]) != TOTAL_OPS:
        raise RuntimeError(f"Operation total incorrect: {output}")
    return float(match.group(1)), counters


def main():
    rows = []
    rng = random.Random(20261017)
    for workload, fractions in WORKLOADS.items():
        for threads in THREADS:
            samples = {"pthread": [], "custom": []}

            for variant in samples:
                run(variant, threads, fractions)

            for trial in range(RUNS):
                order = list(samples)
                rng.shuffle(order)
                for variant in order:
                    duration, counters = run(variant, threads, fractions)
                    samples[variant].append(duration)
                    rows.append({"workload": workload, "threads": threads,
                                 "variant": variant, "trial": trial + 1,
                                 "elapsed_seconds": f"{duration:.9f}", **counters})
            pthread_median = statistics.median(samples["pthread"])
            custom_median = statistics.median(samples["custom"])
            print(f"{workload:22s} threads={threads}: "
                  f"pthread={pthread_median:.5f}s custom={custom_median:.5f}s "
                  f"custom/pthread={custom_median / pthread_median:.2f}x")
    out = Path(__file__).resolve().parent / "results.csv"
    with out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    print(f"Full raw results: {out}")


if __name__ == "__main__":
    main()
