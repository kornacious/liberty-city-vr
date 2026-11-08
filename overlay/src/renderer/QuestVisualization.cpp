#include "common.h"
#include "QuestVisualization.h"
#ifdef RW_VULKAN
#include "vulkan/DebugVisualization.h"
#include "Entity.h"
#include "Building.h"
#include "Physical.h"
#include "ColModel.h"
#include "Camera.h"
#include "vulkan/rwvk.h"
#include "Timer.h"
#include <unordered_set>

namespace QuestVisualization {
using namespace rw::vulkan::DebugVisualization;
bool Visible(CEntity *e) {
    if(!e) return false;
    if(e->IsPed()) return Show(Pedestrians);
    if(e->IsVehicle()) return Show(Vehicles);
    if(e->IsBuilding()) return Show(((CBuilding*)e)->GetIsATreadable() ? Roads : Buildings);
    return Show(Objects);
}
static std::vector<RwIm3DVertex> vertices;
static unsigned lineBudget;
static std::vector<RwImVertexIndex> lineIndices;
static void Line(const CVector &a,const CVector &b,uint32 color) {
    if(!lineBudget) return;
    --lineBudget;
    RwIm3DVertex v[2];
    RwIm3DVertexSetPos(&v[0],a.x,a.y,a.z);
    RwIm3DVertexSetPos(&v[1],b.x,b.y,b.z);
    for(auto &point:v){
        RwIm3DVertexSetRGBA(&point,color>>24,(color>>16)&255,(color>>8)&255,color&255);
        vertices.push_back(point);
    }
}
static void Box(const CMatrix &matrix,const CColBox &box,uint32 color) {
    CVector corners[8];
    for(int i=0;i<8;++i) corners[i]=matrix*CVector(
        (i&1)?box.max.x:box.min.x,(i&2)?box.max.y:box.min.y,(i&4)?box.max.z:box.min.z);
    for(int i=0;i<8;++i) for(int bit=1;bit<=4;bit*=2)
        if(!(i&bit)) Line(corners[i],corners[i|bit],color);
}
void Overlay(CEntity *e) {
    const auto &settings=Get();
    if(!settings.bounds && !settings.collision && !settings.velocity) return;
    if(!Visible(e) || !e->m_rwObject || !e->GetColModel()) return;
    if((e->GetPosition()-TheCamera.GetPosition()).MagnitudeSqr() > float(settings.range*settings.range)) return;
    // Entity render callbacks can submit the same object several times.
    static uint32 frame=~0u;
    static std::unordered_set<CEntity*> drawn;
    if(frame!=CTimer::GetFrameCounter()){ frame=CTimer::GetFrameCounter(); drawn.clear(); lineBudget=16384; }
    if(!lineBudget || drawn.size()>=256 || !drawn.insert(e).second) return;
    rw::vulkan::flushStaticInstances();
    const RwRenderState states[]={rwRENDERSTATETEXTURERASTER,rwRENDERSTATEZTESTENABLE,
        rwRENDERSTATEZWRITEENABLE,rwRENDERSTATEVERTEXALPHAENABLE,rwRENDERSTATESRCBLEND,
        rwRENDERSTATEDESTBLEND,rwRENDERSTATEFOGENABLE};
    void *saved[7]={};
    for(int i=0;i<7;++i) RwRenderStateGet(states[i],&saved[i]);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,nil);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,(void*)TRUE);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,(void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,(void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,(void*)FALSE);
    const CColModel &col=*e->GetColModel();
    vertices.clear();
    const CMatrix &matrix=e->GetMatrix();
    if(settings.collision){
        for(int i=0;i<col.numBoxes && lineBudget;++i) Box(matrix,col.boxes[i],0x40FF40FF);
        for(int i=0;i<col.numTriangles && lineBudget;++i){
            const auto &t=col.triangles[i];
            const CVector a=matrix*col.vertices[t.a].Get();
            const CVector b=matrix*col.vertices[t.b].Get();
            const CVector c=matrix*col.vertices[t.c].Get();
            Line(a,b,0x40FF40FF); Line(b,c,0x40FF40FF); Line(c,a,0x40FF40FF);
        }
        for(int i=0;i<col.numSpheres && lineBudget;++i){
            const auto &sphere=col.spheres[i];
            for(int axis=0;axis<3;++axis) for(int step=0;step<16;++step){
                const float a=step*TWOPI/16.0f,b=(step+1)*TWOPI/16.0f;
                CVector p=sphere.center,q=sphere.center;
                if(axis==0){p.y+=Cos(a)*sphere.radius;p.z+=Sin(a)*sphere.radius;q.y+=Cos(b)*sphere.radius;q.z+=Sin(b)*sphere.radius;}
                if(axis==1){p.x+=Cos(a)*sphere.radius;p.z+=Sin(a)*sphere.radius;q.x+=Cos(b)*sphere.radius;q.z+=Sin(b)*sphere.radius;}
                if(axis==2){p.x+=Cos(a)*sphere.radius;p.y+=Sin(a)*sphere.radius;q.x+=Cos(b)*sphere.radius;q.y+=Sin(b)*sphere.radius;}
                Line(matrix*p,matrix*q,0x40FF40FF);
            }
        }
        for(int i=0;i<col.numLines && lineBudget;++i)
            Line(matrix*col.lines[i].p0,matrix*col.lines[i].p1,0x40FF40FF);
    }
    if(settings.bounds) Box(matrix,col.boundingBox,0x00FFFFFF);
    if(settings.velocity && (e->IsPed() || e->IsVehicle() || e->IsObject())){
        const CVector start=e->GetPosition();
        // Native speeds are game units per 1/50-second simulation step.
        Line(start,start+((CPhysical*)e)->m_vecMoveSpeed*50.0f,0xFFFF00FF);
    }
    if(!vertices.empty() && RwIm3DTransform(vertices.data(),(uint32)vertices.size(),nil,rwIM3D_ALLOPAQUE)){
        lineIndices.resize(vertices.size());
        for(size_t i=0;i<lineIndices.size();++i) lineIndices[i]=(RwImVertexIndex)i;
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST,lineIndices.data(),(int32)lineIndices.size());
        RwIm3DEnd();
    }
    for(int i=0;i<7;++i) RwRenderStateSet(states[i],saved[i]);
}
}
#else
namespace QuestVisualization {
bool Visible(CEntity *) { return true; }
void Overlay(CEntity *) {}
}
#endif
