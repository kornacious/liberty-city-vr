"""Summarize local per-frame captures; accepts several runs for A/B comparison."""
import argparse
import csv
import json
import math
from collections import Counter
from pathlib import Path

EVENTS = {
    1: "damage", 2: "died", 4: "recovered_after_death", 8: "entered_vehicle",
    16: "left_vehicle", 32: "weapon_used", 64: "player_appeared",
    128: "player_disappeared", 256: "relocated_over_50_units",
    512: "pause_changed", 1024: "cutscene_changed", 2048: "mission_changed",
    4096: "wanted_changed",
}


GPU_PASSES = ["sky", "roads", "reflections", "world", "water", "fading", "weather",
              "effects", "ui", "scope", "wrist", "resolve_postprocess", "other"]


def summarize_gpu_passes(rows, selected_ids, start_ns, interval=8, sampling="fixed"):
    if "gpu_pass_frame_id" not in rows[0]:
        return None
    sources = {int(row["frame_id"]): row for row in rows}
    seen = set()
    samples, statuses = [], Counter()
    repeated = unmatched = mismatched = excluded = 0
    for row in rows:  # Filter by the source frame, never the readback row.
        source = int(row["gpu_pass_frame_id"])
        if not source:
            continue
        if source in seen:
            repeated += 1
            continue
        seen.add(source)
        status = int(row["gpu_pass_status"])
        statuses[str(status)] += 1
        if source not in sources:
            unmatched += 1
            continue
        original = sources[source]
        if (int(row["gpu_pass_start_monotonic_ns"]) != int(original["start_monotonic_ns"])
                or source > int(row["frame_id"])):
            mismatched += 1
            continue
        if source not in selected_ids:
            excluded += 1
            continue
        if status != 1:
            continue
        mask = int(row["gpu_pass_valid_mask"])
        frame_ms = float(row["gpu_pass_frame_ms"])
        if not mask or mask & ~((1 << len(GPU_PASSES))-1) or not math.isfinite(frame_ms) or frame_ms < 0:
            mismatched += 1
            continue
        samples.append((source, original, row, mask))
    passes = {}
    for i, name in enumerate(GPU_PASSES):
        matching = [row for _, _, row, mask in samples if mask & (1 << i)]
        passes[name] = {
            "gpu_ms": distribution([float(row[f"gpu_pass_{name}_ms"]) for row in matching]),
            "draw_calls": distribution([int(row[f"gpu_pass_{name}_draw_calls"]) for row in matching]),
        }
    bins = {}
    for _, original, row, mask in samples:
        second = (int(original["start_monotonic_ns"])-start_ns)/1e9
        bins.setdefault(int(second//5)*5, []).append((row, mask))
    windows = []
    for second, group in bins.items():
        means = {}
        for i, name in enumerate(GPU_PASSES):
            values = [float(row[f"gpu_pass_{name}_ms"]) for row, mask in group if mask & (1 << i)]
            values = [v for v in values if math.isfinite(v) and v >= 0]
            if values:
                means[name] = sum(values)/len(values)
        windows.append({"source_elapsed_s": second, "samples": len(group),
                        "gpu_frame_mean_ms": sum(float(row["gpu_pass_frame_ms"]) for row, _ in group)/len(group),
                        "pass_mean_ms": means})
    interval = max(1, interval)
    if sampling == "rotating_phase_one_per_block":
        expected = {source for source in selected_ids if (source-1) % interval == ((source-1)//interval) % interval}
    else:
        expected = {source for source in selected_ids if source % interval == 0}
    return {
        "available": bool(samples), "unique_selected_samples": len(samples),
        "sample_interval_frames": interval, "sampling": sampling, "expected_source_frames": len(expected),
        "unobserved_source_frames": len(expected-seen), "unique_status_counts": dict(statuses),
        "repeated_observations_ignored": repeated, "unmatched_source_frames": unmatched,
        "mismatched_source_frames": mismatched, "excluded_nongameplay_sources": excluded,
        "gpu_frame_ms": distribution([float(row["gpu_pass_frame_ms"]) for _, _, row, _ in samples]),
        "passes": passes,
        "slow_5s_windows": sorted(windows, key=lambda item: item["gpu_frame_mean_ms"], reverse=True)[:10],
        "notes": ["Intervals are approximate primary-lane Vulkan timestamps, not exclusive shader costs.",
                  "Sampling, timestamp dependencies and tile-based execution can perturb or redistribute timings.",
                  "Pending GPU results at capture shutdown are not drained with extra waits."]}


def distribution(values):
    values = sorted(v for v in values if math.isfinite(v) and v >= 0)
    if not values:
        return None

    def percentile(fraction):
        index = (len(values) - 1) * fraction
        lo = int(index)
        hi = min(lo + 1, len(values) - 1)
        return values[lo] + (values[hi] - values[lo]) * (index - lo)

    return {
        "samples": len(values), "mean": sum(values) / len(values),
        "median": percentile(.5), "p95": percentile(.95),
        "p99": percentile(.99), "max": values[-1],
    }


def summarize(path, include_nongameplay=False, allow_incomplete=False):
    if not path.name.endswith(".frames.csv"):
        raise ValueError("Expected a new .frames.csv capture, not the one-second quest_perf.csv")
    metadata_path = path.with_name(path.name.replace(".frames.csv", ".session.csv"))
    with metadata_path.open(encoding="utf-8") as stream:
        metadata = dict((row["key"], row["value"]) for row in csv.DictReader(stream))
    if metadata.get("schema") not in ("1", "2", "3", "4"):
        raise ValueError("Unsupported capture schema")
    complete = metadata.get("closed") == "1" and metadata.get("write_error") == "0"
    if not complete and not allow_incomplete:
        raise ValueError("Capture was not cleanly closed; set profiling OFF first, or use --allow-incomplete")
    with path.open(encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
    if not rows:
        raise ValueError("No captured frames")
    # Ignore a truncated trailing row only when explicitly salvaging a capture.
    partial_rows = sum(None in row or any(value is None for value in row.values()) for row in rows)
    if partial_rows:
        if not allow_incomplete:
            raise ValueError("Malformed/truncated CSV rows")
        rows = [row for row in rows if None not in row and all(v is not None for v in row.values())]
    if not rows:
        raise ValueError("No complete CSV rows")
    ids = [int(row["frame_id"]) for row in rows]
    starts = [int(row["start_monotonic_ns"]) for row in rows]
    if any(b <= a for a, b in zip(ids, ids[1:])) or any(b <= a for a, b in zip(starts, starts[1:])):
        raise ValueError("Frame IDs and monotonic timestamps must increase")
    if complete and int(metadata["written_rows"]) != len(rows):
        raise ValueError("Session row count does not match frame data")
    selected = rows if include_nongameplay else [row for row in rows if
        row["player_valid"] == "1" and row["paused"] == "0" and
        row["cutscene"] == "0" and row["dead"] == "0"]
    selected_ids = {int(row["frame_id"]) for row in selected}
    interval_ids = {b for a, b in zip(ids, ids[1:])
                    if b == a + 1 and a in selected_ids and b in selected_ids}
    events = Counter()
    for row in rows:
        mask = int(row["event_mask"])
        events.update(name for flag, name in EVENTS.items() if mask & flag)
    vehicle_speeds = {}
    for row in selected:
        if row.get("in_vehicle") != "1" or "vehicle_speed_kmh" not in row:
            continue
        model = row["vehicle_model"]
        vehicle_speeds.setdefault(model, []).append(row)
    vehicle_summary = {}
    for model, samples in vehicle_speeds.items():
        speeds = distribution([float(row["vehicle_speed_kmh"]) for row in samples])
        vehicle_summary[model] = {
            "speed_kmh": speeds,
            "observed_top_kmh": speeds["max"] if speeds else None,
            "speed_3d_kmh": distribution([float(row["vehicle_speed_3d_kmh"]) for row in samples]),
            "recorded_occupancy_peak_kmh": distribution([float(row["vehicle_peak_kmh"]) for row in samples]),
            "handling_cruise_kmh": sorted({float(row["vehicle_handling_cruise_kmh"]) for row in samples
                                            if float(row["vehicle_handling_cruise_kmh"]) >= 0}),
            "transmission_max_kmh": sorted({float(row["vehicle_transmission_max_kmh"]) for row in samples
                                             if float(row["vehicle_transmission_max_kmh"]) >= 0}),
        }
    timings = {}
    for key in ["frame_interval_ms", "cpu_local_ms", "vk_begin_ms", "step_ms",
                "recorder_prepare_ms", "runtime_cpu_ms", "runtime_gpu_ms", "vulkan_gpu_ms",
                "game_ms", "streaming_ms", "script_ms", "population_ms", "car_ms",
                "world_process_ms", "world_list_ms", "world_render_ms", "render_fading_ms",
                "vk_begin_pipeline_ms", "vk_begin_fence_wait_ms", "vk_begin_timestamp_ms",
                "vk_begin_retirement_ms", "vk_begin_setup_ms", "vk_begin_wrist_ms",
                "vk_begin_backend_total_ms"]:
        values = []
        if key not in rows[0]:
            timings[key] = None
            continue
        phase_keys = ["game", "streaming", "population", "car", "audio", "world_list",
                      "pre_render", "scene_setup", "world_render", "ui", "particle_update",
                      "particle_render", "script", "pre_world_misc", "world_process",
                      "post_world_camera_fx", "render_sky", "render_roads", "render_reflections",
                      "render_world", "render_water", "render_fading", "render_weather", "render_effects"]
        phase_name = key.removesuffix("_ms")
        for row in selected:
            if key.startswith("vk_begin_") and key != "vk_begin_ms" and row.get("vk_begin_detail_valid") != "1":
                continue
            if key == "frame_interval_ms" and int(row["frame_id"]) not in interval_ids:
                continue
            if phase_name in phase_keys and not int(row["phase_valid_mask"]) & (1 << phase_keys.index(phase_name)):
                continue
            values.append(float(row[key]))
        timings[key] = distribution(values)
    begin_parts = ["vk_begin_pipeline_ms", "vk_begin_fence_wait_ms", "vk_begin_timestamp_ms",
                   "vk_begin_retirement_ms", "vk_begin_setup_ms", "vk_begin_wrist_ms"]
    begin_stalls = []
    for row in sorted(selected, key=lambda item: float(item["vk_begin_ms"]), reverse=True):
        if row.get("vk_begin_detail_valid") != "1" or float(row["vk_begin_ms"]) < 5:
            continue
        parts = {key: float(row[key]) for key in begin_parts}
        parts["vk_begin_outside_backend_ms"] = max(0.0,
            float(row["vk_begin_ms"])-float(row["vk_begin_backend_total_ms"]))
        begin_stalls.append({
            "frame_id": int(row["frame_id"]),
            "elapsed_s": (int(row["start_monotonic_ns"])-starts[0])/1e9,
            "outer_begin_ms": float(row["vk_begin_ms"]), "parts_ms": parts,
            "largest_recorded_section": max(parts, key=parts.get),
            "counts": {key: int(row[key]) for key in ["vk_pipeline_requests", "vk_pipelines_created",
                "vk_pipeline_failures", "vk_retired_buffers", "vk_retired_images",
                "vk_retired_descriptors", "vk_upload_commands", "vk_framebuffer_created",
                "vk_last_pipeline_key"]},
        })
        if len(begin_stalls) == 10:
            break
    return {
        "file": str(path.resolve()), "metadata": metadata, "complete": complete,
        "rows": len(rows), "selected_rows": len(selected), "partial_rows_ignored": partial_rows,
        "selection": "all" if include_nongameplay else "player present, unpaused, outside cutscene, alive",
        "span_seconds": (starts[-1] - starts[0]) / 1e9,
        "missing_ids_between_rows": sum(b-a-1 for a, b in zip(ids, ids[1:])),
        "dropped_rows": int(metadata.get("dropped_rows", rows[-1]["dropped_rows"])),
        "activity_frames": dict(Counter(row["activity"] for row in selected)),
        "vehicle_speeds_by_model": vehicle_summary,
        "recorded_configuration": {key: sorted({row[key] for row in selected}) for key in
            ["active_world", "active_vegetation", "active_vehicles", "active_peds", "active_weapons",
             "render_width", "render_height", "render_scale_effective", "refresh_hz",
             "profiler_display_mode"] if key in rows[0]},
        "events_all_frames": dict(events), "timings_ms": timings,
        "gpu_pass_profile": summarize_gpu_passes(rows, selected_ids, starts[0], int(metadata.get("gpu_pass_interval_frames", "8")), metadata.get("gpu_pass_sampling", "fixed")),
        "vk_begin_stalls_over_5ms_top10": begin_stalls,
        "notes": [
            "GPU samples are latest asynchronous observations, not attributed to the row's CPU frame.",
            "Frame intervals include pacing; they are not compositor missed-frame counts.",
            "Drops, canceled frames and selection gaps limit tail comparisons; inspect them before comparing.",
            "Recorder preparation excludes queue submission and disk-worker contention; measure total overhead separately.",
            "Begin subphase timers are synchronous CPU wall times; waits/preemption can contribute to any section.",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--include-nongameplay", action="store_true")
    parser.add_argument("--allow-incomplete", action="store_true")
    args = parser.parse_args()
    try:
        results = [summarize(path, args.include_nongameplay, args.allow_incomplete) for path in args.captures]
        print(json.dumps(results, indent=2, allow_nan=False))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
