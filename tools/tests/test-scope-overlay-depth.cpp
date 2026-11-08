// Extract the production pipeline selection and surface-mode blocks from vkim.cpp.
#include <cassert>
#include <cstdio>
#include <initializer_list>
using bool32 = int;
using uint32 = unsigned;
using VkPipeline = unsigned;
using VkPrimitiveTopology = unsigned;
enum { SHADER_IM2D = 1, SHADER_IM3D = 2 };
static bool32 gImmediate2DScope, gImmediate2DStrictDepth;
static bool32 gImmediate2DLoadingArtwork, gImmediate2DFullscreen;
static struct { bool32 wristPanelOffscreen; } gvk;
static struct { bool32 zTestEnabled, zWriteEnabled; } gstate;
#include "scope-overlay-strict-production.inc"
static bool32 selectedTest, selectedWrite, selectedStrict;
static VkPipeline pipelineResult;
static VkPipeline getPipeline(uint32, VkPrimitiveTopology)
{
    selectedTest = gstate.zTestEnabled;
    selectedWrite = gstate.zWriteEnabled;
    selectedStrict = getImmediate2DStrictDepth();
    return pipelineResult;
}
static VkPipeline selectPipeline(uint32 shader)
{
    VkPrimitiveTopology topology = 3;
#include "scope-overlay-depth-production.inc"
    return pipeline;
}
static float surfaceMode(uint32 shader)
{
    struct { float surfaceProps[4]; } push = {};
#include "scope-overlay-surface-production.inc"
    return push.surfaceProps[0];
}
int main()
{
    int cases = 0;
    for(uint32 shader : {SHADER_IM2D, SHADER_IM3D})
    for(int scope : {0, 1})
    for(int test : {0, 1})
    for(int write : {0, 1})
    for(int strict : {0, 1})
    for(unsigned result : {0u, 17u}) {
        gImmediate2DScope = scope;
        gImmediate2DStrictDepth = strict;
        gstate = {test, write};
        pipelineResult = result;
        assert(selectPipeline(shader) == result);
        const bool overlay = scope && shader == SHADER_IM2D;
        assert(selectedTest == (overlay ? 0 : test));
        assert(selectedWrite == (overlay ? 0 : write));
        if(overlay) assert(!selectedStrict);
        assert(gstate.zTestEnabled == test && gstate.zWriteEnabled == write);
        assert(surfaceMode(shader) == (overlay ? 3.0f : 1.0f));
        ++cases;
    }
    // Leaving scope mode restores radar masking and normal UI depth behavior.
    gImmediate2DScope = 0;
    gImmediate2DStrictDepth = 1;
    assert(getImmediate2DStrictDepth());
    gImmediate2DLoadingArtwork = 1;
    assert(surfaceMode(SHADER_IM2D) == 3.0f);
    gImmediate2DLoadingArtwork = 0;
    gImmediate2DFullscreen = 1;
    assert(surfaceMode(SHADER_IM2D) == 2.0f);
    std::printf("Scope overlay depth/state restoration passed (%d cases).\n", cases);
}
