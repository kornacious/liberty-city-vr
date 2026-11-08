// Executes production key/warm-up/pending-compile code with driver calls mocked.
#include "../../librw/src/vulkan/DebugVisualization.h"
namespace DebugVisualization=rw::vulkan::DebugVisualization;
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>
using uint32 = uint32_t;
using uint64 = uint64_t;
using float32 = float;
using bool32 = int;
using VkPipeline = uint64;
using VkPrimitiveTopology = uint32;
#define nil nullptr
enum { VK_NULL_HANDLE = 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3, VK_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
    SHADER_WORLD = 0, SHADER_IM2D, SHADER_IM3D, SHADER_SKIN, SHADER_WORLD_INSTANCED, SHADER_COUNT,
    BLENDZERO = 1, BLENDONE = 2, BLENDSRCALPHA = 5, BLENDINVSRCALPHA = 6,
    CULLNONE = 1, CULLBACK = 2, ALPHAALWAYS = 1, ALPHAGREATEREQUAL = 2 };
struct RenderState {
    uint32 vertexAlphaEnabled, srcBlend, dstBlend, zTestEnabled, zWriteEnabled;
    uint32 cullMode, alphaTestFunction, sentinel;
};
static RenderState gstate{};
static bool effects, strict;
static bool worldEffectsActive() { return effects; }
static bool reflectionCoverageActive() { return effects; }
static bool getImmediate2DStrictDepth() { return strict; }
#include "vulkan-pipelines-types.inc"
static struct { std::vector<PipelineEntry> pipelines; std::vector<PendingPipeline> pending; } gs;
static struct { bool inFrame = false; bool neoEnabled = false; } gvk;
static FrameBeginStats gFrameBeginStats{};
static uint64 failKey;
static std::set<uint64> warmed;
uint64 makePipelineKey(uint32, VkPrimitiveTopology);
static VkPipeline getPipeline(uint32 shader, VkPrimitiveTopology topology)
{
    warmed.insert(makePipelineKey(shader, topology));
    return 1;
}
static VkPipeline getPipelineForKey(uint32, VkPrimitiveTopology, uint64 key)
{
    assert(!gvk.inFrame);
    if(key == failKey) return VK_NULL_HANDLE;
    for(const auto &entry : gs.pipelines) if(entry.key == key) return entry.pipeline;
    gs.pipelines.push_back({key, key+10});
    return key+10;
}
#include "vulkan-pipelines-production.inc"

int main()
{
    DebugVisualization::Reset();
    const uint64 litWorld=makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const uint64 litHud=makePipelineKey(SHADER_IM2D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const uint64 litLines=makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    DebugVisualization::Get().shading=DebugVisualization::Overdraw;
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==(litWorld|(uint64(1)<<25)|(uint64(1)<<26)));
    assert(makePipelineKey(SHADER_IM2D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litHud);
    assert(makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litLines);
    DebugVisualization::Get().shading=DebugVisualization::DepthOverdraw;
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==(litWorld|(uint64(1)<<25)|(uint64(1)<<26)|(uint64(1)<<27)));
    assert(makePipelineKey(SHADER_IM2D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litHud);
    assert(makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litLines);
    DebugVisualization::Get().shading=DebugVisualization::Lit;
    DebugVisualization::Get().mesh=DebugVisualization::HiddenWire;
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==(litWorld|(uint64(1)<<26)|(uint64(1)<<28)));
    assert(!(makePipelineKey(SHADER_SKIN,VK_PRIMITIVE_TOPOLOGY_LINE_LIST)&(uint64(1)<<28)));
    assert(makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litLines);
    DebugVisualization::Get().mesh=DebugVisualization::Solid;
    DebugVisualization::Get().shading=DebugVisualization::Albedo;
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==(litWorld|(uint64(1)<<26)));
    assert(makePipelineKey(SHADER_IM2D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litHud);
    assert(makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litLines);
    DebugVisualization::Reset();
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)==litWorld);

    gstate = {0, BLENDSRCALPHA, BLENDINVSRCALPHA, 1, 1, CULLBACK, ALPHAALWAYS, 91};
    const auto opaque = makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    gstate.srcBlend = BLENDONE; gstate.dstBlend = BLENDONE;
    assert(makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) == opaque);
    gstate.vertexAlphaEnabled = 1;
    const auto additive = makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    gstate.srcBlend = BLENDSRCALPHA; gstate.dstBlend = BLENDINVSRCALPHA;
    assert(makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) != additive);
    const RenderState saved = gstate;
    warmStandardPipelines();
    assert(std::memcmp(&saved, &gstate, sizeof(gstate)) == 0);
    // Fading meshes must be ready before the first gameplay frame.
    for(uint32 shader = SHADER_WORLD; shader < SHADER_COUNT; ++shader) {
        if(shader==SHADER_WORLD_INSTANCED) continue; // opaque depth-writing draws only
        gstate.zTestEnabled = 1; gstate.zWriteEnabled = 0;
        assert(warmed.count(makePipelineKey(shader, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)) == 1);
    }
    const auto noWrite = makePipelineKey(SHADER_IM2D, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    strict = true;
    assert(makePipelineKey(SHADER_IM2D, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) != noWrite);
    strict = false;
    auto plain = makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    effects = true;
    assert(makePipelineKey(SHADER_WORLD, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) != plain);
    effects = false;
    gvk.neoEnabled=true;
    assert(makePipelineKey(SHADER_WORLD,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)!=(plain));
    assert(makePipelineKey(SHADER_SKIN,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)&(uint64(1)<<24));
    assert(!(makePipelineKey(SHADER_IM2D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)&(uint64(1)<<24)));
    assert(!(makePipelineKey(SHADER_IM3D,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)&(uint64(1)<<24)));
    gvk.neoEnabled=false;
    const RenderState before = gstate;
    gs.pending = {{1, SHADER_WORLD, 3, saved}, {2, SHADER_WORLD, 3, saved}, {3, SHADER_WORLD, 3, saved}};
    gs.pipelines.push_back({3, 13});
    FrameBeginStats stats{};
    gvk.inFrame = true;
    compilePendingPipelines(&stats);
    assert(gs.pending.size() == 3 && stats.pipelineRequests == 0);
    gvk.inFrame = false; failKey = 2;
    compilePendingPipelines(&stats);
    assert(gs.pending.empty());
    assert(stats.pipelineRequests == 3 && stats.pipelinesCreated == 1 && stats.pipelineFailures == 1);
    assert(stats.lastPipelineKey == 3);
    assert(std::memcmp(&before, &gstate, sizeof(gstate)) == 0);
    gFrameBeginStats.valid = 1; gFrameBeginStats.pipelineMs = 40;
    assert(getFrameBeginStats(&stats) && stats.pipelineMs == 40);
    resetFrameBeginStats();
    assert(!getFrameBeginStats(&stats) && stats.pipelineMs == 0);
    assert(!getFrameBeginStats(nullptr));
    std::puts("PASS: opaque key reuse, independent-depth warm-up, strict/effects isolation, deferred compile counters/state restoration and fresh begin metrics");
}
