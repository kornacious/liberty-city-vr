#include "../../overlay/src/skel/android/QuestPerfCapture.h"
#include "../../librw/src/vulkan/GpuPassProfile.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

static std::vector<std::string> Cells(const std::string &line)
{
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string cell;
    while(std::getline(input, cell, ',')) result.push_back(cell);
    return result;
}
int main()
{
    using namespace questperf;
    // A complete full/minimal switching sequence has one recording session.
    bool recording = false;
    int starts = 0, stops = 0;
    for(int mode : {0, 1, 2, 1, 2, 0}) {
        bool next = RecordsDisplayMode(mode);
        if(next && !recording) ++starts;
        if(!next && recording) ++stops;
        recording = next;
    }
    assert(starts == 1 && stops == 1);
    assert(!RecordsDisplayMode(-1) && !RecordsDisplayMode(3));
    assert(SpeedKmh(0) == 0 && SpeedKmh(1) == 180);
    assert(std::fabs(SpeedKmh(100.0f/180.0f)-100.0f) < 0.0001f);
    assert(SpeedKmh(-1) == -1);
    assert(SpeedKmh(std::numeric_limits<float>::quiet_NaN()) == -1);
    assert(SpeedKmh(std::numeric_limits<float>::infinity()) == -1);
    int carA = 0, carB = 0;
    VehicleSpeedPeak peak;
    assert(peak.Update(&carA, 0) == 0);
    assert(peak.Update(&carA, 180) == 180);
    assert(peak.Update(&carA, 60) == 180);
    assert(peak.Update(&carB, 100) == 100); // a different car, even the same model
    assert(peak.Update(nullptr, -1) == -1);
    assert(peak.Update(&carA, 50) == 50); // re-entering starts a new occupancy
    assert(peak.Update(&carA, -1) == 50);
    PlayerEvents tracker;
    Player p;
    assert(tracker.Update(p, 0) == 0);
    p.valid = 1; p.health = 100; p.activity = Walking;
    assert(tracker.Update(p, 0) == PlayerAppeared);
    p.armour = 20; tracker.Update(p, 0);
    p.armour = 10;
    assert(tracker.Update(p, 2) == (Damage|WeaponUsed));
    p.health = 0; p.dead = 1;
    assert(tracker.Update(p, 0) == (Damage|Died));
    p.valid = 0;
    assert(tracker.Update(p, 0) == PlayerDisappeared);
    p.valid = 1; p.health = 100; p.dead = 0;
    assert(tracker.Update(p, 0) == (PlayerAppeared|Recovered));
    assert(tracker.Update(p, 0) == 0);
    p.inVehicle = 1;
    assert(tracker.Update(p, 0) == EnteredVehicle);
    p.inVehicle = 0; p.x = 51;
    assert(tracker.Update(p, 0) == (LeftVehicle|Relocated));
    PlayerEvents alreadyDead; p.dead = 1;
    assert(alreadyDead.Update(p, 0) == 0); // capture start is not a new death
    assert(std::string(ActivityName(Walking)) == "walking");
    assert(std::string(ActivityName(999)) == "unavailable");

    const auto root = std::filesystem::temp_directory_path()/
        ("lcvr-perf-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    const std::string stem = (root/"session").string();
    Writer writer;
    assert(!writer.Start((root/"missing"/"session").string(), ""));
    assert(writer.Start(stem, "active_model_set,CLASSIC\n"));
    Frame frame;
    static_assert(GpuPassCount==GpuPassProfile::Count,"writer/backend GPU passes");
    for(unsigned i=0;i<GpuPassCount;++i)assert(std::string(GpuPassName(i))==GpuPassProfile::Name(i));
    frame.player = p; frame.cpuMs = 0.123456; frame.phases[0] = 0.012345f;
    frame.player.vehicleSpeedKmh = 100;
    frame.player.vehicleSpeed3dKmh = 105;
    frame.player.vehiclePeakKmh = 120;
    frame.player.vehicleHandlingCruiseKmh = 160;
    frame.player.vehicleTransmissionMaxKmh = 192;
    frame.profilerDisplayMode = 2;
    frame.begin.valid = 1; frame.begin.pipelineMs = 39.125f;
    frame.begin.fenceWaitMs = 0.125f; frame.begin.retirementMs = 0.5f;
    frame.begin.totalMs = 40; frame.begin.pipelinesCreated = 2;
    frame.begin.retiredImages = 7; frame.begin.lastPipelineKey = 123456;
    frame.phaseMask = 1; frame.runtimeGpuMs = -1; frame.events = Damage;
    frame.gpu.frameId=1;frame.gpu.startNs=10000000;frame.gpu.status=GpuPassProfile::Complete;
    frame.gpu.mask=1u<<GpuPassProfile::Scene;frame.gpu.frameMs=4.125f;
    frame.gpu.ms[GpuPassProfile::Scene]=1.234567f;frame.gpu.draws[GpuPassProfile::Fading]=123;
    DrawProfile::Recording detail;
    detail.Begin(DrawProfile::Entity(10,0,"test_model",DrawProfile::Distance));
    for(unsigned i=0;i<123;++i)detail.Draw(GpuPassProfile::Fading,12,3,
        i%2?DrawProfile::Extra|DrawProfile::Ps2:DrawProfile::Mesh|DrawProfile::Ps2);
    detail.End();
    frame.gpu.geometry=std::make_shared<const DrawProfile::Stats>(detail.stats);
    unsigned accepted = 0;
    constexpr unsigned attempts = 20000;
    for(unsigned i = 1; i <= attempts; ++i) {
        frame.id = i; frame.startNs = uint64_t(i)*10000000;
        if(writer.Submit(frame)) ++accepted;
    }
    writer.Stop(); writer.Stop();
    frame.gpu.geometry.reset(); // Queued immutable snapshots owned their data.
    assert(accepted > 0);
    assert(!writer.Start(stem, "overwrite_attempt,1\n"));
    std::ifstream input(stem+".frames.csv");
    std::string line;
    assert(bool(std::getline(input, line)));
    const auto header = Cells(line);
    auto column = [&header](const char *name) {
        auto found = std::find(header.begin(), header.end(), name);
        assert(found != header.end());
        return static_cast<size_t>(found-header.begin());
    };
    uint64_t previous = 0;
    unsigned rows = 0;
    while(std::getline(input, line)) {
        const auto row = Cells(line);
        assert(row.size() == header.size());
        const auto id = std::stoull(row[0]);
        assert(id > previous); previous = id;
        assert(std::stod(row[3]) == 0.123456);
        assert(std::stod(row[11]) == -1); // unknown GPU stays unknown
        assert(std::stod(row[16]) > 0.01234 && std::stod(row[16]) < 0.01235);
        assert(std::stod(row[column("vehicle_speed_kmh")]) == 100);
        assert(std::stod(row[column("vehicle_speed_3d_kmh")]) == 105);
        assert(std::stod(row[column("vehicle_peak_kmh")]) == 120);
        assert(std::stod(row[column("vehicle_handling_cruise_kmh")]) == 160);
        assert(std::stod(row[column("vehicle_transmission_max_kmh")]) == 192);
        assert(std::stoi(row[column("profiler_display_mode")]) == 2);
        assert(std::stoi(row[column("vk_begin_detail_valid")]) == 1);
        assert(std::stod(row[column("vk_begin_pipeline_ms")]) == 39.125);
        assert(std::stod(row[column("vk_begin_fence_wait_ms")]) == 0.125);
        assert(std::stod(row[column("vk_begin_retirement_ms")]) == 0.5);
        assert(std::stod(row[column("vk_begin_backend_total_ms")]) == 40);
        assert(std::stoi(row[column("vk_pipelines_created")]) == 2);
        assert(std::stoi(row[column("vk_retired_images")]) == 7);
        assert(std::stoull(row[column("vk_last_pipeline_key")]) == 123456);
        assert(std::stoull(row[column("gpu_pass_frame_id")])==1);
        assert(std::stoull(row[column("gpu_pass_start_monotonic_ns")])==10000000);
        assert(std::stoi(row[column("gpu_pass_status")])==1);
        assert(std::stoul(row[column("gpu_pass_valid_mask")])==(1u<<GpuPassProfile::Scene));
        assert(std::stod(row[column("gpu_pass_frame_ms")])==4.125);
        assert(std::fabs(std::stod(row[column("gpu_pass_scene_ms")])-1.234567)<.000001);
        assert(std::stoi(row[column("gpu_pass_fading_draw_calls")])==123);
        assert(std::stod(row[column("gpu_pass_sky_ms")])==-1);
        assert(std::stod(row[column("gpu_pass_fading_ms")])==-1);
        ++rows;
    }
    assert(rows == accepted);
    std::ifstream metadata(stem+".session.csv");
    std::string all((std::istreambuf_iterator<char>(metadata)), std::istreambuf_iterator<char>());
    assert(all.find("active_model_set,CLASSIC\n") != std::string::npos);
    assert(all.find("schema,6\n") != std::string::npos);
    assert(all.find("gpu_pass_interval_frames,8\n")!=std::string::npos);
    assert(all.find("gpu_pass_scope,outside_render_pass_bottom_of_pipe_intervals\n")!=std::string::npos);
    assert(all.find("written_rows,"+std::to_string(rows)+"\n") != std::string::npos);
    assert(all.find("dropped_rows,"+std::to_string(attempts-accepted)+"\n") != std::string::npos);
    assert(all.find("closed,1\n") != std::string::npos);
    assert(all.find("draw_samples,1\n") != std::string::npos);
    assert(all.find("draw_rows,15\n") != std::string::npos); // 14 phases + one model; no repeated payloads.
    std::ifstream drawFile(stem+".draws.csv");
    assert(bool(std::getline(drawFile,line)));
    const auto drawHeader=Cells(line);
    auto drawColumn=[&](const char *name){return size_t(std::find(drawHeader.begin(),drawHeader.end(),name)-drawHeader.begin());};
    unsigned detailRows=0;
    while(std::getline(drawFile,line)){
        const auto cells=Cells(line);assert(cells.size()==drawHeader.size());++detailRows;
        assert(cells[0]=="1"&&cells[1]=="10000000");
        if(cells[2]=="model"){
            assert(cells[3]=="fading"&&cells[4]=="10"&&cells[6]=="test_model");
            assert(std::stoul(cells[drawColumn("draws")])==123);
            assert(std::stoul(cells[drawColumn("triangles")])==492);
            assert(std::stoul(cells[drawColumn("mesh_passes")])==62);
            assert(std::stoul(cells[drawColumn("extra_split_draws")])==61);
            assert(std::stoul(cells[drawColumn("distance_fade_draws")])==123);
        }
    }
    assert(detailRows==15);
    assert(all.find("write_error,0\n") != std::string::npos);
    assert(all.find("overwrite_attempt") == std::string::npos);
    std::printf("PASS: player transitions, raw timing precision, CSV schema, bounded writer accounting, shutdown drain and exclusive sessions (%u rows).\n", rows);
}
