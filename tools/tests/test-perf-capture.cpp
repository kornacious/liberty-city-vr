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
    if(!line.empty() && line.back()==',') result.emplace_back();
    return result;
}
int main()
{
    using namespace questperf;
    ProfilingState state;
    assert(!state.Recording() && !state.Collecting());
    for(int mode : {0, 1, 2, 3, 2, 1, 3, 0}) {
        state.display = mode;
        assert(!state.Recording());
        assert(state.Collecting() == (mode == 1 || mode == 3));
    }
    for(bool active : {false, true}) {
        state.active = active;
        state.replay = 1;
        assert(!state.Recording());
        state.replay = 2;
        for(int mode : {0, 1, 2, 3}) {
            state.display = mode;
            assert(state.Recording() && state.Collecting());
        }
        state.replay = 0;
        assert(state.Recording() == active);
    }
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
    frame.atomicSubmission = {4,20,5,20,7,12};
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
        frame.opaqueWorldAlphaFastPathEnabled = i%2;
        frame.replayForwardViewActive = (i/2)%2;
        frame.renderdocCaptureActive = i==10?1:0;
        frame.opaqueOrderExperimentEnabled=i%2;
        frame.opaqueOrder={40,30,i%7};
        frame.alphaBound={30+i%21,i%21,6000000000ULL+i,i%19,i%17,i%13};
        frame.maskedAlphaPassSkipEnabled=i%2;
        frame.classicFoliageCutoutEnabled=(i/2)%2;
        frame.worldOcclusionEnabled=i%2;frame.worldOcclusion.rejected=19;frame.worldOcclusion.vertices=6000000000ULL+i;frame.worldOcclusion.buildMs=.234567;
        frame.worldOcclusion.rasterRows=125;frame.worldOcclusion.selectedTriangles=219;
        frame.worldOcclusion.cacheBuilds=2;frame.worldOcclusion.cacheHits=27;frame.worldOcclusion.cacheScanned=1000;
        frame.worldOcclusion.cacheDeferred=3;frame.worldOcclusion.sourceObjects=17;
        frame.worldOcclusion.sourceProjected=110;frame.worldOcclusion.sourceProjectionRejected=70;
        frame.worldOcclusion.sourceSmall=5;frame.worldOcclusion.sourceBudgetSkipped=64;
        frame.worldOcclusion.sourceTopSamples=8;frame.worldOcclusion.sourceTopModel=1417;
        frame.fxaaIntegerSamplingEnabled=i%2;frame.fxaaIntegerSamplingActive=i%4==1;
        frame.worldOcclusion.polygonCandidates=312;frame.worldOcclusion.selectedPolygons=256;
        frame.worldOcclusion.selectionDropped=56;frame.worldOcclusion.rasterBudgetSkipped=14;
        frame.worldOcclusion.noDepthPolygons=31;frame.worldOcclusion.edgeTests=21970;
        frame.worldOcclusion.redundancyTests=4700;frame.worldOcclusion.redundancyBudgetStops=1;
        frame.worldOcclusion.redundantPolygons=25;frame.worldOcclusion.redundantEyes=58;
        frame.worldOcclusion.tileSkippedCells=3000;frame.worldOcclusion.depthSkippedCells=1700;
        frame.worldOcclusion.spanRows=170;frame.worldOcclusion.spanCells=2600;
        frame.worldOcclusion.spanFallbackRows=81;frame.worldOcclusion.spanEdgeTests=1200;
        frame.worldOcclusion.buildPhasesValid=true;frame.worldOcclusion.phaseClockReads=810;
        for(unsigned phase=0;phase<WorldOcclusion::BuildPhaseCount;++phase)frame.worldOcclusion.buildPhaseMs[phase]=.001234*(phase+1);
        frame.worldOcclusion.contributionCount=3;frame.worldOcclusion.contributions={};
        frame.worldOcclusion.contributions[0].model=1417;
        frame.worldOcclusion.contributions[0].Record(true,false,0,true);
        frame.worldOcclusion.contributions[1].model=222;
        frame.worldOcclusion.contributions[1].Record(true,false,0,false);
        frame.worldOcclusion.contributions[1].Record(true,false,0,false);
        frame.worldOcclusion.contributions[1].Record(true,true,7,false);
        frame.worldOcclusion.contributions[1].Record(false,true,0,false);
        frame.worldOcclusion.contributions[2].model=333;
        frame.worldOcclusion.contributions[2].Record(true,false,8,false);
        frame.worldOcclusion.clipProfile={20,30,40,5,1,170};
        frame.worldOcclusion.sourceGeometryRejected=2;frame.worldOcclusion.sourceNoOpaque=3;
        frame.worldOcclusion.meshOpaque=11;frame.worldOcclusion.meshAlpha=7;frame.worldOcclusion.meshUnsupported=5;
        frame.worldOcclusion.sourceClipped=13;frame.worldOcclusion.clipTriangles=31;frame.worldOcclusion.queryInvalid=17;
        frame.worldOcclusion.footprintCandidates=90;frame.worldOcclusion.footprintRejected=17;
        frame.worldOcclusion.footprintUncertain=3;frame.worldOcclusion.footprintSelected=32;frame.worldOcclusion.footprintCapacitySkipped=10;
          frame.worldBackfaceEnabled=i%2;frame.worldBackface.draws=7;frame.worldBackface.indices=6000000000ULL+i;frame.worldBackface.certifyMs=.123456;
          frame.lodFadeTransitionEnabled=i%2;frame.worldLodMode=i%3;frame.worldLod={11,12,13,14,15,16,17};
        frame.staticInstancesEnabled=i%2;
        frame.staticInstances={};frame.staticInstances.queued=12;frame.staticInstances.groups=2;
        frame.staticInstances.instances=8;frame.staticInstances.savedDraws=6;
        frame.staticInstances.sourceIndices=frame.staticInstances.emittedIndices=6000000000ULL+i;
        frame.staticReuse={};
        frame.staticReuse.sourceFrame=i;frame.staticReuse.sampled=1;
        frame.staticReuse.candidates=10;frame.staticReuse.compatibleInstances=3;
        frame.staticReuse.topModel=1416;frame.staticReuse.cpuMs=.012345;
        frame.opaqueMeshBatchEnabled=i%2;
        frame.opaqueMeshBatch={100,99,64,62,7,8,9};
        frame.staticArrayBatchEnabled=1;
        frame.staticArrayBatch={};
        frame.staticArrayBatch.shortenedBatches=2;
        frame.staticArrayBatch.crossBatches=3;
        frame.staticArrayBatch.crossSavedDraws=8;
        frame.staticArrayBatch.crossUploadBytes=2048;
        frame.staticArrayBatch.crossFallbacks=1;
        frame.staticArrayBatch.crossFlushMs=.125;
        if(i%2){
            frame.staticArrayBatch.crossFlushBreakdownSampled=true;
            frame.staticArrayBatch.crossLookupMs=.025;
            frame.staticArrayBatch.crossBuildMs=.010;
            frame.staticArrayBatch.crossTransformMs=.015;
            frame.staticArrayBatch.crossRecordMs=.040;
            frame.staticArrayBatch.crossFallbackMs=.035;
        }
        frame.staticArrayBatch.crossCacheHits=9;
        frame.staticArrayBatch.crossCacheMisses=3;
        frame.staticArrayBatch.crossCacheBuilds=2;
        frame.staticArrayBatch.crossCacheBytes=65536;
        frame.staticArrayBatch.crossCacheUploadBytes=8192;
        frame.staticArrayBatch.crossArenaBytes=16777216;
        frame.staticArrayBatch.crossArenaReservedBytes=67584;
        frame.staticArrayBatch.crossArenaMisses=1;
        frame.staticArrayBatch.crossPipelineBinds=4;
        frame.staticArrayBatch.crossDescriptorBinds=5;
        frame.staticArrayBatch.crossTransformBinds=3;
        frame.staticArrayBatch.crossOrdinaryDraws=12;
        frame.staticArrayBatch.crossOrdinaryPipelineBinds=8;
        frame.staticArrayBatch.crossOrdinaryTextureBinds=6;
        frame.staticArrayBatch.demandProbes=5;
        frame.staticArrayBatch.demandReplacements=2;
        frame.staticArrayBatch.demandUploadBytes=32768;
        frame.staticArrayBatch.demandPageDeferrals=42;
        frame.staticArrayBatch.adjacentMisses=7;
        frame.staticArrayBatch.adjacentAdmissions=1;
        frame.staticArrayBatch.adjacentMerged=5;
        frame.staticArrayOccupancy={};
        frame.staticArrayResidency={};
        if(i%2){
            frame.staticArrayOccupancy.sampled=true;
            frame.staticArrayOccupancy.count=2;
            frame.staticArrayOccupancy.pages[0]={64,64,16,8,5,1,131072,2};
            frame.staticArrayOccupancy.pages[1]={64,64,16,2,0,0,32768};
            frame.staticArrayResidency.sampled=true;
            frame.staticArrayResidency.Observe(64,64,true,unsigned(StaticArrayBatch::Result::Ready));
            frame.staticArrayResidency.Observe(64,64,true,unsigned(StaticArrayBatch::Result::Ready));
            frame.staticArrayResidency.Observe(128,128,false,StaticArrayBatch::ResidentOnlyMiss);
            frame.staticArrayBatch.duplicateMergedLayers=3;
        }
        frame.staticArrayBatch.pageSizeRetries=1;
        frame.staticArrayBatch.pageMemoryCacheHits=4;
        frame.staticArrayBatch.Fallback(StaticArrayBatch::Result::PreferredPageFull);
        frame.staticArrayBatch.Fallback(StaticArrayBatch::Result::PageMemoryLimit);
        frame.staticArrayBatch.Fallback(StaticArrayBatch::Result::SmallTextureReserve);
        frame.staticArrayBatch.Fallback(StaticArrayBatch::Result::PreferredPageFull);
        for(unsigned reason=0;reason<OpaqueOrder::RejectionCount;++reason)
            frame.opaqueOrder.rejected[reason]=(i+reason)%11;
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
        assert(std::stoi(row[column("opaque_world_alpha_fast_path_enabled")]) == int(id%2));
        assert(std::stoi(row[column("replay_forward_view_active")]) == int((id/2)%2));
        assert(std::stoi(row[column("renderdoc_capture_active")]) == (id==10?1:0));
        assert(std::stoi(row[column("opaque_order_experiment_enabled")]) == int(id%2));
        assert(std::stoi(row[column("opaque_order_candidates")]) == 40);
        assert(std::stoi(row[column("opaque_order_eligible")]) == 30);
        assert(std::stoi(row[column("opaque_order_swaps")]) == int(id%7));
        assert(std::stoul(row[column("masked_alpha_bound_candidates")]) == 30+id%21);
        assert(std::stoul(row[column("masked_alpha_bound_solid_passes_skipped")]) == id%21);
        assert(std::stoull(row[column("masked_alpha_bound_indices_skipped")]) == 6000000000ULL+id);
        assert(std::stoul(row[column("masked_alpha_uploaded_solid_passes_skipped")]) == id%19);
        assert(std::stoul(row[column("masked_alpha_uploaded_faint_passes_skipped")]) == id%17);
        assert(std::stoi(row[column("classic_foliage_cutout_enabled")]) == int((id/2)%2));
        assert(std::stoul(row[column("classic_foliage_faint_passes_skipped")]) == id%13);
        assert(std::stoi(row[column("masked_alpha_pass_skip_enabled")]) == int(id%2));
        assert(std::stoi(row[column("opaque_mesh_batch_enabled")]) == int(id%2));
        assert(std::stoi(row[column("opaque_mesh_batch_saved_draws")]) == 62);
        assert(std::stoull(row[column("static_reuse_source_frame_id")]) == id);
        assert(std::stoi(row[column("static_reuse_compatible_instances")]) == 3);
        assert(std::stoi(row[column("static_reuse_top_model")]) == 1416);
        assert(std::stoi(row[column("world_occlusion_enabled")])==int(id%2));
          assert(std::stoi(row[column("world_occlusion_rejected")])==19);
          assert(std::stoull(row[column("world_occlusion_vertices_avoided")])==6000000000ULL+id);
          assert(std::abs(std::stod(row[column("world_occlusion_build_ms")])-.234567)<1e-7);
          assert(std::stoi(row[column("world_backface_enabled")])==int(id%2));
          assert(std::stoi(row[column("world_backface_draws")])==7);
          assert(std::stoull(row[column("world_backface_indices")])==6000000000ULL+id);
          assert(std::abs(std::stod(row[column("world_backface_certify_ms")])-.123456)<1e-7);
          assert(std::stoi(row[column("lod_fade_transition_enabled")])==int(id%2));
          assert(std::stoi(row[column("world_occlusion_raster_rows")])==125);
          assert(std::stoi(row[column("world_occlusion_selected_triangles")])==219);
          assert(std::stoi(row[column("world_occlusion_cache_builds")])==2);
          assert(std::stoi(row[column("world_occlusion_cache_hits")])==27);
          assert(std::stoi(row[column("world_occlusion_cache_scanned")])==1000);
          assert(std::stoi(row[column("world_occlusion_cache_deferred")])==3);
          assert(std::stoi(row[column("world_occlusion_source_objects")])==17);
          assert(std::stoi(row[column("world_occlusion_source_projected")])==110);
          assert(std::stoi(row[column("world_occlusion_source_projection_rejected")])==70);
          assert(std::stoi(row[column("world_occlusion_source_small")])==5);
          assert(std::stoi(row[column("world_occlusion_source_budget_skipped")])==64);
          assert(std::stoi(row[column("world_occlusion_source_top_samples")])==8);
          assert(std::stoi(row[column("world_occlusion_source_top_model")])==1417);
          assert(std::stoi(row[column("fxaa_integer_sampling_enabled")])==int(id%2));
          assert(std::stoi(row[column("fxaa_integer_sampling_active")])==int(id%4==1));
          assert(std::stoi(row[column("world_occlusion_polygon_candidates")])==312);
          assert(std::stoi(row[column("world_occlusion_selected_polygons")])==256);
          assert(std::stoi(row[column("world_occlusion_selection_dropped")])==56);
          assert(std::stoi(row[column("world_occlusion_raster_budget_skipped")])==14);
          assert(std::stoi(row[column("world_occlusion_no_depth_polygons")])==31);
          assert(std::stoi(row[column("world_occlusion_edge_tests")])==21970);
          assert(std::stoi(row[column("world_occlusion_redundancy_tests")])==4700);
          assert(std::stoi(row[column("world_occlusion_redundancy_budget_stops")])==1);
          assert(std::stoi(row[column("world_occlusion_redundant_polygons")])==25);
          assert(std::stoi(row[column("world_occlusion_redundant_eyes")])==58);
          assert(std::stoi(row[column("world_occlusion_tile_skipped_cells")])==3000);
          assert(std::stoi(row[column("world_occlusion_depth_skipped_cells")])==1700);
          assert(std::stoi(row[column("world_occlusion_span_rows")])==170);
          assert(std::stoi(row[column("world_occlusion_span_cells")])==2600);
          assert(std::stoi(row[column("world_occlusion_span_fallback_rows")])==81);
          assert(std::stoi(row[column("world_occlusion_span_edge_tests")])==1200);
          assert(std::stoi(row[column("world_occlusion_build_phases_valid")])==1);
          assert(std::stoi(row[column("world_occlusion_phase_clock_reads")])==810);
          for(unsigned phase=0;phase<WorldOcclusion::BuildPhaseCount;++phase){
              const std::string key="world_occlusion_build_"+std::string(WorldOcclusion::BuildPhaseName(phase))+"_ms";
              assert(std::abs(std::stod(row[column(key.c_str())])-.001234*(phase+1))<1e-9);
          }
          assert(std::stoi(row[column("world_occlusion_zero_source_1_slot")])==1);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_model")])==222);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_selected")])==4);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_attempted")])==3);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_no_depth")])==2);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_improved")])==1);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_budget")])==2);
          assert(std::stoi(row[column("world_occlusion_zero_source_1_updates")])==7);
          assert(std::stoi(row[column("world_occlusion_zero_source_2_model")])==1417);
          assert(std::stoi(row[column("world_occlusion_zero_source_2_redundant")])==1);
          assert(std::stoi(row[column("world_occlusion_zero_source_3_slot")])==-1);
          assert(std::stoi(row[column("world_occlusion_zero_source_3_model")])==-1);
          assert(std::stoi(row[column("world_occlusion_zero_source_3_no_depth")])==0);
          assert(std::stoi(row[column("world_occlusion_clip_inside")])==20);
          assert(std::stoi(row[column("world_occlusion_clip_outside")])==30);
          assert(std::stoi(row[column("world_occlusion_clip_crossing")])==40);
          assert(std::stoi(row[column("world_occlusion_clip_boundary")])==5);
          assert(std::stoi(row[column("world_occlusion_clip_invalid")])==1);
          assert(std::stoi(row[column("world_occlusion_clip_plane_visits")])==170);
          assert(std::stoi(row[column("static_array_batch_enabled")])==1);
          assert(std::stoi(row[column("static_array_shortened_batches")])==2);
          assert(std::stoi(row[column("cross_array_batches")])==3);
          assert(std::stoi(row[column("cross_array_saved_draws")])==8);
          assert(std::stoi(row[column("cross_array_upload_bytes")])==2048);
          assert(std::stoi(row[column("cross_array_fallbacks")])==1);
          assert(std::stod(row[column("cross_array_flush_ms")])==.125);
          assert(row[column("cross_array_flush_breakdown_sampled")]==(id%2?"1":"0"));
          assert(std::stod(row[column("cross_array_lookup_ms")])==(id%2?.025:0));
          assert(std::stod(row[column("cross_array_build_ms")])==(id%2?.010:0));
          assert(std::stod(row[column("cross_array_transform_ms")])==(id%2?.015:0));
          assert(std::stod(row[column("cross_array_record_ms")])==(id%2?.040:0));
          assert(std::stod(row[column("cross_array_fallback_ms")])==(id%2?.035:0));
          assert(row[column("cross_array_cache_hits")]=="9");
          assert(row[column("cross_array_cache_misses")]=="3");
          assert(row[column("cross_array_cache_builds")]=="2");
          assert(row[column("cross_array_cache_bytes")]=="65536");
          assert(row[column("cross_array_cache_upload_bytes")]=="8192");
          assert(row[column("cross_array_arena_bytes")]=="16777216");
          assert(row[column("cross_array_arena_reserved_bytes")]=="67584");
          assert(row[column("cross_array_arena_misses")]=="1");
          assert(row[column("cross_array_pipeline_binds")]=="4");
          assert(row[column("cross_array_descriptor_binds")]=="5");
          assert(row[column("cross_array_transform_binds")]=="3");
          assert(row[column("cross_array_ordinary_draws")]=="12");
          assert(row[column("cross_array_ordinary_pipeline_binds")]=="8");
          assert(row[column("cross_array_ordinary_texture_binds")]=="6");
          assert(std::stoi(row[column("static_array_page_size_retries")])==1);
          assert(std::stoi(row[column("static_array_page_memory_cache_hits")])==4);
          assert(std::stoi(row[column("static_array_occupancy_sampled")])==int(id%2));
          assert(row[column("static_array_page_occupancy")]==
                 (id%2?"64:64:16:8:5:1:131072|64:64:16:2:0:0:32768":""));
          assert(row[column("static_array_residency_sampled")]==(id%2?"1":"0"));
          assert(row[column("static_array_residency_overflow")]=="0");
          assert(row[column("static_array_duplicate_merged_layers")]==(id%2?"3":"0"));
          assert(row[column("static_array_residency_misses")]==
                 (id%2?"64:64:other_page:uploaded:2|128:128:absent:resident_only:1":""));
          assert(row[column("static_array_page_copies")]==(id%2?"3:2|0:0":""));
          assert(row[column("static_array_demand_probes")]=="5");
          assert(row[column("static_array_demand_replacements")]=="2");
          assert(row[column("static_array_demand_upload_bytes")]=="32768");
          assert(row[column("static_array_demand_page_deferrals")]=="42");
          assert(row[column("static_array_adjacent_misses")]=="7");
          assert(row[column("static_array_adjacent_admissions")]=="1");
          assert(row[column("static_array_adjacent_merged")]=="5");
          unsigned totalFallbacks=0;
          for(unsigned i=0;i<StaticArrayBatch::FallbackCount;++i){
              const std::string key="static_array_fallback_"+std::string(StaticArrayBatch::FallbackName(i));
              const unsigned count=std::stoul(row[column(key.c_str())]);
              const auto reason=StaticArrayBatch::Result(i);
              assert(count==(reason==StaticArrayBatch::Result::PreferredPageFull?2u:
                             (reason==StaticArrayBatch::Result::PageMemoryLimit ||
                              reason==StaticArrayBatch::Result::SmallTextureReserve)?1u:0u));
              totalFallbacks+=count;
          }
          assert(totalFallbacks==4 && std::stoul(row[column("static_array_fallbacks")])==totalFallbacks);
          assert(std::stoi(row[column("world_occlusion_source_geometry_rejected")])==2);
          assert(std::stoi(row[column("world_occlusion_source_no_opaque")])==3);
          assert(std::stoi(row[column("world_occlusion_mesh_opaque")])==11);
          assert(std::stoi(row[column("world_occlusion_mesh_alpha")])==7);
          assert(std::stoi(row[column("world_occlusion_mesh_unsupported")])==5);
          assert(std::stoi(row[column("world_occlusion_source_clipped")])==13);
          assert(std::stoi(row[column("world_occlusion_clip_triangles")])==31);
          assert(std::stoi(row[column("world_occlusion_query_invalid")])==17);
          assert(std::stoi(row[column("world_occlusion_cells_per_eye")])==2304);
          assert(std::stoi(row[column("world_occlusion_footprint_candidates")])==90);
          assert(std::stoi(row[column("world_occlusion_footprint_rejected")])==17);
          assert(std::stoi(row[column("world_occlusion_footprint_uncertain")])==3);
          assert(std::stoi(row[column("world_occlusion_footprint_selected")])==32);
          assert(std::stoi(row[column("world_occlusion_footprint_capacity_skipped")])==10);
          assert(std::stoi(row[column("world_lod_mode")]) == int(id%3));
        assert(std::stoi(row[column("world_lod_lower_selections")]) == 12);
        assert(std::stoi(row[column("world_lod_unpaired_models")]) == 17);
        assert(std::stoi(row[column("static_instances_enabled")]) == int(id%2));
        assert(std::stoi(row[column("static_instances_saved_draws")]) == 6);
        assert(std::stoull(row[column("static_instances_emitted_indices")]) == 6000000000ULL+id);
        assert(std::abs(std::stod(row[column("static_reuse_cpu_ms")])-.012345)<1e-7);
        assert(std::stoi(row[column("opaque_mesh_batch_reject_range")]) == 9);
        for(unsigned reason=0;reason<OpaqueOrder::RejectionCount;++reason){
            std::string key="opaque_order_reject_";key+=OpaqueOrder::RejectionName(reason);
            assert(std::stoi(row[column(key.c_str())]) == int((id+reason)%11));
        }
        assert(std::stoi(row[column("vk_static_atomic_calls")]) == 4);
        assert(std::stoi(row[column("vk_static_atomic_pipeline_requests")]) == 20);
        assert(std::stoi(row[column("vk_static_atomic_pipeline_binds")]) == 5);
        assert(std::stoi(row[column("vk_static_atomic_texture_requests")]) == 20);
        assert(std::stoi(row[column("vk_static_atomic_texture_binds")]) == 7);
        assert(std::stoi(row[column("vk_static_atomic_texture_lookups")]) == 12);
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
    assert(all.find("opaque_world_alpha_fast_path,guarded_opaque_positive_alpha_test_ref\n") != std::string::npos);
    assert(all.find("draw_model_capacity,dynamic\n") != std::string::npos);
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
            assert(std::stoul(cells[drawColumn("model_buckets")])==1);
            assert(std::stoul(cells[drawColumn("model_capacity")])>=DrawProfile::InitialModels);
            assert(std::stoul(cells[drawColumn("model_allocation_failures")])==0);
        }
    }
    assert(detailRows==15);
    assert(all.find("write_error,0\n") != std::string::npos);
    assert(all.find("overwrite_attempt") == std::string::npos);
    // Serialize a census well above the old cap and the new initial reserve.
    DrawProfile::Recording large;large.Reset();
    const unsigned owners=DrawProfile::InitialModels+257;
    for(unsigned i=0;i<owners;++i)for(unsigned phase:{unsigned(GpuPassProfile::World),unsigned(GpuPassProfile::Fading),unsigned(GpuPassProfile::Effects)}){
        large.Begin(DrawProfile::Entity(int(i),0,"large_model",0));large.Draw(phase,3,3,DrawProfile::Mesh);large.End();
    }
    frame.id=frame.gpu.frameId=1;frame.startNs=frame.gpu.startNs=10000000;
    frame.gpu.geometry=std::make_shared<const DrawProfile::Stats>(large.stats);
    for(unsigned i=0;i<GpuPassCount;++i)frame.gpu.draws[i]=large.stats.passes[i][DrawProfile::Draws];
    assert(writer.Start(stem+"-large", "active_model_set,CLASSIC\n"));
    bool submitted=false;
    for(unsigned retry=0;retry<1000&&!submitted;++retry){submitted=writer.Submit(frame);if(!submitted)std::this_thread::yield();}
    assert(submitted);writer.Stop();
    std::ifstream largeRows(stem+"-large.draws.csv");
    unsigned exported=0;while(std::getline(largeRows,line))++exported;
    assert(exported==1+GpuPassCount+owners*3);
    // Wrap the SPSC ring repeatedly while the reader drains it. Retry only
    // genuine full-queue submissions; accepted samples must remain ordered.
    assert(writer.Start(stem+"-wrap", "queue_test,spsc_wrap\n"));
    Frame paced;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    for(unsigned id=1;id<=4096;++id) {
        paced.id=id;paced.startNs=uint64_t(id)*10000000;paced.cpuMs=id*.001;
        while(!writer.Submit(paced)) {
            assert(std::chrono::steady_clock::now()<deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    writer.Stop();
    std::ifstream wrapped(stem+"-wrap.frames.csv");std::string wrappedLine;
    assert(std::getline(wrapped,wrappedLine));unsigned expectedId=1;
    while(std::getline(wrapped,wrappedLine)) {
        const auto cells=Cells(wrappedLine);
        assert(std::stoull(cells[0])==expectedId && std::stoull(cells[1])==uint64_t(expectedId)*10000000);
        assert(std::fabs(std::stod(cells[3])-expectedId*.001)<.000001);++expectedId;
    }
    assert(expectedId==4097);
    std::printf("PASS: player transitions, raw timing precision, CSV schema, bounded writer accounting, shutdown drain, exclusive sessions and 4096 ordered SPSC samples (%u flood rows).\n", rows);
}
