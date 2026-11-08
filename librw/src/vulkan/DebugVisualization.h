#pragma once
#include <cstdint>
#include <vector>
namespace rw { namespace vulkan { namespace DebugVisualization {
enum Mesh { Solid, Wire, SolidWire, HiddenWire, MeshCount };
enum Shading { Lit, Albedo, Clay, Normals, VertexColour, UVChecker, Lighting, Depth, Overdraw, DepthOverdraw, ShadingCount };
enum Layer { Sky, Buildings, Roads, Pedestrians, Vehicles, Objects, Water, Particles, Shadows, LayerCount };
struct Settings {
    int mesh = Solid, shading = Lit;
    bool visible[LayerCount] = {true,true,true,true,true,true,true,true,true};
    bool bounds = false, collision = false, velocity = false;
    int range = 50;
};
// Session-only, shared by the game and statically linked Vulkan backend.
inline Settings &Get() { static Settings settings; return settings; }
inline void Reset() { Get() = Settings(); }
inline bool MeshDebug() { return Get().mesh != Solid || Get().shading != Lit; }
inline bool Show(Layer layer) { return Get().visible[layer]; }
inline bool IsOverdraw() { return Get().shading == Overdraw || Get().shading == DepthOverdraw; }
inline bool IsHiddenWire() { return Get().mesh == HiddenWire && !IsOverdraw(); }
// Preserve material boundaries and skip degenerate strip connectors.
inline void AppendEdges(const uint16_t *indices, unsigned count, bool strip,
                        std::vector<uint16_t> &edges) {
    for(unsigned i=0; i+2<count; i += strip ? 1 : 3) {
        const auto a=indices[i], b=indices[i+1], c=indices[i+2];
        if(a==b || b==c || a==c) continue;
        edges.insert(edges.end(), {a,b,b,c,c,a});
    }
}
} } }
