import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("perf_analysis", Path(__file__).parents[1]/"analyze-perf-capture.py")
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


class AnalysisTests(unittest.TestCase):
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
            session.write_text("key,value\nschema,1\n")
            with self.assertRaises(ValueError):
                analysis.summarize(path)
            self.assertFalse(analysis.summarize(path, allow_incomplete=True)["complete"])


if __name__ == "__main__":
    unittest.main()
