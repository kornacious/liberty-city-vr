#include <chrono>
#include <cstring>
#include <cassert>
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwvk.h"
#include "rwvkimpl.h"

#ifdef RW_VULKAN
namespace rw { namespace vulkan {
namespace {
bool enabled=false,building=false,ready=false;
WorldOcclusion::Stereo grid;
WorldOcclusion::Stats stats;
WorldOcclusion::Selection selection;
struct Source {InstanceDataHeader *header;double matrix[2][16];unsigned samples;int32 model;};
Source sources[WorldOcclusion::MaxSourceObjects];
unsigned sourceCount=0;
float views[2][16],world[16];
double clip[2][16];
std::chrono::steady_clock::time_point started;

bool mainView(){
    return gvk.inFrame&&gvk.firstPersonActive&&gvk.stereoCullPlanesValid&&
        !gvk.wristPanelOffscreen&&!physicalScopeCaptureActive()&&!worldEffectsActive()&&
        !gvk.im3dWaterDraw&&gvk.sgsrMode==SGSR_OFF;
}
bool sameView(){
    return mainView()&&!getWorldBackfaceEnabled()&&!std::memcmp(world,gvk.worldToPlay,sizeof(world))&&
        !std::memcmp(views,getSceneData()->viewProj,sizeof(views));
}
InstanceDataHeader *source(Atomic *a,bool opaque){
    if(!a||!a->getFrame()||!isDefaultWorldRenderCallback(a))return nil;
    auto *pipe=a->getPipeline();auto *g=a->geometry;
    if(!pipe||pipe->platform!=PLATFORM_VULKAN||
       ((ObjPipeline*)pipe)->instanceCB!=defaultInstanceCB||((ObjPipeline*)pipe)->renderCB!=defaultRenderCB||
       !g||g->numMorphTargets!=1||g->lockedSinceInst||(g->flags&Geometry::NATIVE)||
       !g->instData||g->instData->platform!=PLATFORM_VULKAN||!g->meshHeader)return nil;
    auto *h=(InstanceDataHeader*)g->instData;
    if(!h->orderBoundsValid||!h->vertexBuffer||!h->indexBuffer||!h->inst||!h->numMeshes||
       h->primType!=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST||h->serialNumber!=g->meshHeader->serialNum)return nil;
    if(opaque)for(unsigned i=0;i<h->numMeshes;++i){
        const auto &mesh=h->inst[i];auto *m=mesh.material;
        if(mesh.vertexAlpha||!m||m->color.alpha!=255||MatFX::getEffects(m)!=MatFX::NOTHING)return nil;
        auto *r=m->texture?m->texture->raster:nil;
        if(r&&(r->platform!=PLATFORM_VULKAN||(r->type&0xF)!=Raster::TEXTURE||
            (r->parent&&r->parent!=r)||rasterHasAlpha(r)))return nil;
        if(m->texture){
            const unsigned u=m->texture->getAddressU(),v=m->texture->getAddressV();
            if((u!=Texture::WRAP&&u!=Texture::MIRROR&&u!=Texture::CLAMP)||
               (v!=Texture::WRAP&&v!=Texture::MIRROR&&v!=Texture::CLAMP))return nil;
        }
    }
    return h;
}
void matrices(Atomic *a,double (&out)[2][16]){
    const auto *m=a->getFrame()->getLTM();
    const double object[16]={m->right.x,m->right.y,m->right.z,0,m->up.x,m->up.y,m->up.z,0,
        m->at.x,m->at.y,m->at.z,0,m->pos.x,m->pos.y,m->pos.z,1};
    for(unsigned e=0;e<2;++e)for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){
        out[e][col*4+row]=0;
        for(unsigned k=0;k<4;++k)out[e][col*4+row]+=clip[e][k*4+row]*object[col*4+k];
    }
}
}

void setWorldOcclusionEnabled(bool32 value){
    if(enabled!=(value!=0)){flushStaticInstances();enabled=value!=0;ready=building=false;}
}
bool32 getWorldOcclusionEnabled(void){return enabled;}
void resetWorldOcclusion(void){ready=building=false;stats={};}
WorldOcclusion::Stats getWorldOcclusionStats(void){return stats;}
bool32 beginWorldOcclusion(void){
    if(!enabled||building||ready)return 0;
    // Keep this experiment independent of the backface experiment: the map
    // rasterizes both sides of real triangles, matching the ordinary path.
    if(!mainView()||getWorldBackfaceEnabled()||!gstate.zTestEnabled||!gstate.zWriteEnabled||gstate.cullMode!=CULLNONE){++stats.invalid;return 0;}
#if defined(MIAMIVR_DEV_TOOLS) && MIAMIVR_DEV_TOOLS
    if(gvk.debugVizActive){++stats.invalid;return 0;}
#endif
    if(gstate.alphaTestFunction!=ALPHAALWAYS &&
       (gstate.alphaTestFunction!=ALPHAGREATEREQUAL||gstate.alphaTestRef>=254)){
        ++stats.invalid;return 0;
    }
    std::memcpy(views,getSceneData()->viewProj,sizeof(views));
    std::memcpy(world,gvk.worldToPlay,sizeof(world));
    for(unsigned e=0;e<2;++e)for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){
        clip[e][col*4+row]=0;
        for(unsigned k=0;k<4;++k)clip[e][col*4+row]+=double(views[e][k*4+row])*world[col*4+k];
    }
    started=std::chrono::steady_clock::now();grid.Clear();selection.Clear();sourceCount=0;building=true;++stats.builds;
    return 1;
}
void addWorldOccluder(Atomic *a,int32 model){
    if(!building)return;
    ++stats.occluderCandidates;
    if(stats.occluderCandidates>WorldOcclusion::MaxSourceObjects){++stats.budget;return;}
    auto *h=source(a,true);
    if(!h||h->totalNumIndex%3||!h->totalNumIndex){++stats.invalid;return;}
    const auto *v=(const WorldVertex*)h->vertexBuffer;
    const unsigned faces=h->totalNumIndex/3;
    auto &cache=h->occlusionSource;
    if(!cache.ready){
        const unsigned scan=std::min(faces,WorldOcclusion::MaxCacheScan);
        if(stats.cacheScanned+scan>WorldOcclusion::MaxCacheScanPerFrame){++stats.cacheDeferred;return;}
        stats.cacheScanned+=scan;++stats.cacheBuilds;
        cache.Build(faces,[&](unsigned face,WorldOcclusion::Point (&p)[3]){
            const uint16 *ix=h->indexBuffer+face*3;
            for(unsigned k=0;k<3;++k){
                if(ix[k]>=h->totalNumVertex){++stats.invalid;return false;}
                const auto &position=v[ix[k]].position;p[k]={position[0],position[1],position[2]};
            }
            return true;
        });
    }else ++stats.cacheHits;
    if(!cache.count)return;
    auto &src=sources[sourceCount++];src.header=h;src.samples=0;src.model=model;matrices(a,src.matrix);
    ++stats.sourceObjects;
}
void finishWorldOcclusion(void){
    if(!building)return;
    unsigned available=0;for(unsigned i=0;i<sourceCount;++i)available+=sources[i].header->occlusionSource.count;
    stats.triangles=WorldOcclusion::Distribute(sourceCount,
      [&](unsigned object){return sources[object].header->occlusionSource.count;},
      [&](unsigned object,unsigned rank){
        auto &src=sources[object];auto *h=src.header;
        ++src.samples;
        if(src.samples>stats.sourceTopSamples){stats.sourceTopSamples=src.samples;stats.sourceTopModel=src.model;}
        const unsigned face=h->occlusionSource.faces[rank];
        const uint16 *ix=h->indexBuffer+face*3;
        if(ix[0]>=h->totalNumVertex||ix[1]>=h->totalNumVertex||ix[2]>=h->totalNumVertex){++stats.invalid;return;}
        const auto *v=(const WorldVertex*)h->vertexBuffer;
        WorldOcclusion::Triangle triangle;triangle.owner=object;
        double score[2]={};
        for(unsigned eye=0;eye<2;++eye){
            auto &p=triangle.eye[eye];bool valid=true;
            for(unsigned k=0;k<3;++k){
                const auto &vtx=v[ix[k]];const double point[3]={vtx.position[0],vtx.position[1],vtx.position[2]};
                if(!WorldOcclusion::Project(src.matrix[eye],point,p[k])){valid=false;break;}
            }
            triangle.valid[eye]=valid;
            if(valid)score[eye]=WorldOcclusion::Priority(p);
        }
        // Favor usefulness to both eyes, while retaining one-eye contributors
        // that can complement other triangles. This score never proves hiding.
        triangle.priority=std::min(score[0],score[1])+.25*std::max(score[0],score[1]);
        if(!triangle.valid[0]&&!triangle.valid[1])++stats.sourceProjectionRejected;
        else if(triangle.priority<=0)++stats.sourceSmall;
        else ++stats.sourceProjected;
        selection.Add(triangle);
      });
    stats.sourceBudgetSkipped=available-stats.triangles;
    if(stats.sourceBudgetSkipped)++stats.budget;
    stats.selectedTriangles=selection.Count();
    bool contributing[WorldOcclusion::MaxSourceObjects]={};
    selection.Visit([&](const WorldOcclusion::Triangle &t){
        if(stats.rasterTests>=WorldOcclusion::MaxRasterTests||stats.rasterRows>=WorldOcclusion::MaxRasterRows){++stats.budget;return;}
        if(!contributing[t.owner]&&stats.occluders>=WorldOcclusion::MaxOccluders){++stats.budget;return;}
        const unsigned previous=stats.cells;
        for(unsigned eye=0;eye<2;++eye)if(t.valid[eye]){
            const auto &p=t.eye[eye];grid.eye[eye].Raster(p[0],p[1],p[2],stats);
        }
        if(stats.cells>previous&&!contributing[t.owner]){contributing[t.owner]=true;++stats.occluders;}
    });
    building=false;ready=sameView();
    if(!ready)++stats.invalid;
    stats.leftCells=grid.eye[0].Coverage();stats.rightCells=grid.eye[1].Coverage();
    ready=ready&&stats.leftCells>0&&stats.rightCells>0;
    stats.buildMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
bool32 isWorldOccluded(Atomic *a){
    if(!enabled||!ready)return 0;
    if(stats.queryTests>=WorldOcclusion::MaxQueryTests){++stats.budget;return 0;}
    const auto time=std::chrono::steady_clock::now();++stats.candidates;
    const auto result=[&](){
        if(!sameView()||!gstate.zTestEnabled){++stats.invalid;return false;}
        auto *h=source(a,false);
        if(!h){++stats.invalid;return false;}
        double m[2][16];matrices(a,m);WorldOcclusion::Box box[2];
        for(unsigned e=0;e<2;++e){
            WorldOcclusion::Point corners[8];
            for(unsigned c=0;c<8;++c){
                double p[3];for(unsigned axis=0;axis<3;++axis)p[axis]=(c&(1<<axis))?h->orderMax[axis]:h->orderMin[axis];
                if(!WorldOcclusion::Project(m[e],p,corners[c])){++stats.invalid;return false;}
            }
            box[e]=WorldOcclusion::Bounds(corners);
            // If visible to the left eye, avoid projecting/testing the right.
            if(!grid.eye[e].Hidden(box[e],stats))return false;
        }
        ++stats.rejected;stats.indices+=h->totalNumIndex;stats.vertices+=h->totalNumVertex;return true;
    }();
    if(!result)++stats.visible;
    stats.queryMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-time).count();
    return result;
}
} }
#endif
