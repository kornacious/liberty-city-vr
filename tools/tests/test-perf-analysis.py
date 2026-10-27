import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("perf_analysis", Path(__file__).parents[1]/"analyze-perf-capture.py")
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


class AnalysisTests(unittest.TestCase):
    def test_gpu_pass_source_matching_dedup_and_validity(self):
        rows = []
        for i in range(1, 41):
            row = dict(frame_id=str(i), start_monotonic_ns=str(i*10000000),
                       gpu_pass_frame_id="0", gpu_pass_start_monotonic_ns="0",
                       gpu_pass_status="0", gpu_pass_valid_mask="0", gpu_pass_frame_ms="-1")
            for name in analysis.GPU_PASSES:
                row[f"gpu_pass_{name}_ms"] = "-1"
                row[f"gpu_pass_{name}_draw_calls"] = "0"
            rows.append(row)

        def sample(readback, source, status=1):
            rows[readback-1].update(gpu_pass_frame_id=str(source), gpu_pass_start_monotonic_ns=str(source*10000000),
                                    gpu_pass_status=str(status), gpu_pass_valid_mask=str(1 << 5), gpu_pass_frame_ms="4.5",
                                    gpu_pass_fading_ms="1.25", gpu_pass_fading_draw_calls="120")
        sample(10, 8)
        sample(11, 8)  # Repeated latest value is not a second sample.
        sample(18, 16)  # Exclude by source gameplay state, even if readback is active.
        sample(26, 24, 2)  # Overflow is not a zero-duration measurement.
        sample(30, 99)  # Dropped/missing source cannot be aligned safely.
        sample(34, 32)
        rows[33]["gpu_pass_start_monotonic_ns"] = "1"  # Stale/cross-session identity.
        selected = set(range(1, 41))-{10, 11, 16}  # Readback rows 10/11 are paused; source 8 is valid.
        result = analysis.summarize_gpu_passes(rows, selected, 10000000)
        self.assertTrue(result["available"])
        self.assertEqual(result["unique_selected_samples"], 1)
        self.assertEqual(result["repeated_observations_ignored"], 1)
        self.assertEqual(result["excluded_nongameplay_sources"], 1)
        self.assertEqual(result["unmatched_source_frames"], 1)
        self.assertEqual(result["mismatched_source_frames"], 1)
        self.assertEqual(result["unobserved_source_frames"], 1)  # Pending tail source 40.
        self.assertEqual(result["passes"]["fading"]["gpu_ms"]["mean"], 1.25)
        self.assertEqual(result["passes"]["fading"]["draw_calls"]["mean"], 120)
        self.assertIsNone(result["passes"]["sky"]["gpu_ms"])
        self.assertEqual(result["slow_5s_windows"][0]["source_elapsed_s"], 0)
        self.assertIsNone(analysis.summarize_gpu_passes([{"frame_id": "1"}], {1}, 0))
        rotating = analysis.summarize_gpu_passes(rows, set(range(1,41)), 10000000, sampling="rotating_phase_one_per_block")
        self.assertEqual(rotating["expected_source_frames"], 5) # Sources 1,10,19,28,37.

    def test_percentiles_and_missing(self):
        self.assertIsNone(analysis.distribution([-1, float("nan")]))
        stats = analysis.distribution([1, 2, 3, 4])
        self.assertEqual(stats["median"], 2.5)
        self.assertAlmostEqual(stats["p95"], 3.85)

    def test_closed_capture_filter_and_provenance(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"run.frames.csv"
            fields = ["frame_id", "start_monotonic_ns", "player_valid", "paused", "cutscene", "dead",
                      "event_mask", "dropped_rows", "activity", "phase_valid_mask"]
            timings = ["frame_interval_ms", "cpu_local_ms", "vk_begin_ms", "step_ms", "recorder_prepare_ms",
                       "runtime_cpu_ms", "runtime_gpu_ms", "vulkan_gpu_ms", "game_ms", "streaming_ms",
                       "script_ms", "population_ms", "car_ms", "world_process_ms", "world_list_ms",
                       "world_render_ms", "render_fading_ms"]
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=fields+timings)
                writer.writeheader()
                for i in range(1, 5):
                    row = dict.fromkeys(timings, "1.25")
                    row.update(frame_id=i, start_monotonic_ns=i*10000000, player_valid=1,
                               paused=int(i == 4), cutscene=0, dead=0, event_mask=1 if i == 2 else 0,
                               dropped_rows=0, activity="walking", phase_valid_mask=1,
                               runtime_gpu_ms=-1)
                    writer.writerow(row)
            session = path.with_name("run.session.csv")
            session.write_text("key,value\nschema,1\nactive_model_set,CLASSIC\nwritten_rows,4\ndropped_rows,0\nwrite_error,0\nclosed,1\n")
            result = analysis.summarize(path)
            self.assertEqual(result["rows"], 4)
            self.assertEqual(result["selected_rows"], 3)
            self.assertEqual(result["metadata"]["active_model_set"], "CLASSIC")
            self.assertEqual(result["events_all_frames"]["damage"], 1)
            self.assertIsNone(result["timings_ms"]["streaming_ms"])
            self.assertIsNone(result["timings_ms"]["runtime_gpu_ms"])
            self.assertEqual(result["timings_ms"]["game_ms"]["samples"], 3)
            self.assertEqual(analysis.summarize(path, True)["selected_rows"], 4)
            self.assertEqual(result["vehicle_speeds_by_model"], {}) # original schema 1
            with path.open() as stream:
                vehicle_rows = list(csv.DictReader(stream))
            for i, row in enumerate(vehicle_rows):
                row.update(in_vehicle="1", vehicle_model="119", vehicle_speed_kmh=str(90+i*10),
                           vehicle_speed_3d_kmh=str(95+i*10), vehicle_peak_kmh="110",
                           vehicle_handling_cruise_kmh="160", vehicle_transmission_max_kmh="192")
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(vehicle_rows[0]))
                writer.writeheader()
                writer.writerows(vehicle_rows)
            session.write_text(session.read_text().replace("schema,1", "schema,2"))
            vehicle = analysis.summarize(path)["vehicle_speeds_by_model"]["119"]
            self.assertEqual(vehicle["speed_kmh"]["max"], 110) # paused sample excluded
            self.assertEqual(vehicle["observed_top_kmh"], 110)
            self.assertEqual(vehicle["recorded_occupancy_peak_kmh"]["max"], 110)
            self.assertEqual(vehicle["handling_cruise_kmh"], [160])
            self.assertEqual(vehicle["transmission_max_kmh"], [192])
            for row in vehicle_rows:
                row.update(vk_begin_detail_valid="1", vk_begin_ms="40",
                           vk_begin_pipeline_ms="39", vk_begin_fence_wait_ms="0.1",
                           vk_begin_timestamp_ms="0.1", vk_begin_retirement_ms="0.2",
                           vk_begin_setup_ms="0.4", vk_begin_wrist_ms="0.2",
                           vk_begin_backend_total_ms="40", vk_pipeline_requests="1",
                           vk_pipelines_created="1", vk_pipeline_failures="0", vk_retired_buffers="2",
                           vk_retired_images="3", vk_retired_descriptors="4", vk_upload_commands="5",
                           vk_framebuffer_created="0", vk_last_pipeline_key="12345")
            vehicle_rows[0]["vk_begin_detail_valid"] = "0" # stale/unavailable sections excluded
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(vehicle_rows[0]))
                writer.writeheader()
                writer.writerows(vehicle_rows)
            session.write_text(session.read_text().replace("schema,2", "schema,3"))
            result = analysis.summarize(path)
            self.assertEqual(result["timings_ms"]["vk_begin_pipeline_ms"]["samples"], 2)
            self.assertEqual(len(result["vk_begin_stalls_over_5ms_top10"]), 2)
            stall = result["vk_begin_stalls_over_5ms_top10"][0]
            self.assertEqual(stall["largest_recorded_section"], "vk_begin_pipeline_ms")
            self.assertEqual(stall["counts"]["vk_pipelines_created"], 1)
            session.write_text(session.read_text().replace("schema,3", "schema,4"))
            self.assertIsNone(analysis.summarize(path)["gpu_pass_profile"]) # Absent fields never become zero.
            session.write_text("key,value\nschema,1\n")
            with self.assertRaises(ValueError):
                analysis.summarize(path)
            self.assertFalse(analysis.summarize(path, allow_incomplete=True)["complete"])


if __name__ == "__main__":
    unittest.main()
