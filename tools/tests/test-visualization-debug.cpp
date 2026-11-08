#include "../../librw/src/vulkan/DebugVisualization.h"
#include <cassert>
#include <cstdio>
using namespace rw::vulkan::DebugVisualization;
int main() {
    Reset();
    assert(!MeshDebug() && !IsOverdraw());
    for(int i=0;i<LayerCount;++i) assert(Show((Layer)i));
    std::vector<uint16_t> edges;
    const uint16_t list[]={0,1,2,2,3,0,9,9,9,1,2};
    AppendEdges(list,11,false,edges);
    assert((edges==std::vector<uint16_t>{0,1,1,2,2,0,2,3,3,0,0,2}));
    edges.clear();
    const uint16_t strip[]={0,1,2,2,3,3,4,5};
    AppendEdges(strip,8,true,edges);
    assert((edges==std::vector<uint16_t>{0,1,1,2,2,0,3,4,4,5,5,3}));
    // Calls for adjacent material meshes must not create cross-material edges.
    edges.clear();
    AppendEdges(list,3,false,edges); AppendEdges(list+3,3,false,edges);
    assert(edges.size()==12 && edges[6]==2 && edges[11]==2);
    for(unsigned count=0;count<3;++count){
        edges.clear();AppendEdges(list,count,false,edges);assert(edges.empty());
        AppendEdges(list,count,true,edges);assert(edges.empty());
    }
    const uint16_t large[]={65535,1,0};edges.clear();AppendEdges(large,3,false,edges);
    assert(edges[0]==65535 && edges[5]==65535);
    for(int mesh=0;mesh<MeshCount;++mesh) for(int shading=0;shading<ShadingCount;++shading){
        Get().mesh=mesh;Get().shading=shading;
        assert(MeshDebug()==(mesh!=Solid||shading!=Lit));
        assert(IsOverdraw()==(shading==Overdraw||shading==DepthOverdraw));
        assert(IsHiddenWire()==(mesh==HiddenWire&&!IsOverdraw()));
    }
    Get().bounds=Get().collision=Get().velocity=true; Get().range=100;
    for(auto &visible:Get().visible) visible=false;
    Reset();assert(Get().mesh==Solid && Get().shading==Lit && Get().range==50);
    assert(!Get().bounds&&!Get().collision&&!Get().velocity);
    for(int i=0;i<LayerCount;++i) assert(Show((Layer)i));
    puts("Visualization topology, all view combinations, reset and default visibility: PASS");
}
