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
    started=std::chrono::steady_clock::now();grid.Clear();building=true;++stats.builds;
    return 1;
}
void addWorldOccluder(Atomic *a){
    if(!building)return;
    ++stats.occluderCandidates;
    if(stats.occluders>=WorldOcclusion::MaxOccluders||stats.triangles>=WorldOcclusion::MaxTriangles||stats.rasterTests>=WorldOcclusion::MaxRasterTests){++stats.budget;return;}
    auto *h=source(a,true);
    if(!h||h->totalNumIndex%3||!h->totalNumIndex){++stats.invalid;return;}
    double m[2][16];matrices(a,m);
    const auto *v=(const WorldVertex*)h->vertexBuffer;
    const unsigned faces=h->totalNumIndex/3;
    const unsigned count=std::min({faces,WorldOcclusion::PerObjectTriangles,WorldOcclusion::MaxTriangles-stats.triangles});
    const unsigned previous=stats.cells;
    for(unsigned i=0;i<count;++i){
        if(stats.rasterTests>=WorldOcclusion::MaxRasterTests)break;
        const unsigned face=unsigned(uint64(i)*faces/count);
        const uint16 *ix=h->indexBuffer+face*3;
        if(ix[0]>=h->totalNumVertex||ix[1]>=h->totalNumVertex||ix[2]>=h->totalNumVertex){++stats.invalid;continue;}
        ++stats.triangles;
        for(unsigned eye=0;eye<2;++eye){
            WorldOcclusion::Point p[3];bool valid=true;
            for(unsigned k=0;k<3;++k){
                const auto &vtx=v[ix[k]];const double point[3]={vtx.position[0],vtx.position[1],vtx.position[2]};
                if(!WorldOcclusion::Project(m[eye],point,p[k])){valid=false;break;}
            }
            if(valid)grid.eye[eye].Raster(p[0],p[1],p[2],stats);
        }
    }
    if(stats.cells>previous)++stats.occluders;
}
void finishWorldOcclusion(void){
    if(!building)return;
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
