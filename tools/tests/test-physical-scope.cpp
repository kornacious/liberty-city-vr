#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <limits>
#include "../../librw/src/vulkan/PhysicalScopeMath.h"
#include "../../overlay/src/vr/QuestScopeLens.h"

using uint32 = uint32_t;
using bool32 = int;
using float32 = float;
using VkPipeline = uint64_t;
using VkDescriptorSet = uint64_t;
using VkBuffer = uint64_t;
using VkDeviceSize = uint64_t;
using VkCommandBuffer = uint64_t;
using VkPipelineLayout = uint64_t;
struct PushConstants { float model[16]; };
static const uint32 VK_PIPELINE_BIND_POINT_GRAPHICS = 1;
static const uint32 VK_SHADER_STAGE_VERTEX_BIT = 2;
static const uint32 VK_SHADER_STAGE_FRAGMENT_BIT = 4;
static const uint32 VK_INDEX_TYPE_UINT16 = 5;
static const int nil = 0;
static bool32 gScopeCapture, gScopeFrameValid;
static int gScopeDrawRole;
static struct { VkCommandBuffer frameCommands; bool32 wristPanelOffscreen; float worldToPlay[16]; } gvk;
struct Sphere { struct { float x,y,z; } center; float radius; };
struct Atomic { Sphere sphere; const Sphere *getWorldBoundingSphere() const { return &sphere; } };
bool32 capturePhysicalScopeDraw(VkPipeline, VkDescriptorSet, const PushConstants &,
    VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32, uint32,
    const uint32 * = nullptr, Atomic * = nullptr);
struct Call { uint32 kind; uint64_t a,b,c; };
static std::vector<Call> calls;
static VkPipelineLayout getPipelineLayout() { return 44; }
static VkDescriptorSet getBoneDescriptor() { return 55; }
static void vkCmdBindPipeline(VkCommandBuffer, uint32, VkPipeline p)
{ calls.push_back({1,p,0,0}); }
static void vkCmdBindDescriptorSets(VkCommandBuffer, uint32, VkPipelineLayout,
    uint32 first, uint32 count, const VkDescriptorSet *sets, uint32 dynamicCount,
    const uint32 *offset)
{ assert(count == 1); calls.push_back({2,first,*sets,dynamicCount ? *offset : 0}); }
static void vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, uint32, uint32,
    size_t, const PushConstants *p)
{ calls.push_back({3,(uint64_t)p->model[0],0,0}); }
static void vkCmdBindVertexBuffers(VkCommandBuffer, uint32, uint32,
    const VkBuffer *p, const VkDeviceSize *offset)
{ calls.push_back({4,*p,*offset,0}); }
static void vkCmdBindIndexBuffer(VkCommandBuffer, VkBuffer p, VkDeviceSize off, uint32)
{ calls.push_back({5,p,off,0}); }
static void vkCmdDrawIndexed(VkCommandBuffer, uint32 count, uint32, uint32 first, int, uint32)
{ calls.push_back({6,count,first,0}); }
static void vkCmdDraw(VkCommandBuffer, uint32 count, uint32, uint32 first, uint32)
{ calls.push_back({7,count,first,0}); }
#include "physical-scope-production.inc"

static bool wanted, active, overlay;
static int mode, weapon;
static const int WEAPONTYPE_SNIPERRIFLE = 7;
namespace OculusVR {
static bool IsTrackedScopeActive() { return active; }
static bool IsTrackedScopeOverlayActive() { return overlay; }
static int GetQuestScopeAimMode() { return mode; }
static int GetTrackedScopeWeaponType() { return weapon; }
}
namespace rw { namespace vulkan {
static void setPhysicalScopeWanted(bool32 value) { wanted = value != 0; }
} }
#include "physical-scope-zoom.inc"

static void near(float a, float b) { assert(fabsf(a-b) < 0.0001f); }
static void project(const float m[16], const float p[3], float out[3])
{
	float clip[4];
	for(int i=0;i<4;i++) clip[i]=m[i]*p[0]+m[4+i]*p[1]+m[8+i]*p[2]+m[12+i];
	for(int i=0;i<3;i++) out[i]=clip[i]/clip[3];
}
static void testProjection()
{
	float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
	float p[3]={2,3,4}, r[3]={1,0,0}, u[3]={0,0,1}, f[3]={0,1,0}, m[16];
	const float fov=0.2f, aspect=1.2f;
	assert(PhysicalScopeMath::projection(m,identity,p,r,u,f,fov,aspect));
	float centre[3]={2,13,4}, result[3]; project(m,centre,result);
	near(result[0],0); near(result[1],0); near(result[2],0.995f);
	float upper[3]={2,13,4+10*tanf(fov/2)}; project(m,upper,result); near(result[1],-1);
	float edge[3]={2+10*tanf(fov/2)*aspect,13,4}; project(m,edge,result); near(result[0],1);
	float sphere[4]={2,13,4,0.5f};
	assert(PhysicalScopeMath::sphereVisible(m,sphere));
	sphere[0]=20; assert(!PhysicalScopeMath::sphereVisible(m,sphere));
	sphere[0]=2; sphere[1]=1; assert(!PhysicalScopeMath::sphereVisible(m,sphere));
	// A rotated/translated room-scale anchor must not move a world target
	// within the optic. Fold the target as the production model path does.
	float fold[16]={0,1,0,0,-1,0,0,0,0,0,1,0,7,8,9,1};
	assert(PhysicalScopeMath::projection(m,fold,p,r,u,f,fov,aspect));
	float folded[3]={7-centre[1],8+centre[0],9+centre[2]}; project(m,folded,result);
	near(result[0],0); near(result[1],0); near(result[2],0.995f);
	// Rifle roll changes the image basis, and the centre still follows its bore.
	float rollR[3]={0,0,1}, rollU[3]={-1,0,0};
	assert(PhysicalScopeMath::projection(m,identity,p,rollR,rollU,f,fov,aspect));
	project(m,centre,result); near(result[0],0); near(result[1],0);
	assert(!PhysicalScopeMath::projection(m,identity,p,r,u,f,0,aspect));
	assert(!PhysicalScopeMath::projection(m,identity,p,r,u,f,fov,0));
	float bad[3]={std::numeric_limits<float>::quiet_NaN(),0,0};
	assert(!PhysicalScopeMath::projection(m,identity,p,r,u,bad,fov,aspect));
}
struct Triangle { uint16_t v[3]; };
static void cap(std::vector<QuestScopeLens::Vec> &v, std::vector<Triangle> &t,
    float x, float z, float radius)
{
	const uint16_t base=(uint16_t)v.size(); v.push_back({x,0,z});
	for(int i=0;i<8;i++){
		float angle=i*6.2831853f/8;
		v.push_back({x,radius*cosf(angle),z+radius*sinf(angle)});
	}
	for(int i=0;i<8;i++) t.push_back({{base,(uint16_t)(base+1+i),(uint16_t)(base+1+(i+1)%8)}});
}
static void testLens()
{
	std::vector<QuestScopeLens::Vec> v; std::vector<Triangle> t;
	cap(v,t,-0.3f,0.02f,0.05f); // stock, below bore
	cap(v,t,0.8f,0.09f,0.01f); // muzzle
	cap(v,t,0.3f,0.16f,0.03f); // front scope cap
	cap(v,t,0.0f,0.16f,0.03f); // rear scope cap
	QuestScopeLens::Lens lens;
	assert(QuestScopeLens::fit(v.data(),(int)v.size(),t.data(),(int)t.size(),
		{1,0,0},{0,0,1},{0.8f,0,0.09f},lens));
	near(lens.centre.x,0); near(lens.centre.z,0.16f);
	near(lens.width,0.027f); near(lens.height,0.027f);
	// The mesh cap may be slightly angled relative to the calibrated bullet
	// direction. Fit its actual plane rather than slicing triangles by bore.
	assert(QuestScopeLens::fit(v.data(),(int)v.size(),t.data(),(int)t.size(),
		{0.9891f,-0.1477f,0},{0,0,1},{0.8f,0,0.09f},lens));
	near(lens.centre.x,0); near(lens.centre.z,0.16f);
	near(lens.width,0.027f); near(lens.height,0.027f);
	assert(!QuestScopeLens::fit(v.data(),(int)v.size(),t.data(),16,
		{1,0,0},{0,0,1},{0.8f,0,0.09f},lens));
	assert(!QuestScopeLens::fit(v.data(),(int)v.size(),t.data(),(int)t.size(),
		{0,0,0},{0,0,1},{0,0,0},lens));
}
static void testCaptureReplay()
{
	gScopeCapture=1; gScopeFrameValid=1; gScopeDrawRole=0;
	PushConstants push={}; push.model[0]=99; uint32 bones=256;
	assert(!capturePhysicalScopeDraw(10,11,push,12,64,13,32,18,4,&bones));
	push.model[0]=3; bones=512;
	assert(gScopeWorldDraws.size()==1);
	replayScopeDraws(gScopeWorldDraws);
	assert(calls.size()==7);
	assert(calls[0].kind==1 && calls[0].a==10);
	assert(calls[1].kind==2 && calls[1].a==1 && calls[1].b==11);
	assert(calls[2].kind==2 && calls[2].a==2 && calls[2].b==55 && calls[2].c==256);
	assert(calls[3].kind==3 && calls[3].a==99); // copied push constants
	assert(calls[4].kind==4 && calls[4].a==12 && calls[4].b==64);
	assert(calls[5].kind==5 && calls[5].a==13 && calls[5].b==32);
	assert(calls[6].kind==6 && calls[6].a==18 && calls[6].b==4);
	gScopeDrawRole=1;
	assert(!capturePhysicalScopeDraw(10,11,push,12,0,0,0,3,0,nullptr));
	assert(gScopeWorldDraws.size()==1); // hands and weapon excluded
	gScopeDrawRole=0; gvk.wristPanelOffscreen=1;
	assert(!capturePhysicalScopeDraw(10,11,push,12,0,0,0,3,0,nullptr));
	assert(gScopeWorldDraws.size()==1);
	gvk.wristPanelOffscreen=0; gScopeDrawRole=2;
	assert(capturePhysicalScopeDraw(10,11,push,12,0,0,0,3,0,nullptr));
	assert(gScopeWorldDraws.size()==1 && gScopeLensDraws.size()==1);
	calls.clear(); replayScopeDraws(gScopeLensDraws);
	assert(calls.back().kind==7 && calls.back().a==3);
	gScopeCapture=0;
	assert(!capturePhysicalScopeDraw(10,11,push,12,0,0,0,3,0,nullptr));
	assert(gScopeLensDraws.size()==1);
	// Bounds are captured in the same play space as their draw matrices.
	gScopeCapture=1; gScopeDrawRole=0;
	gvk.worldToPlay[0]=gvk.worldToPlay[5]=gvk.worldToPlay[10]=gvk.worldToPlay[15]=1;
	Atomic object={{{100,100,100},1}};
	capturePhysicalScopeDraw(10,11,push,12,0,0,0,3,0,nullptr,&object);
	assert(gScopeWorldDraws.back().bounds[0]==100);
	float projection[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
	calls.clear();
	std::vector<ScopeDraw> outside={gScopeWorldDraws.back()};
	replayScopeDraws(outside,projection);
	assert(calls.empty());
}
static void testModes()
{
	weapon=7; active=true; mode=2; overlay=false;
	near(VrScopeZoomFactor(),1); assert(wanted);
	mode=1; overlay=true; near(VrScopeZoomFactor(),2.5f); assert(!wanted);
	mode=0; overlay=false; active=false; near(VrScopeZoomFactor(),1); assert(!wanted);
	mode=2; active=true; near(VrScopeZoomFactor(),1); assert(wanted);
	active=false; near(VrScopeZoomFactor(),1); assert(!wanted); // lowering clears request
	active=true; weapon=8; near(VrScopeZoomFactor(),1); assert(!wanted);
}
int main()
{
	testProjection(); testLens(); testCaptureReplay(); testModes();
	puts("Physical scope: projection, room-scale, roll, cap fitting, draw replay/exclusions and modes passed.");
}
