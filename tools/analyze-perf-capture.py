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
              "effects", "ui", "scope", "wrist", "resolve_postprocess", "other", "scene"]
DRAW_COUNTERS = ["draws", "elements", "triangles", "geometry_known_draws", "mesh_passes",
                 "extra_split_draws", "alpha_requested_draws", "texture_alpha_draws",
                 "vertex_alpha_draws", "material_alpha_draws", "alpha_test_draws",
                 "depth_write_draws", "ps2_split_draws", "vehicle_split_draws",
                 "distance_fade_draws", "skinned_draws"]


ATOMIC_SUBMISSION_COLUMNS = ["vk_static_atomic_calls", "vk_static_atomic_pipeline_requests",
    "vk_static_atomic_pipeline_binds", "vk_static_atomic_texture_requests",
    "vk_static_atomic_texture_binds", "vk_static_atomic_texture_lookups"]


def summarize_atomic_submission(rows):
    if not rows or any(key not in rows[0] for key in ATOMIC_SUBMISSION_COLUMNS):
        return None
    totals = Counter()
    valid = invalid = 0
    for row in rows:
        if row.get("vk_begin_detail_valid") != "1":
            continue
        values = {key: int(row[key]) for key in ATOMIC_SUBMISSION_COLUMNS}
        p, pb, t, tb, lookups = (values[key] for key in ATOMIC_SUBMISSION_COLUMNS[1:])
        if min(values.values()) < 0 or pb > p or tb > t or lookups > t or p != t:
            invalid += 1
            continue
        valid += 1
        totals.update(values)
    if not valid:
        return {"valid_frames": 0, "invalid_frames": invalid}
    p, pb, t, tb, lookups = (totals[key] for key in ATOMIC_SUBMISSION_COLUMNS[1:])
    return {"valid_frames": valid, "invalid_frames": invalid, "totals": dict(totals),
        "mean_per_frame": {key: value/valid for key, value in totals.items()},
        "pipeline_binds_skipped": p-pb, "texture_binds_skipped": t-tb,
        "texture_lookups_avoided": t-lookups,
        "pipeline_skip_percent": 100*(p-pb)/p if p else None,
        "texture_skip_percent": 100*(t-tb)/t if t else None}


def summarize_alpha_bound(rows):
    keys = ["masked_alpha_bound_candidates", "masked_alpha_bound_solid_passes_skipped",
            "masked_alpha_bound_indices_skipped"]
    if not rows or any(key not in rows[0] for key in keys):
        return None
    valid, invalid = [], 0
    for row in rows:
        values = [int(row[key]) for key in keys]
        if min(values) < 0 or values[1] > values[0]:
            invalid += 1
        else:
            valid.append(values)
    return {"valid_frames": len(valid), "invalid_frames": invalid,
            "frames_with_skips": sum(v[1] > 0 for v in valid),
            "totals": {key: sum(v[i] for v in valid) for i, key in enumerate(keys)},
            "per_frame": {key: distribution([v[i] for v in valid]) for i, key in enumerate(keys)},
            "note": "Conservative empty solid-pass decisions; indices are not multiplied by eye count. Not measured GPU savings."}


def summarize_opaque_order(rows):
    keys = ["opaque_order_candidates", "opaque_order_eligible", "opaque_order_swaps"]
    if not rows or any(key not in rows[0] for key in keys):
        return None
    rejection_keys = [key for key in rows[0] if key.startswith("opaque_order_reject_")]
    totals = {key: sum(int(row[key]) for row in rows) for key in rejection_keys}
    accounted = (all(int(row["opaque_order_candidates"]) == int(row["opaque_order_eligible"]) +
                     sum(int(row[key]) for key in rejection_keys) for row in rows)
                 if rejection_keys else None)
    return {"per_frame": {key: distribution([float(row[key]) for row in rows]) for key in keys},
            "rejection_totals": totals, "candidate_accounting_valid": accounted,
            "frames_with_swaps": sum(int(row["opaque_order_swaps"]) > 0 for row in rows),
            "note": "Swaps are adjacent exchanges within groups of at most 32 visible slots; not draw-call savings."}


def summarize_draw_details(path, rows, selected_ids, metadata):
    companion = path.with_name(path.name.replace(".frames.csv", ".draws.csv"))
    if not companion.is_file():
        return {"available": False, "reason": "Missing .draws.csv companion", "file": str(companion)}
    sources = {int(r["frame_id"]): r for r in rows}
    gpu = {}
    for r in rows:
        source = int(r.get("gpu_pass_frame_id", 0))
        if (r.get("gpu_pass_status") == "1" and source in sources and source <= int(r["frame_id"])
                and r["gpu_pass_start_monotonic_ns"] == sources[source]["start_monotonic_ns"]
                and math.isfinite(float(r["gpu_pass_frame_ms"])) and float(r["gpu_pass_frame_ms"]) >= 0):
            gpu[source] = r
    accepted, seen = set(), set()
    phases, models, windows = {}, {}, {}
    invalid = excluded = unmatched = count_rows = overflow = contexts = unattributed = 0
    peak_models = peak_capacity = growths = allocation_failures = overflow_samples = 0
    omitted_by_phase = Counter()
    start_ns = int(rows[0]["start_monotonic_ns"])

    def accumulate(target, key, values):
        target.setdefault(key, Counter()).update(values)

    def consume(group):
        nonlocal invalid, excluded, unmatched, overflow, contexts, unattributed
        nonlocal peak_models, peak_capacity, growths, allocation_failures, overflow_samples
        try:
            source = int(group[0]["source_frame_id"])
            if source in seen:
                raise ValueError("Repeated source group")
            seen.add(source)
            if source not in sources or source not in gpu:
                unmatched += 1
                return
            if any(int(r["source_start_ns"]) != int(sources[source]["start_monotonic_ns"]) for r in group):
                raise ValueError("Source timestamp mismatch")
            if source not in selected_ids:
                excluded += 1
                return
            totals, detail, keys = {}, [], set()
            lost = int(group[0]["model_overflow_draws"])
            context = int(group[0]["context_overflows"])
            if lost < 0 or context < 0:
                raise ValueError("Negative loss counts")
            for r in group:
                phase = r["phase"]
                values = {k: int(r[k]) for k in DRAW_COUNTERS}
                if phase not in GPU_PASSES or any(v < 0 for v in values.values()):
                    raise ValueError("Invalid counters")
                if int(r["model_overflow_draws"]) != lost or int(r["context_overflows"]) != context:
                    raise ValueError("Inconsistent source loss counts")
                if r["kind"] == "phase":
                    key = ("phase", phase)
                    totals[phase] = values
                elif r["kind"] == "model":
                    key = (phase, int(r["model_id"]), int(r["entity_class"]), r["model_name"])
                    detail.append((key, values))
                else:
                    raise ValueError("Invalid record kind")
                if key in keys:
                    raise ValueError("Repeated source bucket")
                keys.add(key)
            if set(totals) != set(GPU_PASSES):
                raise ValueError("Incomplete phase group")
            sums = {phase: Counter() for phase in GPU_PASSES}
            for key, values in detail:
                sums[key[0]].update(values)
            for phase in GPU_PASSES:
                if totals[phase]["draws"] != int(gpu[source][f"gpu_pass_{phase}_draw_calls"]):
                    raise ValueError("Draw counts disagree with GPU payload")
                if any(sums[phase][k] > totals[phase][k] for k in DRAW_COUNTERS):
                    raise ValueError("Model counters exceed phase totals")
                if not lost and any(sums[phase][k] != totals[phase][k] for k in DRAW_COUNTERS):
                    raise ValueError("Unaccounted model counters")
            if sum(v["draws"] for v in totals.values())-sum(v["draws"] for _, v in detail) != lost:
                raise ValueError("Overflow does not reconcile")
            capacity_fields = ["model_buckets", "model_capacity", "model_table_growths", "model_allocation_failures"]
            capacity_info = None
            if any(k in group[0] for k in capacity_fields):
                capacity_info = tuple(int(group[0][k]) for k in capacity_fields)
                buckets, capacity, expanded, failures = capacity_info
                if min(capacity_info) < 0 or buckets != len(detail) or capacity < buckets:
                    raise ValueError("Invalid model capacity accounting")
                if any(tuple(int(r[k]) for k in capacity_fields) != capacity_info for r in group):
                    raise ValueError("Inconsistent capacity accounting")
            second = int(((int(sources[source]["start_monotonic_ns"])-start_ns)/1e9)//5)*5
            window = windows.setdefault(second, {"samples": 0, "gpu_sum": 0, "phases": {}, "models": {},
                                                "model_overflow_draws": 0, "peak_model_buckets": 0})
            window["samples"] += 1
            window["gpu_sum"] += float(gpu[source]["gpu_pass_frame_ms"])
            window["model_overflow_draws"] += lost
            window["peak_model_buckets"] = max(window["peak_model_buckets"], len(detail))
            for phase, values in totals.items():
                accumulate(phases, phase, values)
                accumulate(window["phases"], phase, values)
                omitted_by_phase[phase] += values["draws"]-sums[phase]["draws"]
            for key, values in detail:
                accumulate(models, key, values)
                accumulate(window["models"], key, values)
                if key[1] < 0:
                    unattributed += values["draws"]
            overflow += lost
            contexts += context
            overflow_samples += int(lost > 0)
            peak_models = max(peak_models, len(detail))
            if capacity_info:
                peak_capacity = max(peak_capacity, capacity)
                growths += expanded
                allocation_failures += failures
            accepted.add(source)
        except (KeyError, ValueError, TypeError):
            invalid += 1

    with companion.open(newline="", encoding="utf-8") as stream:
        group, identity = [], None
        for row in csv.DictReader(stream):
            count_rows += 1
            next_id = row.get("source_frame_id")
            if group and next_id != identity:
                consume(group)
                group = []
            identity = next_id
            group.append(row)
        if group:
            consume(group)

    def phase_summary(data, n):
        return {phase: {"totals": dict(values), "mean_per_sample": {k: v/n for k, v in values.items()}}
                for phase, values in data.items()} if n else {}

    def model_summary(data, n, rank="draws"):
        return [{"phase": key[0], "model_id": key[1], "entity_class": key[2], "name": key[3],
                 "totals": dict(values), "mean_draws_per_sample": values["draws"]/n,
                 "mean_triangles_per_sample": values["triangles"]/n}
                for key, values in sorted(data.items(), key=lambda item: item[1][rank], reverse=True)[:20]] if n else []

    n = len(accepted)
    total_draws = sum(v["draws"] for v in phases.values())
    return {"available": bool(n), "file": str(companion.resolve()), "samples": n,
            "rows": count_rows, "row_count_matches_metadata": count_rows == int(metadata.get("draw_rows", count_rows)),
            "invalid_source_groups": invalid, "unmatched_sources": unmatched, "excluded_nongameplay_sources": excluded,
            "missing_gpu_sources": len((set(gpu) & selected_ids)-accepted),
            "model_overflow_draws": overflow, "context_overflows": contexts,
            "model_rankings_truncated": bool(overflow), "overflow_samples": overflow_samples,
            "peak_model_buckets": peak_models, "peak_model_capacity": peak_capacity or None,
            "model_table_growths": growths, "model_allocation_failures": allocation_failures,
            "omitted_model_draws_by_phase": dict(omitted_by_phase),
            "unattributed_draw_fraction": unattributed/total_draws if total_draws else None,
            "attributed_draw_fraction": (total_draws-unattributed-overflow)/total_draws if total_draws else None,
            "phases": phase_summary(phases, n), "top_models_by_draws": model_summary(models, n),
            "top_models_by_triangles": model_summary(models, n, "triangles"),
            "slow_5s_windows": [{"source_elapsed_s": second, "samples": w["samples"],
                "gpu_frame_mean_ms": w["gpu_sum"]/w["samples"],
                "model_overflow_draws": w["model_overflow_draws"], "peak_model_buckets": w["peak_model_buckets"],
                "phases": phase_summary(w["phases"], w["samples"]),
                "top_models_by_draws": model_summary(w["models"], w["samples"]),
                "top_models_by_triangles": model_summary(w["models"], w["samples"], "triangles")}
                for second, w in sorted(windows.items(), key=lambda item: item[1]["gpu_sum"]/item[1]["samples"], reverse=True)[:10]],
            "notes": ["Triangles count submitted primitives, including split passes; strip/fan degenerates are not removed.",
                      "Multiview draw and triangle counts are not multiplied by eye count.",
                      "Reasons overlap and indicate submitted features, not pixel coverage or exclusive GPU cost.",
                      "Model scope includes descendants rendered inside the entity; unscopeable draws are explicitly unattributed.",
                      "Model histogram overflow leaves phase totals intact and limits model rankings."]}


def summarize_gpu_passes(rows, selected_ids, start_ns, interval=8, sampling="fixed", timing_scope="legacy_in_render_pass"):
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
            "draw_calls": distribution([int(row[f"gpu_pass_{name}_draw_calls"])
                                        for _, _, row, _ in samples
                                        if f"gpu_pass_{name}_draw_calls" in row]),
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
        "sample_interval_frames": interval, "sampling": sampling,
        "timing_scope": timing_scope,
        "timing_attribution_reliable": timing_scope == "outside_render_pass_bottom_of_pipe_intervals",
        "expected_source_frames": len(expected),
        "unobserved_source_frames": len(expected-seen), "unique_status_counts": dict(statuses),
        "repeated_observations_ignored": repeated, "unmatched_source_frames": unmatched,
        "mismatched_source_frames": mismatched, "excluded_nongameplay_sources": excluded,
        "gpu_frame_ms": distribution([float(row["gpu_pass_frame_ms"]) for _, _, row, _ in samples]),
        "passes": passes,
        "slow_5s_windows": sorted(windows, key=lambda item: item["gpu_frame_mean_ms"], reverse=True)[:10],
        "notes": (["Legacy in-render-pass timestamps cannot reliably attribute scene cost on tiled GPUs."]
                  if timing_scope != "outside_render_pass_bottom_of_pipe_intervals" else
                  ["Only whole render passes are GPU-timed; logical scene phases have draw counts and CPU timings only."]) +
                 ["Intervals are approximate primary-lane Vulkan timestamps, not exclusive shader costs.",
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
    if metadata.get("schema") not in ("1", "2", "3", "4", "5", "6"):
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
             "profiler_display_mode", "opaque_world_alpha_fast_path_enabled", "opaque_order_experiment_enabled", "replay_forward_view_active"] if key in rows[0]},
        "events_all_frames": dict(events), "timings_ms": timings,
        "atomic_submission": summarize_atomic_submission(selected),
        "opaque_order": summarize_opaque_order(selected),
        "masked_alpha_bound": summarize_alpha_bound(selected),
        "renderdoc_capture": {
            "diagnostic_run": metadata.get("renderdoc_attached_at_start") == "1" or
                ";renderdoc=1;" in metadata.get("simulation_replay", "") or
                any(row.get("renderdoc_capture_active") == "1" for row in rows),
            "active_frame_ids": [int(row["frame_id"]) for row in rows if row.get("renderdoc_capture_active") == "1"],
            "note": "Capture instrumentation and file writing can affect this and neighboring frames; use an unhooked run for benchmarking.",
        } if "renderdoc_capture_active" in rows[0] or ";renderdoc=" in metadata.get("simulation_replay", "") or metadata.get("renderdoc_attached_at_start") == "1" else None,
        "gpu_pass_profile": summarize_gpu_passes(rows, selected_ids, starts[0], int(metadata.get("gpu_pass_interval_frames", "8")), metadata.get("gpu_pass_sampling", "fixed"), metadata.get("gpu_pass_scope", "legacy_in_render_pass")),
        "draw_profile": summarize_draw_details(path, rows, selected_ids, metadata) if metadata.get("schema") == "6" else None,
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
