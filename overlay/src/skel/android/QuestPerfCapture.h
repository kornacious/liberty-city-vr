#pragma once

// No engine objects cross the writer boundary. The game thread only copies
// numeric samples into a bounded queue; formatting and disk I/O run elsewhere.
#include "vulkan/DrawProfile.h"
#include "vulkan/AtomicSubmission.h"
#include "vulkan/OpaqueMeshBatch.h"
#include "vulkan/StaticReuseAudit.h"
#include "vulkan/StaticInstances.h"
#include "vulkan/WorldBackface.h"
#include "vulkan/WorldOcclusion.h"
#include "../../renderer/WorldLodPolicy.h"
#include "vulkan/OpaqueOrder.h"
#include "vulkan/UniformAlphaPolicy.h"
#include <memory>
#include <algorithm>
#include <utility>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

namespace questperf {

// Replay preparation suspends capture; playback overrides the saved preference.
struct ProfilingState {
    int display = 0;
    bool active = false;
    int replay = 0; // 0: normal, 1: preparing, 2: playing
    bool Recording() const { return replay ? replay == 2 : active; }
    bool Collecting() const { return Recording() || display == 1 || display == 3; }
};

inline float SpeedKmh(float engineSpeed)
{
    // re3 velocities use game units per 50 Hz reference step. Handling data
    // converts km/h with the inverse of this factor (HandlingMgr.cpp).
    const float kmh = engineSpeed*180.0f;
    return std::isfinite(kmh) && kmh >= 0 ? kmh : -1.0f;
}

class VehicleSpeedPeak {
    const void *current = nullptr; // Never serialized or exposed in diagnostics.
    float peak = -1;
public:
    float Update(const void *vehicle, float speedKmh)
    {
        if(vehicle != current) { current = vehicle; peak = -1; }
        if(!vehicle) { peak = -1; return -1; }
        if(std::isfinite(speedKmh) && speedKmh >= 0 && speedKmh > peak) peak = speedKmh;
        return peak;
    }
};

enum Event : uint32_t {
    Damage = 1, Died = 2, Recovered = 4, EnteredVehicle = 8,
    LeftVehicle = 16, WeaponUsed = 32, PlayerAppeared = 64,
    PlayerDisappeared = 128, Relocated = 256, PauseChanged = 512,
    CutsceneChanged = 1024, MissionChanged = 2048, WantedChanged = 4096
};
enum Activity { Unavailable, Still, Walking, Running, Sprinting, Vehicle,
    Melee, Attacking, Aiming, Dead, Arrested };
inline const char *ActivityName(int value)
{
    static const char *names[] = {"unavailable", "still", "walking", "running",
        "sprinting", "vehicle", "melee", "attacking", "aiming", "dead", "arrested"};
    return value >= 0 && value < 11 ? names[value] : names[0];
}
struct Player {
    int valid = 0, activity = Unavailable, pedState = -1, moveState = -1;
    int vehicleModel = -1, weapon = -1, weaponState = -1, ammo = -1, wanted = -1;
    int dead = 0, inVehicle = 0, paused = 0, cutscene = 0, mission = 0;
    float health = 0, armour = 0, x = 0, y = 0, z = 0, speed = 0;
    float vehicleSpeedKmh = -1, vehicleSpeed3dKmh = -1, vehiclePeakKmh = -1;
    float vehicleHandlingCruiseKmh = -1, vehicleTransmissionMaxKmh = -1;
};
class PlayerEvents {
    Player previous{};
    bool observed = false, sawDead = false;
public:
    uint32_t Update(const Player &p, uint32_t weaponUses)
    {
        uint32_t events = weaponUses ? WeaponUsed : 0;
        if(observed) {
            if(p.paused != previous.paused) events |= PauseChanged;
            if(p.cutscene != previous.cutscene) events |= CutsceneChanged;
            if(p.mission != previous.mission) events |= MissionChanged;
            if(p.valid && previous.valid && p.wanted != previous.wanted) events |= WantedChanged;
        }
        if(observed && p.valid != previous.valid)
            events |= p.valid ? PlayerAppeared : PlayerDisappeared;
        if(p.valid) {
            if(observed && previous.valid) {
                if(p.health < previous.health || p.armour < previous.armour) events |= Damage;
                if(p.dead && !previous.dead) events |= Died;
                if(p.inVehicle != previous.inVehicle)
                    events |= p.inVehicle ? EnteredVehicle : LeftVehicle;
                const float dx = p.x-previous.x, dy = p.y-previous.y, dz = p.z-previous.z;
                if(dx*dx+dy*dy+dz*dz > 2500.0f) events |= Relocated;
            }
            if(!p.dead && sawDead) events |= Recovered;
            sawDead = p.dead != 0;
        }
        previous = p;
        observed = true;
        return events;
    }
};

constexpr size_t PhaseCount = 24;
struct BeginStats {
    int valid = 0;
    float totalMs = 0, pipelineMs = 0, fenceWaitMs = 0, timestampMs = 0;
    float retirementMs = 0, setupMs = 0, wristMs = 0;
    uint32_t pipelineRequests = 0, pipelinesCreated = 0, pipelineFailures = 0;
    uint32_t retiredBuffers = 0, retiredImages = 0, retiredDescriptors = 0, uploadCommands = 0;
    uint32_t framebufferCreated = 0;
    uint64_t lastPipelineKey = 0;
};
constexpr unsigned GpuPassCount=14;
inline const char *GpuPassName(unsigned i){
    static const char *names[]={"sky","roads","reflections","world","water","fading","weather","effects","ui","scope","wrist","resolve_postprocess","other","scene"};
    static_assert(sizeof(names)/sizeof(names[0])==GpuPassCount,"GPU capture pass names");
    return i<GpuPassCount?names[i]:"invalid";
}
struct GpuPassStats {
    uint64_t frameId=0,startNs=0;
    uint32_t mask=0,status=0;
    float frameMs=-1,ms[GpuPassCount];
    uint32_t draws[GpuPassCount]={};
    std::shared_ptr<const DrawProfile::Stats> geometry;
    GpuPassStats(){for(float &v:ms)v=-1;}
};
struct Frame {
    uint64_t id = 0, startNs = 0, phaseMask = 0;
    double intervalMs = -1, cpuMs = 0;
    float stages[6]{}; // pre, vk begin, step, post, vk end, recorder preparation
    float phases[PhaseCount]{};
    float runtimeCpuMs = -1, runtimeGpuMs = -1, vulkanGpuMs = -1, refreshHz = 0;
    int width = 0, height = 0;
    int profilerDisplayMode = 0;
    int opaqueWorldAlphaFastPathEnabled = 1;
    int replayForwardViewActive = 0;
    int renderdocCaptureActive = 0;
    int opaqueOrderExperimentEnabled = 0;
    OpaqueOrder::Stats opaqueOrder{};
    rw::vulkan::UniformAlphaPolicy::BoundStats alphaBound{};
    int opaqueMeshBatchEnabled=0;
    OpaqueMeshBatch::Stats opaqueMeshBatch{};
    StaticReuseAudit::Stats staticReuse{};
    int lodFadeTransitionEnabled=0;
    int fxaaIntegerSamplingEnabled=0,fxaaIntegerSamplingActive=0;
    int worldLodMode=WorldLodPolicy::On;
    WorldLodPolicy::Stats worldLod{};
    int worldOcclusionEnabled=0;
    WorldOcclusion::Stats worldOcclusion{};
    int worldBackfaceEnabled=0;
    WorldBackface::Stats worldBackface{};
    int staticInstancesEnabled=0;
    StaticInstances::Stats staticInstances{};
    int staticArrayBatchEnabled=0;
    StaticArrayBatch::Stats staticArrayBatch{};
    StaticArrayBatch::Occupancy staticArrayOccupancy{};
    StaticArrayBatch::ResidencyStats staticArrayResidency{};
    int maskedAlphaPassSkipEnabled=0;
    int classicFoliageCutoutEnabled=0;
    int activeModels[5]{}, requestedModel = 0;
    float renderScale = -1;
    uint32_t events = 0, weaponUses = 0, renderCalls[3]{};
    Player player{};
    BeginStats begin{};
    AtomicSubmission::Stats atomicSubmission{};
    GpuPassStats gpu{};
};

class Writer {
    static constexpr size_t Capacity = 1024;
    std::unique_ptr<std::array<Frame,Capacity>> queue;
    // One game-thread producer and one writer-thread consumer. Release/acquire
    // transfers slot ownership; transient writer activity cannot drop a sample.
    std::atomic<size_t> readSequence{0};
    std::atomic<size_t> writeSequence{0};
    std::mutex mutex;
    std::condition_variable ready;
    std::thread worker;
    std::atomic<bool> stopping{false};
    FILE *frames = nullptr, *metadata = nullptr, *draws = nullptr;
    uint64_t lastDrawFrame=0,drawRows=0,drawSamples=0;
    std::atomic<uint64_t> dropped{0};
    uint64_t written = 0;

    static FILE *OpenExclusive(const std::string &path)
    {
#ifdef _MSC_VER
        FILE *file = nullptr;
        return fopen_s(&file, path.c_str(), "wx") == 0 ? file : nullptr;
#else
        return std::fopen(path.c_str(), "wx");
#endif
    }

    void Write(const Frame &f)
    {
        const Player &p = f.player;
        std::fprintf(frames, "%llu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%llu,%llu",
            (unsigned long long)f.id, (unsigned long long)f.startNs, f.intervalMs, f.cpuMs,
            f.stages[0], f.stages[1], f.stages[2], f.stages[3], f.stages[4], f.stages[5],
            f.runtimeCpuMs, f.runtimeGpuMs, f.vulkanGpuMs, f.refreshHz,
            (unsigned long long)f.phaseMask, (unsigned long long)dropped.load());
        for(float phase : f.phases) std::fprintf(frames, ",%.6f", phase);
        std::fprintf(frames, ",%u,%u,%u,%u,%u,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%d,%d,%.3f,%d,%d,%d,%d,%d,%d",
            f.renderCalls[0], f.renderCalls[1], f.renderCalls[2], f.events, f.weaponUses,
            p.valid, ActivityName(p.activity), p.pedState, p.moveState, p.vehicleModel,
            p.weapon, p.weaponState, p.ammo, p.wanted, p.dead, p.inVehicle,
            p.paused, p.cutscene, p.mission, p.health, p.armour, p.x, p.y, p.z, p.speed,
            f.width, f.height, f.renderScale, f.activeModels[0], f.activeModels[1],
            f.activeModels[2], f.activeModels[3], f.activeModels[4], f.requestedModel);
        std::fprintf(frames, ",%.6f,%.6f,%.6f,%.6f,%.6f,%d", p.vehicleSpeedKmh,
            p.vehicleSpeed3dKmh, p.vehiclePeakKmh, p.vehicleHandlingCruiseKmh,
            p.vehicleTransmissionMaxKmh, f.profilerDisplayMode);
        const BeginStats &b = f.begin;
        std::fprintf(frames, ",%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%u,%u,%u,%u,%u,%u,%llu",
            b.valid, b.pipelineMs, b.fenceWaitMs, b.timestampMs, b.retirementMs,
            b.setupMs, b.wristMs, b.totalMs, b.pipelineRequests, b.pipelinesCreated,
            b.pipelineFailures, b.retiredBuffers, b.retiredImages, b.retiredDescriptors,
            b.uploadCommands, b.framebufferCreated, (unsigned long long)b.lastPipelineKey);
        const GpuPassStats &g=f.gpu;
        static const DrawProfile::Stats emptyGeometry;
        const auto &geometry=g.geometry?*g.geometry:emptyGeometry;
        std::fprintf(frames,",%llu,%llu,%u,%u,%.6f",(unsigned long long)g.frameId,
            (unsigned long long)g.startNs,g.status,g.mask,g.frameMs);
        for(unsigned i=0;i<GpuPassCount;++i)std::fprintf(frames,",%.6f,%u",g.ms[i],g.draws[i]);
        std::fprintf(frames,",%d,%d",f.opaqueWorldAlphaFastPathEnabled,f.replayForwardViewActive);
        const auto &a=f.atomicSubmission;
        std::fprintf(frames,",%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%u",a.atomics,a.pipelineRequests,a.pipelineBinds,
            a.textureRequests,a.textureBinds,a.textureLookups,f.renderdocCaptureActive,
            f.opaqueOrderExperimentEnabled,f.opaqueOrder.candidates,f.opaqueOrder.eligible,f.opaqueOrder.swaps);
        for(unsigned i=0;i<OpaqueOrder::RejectionCount;++i)
            std::fprintf(frames,",%u",f.opaqueOrder.rejected[i]);
        std::fprintf(frames,",%u,%u,%llu,%d",f.alphaBound.candidates,f.alphaBound.solidPassesSkipped,
            (unsigned long long)f.alphaBound.indicesSkipped,f.maskedAlphaPassSkipEnabled);
        const auto &mb=f.opaqueMeshBatch;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u",f.opaqueMeshBatchEnabled,
            mb.eligible,mb.checks,mb.merged,mb.savedDraws,mb.rejectedState,mb.rejectedMaterial,mb.rejectedRange);
        const auto &reuse=f.staticReuse;
        std::fprintf(frames,",%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%.6f",
            (unsigned long long)reuse.sourceFrame,reuse.sampled,reuse.candidates,reuse.groups,
            reuse.geometryRepeats,reuse.compatibleInstances,reuse.nearbyInstances,reuse.adjacentInstances,
            reuse.rejectedState,reuse.rejectedOwner,reuse.rejectedTransform,reuse.tableOverflow,
            reuse.ownerOverflow,reuse.maxInstances,reuse.topModel,reuse.cpuMs);
        const auto &si=f.staticInstances;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%.6f",
            f.staticInstancesEnabled,si.queued,si.groups,si.instances,si.savedDraws,
            si.rejectState,si.rejectBounds,si.rejectOrder,si.rejectSpatial,si.pipelineMiss,si.allocationFailure,
            (unsigned long long)si.sourceIndices,(unsigned long long)si.emittedIndices,si.flushMs);
        const auto &lod=f.worldLod;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u",f.worldLodMode,lod.highSelections,lod.lowerSelections,
            lod.lodSelections,lod.suppressedLod,lod.missingSelections,lod.pairedModels,lod.unpairedLodModels);
        const auto &bc=f.worldBackface;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u,%llu,%.6f",f.worldBackfaceEnabled,
            bc.candidates,bc.draws,bc.rejectState,bc.rejectGeometry,bc.rejectView,bc.rejectTransform,
            bc.certifications,(unsigned long long)bc.indices,bc.certifyMs);
        const auto &oc=f.worldOcclusion;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%.6f,%.6f",
            f.worldOcclusionEnabled,oc.builds,oc.occluderCandidates,oc.occluders,oc.triangles,oc.cells,
            oc.candidates,oc.rejected,oc.visible,oc.invalid,oc.budget,oc.leftCells,oc.rightCells,
            oc.rasterTests,oc.queryTests,(unsigned long long)oc.indices,(unsigned long long)oc.vertices,oc.buildMs,oc.queryMs);
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d",
            f.lodFadeTransitionEnabled,oc.rasterRows,oc.selectedTriangles,
            oc.cacheBuilds,oc.cacheHits,oc.cacheScanned,oc.cacheDeferred,oc.sourceObjects,
            oc.sourceProjected,oc.sourceProjectionRejected,oc.sourceSmall,oc.sourceBudgetSkipped,
            oc.sourceTopSamples,oc.sourceTopModel);
        std::fprintf(frames,",%u,%u,%u,%u,%u,%d,%d",oc.footprintCandidates,oc.footprintRejected,oc.footprintUncertain,oc.footprintSelected,oc.footprintCapacitySkipped,f.fxaaIntegerSamplingEnabled,f.fxaaIntegerSamplingActive);
        std::fprintf(frames,",%u,%u,%u,%u,%u,%u,%u,%u,%u",oc.sourceGeometryRejected,oc.sourceNoOpaque,oc.meshOpaque,oc.meshAlpha,oc.meshUnsupported,oc.sourceClipped,oc.clipTriangles,oc.queryInvalid,WorldOcclusion::Size*WorldOcclusion::Size);
        std::fprintf(frames,",%u,%u,%u,%u,%u,%u",oc.polygonCandidates,oc.selectedPolygons,oc.selectionDropped,oc.rasterBudgetSkipped,oc.noDepthPolygons,oc.edgeTests);
        std::fprintf(frames,",%u,%u,%u,%u,%u,%u",oc.redundancyTests,oc.redundancyBudgetStops,oc.redundantPolygons,oc.redundantEyes,oc.tileSkippedCells,oc.depthSkippedCells);
        std::fprintf(frames,",%u,%u,%u,%u,%d,%u",oc.spanRows,oc.spanCells,oc.spanFallbackRows,oc.spanEdgeTests,oc.buildPhasesValid?1:0,oc.phaseClockReads);
        for(unsigned phase=0;phase<WorldOcclusion::BuildPhaseCount;++phase)std::fprintf(frames,",%.6f",oc.buildPhaseMs[phase]);
        const auto sourceRanks=WorldOcclusion::TopNoDepthSources(oc);
        const WorldOcclusion::SourceContribution emptySource;
        for(unsigned rank=0;rank<WorldOcclusion::TopSourceCount;++rank){
            const int slot=sourceRanks[rank];
            const auto &source=slot<0?emptySource:oc.contributions[unsigned(slot)];
            std::fprintf(frames,",%d,%d,%u,%u,%u,%u,%u,%u,%u",slot,source.model,source.selected,source.attempted,
                source.noDepth,source.improved,source.budget,source.redundant,source.updates);
        }
        std::fprintf(frames,",%u,%u,%u,%u,%u,%u",oc.clipProfile.inside,oc.clipProfile.outside,oc.clipProfile.crossing,
            oc.clipProfile.boundary,oc.clipProfile.invalid,oc.clipProfile.planeVisits);
        const auto &array=f.staticArrayBatch;
        std::fprintf(frames,",%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",f.staticArrayBatchEnabled,
            array.batches,array.savedDraws,array.indices,array.cacheBuilds,array.fallbacks,
            array.pageBytes,array.pageUploadBytes,array.geometryBytes,array.geometryUploadBytes,array.shortenedBatches);
        for(auto count:array.fallbackReasons) std::fprintf(frames,",%u",count);
        const auto &occupancy=f.staticArrayOccupancy;
        std::fprintf(frames,",%u,%u,%u,%u,%.6f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,",
            array.crossBatches,array.crossSavedDraws,array.crossUploadBytes,array.crossFallbacks,array.crossFlushMs,
            array.crossCacheHits,array.crossCacheMisses,array.crossCacheBuilds,array.crossCacheBytes,array.crossCacheUploadBytes,
            array.crossPipelineBinds,array.crossDescriptorBinds,array.crossTransformBinds,
            array.pageSizeRetries,array.pageMemoryCacheHits,occupancy.sampled?1:0);
        if(occupancy.sampled)
            for(unsigned i=0;i<occupancy.count;++i){
                const auto &page=occupancy.pages[i];
                if(i) std::fputc('|',frames);
                std::fprintf(frames,"%u:%u:%u:%u:%u:%u:%u",page.width,page.height,page.samplerKey,
                             page.layers,page.live,page.held,page.bytes);
            }
        const auto &residency=f.staticArrayResidency;
        std::fprintf(frames,",%d,%u,%u,",residency.sampled?1:0,
            residency.overflow,residency.sampled?array.duplicateMergedLayers:0);
        if(residency.sampled)
            for(unsigned i=0;i<residency.count;++i){
                const auto &event=residency.events[i];
                if(i) std::fputc('|',frames);
                std::fprintf(frames,"%u:%u:%s:%s:%u",unsigned(event.width),unsigned(event.height),
                             event.elsewhere?"other_page":"absent",
                             StaticArrayBatch::ResidencyOutcomeName(event.outcome),event.requests);
            }
        std::fputc(',',frames);
        if(occupancy.sampled)
            for(unsigned i=0;i<occupancy.count;++i){
                const auto &page=occupancy.pages[i];
                if(i) std::fputc('|',frames);
                std::fprintf(frames,"%u:%u",page.live-page.duplicateLayers,page.duplicateLayers);
            }
        std::fprintf(frames,",%u,%u,%u,%u",array.demandProbes,array.demandReplacements,
                     array.demandUploadBytes,array.demandPageDeferrals);
        std::fprintf(frames,",%u,%u,%u",array.adjacentMisses,array.adjacentAdmissions,array.adjacentMerged);
        std::fprintf(frames,",%d,%.6f,%.6f,%.6f,%.6f,%.6f",array.crossFlushBreakdownSampled?1:0,
                     array.crossLookupMs,array.crossBuildMs,array.crossTransformMs,
                     array.crossRecordMs,array.crossFallbackMs);
        std::fprintf(frames,",%u,%u,%u",array.crossOrdinaryDraws,array.crossOrdinaryPipelineBinds,
                     array.crossOrdinaryTextureBinds);
        std::fprintf(frames,",%u,%u,%u",array.crossArenaBytes,array.crossArenaReservedBytes,
                     array.crossArenaMisses);
        std::fprintf(frames,",%u,%u",f.alphaBound.uploadedSolidSkipped,f.alphaBound.uploadedFaintSkipped);
        std::fprintf(frames,",%d,%u",f.classicFoliageCutoutEnabled,f.alphaBound.foliageFaintSkipped);
        std::fputc('\n',frames);
        if(g.frameId && g.frameId!=lastDrawFrame){
            lastDrawFrame=g.frameId;
            if(g.status==1){
                ++drawSamples;
                auto row=[&](const char *kind,unsigned pass,int model,int entityClass,const char *name,const uint32_t *counters){
                    std::fprintf(draws,"%llu,%llu,%s,%s,%d,%d,%s,%u,%u",
                        (unsigned long long)g.frameId,(unsigned long long)g.startNs,kind,GpuPassName(pass),
                        model,entityClass,name,geometry.overflowDraws,geometry.contextOverflows);
                    for(unsigned j=0;j<DrawProfile::CounterCount;++j)std::fprintf(draws,",%u",counters[j]);
                    std::fprintf(draws,",%u,%u,%u,%u\n",geometry.modelCount,geometry.modelCapacity,
                        geometry.tableGrowths,geometry.allocationFailures);++drawRows;
                };
                for(unsigned i=0;i<GpuPassCount;++i)row("phase",i,-1,4,"all",geometry.passes[i]);
                for(unsigned i=0;i<geometry.modelCount;++i){const auto &m=geometry.models[i];
                    row("model",m.pass,m.entity.model,m.entity.entityClass,m.entity.name,m.counters);}
            }
        }
    }
    void Run()
    {
        auto batchStorage=std::make_unique<std::array<Frame,128>>();
        auto &batch=*batchStorage;
        bool failed = false;
        auto lastFlush = std::chrono::steady_clock::now();
        for(;;) {
            const size_t read = readSequence.load(std::memory_order_relaxed);
            const size_t available = writeSequence.load(std::memory_order_acquire)-read;
            const size_t n = std::min(available,batch.size());
            if(!n) {
                if(stopping.load(std::memory_order_acquire) && writeSequence.load(std::memory_order_acquire)==read) break;
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait_for(lock, std::chrono::milliseconds(200), [this]{return writeSequence.load(std::memory_order_acquire)!=readSequence.load(std::memory_order_relaxed) || stopping.load(std::memory_order_acquire);});
                continue;
            }
            for(size_t i=0;i<n;++i) batch[i]=std::move((*queue)[(read+i)%Capacity]);
            readSequence.store(read+n,std::memory_order_release);
            for(size_t i = 0; i < n; ++i) {
                if(!failed) {
                    Write(batch[i]);
                    failed = std::ferror(frames) != 0 || std::ferror(draws) != 0;
                }
                if(failed) ++dropped;
                else ++written;
            }
            const auto now = std::chrono::steady_clock::now();
            if(now-lastFlush >= std::chrono::seconds(1)) {
                if(std::fflush(frames) != 0 || std::fflush(draws) != 0) failed = true;
                lastFlush = now;
            }
        }
        if(std::fflush(frames) != 0 || std::fflush(draws) != 0) failed = true;
        std::fprintf(metadata, "draw_rows,%llu\ndraw_samples,%llu\nwritten_rows,%llu\ndropped_rows,%llu\nwrite_error,%d\nclosed,1\n",
            (unsigned long long)drawRows,(unsigned long long)drawSamples,
            (unsigned long long)written, (unsigned long long)dropped.load(), failed ? 1 : 0);
    }
public:
    ~Writer() { Stop(); }
    // Exclusive creation prevents a repeated session name overwriting a capture.
    bool Start(const std::string &stem, const std::string &details)
    {
        Stop();
        frames = OpenExclusive(stem+".frames.csv");
        if(!frames) return false;
        metadata = OpenExclusive(stem+".session.csv");
        if(!metadata) { std::fclose(frames); frames = nullptr; return false; }
        draws = OpenExclusive(stem+".draws.csv");
        if(!draws){std::fclose(frames);std::fclose(metadata);frames=metadata=nullptr;return false;}
        std::setvbuf(draws,nullptr,_IOFBF,64*1024);
        std::fputs("source_frame_id,source_start_ns,kind,phase,model_id,entity_class,model_name,model_overflow_draws,context_overflows",draws);
        for(unsigned i=0;i<DrawProfile::CounterCount;++i)std::fprintf(draws,",%s",DrawProfile::Name(i));
        std::fputs(",model_buckets,model_capacity,model_table_growths,model_allocation_failures\n",draws);
        std::setvbuf(frames, nullptr, _IOFBF, 64*1024);
        std::fputs("key,value\nschema,6\ndraw_profile,sampled_source_matched_companion_draws_csv\nps2_uniform_alpha_fast_path,world_constant_alpha_margin_1_over_1024\nopaque_world_alpha_fast_path,guarded_opaque_positive_alpha_test_ref\nstatic_atomic_submission,local_bind_cache_and_split_descriptor_reuse_v1\nrenderdoc_capture,explicit_XR_frame_api_1_4_diagnostic_only\nstatic_atomic_submission_scope,world_shader_non_dynamic_non_vehicle_non_skinned\nstatic_atomic_submission_validity,vk_begin_detail_valid\ndraw_model_capacity,dynamic\ndraw_model_initial_capacity,2048\ndraw_model_growth,automatic_double_with_storage_reuse\ndraw_entity_classes,0_building_1_vehicle_2_ped_3_object_4_unknown\ndraw_triangles,submitted_including_split_passes_strip_degenerates_not_removed_not_multiplied_by_eyes\ndraw_reasons,overlapping_draw_flags_not_exclusive_pixel_costs\nclock,CLOCK_MONOTONIC\ngpu_samples,asynchronous_latest_not_matched_to_frame\ngpu_pass_samples,delayed_source_frame_id_and_start_ns\ngpu_pass_interval_frames,8\ngpu_pass_sampling,rotating_phase_one_per_block\ngpu_pass_scope,outside_render_pass_bottom_of_pipe_intervals\ngpu_pass_timing_buckets,scene_scope_wrist_resolve_postprocess_other\ngpu_pass_scene_scope,whole_scene_including_msaa_resolve_and_store\ngpu_pass_draw_scope,logical_phase_independent_of_timing_mask\ngpu_pass_tail,pending_tail_not_drained_no_extra_wait\ngpu_pass_status,0_none_1_complete_2_overflow_3_unbalanced_4_unavailable\nvehicle_speed_units,km/h_simulation_velocity\nvehicle_peak_scope,current_vehicle_occupancy_in_capture\nvk_begin_scope,synchronous_backend_cpu\n", metadata);
        std::fputs(details.c_str(), metadata);
        std::fflush(metadata);
        std::fputs("frame_id,start_monotonic_ns,frame_interval_ms,cpu_local_ms,pre_ms,vk_begin_ms,step_ms,post_ms,vk_end_ms,recorder_prepare_ms,runtime_cpu_ms,runtime_gpu_ms,vulkan_gpu_ms,refresh_hz,phase_valid_mask,dropped_rows", frames);
        static const char *names[] = {"game", "streaming", "population", "car", "audio",
            "world_list", "pre_render", "scene_setup", "world_render", "ui", "particle_update",
            "particle_render", "script", "pre_world_misc", "world_process", "post_world_camera_fx",
            "render_sky", "render_roads", "render_reflections", "render_world", "render_water",
            "render_fading", "render_weather", "render_effects"};
        static_assert(sizeof(names)/sizeof(names[0]) == PhaseCount, "capture phase schema");
        for(const char *name : names) std::fprintf(frames, ",%s_ms", name);
        std::fputs(",vehicle_render_calls,ped_render_calls,static_render_calls,event_mask,weapon_uses,player_valid,activity,ped_state,move_state,vehicle_model,weapon,weapon_state,ammo,wanted,dead,in_vehicle,paused,cutscene,mission,health,armour,x,y,z,speed_game_units,render_width,render_height,render_scale_effective,active_world,active_vegetation,active_vehicles,active_peds,active_weapons,requested_model_set,vehicle_speed_kmh,vehicle_speed_3d_kmh,vehicle_peak_kmh,vehicle_handling_cruise_kmh,vehicle_transmission_max_kmh,profiler_display_mode", frames);
        std::fputs(",vk_begin_detail_valid,vk_begin_pipeline_ms,vk_begin_fence_wait_ms,vk_begin_timestamp_ms,vk_begin_retirement_ms,vk_begin_setup_ms,vk_begin_wrist_ms,vk_begin_backend_total_ms,vk_pipeline_requests,vk_pipelines_created,vk_pipeline_failures,vk_retired_buffers,vk_retired_images,vk_retired_descriptors,vk_upload_commands,vk_framebuffer_created,vk_last_pipeline_key", frames);
        std::fputs(",gpu_pass_frame_id,gpu_pass_start_monotonic_ns,gpu_pass_status,gpu_pass_valid_mask,gpu_pass_frame_ms",frames);
        for(unsigned i=0;i<GpuPassCount;++i)std::fprintf(frames,",gpu_pass_%s_ms,gpu_pass_%s_draw_calls",GpuPassName(i),GpuPassName(i));
        std::fputs(",opaque_world_alpha_fast_path_enabled,replay_forward_view_active,vk_static_atomic_calls,vk_static_atomic_pipeline_requests,vk_static_atomic_pipeline_binds,vk_static_atomic_texture_requests,vk_static_atomic_texture_binds,vk_static_atomic_texture_lookups,renderdoc_capture_active,opaque_order_experiment_enabled,opaque_order_candidates,opaque_order_eligible,opaque_order_swaps",frames);
        for(unsigned i=0;i<OpaqueOrder::RejectionCount;++i)
            std::fprintf(frames,",opaque_order_reject_%s",OpaqueOrder::RejectionName(i));
        std::fputs(",masked_alpha_bound_candidates,masked_alpha_bound_solid_passes_skipped,masked_alpha_bound_indices_skipped,masked_alpha_pass_skip_enabled,opaque_mesh_batch_enabled,opaque_mesh_batch_eligible,opaque_mesh_batch_checks,opaque_mesh_batch_merged_meshes,opaque_mesh_batch_saved_draws,opaque_mesh_batch_reject_state,opaque_mesh_batch_reject_material,opaque_mesh_batch_reject_range,static_reuse_source_frame_id,static_reuse_sampled,static_reuse_candidates,static_reuse_groups,static_reuse_geometry_repeats,static_reuse_compatible_instances,static_reuse_nearby_instances,static_reuse_adjacent_instances,static_reuse_reject_state,static_reuse_reject_owner,static_reuse_reject_transform,static_reuse_table_overflow,static_reuse_owner_overflow,static_reuse_max_instances,static_reuse_top_model,static_reuse_cpu_ms,static_instances_enabled,static_instances_queued_draws,static_instances_groups,static_instances_instances,static_instances_saved_draws,static_instances_reject_state,static_instances_reject_bounds,static_instances_reject_order,static_instances_reject_spatial,static_instances_pipeline_misses,static_instances_allocation_failures,static_instances_source_indices,static_instances_emitted_indices,static_instances_flush_ms,world_lod_mode,world_lod_high_selections,world_lod_lower_selections,world_lod_entity_selections,world_lod_suppressed,world_lod_missing_selections,world_lod_paired_models,world_lod_unpaired_models,world_backface_enabled,world_backface_candidates,world_backface_draws,world_backface_reject_state,world_backface_reject_geometry,world_backface_reject_view,world_backface_reject_transform,world_backface_certifications,world_backface_indices,world_backface_certify_ms,world_occlusion_enabled,world_occlusion_builds,world_occlusion_occluder_candidates,world_occlusion_occluders,world_occlusion_triangles,world_occlusion_cell_updates,world_occlusion_candidates,world_occlusion_rejected,world_occlusion_visible,world_occlusion_invalid,world_occlusion_budget,world_occlusion_left_cells,world_occlusion_right_cells,world_occlusion_raster_tests,world_occlusion_query_tests,world_occlusion_indices_avoided,world_occlusion_vertices_avoided,world_occlusion_build_ms,world_occlusion_query_ms,lod_fade_transition_enabled,world_occlusion_raster_rows,world_occlusion_selected_triangles,world_occlusion_cache_builds,world_occlusion_cache_hits,world_occlusion_cache_scanned,world_occlusion_cache_deferred,world_occlusion_source_objects,world_occlusion_source_projected,world_occlusion_source_projection_rejected,world_occlusion_source_small,world_occlusion_source_budget_skipped,world_occlusion_source_top_samples,world_occlusion_source_top_model,world_occlusion_footprint_candidates,world_occlusion_footprint_rejected,world_occlusion_footprint_uncertain,world_occlusion_footprint_selected,world_occlusion_footprint_capacity_skipped,fxaa_integer_sampling_enabled,fxaa_integer_sampling_active,world_occlusion_source_geometry_rejected,world_occlusion_source_no_opaque,world_occlusion_mesh_opaque,world_occlusion_mesh_alpha,world_occlusion_mesh_unsupported,world_occlusion_source_clipped,world_occlusion_clip_triangles,world_occlusion_query_invalid,world_occlusion_cells_per_eye,world_occlusion_polygon_candidates,world_occlusion_selected_polygons,world_occlusion_selection_dropped,world_occlusion_raster_budget_skipped,world_occlusion_no_depth_polygons,world_occlusion_edge_tests,world_occlusion_redundancy_tests,world_occlusion_redundancy_budget_stops,world_occlusion_redundant_polygons,world_occlusion_redundant_eyes,world_occlusion_tile_skipped_cells,world_occlusion_depth_skipped_cells,world_occlusion_span_rows,world_occlusion_span_cells,world_occlusion_span_fallback_rows,world_occlusion_span_edge_tests,world_occlusion_build_phases_valid,world_occlusion_phase_clock_reads",frames);
        for(unsigned phase=0;phase<WorldOcclusion::BuildPhaseCount;++phase)
            std::fprintf(frames,",world_occlusion_build_%s_ms",WorldOcclusion::BuildPhaseName(phase));
        for(unsigned rank=1;rank<=WorldOcclusion::TopSourceCount;++rank)
            for(const char *field:{"slot","model","selected","attempted","no_depth","improved","budget","redundant","updates"})
                std::fprintf(frames,",world_occlusion_zero_source_%u_%s",rank,field);
        std::fputs(",world_occlusion_clip_inside,world_occlusion_clip_outside,world_occlusion_clip_crossing,world_occlusion_clip_boundary,world_occlusion_clip_invalid,world_occlusion_clip_plane_visits",frames);
        std::fputs(",static_array_batch_enabled,static_array_batches,static_array_saved_draws,static_array_indices,static_array_cache_builds,static_array_fallbacks,static_array_page_bytes,static_array_page_upload_bytes,static_array_geometry_bytes,static_array_geometry_upload_bytes,static_array_shortened_batches",frames);
        for(unsigned i=0;i<StaticArrayBatch::FallbackCount;++i)
            std::fprintf(frames,",static_array_fallback_%s",StaticArrayBatch::FallbackName(i));
        std::fputs(",cross_array_batches,cross_array_saved_draws,cross_array_upload_bytes,cross_array_fallbacks,cross_array_flush_ms"
            ",cross_array_cache_hits,cross_array_cache_misses,cross_array_cache_builds,cross_array_cache_bytes,cross_array_cache_upload_bytes"
            ",cross_array_pipeline_binds,cross_array_descriptor_binds,cross_array_transform_binds"
            ",static_array_page_size_retries,static_array_page_memory_cache_hits,static_array_occupancy_sampled,static_array_page_occupancy"
            ",static_array_residency_sampled,static_array_residency_overflow,static_array_duplicate_merged_layers"
            ",static_array_residency_misses,static_array_page_copies"
            ",static_array_demand_probes,static_array_demand_replacements,static_array_demand_upload_bytes"
            ",static_array_demand_page_deferrals,static_array_adjacent_misses"
            ",static_array_adjacent_admissions,static_array_adjacent_merged"
            ",cross_array_flush_breakdown_sampled,cross_array_lookup_ms,cross_array_build_ms"
            ",cross_array_transform_ms,cross_array_record_ms,cross_array_fallback_ms"
            ",cross_array_ordinary_draws,cross_array_ordinary_pipeline_binds,cross_array_ordinary_texture_binds"
            ",cross_array_arena_bytes,cross_array_arena_reserved_bytes,cross_array_arena_misses"
            ",masked_alpha_uploaded_solid_passes_skipped,masked_alpha_uploaded_faint_passes_skipped"
            ",classic_foliage_cutout_enabled,classic_foliage_faint_passes_skipped\n",frames);
        if(std::ferror(frames) || std::ferror(metadata) || std::ferror(draws)) {
            std::fclose(frames); std::fclose(metadata); std::fclose(draws);
            frames = metadata = draws = nullptr;
            return false;
        }
        lastDrawFrame=drawRows=drawSamples=0;
        readSequence=0;writeSequence=0;written = 0; dropped = 0; stopping = false;
        queue=std::make_unique<std::array<Frame,Capacity>>();
        worker = std::thread(&Writer::Run, this);
        return true;
    }
    bool Submit(const Frame &frame)
    {
        if(!worker.joinable()) return false;
        const size_t write=writeSequence.load(std::memory_order_relaxed);
        if(write-readSequence.load(std::memory_order_acquire)>=Capacity) { ++dropped; return false; }
        (*queue)[write%Capacity] = frame;
        writeSequence.store(write+1,std::memory_order_release);
        if(write+1-readSequence.load(std::memory_order_acquire)>=64) ready.notify_one();
        return true;
    }
    void Stop()
    {
        if(worker.joinable()) {
            { std::lock_guard<std::mutex> lock(mutex); stopping.store(true,std::memory_order_release); }
            ready.notify_one();
            worker.join();
        }
        if(frames) std::fclose(frames);
        if(metadata) std::fclose(metadata);
        if(draws) std::fclose(draws);
        frames = metadata = draws = nullptr;
        queue.reset();
    }
};
} // namespace questperf
