"""Summarize local vehicle telemetry; never transfers data or changes a tape."""
import argparse
import csv
import json
import math
from pathlib import Path


def summarize(path):
    with path.open(encoding="utf-8") as stream:
        lines = list(stream)
    comments = [line.strip() for line in lines if line.startswith("#")]
    rows = list(csv.DictReader(line for line in lines if not line.startswith("#")))
    if not rows:
        raise ValueError(f"No telemetry rows: {path}")
    if not any(line.startswith("# end,") for line in comments):
        raise ValueError(f"Trace is not closed; stop before analyzing: {path}")
    def vector(row, prefix):
        return tuple(float(row[prefix + axis]) for axis in "xyz")
    dt = [float(row["dt_s"]) for row in rows]
    if any(not math.isfinite(value) or value <= 0 for value in dt):
        raise ValueError(f"Invalid timestep in {path}")
    positions = [vector(row, "") for row in rows]
    total = sum(dt)
    result = {
        "file": str(path.resolve()), "metadata": comments, "frames": len(rows),
        "game_seconds": total, "simulation_updates_per_game_second": len(rows) / total,
        "min_dt_ms": min(dt) * 1000, "max_dt_ms": max(dt) * 1000,
        "path_between_samples_m": sum(math.dist(a, b) for a, b in zip(positions, positions[1:])),
        "peak_world_ms_excluding_trace_write": max(int(row["world_ns"]) for row in rows) / 1e6,
        "peak_speed_mps": max(math.sqrt(sum(v*v for v in vector(row, "v"))) * 50 for row in rows),
        "peak_roll_deg": max(abs(math.degrees(math.atan2(float(row["rz"]), float(row["uz"])))) for row in rows),
        "peak_pitch_deg": max(abs(math.degrees(math.atan2(float(row["fz"]), float(row["uz"])))) for row in rows),
        "collision_calls": sum(int(row["collision_calls"]) for row in rows),
        "body_contacts_including_retries": sum(int(row["body_contacts"]) for row in rows),
        "retry_probes": sum(int(row["retries"]) for row in rows),
        "planned_substeps": sum(int(row["substeps"]) for row in rows),
    }
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps([summarize(path) for path in args.traces], indent=2, allow_nan=False))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"{error}\n")
