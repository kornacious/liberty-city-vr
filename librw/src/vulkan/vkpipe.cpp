#include "DebugVisualization.h"
#include "VehicleAlphaPolicy.h"
#include "UniformAlphaPolicy.h"
#include "NeoRim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <chrono>
#include <unordered_map>

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

#define PLUGIN_ID ID_DRIVER

#ifdef RW_VULKAN

#include "VulkanPlatform.h"

namespace rw {
namespace vulkan {

static bool32 vehicleAlphaPass;
static bool32 hideVehicleWindows;

bool32 setHideVehicleWindows(bool32 enabled)
{
	const bool32 previous = hideVehicleWindows;
	hideVehicleWindows = enabled;
	return previous;
}

bool32 getHideVehicleWindows() { return hideVehicleWindows; }
static bool32 dynamicObjectPass;
static NeoRim::Selection gPedestrianRim;
bool32 setPedestrianRimPass(bool32 enabled) { return gPedestrianRim.SetPedestrian(enabled != 0); }
static AtomicSubmissionStats gAtomicSubmissionStats;
static WorldBackface::Stats gWorldBackfaceStats;
static bool32 gOpaqueOrderEnabled;
static Atomic::RenderCB gOpaqueOrderDefaultWrapper;
static OpaqueOrder::Stats gOpaqueOrderStats;
static UniformAlphaPolicy::BoundStats gAlphaBoundStats;
static StaticReuseAudit::Recording<> gStaticReuse;
void resetStaticReuseSample(bool32 sampled,uint64 sourceFrame){gStaticReuse.Reset(sampled!=0,sourceFrame);}
void staticReuseBoundary(void){gStaticReuse.Boundary();}
void finishStaticReuseDraw(void){gStaticReuse.FinishDraw();}
StaticReuseAudit::Stats getStaticReuseStats(void){return gStaticReuse.stats;}
static bool32 gOpaqueMeshBatchEnabled;
static OpaqueMeshBatch::Stats gOpaqueMeshBatchStats;
void setOpaqueMeshBatchEnabled(bool32 enabled){gOpaqueMeshBatchEnabled=enabled!=0;}
bool32 getOpaqueMeshBatchEnabled(void){return gOpaqueMeshBatchEnabled;}
OpaqueMeshBatch::Stats getOpaqueMeshBatchStats(void){return gOpaqueMeshBatchStats;}
static bool32 gMaskedAlphaPassSkipEnabled;
void setMaskedAlphaPassSkipEnabled(bool32 enabled){gMaskedAlphaPassSkipEnabled=enabled!=0;}
bool32 getMaskedAlphaPassSkipEnabled(void){return gMaskedAlphaPassSkipEnabled;}
bool32 isDefaultWorldRenderCallback(Atomic *atomic){return atomic&&OpaqueOrder::IsDefaultCallback(atomic->renderCB,Atomic::defaultRenderCB,gOpaqueOrderDefaultWrapper);}
void setOpaqueOrderDefaultRenderWrapper(Atomic::RenderCB callback){gOpaqueOrderDefaultWrapper=callback;}
void setOpaqueOrderEnabled(bool32 enabled){gOpaqueOrderEnabled=enabled!=0;}
bool32 getOpaqueOrderEnabled(void){return gOpaqueOrderEnabled;}
OpaqueOrder::Stats getOpaqueOrderStats(void){return gOpaqueOrderStats;}
void recordOpaqueOrderSwaps(uint32 swaps){gOpaqueOrderStats.swaps+=swaps;}
static void resetStaticInstanceStats(void);
void resetAtomicSubmissionStats(void){resetStaticInstanceStats();gAtomicSubmissionStats={};gOpaqueOrderStats={};gAlphaBoundStats={};gOpaqueMeshBatchStats={};gWorldBackfaceStats={};resetWorldOcclusion();resetStaticReuseSample(0,0);}
AtomicSubmissionStats getAtomicSubmissionStats(void){return gAtomicSubmissionStats;}
UniformAlphaPolicy::BoundStats getAlphaBoundStats(void){return gAlphaBoundStats;}

static bool32 gWorldBackfaceEnabled;

void setWorldBackfaceEnabled(bool32 enabled){
    if(gWorldBackfaceEnabled!=(enabled!=0)){flushStaticInstances();gWorldBackfaceEnabled=enabled!=0;}
}
bool32 getWorldBackfaceEnabled(void){return gWorldBackfaceEnabled;}
WorldBackface::Stats getWorldBackfaceStats(void){return gWorldBackfaceStats;}
static bool32 gStaticInstancesEnabled;
static StaticInstances::Stats gStaticInstanceStats;
static DrawProfile::Context gInstanceOwner,gInstanceParents[DrawProfile::ContextDepth];
static unsigned gInstanceDepth;
struct StaticInstanceDraw {
	VkPipeline pipeline=VK_NULL_HANDLE,instanced=VK_NULL_HANDLE;
	VkDescriptorSet texture=VK_NULL_HANDLE;
	VkBuffer vertices=VK_NULL_HANDLE,indices=VK_NULL_HANDLE;
	uint32 first=0,count=0,flags=0;
	PushConstants push{};
	DrawProfile::Context owner;
	uint64 object=0;
	float position[3]{};
	StaticInstances::Bounds bounds;
	bool opaque=false;
};
static StaticInstanceDraw gInstanceDraws[StaticInstances::Window];
static unsigned gInstanceCount;
static void resetStaticInstanceStats(void){assert(gInstanceCount==0);gStaticInstanceStats={};}

bool32 getStaticInstancesEnabled(void){return gStaticInstancesEnabled;}
void setStaticInstancesEnabled(bool32 enabled){
	if(gStaticInstancesEnabled!=(enabled!=0)){flushStaticInstances();gStaticInstancesEnabled=enabled!=0;}
}
StaticInstances::Stats getStaticInstancesStats(void){return gStaticInstanceStats;}
bool32 beginStaticInstanceEntity(int32 model,int32 entityClass,const char *name,uint32 flags){
	if(!gStaticInstancesEnabled&&!gWorldBackfaceEnabled&&!getWorldOcclusionEnabled())return 0;
	if(gInstanceDepth<DrawProfile::ContextDepth)gInstanceParents[gInstanceDepth]=gInstanceOwner;
	gInstanceOwner=gInstanceDepth<DrawProfile::ContextDepth?DrawProfile::Entity(model,entityClass,name,flags):DrawProfile::Context{};
	++gInstanceDepth;return 1;
}
void endStaticInstanceEntity(void){
	if(gInstanceDepth && --gInstanceDepth<DrawProfile::ContextDepth)gInstanceOwner=gInstanceParents[gInstanceDepth];
}
static void emitStaticInstanceDraw(const StaticInstanceDraw &d,VkPipeline pipeline,
                                   VkBuffer transforms,VkDeviceSize transformOffset,unsigned instances){
	const VkCommandBuffer cb=gvk.frameCommands;
	const VkDeviceSize zero=0;
	vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
	vkCmdBindVertexBuffers(cb,0,1,&d.vertices,&zero);
	if(instances>1)vkCmdBindVertexBuffers(cb,1,1,&transforms,&transformOffset);
	vkCmdBindIndexBuffer(cb,d.indices,0,VK_INDEX_TYPE_UINT16);
	vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,getPipelineLayout(),1,1,&d.texture,0,nil);
	vkCmdPushConstants(cb,getPipelineLayout(),VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(d.push),&d.push);
	const bool sampled=gpuProfileSampling();
	DrawProfile::Context previousOwner;
	if(sampled){previousOwner=gvk.frames[gvk.activeFrame].gpuProfile.geometry.entity;
		gvk.frames[gvk.activeFrame].gpuProfile.geometry.entity=d.owner;}
	countGpuProfileDraw(d.count*instances,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,d.flags);
	if(sampled)gvk.frames[gvk.activeFrame].gpuProfile.geometry.entity=previousOwner;
	vkCmdDrawIndexed(cb,d.count,instances,d.first,0,0);
	gStaticInstanceStats.emittedIndices+=uint64(d.count)*instances;
}
void flushStaticInstances(void){
	if(!gInstanceCount)return;
	assert(gvk.inFrame);
	const auto start=std::chrono::steady_clock::now();
	bool used[StaticInstances::Window]={};
	for(unsigned i=0;i<gInstanceCount;++i){
		if(used[i])continue;
		unsigned selected[StaticInstances::MaxInstances];
		const auto match=[](const StaticInstanceDraw &a,const StaticInstanceDraw &b){
			return a.object!=b.object && a.pipeline==b.pipeline && a.instanced && a.instanced==b.instanced &&
				a.vertices==b.vertices && a.indices==b.indices && a.first==b.first && a.count==b.count &&
				a.texture==b.texture && a.flags==b.flags && a.owner.model==b.owner.model &&
				a.owner.flags==b.owner.flags &&
				memcmp(a.push.materialColour,b.push.materialColour,64)==0;
		};
		unsigned n=StaticInstances::Plan(gInstanceDraws,gInstanceCount,i,used,selected,match,gStaticInstanceStats);
		VkBuffer buffer=VK_NULL_HANDLE;VkDeviceSize offset=0;void *mapped=nil;
		if(n>1 && (gInstanceDraws[i].count>~0u/n || !allocateStaticInstanceTransforms(n,&buffer,&offset,&mapped))){
			++gStaticInstanceStats.allocationFailure;n=1;
		}
		if(n>1){
			for(unsigned j=0;j<n;++j){
				float *model=(float*)mapped+j*16;
				memcpy(model,gInstanceDraws[selected[j]].push.model,64);
				// RwModelMatrix restores the root-motion transport row likewise.
				model[3]=model[7]=model[11]=0;model[15]=1;
			}
			++gStaticInstanceStats.groups;gStaticInstanceStats.instances+=n;
			gStaticInstanceStats.savedDraws+=n-1;
		}
		emitStaticInstanceDraw(gInstanceDraws[i],n>1?gInstanceDraws[i].instanced:gInstanceDraws[i].pipeline,buffer,offset,n);
		for(unsigned j=0;j<n;++j)used[selected[j]]=true;
	}
	gInstanceCount=0;
	gStaticInstanceStats.flushMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}

struct AtomicMotionRecord
{
	float32 model[16];
	float32 motion[4];
	uint64 serial;
	bool32 initialized;
};

static std::unordered_map<Atomic*, AtomicMotionRecord> atomicMotionHistory;

void
resetAtomicMotionHistory(void)
{
	atomicMotionHistory.clear();
}

static void
packAtomicRootMotion(Atomic *atomic, bool32 potentiallyDynamic,
                     PushConstants &push)
{
	// Affine matrices always have (0,0,0,1) in this row. The shaders restore
	// those constants before use, so the four slots are safe debug-only
	// transport for previous-current root translation and validity.
	float32 motion[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if(gvk.sgsrMode != SGSR_OFF && potentiallyDynamic){
		AtomicMotionRecord &record = atomicMotionHistory[atomic];
		if(record.serial == gvk.motionFrameSerial){
			memcpy(motion, record.motion, sizeof(motion));
		}else{
			if(record.initialized && record.serial+1 == gvk.motionFrameSerial){
				const SceneData *scene = getSceneData();
				for(int32 eye = 0; eye < 2; eye++){
					// SGSR2 receives projection jitter separately. Root velocity is
					// therefore measured between unjittered frames, matching the
					// reference dynamic-motion contract.
					const float32 *currentVp =
						gvk.stereoViewProjectionUnjittered[eye];
					const float32 *previousVp = scene->previousViewProj[eye];
					const float32 *currentModel = push.model;
					const float32 *previousModel = record.model;
					float32 currentClip[4], previousClip[4];
					for(int32 row = 0; row < 4; row++){
						currentClip[row] =
							currentVp[row]*currentModel[12] +
							currentVp[4+row]*currentModel[13] +
							currentVp[8+row]*currentModel[14] +
							currentVp[12+row];
						previousClip[row] =
							previousVp[row]*previousModel[12] +
							previousVp[4+row]*previousModel[13] +
							previousVp[8+row]*previousModel[14] +
							previousVp[12+row];
					}
					if(isfinite(currentClip[3]) && isfinite(previousClip[3]) &&
					   fabsf(currentClip[3]) > 1.0e-6f &&
					   fabsf(previousClip[3]) > 1.0e-6f){
						const float32 x = currentClip[0]/currentClip[3] -
						                  previousClip[0]/previousClip[3];
						const float32 y = currentClip[1]/currentClip[3] -
						                  previousClip[1]/previousClip[3];
						if(isfinite(x) && isfinite(y) && x*x+y*y <= 4.0f){
							motion[eye*2+0] = x;
							motion[eye*2+1] = y;
						}
					}
				}
			}
			memcpy(record.model, push.model, sizeof(record.model));
			memcpy(record.motion, motion, sizeof(record.motion));
			record.serial = gvk.motionFrameSerial;
			record.initialized = 1;
		}
	}
	push.model[3] = motion[0];
	push.model[7] = motion[1];
	push.model[11] = motion[2];
	push.model[15] = motion[3];
}

void
setVehicleAlphaPass(bool32 enabled)
{
	vehicleAlphaPass = enabled;
}

void
setDynamicObjectPass(bool32 enabled)
{
	dynamicObjectPass = enabled;
}

void
destroyStaticBuffer(VkBuffer &buffer, VkDeviceMemory &memory)
{
	if(buffer != VK_NULL_HANDLE || memory != VK_NULL_HANDLE)
		retireBuffer(buffer, memory);
	buffer = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
}

// Uploads through a temporary staging buffer into device-local memory. World
// geometry is written once and drawn for the lifetime of the model, so the
// staging cost is paid back immediately by keeping the vertex fetch on fast
// memory.
bool32
uploadStaticBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
                   VkBuffer *bufferOut, VkDeviceMemory *memoryOut)
{
	if(!createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferOut, memoryOut))
		return 0;

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	if(!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
	                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                 &staging, &stagingMemory)){
		destroyStaticBuffer(*bufferOut, *memoryOut);
		return 0;
	}

	void *mapped = nil;
	vkMapMemory(gvk.device, stagingMemory, 0, size, 0, &mapped);
	memcpy(mapped, data, (size_t)size);
	vkUnmapMemory(gvk.device, stagingMemory);

	VkCommandBuffer commandBuffer = beginOneShot();
	if(commandBuffer != VK_NULL_HANDLE){
		VkBufferCopy copy = {};
		copy.size = size;
		vkCmdCopyBuffer(commandBuffer, staging, *bufferOut, 1, &copy);

		VkBufferMemoryBarrier ready = {};
		ready.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		ready.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		ready.dstAccessMask =
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
			VK_ACCESS_INDEX_READ_BIT;
		ready.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ready.buffer = *bufferOut;
		ready.offset = 0;
		ready.size = size;
		vkCmdPipelineBarrier(commandBuffer,
		                     VK_PIPELINE_STAGE_TRANSFER_BIT,
		                     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		                     0, 0, nil, 1, &ready, 0, nil);
		endOneShot(commandBuffer);
	}

	// A streamed model may be drawn later in this same frame. Submission order
	// makes the copy visible first; defer the upload source until the frame
	// fence proves that both operations have completed.
	retireBuffer(staging, stagingMemory);
	return 1;
}

namespace {

InstanceDataHeader *
instanceMesh(rw::ObjPipeline *, Geometry *geo)
{
	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1,
	                                    MEMDUR_EVENT | ID_GEOMETRY);
	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_VULKAN;
	// Custom instancers share this header but do not establish ordering bounds.
	header->orderBoundsValid = 0;
	header->backfaceCertificate = -1;
	header->occlusionSource.Reset();

	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ?
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = rwNewT(InstanceData, header->numMeshes,
	                      MEMDUR_EVENT | ID_GEOMETRY);

	header->indexBuffer = rwNewT(uint16, header->totalNumIndex,
	                             MEMDUR_EVENT | ID_GEOMETRY);
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 offset = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, &inst->numVertices);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->offset = offset;
		memcpy((uint8*)header->indexBuffer + inst->offset,
		       mesh->indices, inst->numIndex*2);
		offset += inst->numIndex*2;
		mesh++;
		inst++;
	}

	header->vertexBuffer = nil;
	header->vbo = VK_NULL_HANDLE;
	header->vboMemory = VK_NULL_HANDLE;
	header->ibo = VK_NULL_HANDLE;
	header->iboMemory = VK_NULL_HANDLE;
	header->wireIbo = VK_NULL_HANDLE;
	header->wireMemory = VK_NULL_HANDLE;

	if(header->totalNumIndex > 0)
		uploadStaticBuffer(header->indexBuffer, header->totalNumIndex*2,
		             VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		             &header->ibo, &header->iboMemory);

	return header;
}

void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if(geo->flags & Geometry::NATIVE)
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(geo->instData){
		if(header->serialNumber != geo->meshHeader->serialNum)
			freeInstanceData(geo);
	}

	if(geo->instData == nil){
		geo->instData = instanceMesh(rwpipe, geo);
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 0);
	}else if(geo->lockedSinceInst)
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 1);

	geo->lockedSinceInst = 0;
}

void
uninstance(rw::ObjPipeline *, Atomic *)
{
	assert(0 && "can't uninstance");
}

void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	pipe->instance(atomic);
	if(geo->instData == nil || geo->instData->platform != PLATFORM_VULKAN)
		return;
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

} // namespace

void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_VULKAN)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;

	if(gvk.device != VK_NULL_HANDLE){
		destroyStaticBuffer(header->vbo, header->vboMemory);
		destroyStaticBuffer(header->ibo, header->iboMemory);
		destroyStaticBuffer(header->wireIbo, header->wireMemory);
	}
	rwFree(header->indexBuffer);
	rwFree(header->vertexBuffer);
	rwFree(header->inst);
	rwFree(header);
}

void *
destroyNativeGeometryData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
    header->backfaceCertificate=-1;
    header->occlusionSource.Reset();
	const bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	const bool hasNormals = !!(geo->flags & Geometry::NORMALS);
	const bool hasTexCoords = geo->numTexCoordSets > 0;

	const VkDeviceSize size =
		(VkDeviceSize)header->totalNumVertex * sizeof(WorldVertex);

	if(!reinstance){
		header->vertexBuffer = rwNewT(uint8, (uint32)size,
		                              MEMDUR_EVENT | ID_GEOMETRY);
	}else{
		// A reinstance rewrites the whole interleaved block. Doing it
		// wholesale is cheaper than tracking which of the four attributes
		// changed, because they share cache lines in this layout anyway.
		destroyStaticBuffer(header->vbo, header->vboMemory);
	}

	WorldVertex *vertices = (WorldVertex*)header->vertexBuffer;
	V3d *positions = geo->morphTargets[0].vertices;
	V3d *normals = geo->morphTargets[0].normals;
	TexCoords *texCoords = hasTexCoords ? geo->texCoords[0] : nil;
	TexCoords *texCoords1=geo->numTexCoordSets>1?geo->texCoords[1]:nil;
	RGBA *colours = isPrelit ? geo->colors : nil;
	header->orderBoundsValid = header->totalNumVertex > 0 && positions != nil;

	for(uint32 i = 0; i < header->totalNumVertex; i++){
		WorldVertex &v = vertices[i];
		v.texCoord1[0]=texCoords1?texCoords1[i].u:0.0f;
		v.texCoord1[1]=texCoords1?texCoords1[i].v:0.0f;
		v.position[0] = positions[i].x;
		v.position[1] = positions[i].y;
		v.position[2] = positions[i].z;
		for(int axis=0;axis<3;++axis){
			const float value=v.position[axis];
			if(!isfinite(value)) header->orderBoundsValid=0;
			if(i==0) header->orderMin[axis]=header->orderMax[axis]=value;
			else{
				header->orderMin[axis]=fminf(header->orderMin[axis],value);
				header->orderMax[axis]=fmaxf(header->orderMax[axis],value);
			}
		}

		if(hasNormals && normals != nil){
			v.normal[0] = normals[i].x;
			v.normal[1] = normals[i].y;
			v.normal[2] = normals[i].z;
		}else{
			// Unlit geometry still runs the lit shader; an up-facing normal
			// keeps it from going black rather than adding a shader variant.
			v.normal[0] = 0.0f;
			v.normal[1] = 0.0f;
			v.normal[2] = 1.0f;
		}

		if(colours != nil){
			v.colour[0] = colours[i].red;
			v.colour[1] = colours[i].green;
			v.colour[2] = colours[i].blue;
			v.colour[3] = colours[i].alpha;
		}else if(geo->flags & Geometry::LIGHT){
			// The vertex colour is the prelight term of RenderWare's additive
			// sum. Lit geometry without prelight has no emissive contribution
			// -- black -- and takes its brightness from ambient + directional.
			v.colour[0] = v.colour[1] = v.colour[2] = 0x00;
			v.colour[3] = 0xFF;
		}else{
			// Neither prelit nor lit: fixed function draws it full bright.
			// Cutscene character models are exactly this case; a black base
			// here renders them as silhouettes.
			v.colour[0] = v.colour[1] = v.colour[2] = 0xFF;
			v.colour[3] = 0xFF;
		}

		if(texCoords != nil){
			v.texCoord[0] = texCoords[i].u;
			v.texCoord[1] = texCoords[i].v;
		}else{
			v.texCoord[0] = 0.0f;
			v.texCoord[1] = 0.0f;
		}
	}

	// Per-mesh vertex alpha, used by the sorting code to decide whether a mesh
	// needs the transparent pass.
	if(isPrelit){
		InstanceData *inst = header->inst;
		for(uint32 m = 0; m < header->numMeshes; m++, inst++){
			inst->vertexAlpha = 0;
			for(int32 i = 0; i < inst->numVertices; i++)
				if(vertices[inst->minVert + i].colour[3] != 0xFF){
					inst->vertexAlpha = 1;
					break;
				}
		}
	}

	if(size > 0)
		uploadStaticBuffer(header->vertexBuffer, size,
		             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		             &header->vbo, &header->vboMemory);
}

void
defaultUninstanceCB(Geometry *, InstanceDataHeader *)
{
	assert(0 && "can't uninstance");
}

// Squared distance from a point to the segment ab; the per-draw glow test
// asks how close a light sits to the line of sight toward an atomic.
static float32
pointSegmentDistSq(const float32 point[3], const float32 a[3],
                   const float32 b[3])
{
	const float32 abx = b[0]-a[0], aby = b[1]-a[1], abz = b[2]-a[2];
	const float32 apx = point[0]-a[0], apy = point[1]-a[1],
	              apz = point[2]-a[2];
	const float32 abLenSq = abx*abx + aby*aby + abz*abz;
	float32 t = abLenSq > 0.0f ?
		(apx*abx + apy*aby + apz*abz)/abLenSq : 0.0f;
	if(t < 0.0f) t = 0.0f;
	if(t > 1.0f) t = 1.0f;
	const float32 dx = apx - abx*t, dy = apy - aby*t, dz = apz - abz*t;
	return dx*dx + dy*dy + dz*dz;
}

// Shared by the default and skin pipelines: same material walk, same state,
// only the shader variant and the extra bone descriptor differ. boneOffset is
// nil for unskinned geometry.
static uint32
selectAtomicAlphaPasses(const InstanceData *inst,const Material *material,
                       const Raster *raster,const Geometry *geometry,uint32 shader,
                       const uint32 *boneOffset,bool gsSplit,bool vehicleSplit,
                       bool textureAlpha,float reference)
{
	if(!gsSplit||vehicleSplit||vehicleAlphaPass||dynamicObjectPass||
	   shader!=SHADER_WORLD||boneOffset!=nil||inst->vertexAlpha||
	   gstate.alphaTestFunction==ALPHAALWAYS)
		return UniformAlphaPolicy::Both;
	// Transparent border samples can vary alpha even for an opaque image or
	// the white fallback. Test actual state, including inherited sampler state.
	const uint32 u=gstate.textureAddressU,v=gstate.textureAddressV;
	if((u!=Texture::WRAP&&u!=Texture::MIRROR&&u!=Texture::CLAMP)||
	   (v!=Texture::WRAP&&v!=Texture::MIRROR&&v!=Texture::CLAMP))
		return UniformAlphaPolicy::Both;
	// OFF restores the pre-extension texture-alpha path before proof/counters.
	// Existing uniform-alpha and opaque-world optimizations stay independent.
	if(textureAlpha && !gMaskedAlphaPassSkipEnabled) return UniformAlphaPolicy::Both;
	// Match the actual shader multiplier, including ignored material colour
	// without MODULATE. Constant vertex alpha is one after the guard above.
	const float alpha=material&&(geometry->flags&Geometry::MODULATE)?
		material->color.alpha/255.0f:1.0f;
	if(textureAlpha){
		++gAlphaBoundStats.candidates;
		if(!rasterHasNormalizedAlpha(raster)) return UniformAlphaPolicy::Both;
		// UNORM texture alpha (including filtered/mipped samples) is in [0,1].
		// Hence fragColour.a * texColour.a <= material alpha. Preserve the
		// original faint pass and its discard/blend/depth/motion behaviour.
		const uint32 passes=UniformAlphaPolicy::FromUpperBound(alpha,reference);
		if(passes==UniformAlphaPolicy::Faint){
			++gAlphaBoundStats.solidPassesSkipped;
			gAlphaBoundStats.indicesSkipped+=inst->numIndex;
		}
		return passes;
	}
	// A false alpha hint from an unsupported/subraster is not proof of
	// uniform sampled alpha. Untextured draws bind the opaque white texture.
	if(raster!=nil&&
	   (raster->platform!=PLATFORM_VULKAN||
	    (raster->type&0xF)!=Raster::TEXTURE||
	    (raster->parent!=nil&&raster->parent!=raster)))
		return UniformAlphaPolicy::Both;
	// Existing uniform-alpha path: vertex and texture alpha are both one.
	return UniformAlphaPolicy::Select(alpha,reference);
}

static bool
useOpaqueWorldPipeline(const InstanceData *inst,const Material *material,
                      const Raster *raster,const Geometry *geometry,uint32 shader,
                      const uint32 *boneOffset,bool gsSplit,bool wantsBlend,
                      bool textureAlpha)
{
	if(!getOpaqueWorldAlphaFastPathEnabled()||gsSplit||wantsBlend||gstate.vertexAlphaEnabled)
		return false;
	// Probe the same conservative opacity/threshold proof without enabling a
	// split. This draw uses the normal positive alphaTestRef, not gsAlphaTestRef.
	return selectAtomicAlphaPasses(inst,material,raster,geometry,shader,boneOffset,
		true,false,textureAlpha,gstate.alphaTestRef/255.0f)==UniformAlphaPolicy::Solid;
}

static bool
sameOpaqueBatchMaterial(const Material *a,const Material *b)
{
	if(!a || !b || MatFX::get(a) || MatFX::get(b)) return false;
	return a->texture==b->texture &&
		a->color.red==b->color.red && a->color.green==b->color.green &&
		a->color.blue==b->color.blue && a->color.alpha==255 && b->color.alpha==255 &&
		a->surfaceProps.ambient==b->surfaceProps.ambient &&
		a->surfaceProps.diffuse==b->surfaceProps.diffuse &&
		a->surfaceProps.specular==b->surfaceProps.specular;
}

static StaticInstances::Bounds staticInstanceBounds(const InstanceDataHeader *header,const PushConstants &push){
	StaticInstances::Bounds out;
	if(!header->orderBoundsValid)return out;
	float model[16];memcpy(model,push.model,64);model[3]=model[7]=model[11]=0;model[15]=1;
	for(unsigned eye=0;eye<2;++eye){
		float clip[16];multiplyMatrix(clip,getSceneData()->viewProj[eye],model);
		out.left[eye]=out.bottom[eye]=out.depth.nearDepth[eye]=1e30f;
		out.right[eye]=out.top[eye]=out.depth.farDepth[eye]=-1e30f;
		for(unsigned corner=0;corner<8;++corner){
			float p[3];for(unsigned a=0;a<3;++a)p[a]=(corner&(1u<<a))?header->orderMax[a]:header->orderMin[a];
			float c[4];for(unsigned r=0;r<4;++r)c[r]=clip[r]*p[0]+clip[4+r]*p[1]+clip[8+r]*p[2]+clip[12+r];
			if(!isfinite(c[3])||c[3]<=0.001f)return out;
			const float x=c[0]/c[3],y=c[1]/c[3],z=c[2]/c[3];
			if(!isfinite(x)||!isfinite(y)||!isfinite(z)||z<0||z>1)return out;
			out.left[eye]=fminf(out.left[eye],x);out.right[eye]=fmaxf(out.right[eye],x);
			out.bottom[eye]=fminf(out.bottom[eye],y);out.top[eye]=fmaxf(out.top[eye],y);
			out.depth.nearDepth[eye]=fminf(out.depth.nearDepth[eye],z);
			out.depth.farDepth[eye]=fmaxf(out.depth.farDepth[eye],z);
		}
		// Expand by one raster pixel as well as the plan's fixed margin.
		// This remains conservative at reduced or externally overridden sizes.
		const uint32 dimension=gvk.sceneWidth<gvk.sceneHeight?gvk.sceneWidth:gvk.sceneHeight;
		if(!dimension)return out;
		const float padding=2.0f/dimension;
		out.left[eye]-=padding;out.right[eye]+=padding;
		out.bottom[eye]-=padding;out.top[eye]+=padding;
	}
	out.valid=true;return out;
}

static bool
worldBackfaceAtomic(Atomic *atomic,Geometry *geometry,InstanceDataHeader *header,
                    uint32 shader,const uint32 *bones,const float *model,const float *object)
{
    if(shader!=SHADER_WORLD||bones||dynamicObjectPass||vehicleAlphaPass||
       gInstanceOwner.entityClass!=0||gInstanceOwner.model<0||
       (gInstanceOwner.flags&DrawProfile::Distance)||
       !gvk.firstPersonActive||!gvk.stereoCullPlanesValid||gvk.wristPanelOffscreen||
       physicalScopeCaptureActive()||worldEffectsActive()||gvk.im3dWaterDraw||gvk.sgsrMode!=SGSR_OFF||
       !OpaqueOrder::IsDefaultCallback(atomic->renderCB,Atomic::defaultRenderCB,gOpaqueOrderDefaultWrapper)||
       geometry->numMorphTargets!=1||geometry->lockedSinceInst||
       (geometry->flags&Geometry::NATIVE)||header->primType!=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST){
        ++gWorldBackfaceStats.rejectState;return false;
    }
#if defined(LIBERTYVR_DEV_TOOLS) && LIBERTYVR_DEV_TOOLS
    if(gvk.debugVizActive){++gWorldBackfaceStats.rejectState;return false;}
#endif
    auto *base=atomic->getPipeline();
    if(!base||base->platform!=PLATFORM_VULKAN||
       ((ObjPipeline*)base)->instanceCB!=defaultInstanceCB||
       ((ObjPipeline*)base)->renderCB!=defaultRenderCB){
        ++gWorldBackfaceStats.rejectState;return false;
    }
    // Reject the complete geometry if any submesh is intentionally two-sided
    // or uses uncertain alpha/effect state; do not infer sidedness by its name.
    for(uint32 i=0;i<header->numMeshes;++i){
        const auto &mesh=header->inst[i];Material *m=mesh.material;
        if(!m||mesh.vertexAlpha||m->color.alpha!=255||MatFX::getEffects(m)!=MatFX::NOTHING){
            ++gWorldBackfaceStats.rejectState;return false;
        }
        Raster *r=m->texture?m->texture->raster:nil;
        if(r&&(r->platform!=PLATFORM_VULKAN||(r->type&0xF)!=Raster::TEXTURE||
            (r->parent&&r->parent!=r)||rasterHasAlpha(r))){
            ++gWorldBackfaceStats.rejectState;return false;
        }
        if(m->texture){
            const uint32 u=m->texture->getAddressU(),v=m->texture->getAddressV();
            if((u!=Texture::WRAP&&u!=Texture::MIRROR&&u!=Texture::CLAMP)||
               (v!=Texture::WRAP&&v!=Texture::MIRROR&&v!=Texture::CLAMP)){
                ++gWorldBackfaceStats.rejectState;return false;
            }
        }
    }
    if(!WorldBackface::PositiveTransform(model)){
        ++gWorldBackfaceStats.rejectTransform;return false;
    }
    // Atomic::getWorldBoundingSphere does not account for scaling. Transform
    // the certified vertex AABB instead, conservatively enclosing every corner.
    float centre[3]={},extent[3]={};
    if(!header->orderBoundsValid){++gWorldBackfaceStats.rejectView;return false;}
    for(unsigned row=0;row<3;++row){
        centre[row]=object[12+row];
        for(unsigned axis=0;axis<3;++axis){
            const float mid=(header->orderMin[axis]+header->orderMax[axis])*0.5f;
            const float half=(header->orderMax[axis]-header->orderMin[axis])*0.5f;
            centre[row]+=object[axis*4+row]*mid;
            extent[row]+=fabsf(object[axis*4+row])*half;
        }
    }
    const float radius=sqrtf(extent[0]*extent[0]+extent[1]*extent[1]+extent[2]*extent[2]);
    if(!WorldBackface::ViewerOutside(gvk.fpHeadWorld,centre,radius)){
        ++gWorldBackfaceStats.rejectView;return false;
    }
    if(header->backfaceCertificate<0){
        const auto start=std::chrono::steady_clock::now();
        ++gWorldBackfaceStats.certifications;
        header->backfaceCertificate=0;
        try {
            if(header->vertexBuffer&&header->indexBuffer){
                const auto *v=(const WorldVertex*)header->vertexBuffer;
                header->backfaceCertificate=WorldBackface::ClosedOutward(header->totalNumVertex,
                    header->totalNumIndex,header->indexBuffer,[&](unsigned i){
                        return WorldBackface::Point{v[i].position[0],v[i].position[1],v[i].position[2]};
                    })?1:0;
            }
        }catch(const std::bad_alloc&){ /* original two-sided rendering */ }
        gWorldBackfaceStats.certifyMs+=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
    }
    if(!header->backfaceCertificate){++gWorldBackfaceStats.rejectGeometry;return false;}
    return true;
}

struct DebugWireDraw {
    VkPipeline pipeline;
    VkBuffer vertices,indices;
    VkDescriptorSet texture,neo[2];
    PushConstants push;
    uint32 first,count,bone,flags;
    bool skinned;
};
static std::vector<DebugWireDraw> debugWireDraws;
void discardDebugWires(void) { debugWireDraws.clear(); }
void flushDebugWires(void)
{
    if(debugWireDraws.empty()) return;
    flushStaticInstances();
    const auto commandBuffer=gvk.frameCommands;
    const auto layout=getPipelineLayout();
    const auto scene=getSceneDescriptor();
    vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&scene,0,nil);
    for(const auto &draw:debugWireDraws){
        const VkDeviceSize offset=0;
        vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,draw.pipeline);
        vkCmdBindVertexBuffers(commandBuffer,0,1,&draw.vertices,&offset);
        vkCmdBindIndexBuffer(commandBuffer,draw.indices,0,VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,1,&draw.texture,0,nil);
        if(draw.skinned){
            const auto bones=getBoneDescriptor();
            vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,2,1,&bones,1,&draw.bone);
        }
        if(draw.neo[0] && draw.neo[1])
            vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,4,2,draw.neo,0,nil);
        vkCmdPushConstants(commandBuffer,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(draw.push),&draw.push);
        countGpuProfileDraw(draw.count,VK_PRIMITIVE_TOPOLOGY_LINE_LIST,draw.flags);
        vkCmdDrawIndexed(commandBuffer,draw.count,1,draw.first,0,0);
    }
    debugWireDraws.clear();
}

// Reuse the original vertex stream and bone descriptor for animated wires.
static void DrawDebugEdges(VkCommandBuffer commandBuffer, InstanceDataHeader *header,
    InstanceData *inst, uint32 shader, const PushConstants &push,
    VkDescriptorSet texture, const uint32 *boneOffset, const VkDescriptorSet *neo, uint32 flags)
{
    if(header->wireIbo == VK_NULL_HANDLE){
        std::vector<uint16_t> edges;
        for(uint32 i=0; i<header->numMeshes; ++i){
            auto &mesh=header->inst[i];
            mesh.wireOffset=(uint32)edges.size();
            DebugVisualization::AppendEdges(header->indexBuffer+mesh.offset/2,
                mesh.numIndex, header->primType==VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, edges);
            mesh.wireCount=(uint32)edges.size()-mesh.wireOffset;
        }
        if(edges.empty() || !uploadStaticBuffer(edges.data(), edges.size()*sizeof(uint16),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &header->wireIbo, &header->wireMemory)) return;
    }
    if(!inst->wireCount) return;
    const auto saved=gstate;
    gstate.vertexAlphaEnabled=0;
    gstate.alphaTestFunction=ALPHAALWAYS;
    gstate.zWriteEnabled=0;
    if(DebugVisualization::IsHiddenWire()) gstate.zTestEnabled=1;
    const VkPipeline pipeline=getPipeline(shader, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    gstate=saved;
    if(!pipeline) return;
    PushConstants wire=push;
    wire.surfaceProps[0]=-999.0f; // fragment wire marker; keep the 128-byte layout
    if(DebugVisualization::IsHiddenWire()){
        // Save values, not entity/geometry pointers: streaming can retire an object
        // before the frame ends. GPU buffers already have fence-based retirement.
        debugWireDraws.push_back({pipeline,header->vbo,header->wireIbo,texture,{neo[0],neo[1]},
            wire,inst->wireOffset,inst->wireCount,boneOffset?*boneOffset:0,flags,boneOffset!=nil});
        return;
    }
    vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    vkCmdBindIndexBuffer(commandBuffer,header->wireIbo,0,VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(commandBuffer,getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(wire),&wire);
    countGpuProfileDraw(inst->wireCount,VK_PRIMITIVE_TOPOLOGY_LINE_LIST,flags);
    vkCmdDrawIndexed(commandBuffer,inst->wireCount,1,inst->wireOffset,0,0);
    vkCmdBindIndexBuffer(commandBuffer,header->ibo,0,VK_INDEX_TYPE_UINT16);
}

void
drawAtomicMeshes(Atomic *atomic, InstanceDataHeader *header, uint32 shader,
                 const uint32 *boneOffset)
{
	VkCommandBuffer commandBuffer = gvk.frameCommands;

	PushConstants push;
	Matrix *ltm = atomic->getFrame()->getLTM();
	// rw::Matrix is 3 rights/ups/ats plus a position, stored without the
	// fourth column; expand it into a column-major 4x4.
	float32 objectToWorld[16];
	objectToWorld[0]  = ltm->right.x; objectToWorld[1]  = ltm->right.y;
	objectToWorld[2]  = ltm->right.z; objectToWorld[3]  = 0.0f;
	objectToWorld[4]  = ltm->up.x;    objectToWorld[5]  = ltm->up.y;
	objectToWorld[6]  = ltm->up.z;    objectToWorld[7]  = 0.0f;
	objectToWorld[8]  = ltm->at.x;    objectToWorld[9]  = ltm->at.y;
	objectToWorld[10] = ltm->at.z;    objectToWorld[11] = 0.0f;
	objectToWorld[12] = ltm->pos.x;   objectToWorld[13] = ltm->pos.y;
	objectToWorld[14] = ltm->pos.z;   objectToWorld[15] = 1.0f;
	// Carry the current camera into the draw, so the shader's single per-eye
	// matrix is all that is left to apply. Done here rather than in the scene
	// block because the camera can change again before the frame is submitted.
	multiplyMatrix(push.model, gvk.worldToPlay, objectToWorld);
	Geometry *geometry = atomic->geometry;
	// Vice City's static world is prelit and has LIGHT unset. Peds, vehicles,
	// held weapons and their moving parts use dynamic lighting, which gives us
	// a conservative backend-level dynamic-object classifier without exposing
	// game entity types to librw.
	// Do not infer motion from Geometry::LIGHT: some static road/building
	// sectors carry that flag and produced alternating diagnostic triangles.
	// Game entities mark their synchronous clump explicitly; deferred vehicle
	// glass/body alpha atomics use the dedicated vehicle pass; skinned peds are
	// always persistent dynamic geometry.
	packAtomicRootMotion(atomic,
		dynamicObjectPass || vehicleAlphaPass || shader == SHADER_SKIN, push);

	// Lighting, resolved per atomic exactly as gl3's lightingCB does: world
	// geometry is unlit (prelight only), peds and vehicles take the world's
	// ambient plus its first directional -- Vice City registers exactly one.
	push.ambientLight[0] = push.ambientLight[1] = push.ambientLight[2] = 0.0f;
	// w carries the fog switch for this draw. The game turns fog on for the
	// world and off for the sky, the water and anything it wants left flat,
	// and the scene block is uploaded once a frame, so the per-draw state
	// has to travel with the draw.
	push.ambientLight[3] = gstate.fogEnabled ? 1.0f : 0.0f;
	push.lightDirColour[0] = 0.0f;
	push.lightDirColour[1] = 0.0f;
	push.lightDirColour[2] = 1.0f;
	push.lightDirColour[3] = 0.0f;	// packed RGBA8 black: no directional
	if((geometry->flags & Geometry::LIGHT) && engine->currentWorld != nil){
		Light *directionals[8];
		WorldLights lights;
		memset(&lights, 0, sizeof(lights));
		lights.directionals = directionals;
		lights.numDirectionals = 8;
		((World*)engine->currentWorld)->enumerateLights(atomic, &lights);

		push.ambientLight[0] = lights.ambient.red;
		push.ambientLight[1] = lights.ambient.green;
		push.ambientLight[2] = lights.ambient.blue;

		if(lights.numDirectionals > 0 && (geometry->flags & Geometry::NORMALS)){
			Light *sun = directionals[0];
			const V3d &at = sun->getFrame()->getLTM()->at;
			// The shader's normals are in play space (push.model includes the
			// camera), so the direction has to be rotated the same way or the
			// dot product compares vectors from two different spaces.
			const float32 *wp = gvk.worldToPlay;
			push.lightDirColour[0] = wp[0]*at.x + wp[4]*at.y + wp[8]*at.z;
			push.lightDirColour[1] = wp[1]*at.x + wp[5]*at.y + wp[9]*at.z;
			push.lightDirColour[2] = wp[2]*at.x + wp[6]*at.y + wp[10]*at.z;
			RGBAf sunColour = sun->color;
			clamp(&sunColour);
			const uint32 packed =
				(uint32)(sunColour.red   * 255.0f) |
				(uint32)(sunColour.green * 255.0f) << 8 |
				(uint32)(sunColour.blue  * 255.0f) << 16 |
				0xFF000000u;
			memcpy(&push.lightDirColour[3], &packed, sizeof(packed));
		}
	}

	// Which of the frame's dynamic lights are worth this atomic's pixels,
	// decided once per draw so the fragment loop is only ever paid where a
	// light can actually land: a sphere overlap for the surface term, and for
	// the air glow a light near the eye-to-atomic segment, because the glow
	// shows on geometry far behind the source. Most draws end up with no bits
	// set and the shader skips the whole block.
	uint32 lightMask = 0;
	uint32 ownVehicleBit = 0;
	// The near water renders as atomics (the wavy and mask patches), so the
	// water bit has to travel this path too, not only vkim's.
	const uint32 waterBit =
		gvk.modernWater && gvk.im3dWaterDraw ? 0x1000000u : 0u;
	const bool32 testOwnVehicle = gvk.playerVehicleActive &&
		carReflectionsActive() && gvk.carReflectionSsr > 0.0f;
	if(gvk.pointLightCount > 0 || testOwnVehicle){
		const Sphere *bounds = atomic->getWorldBoundingSphere();
		if(testOwnVehicle){
			// Every atomic centre of the occupied vehicle -- doors and
			// bumpers included -- lies inside its model bounding sphere;
			// the margin covers the one simulation step between where the
			// game reported the vehicle and where it renders.
			const float32 vx =
				bounds->center.x - gvk.playerVehiclePos[0];
			const float32 vy =
				bounds->center.y - gvk.playerVehiclePos[1];
			const float32 vz =
				bounds->center.z - gvk.playerVehiclePos[2];
			const float32 reach = gvk.playerVehicleRadius + 0.75f;
			if(vx*vx + vy*vy + vz*vz < reach*reach)
				ownVehicleBit = 0x800000u;
		}
		for(uint32 i = 0; i < gvk.pointLightCount; i++){
			const PointLight &light = gvk.pointLights[i];
			const float32 dx = bounds->center.x - light.position[0];
			const float32 dy = bounds->center.y - light.position[1];
			const float32 dz = bounds->center.z - light.position[2];
			const float32 reach = bounds->radius + light.radius;
			if(dx*dx + dy*dy + dz*dz < reach*reach)
				lightMask |= 1u<<i;
			if(gvk.pointLightGlowStrength > 0.0f){
				// Matches the shader's glow radius of 0.4 of the light's.
				const float32 glowReach =
					bounds->radius + light.radius*0.4f;
				if(pointSegmentDistSq(light.position, gvk.cameraWorldPos,
				                      &bounds->center.x) <
				   glowReach*glowReach)
					lightMask |= 1u<<(8+i);
			}
		}
	}

	// The cache is local to this atomic: no other command writer runs between
	// its meshes. Dynamic/skinned/vehicle paths retain their original binds.
    const bool backfaceAtomic=gWorldBackfaceEnabled && worldBackfaceAtomic(
        atomic,geometry,header,shader,boneOffset,push.model,objectToWorld);
	const bool cacheBindings = shader == SHADER_WORLD && boneOffset == nil &&
		!dynamicObjectPass && !vehicleAlphaPass;
	bool instanceSubmission=!DebugVisualization::MeshDebug() && !gvk.neoEnabled && gStaticInstancesEnabled && gvk.sgsrMode==SGSR_OFF &&
		!gvk.wristPanelOffscreen && !physicalScopeCaptureActive() && !worldEffectsActive() && !gvk.im3dWaterDraw;
#if defined(LIBERTYVR_DEV_TOOLS) && LIBERTYVR_DEV_TOOLS
	if(gvk.debugVizActive)instanceSubmission=false;
#endif
	if(!instanceSubmission)flushStaticInstances();
	const bool measureBindings = cacheBindings && gvk.frameTimestampEnabled && !instanceSubmission;
	const bool auditSampling = gStaticReuse.stats.sampled!=0;
	const bool staticAtomic = (gOpaqueMeshBatchEnabled || auditSampling || instanceSubmission) && cacheBindings &&
		OpaqueOrder::IsDefaultCallback(atomic->renderCB,Atomic::defaultRenderCB,gOpaqueOrderDefaultWrapper) &&
		geometry->numMorphTargets==1 && !geometry->lockedSinceInst &&
		atomic->getPipeline() && atomic->getPipeline()->platform==PLATFORM_VULKAN &&
		((ObjPipeline*)atomic->getPipeline())->instanceCB==defaultInstanceCB &&
		((ObjPipeline*)atomic->getPipeline())->renderCB==defaultRenderCB &&
		!(geometry->flags & Geometry::NATIVE) &&
		header->primType==VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const bool batchAtomic = !DebugVisualization::MeshDebug() && gOpaqueMeshBatchEnabled && staticAtomic;
	const bool measureBatch = gOpaqueMeshBatchEnabled && gvk.frameTimestampEnabled;
	bool bufferStatic=instanceSubmission && staticAtomic && gInstanceOwner.entityClass==0 && gInstanceOwner.model>=0;
	StaticInstances::Bounds instanceBounds;
	if(bufferStatic){
		instanceBounds=staticInstanceBounds(header,push);
		if(!StaticInstances::FiniteBounds(instanceBounds)){++gStaticInstanceStats.rejectBounds;bufferStatic=false;}
	}
	AtomicSubmission::Cache<VkPipeline,VkDescriptorSet,VkPipelineLayout>
		bindings(cacheBindings,measureBindings);
	const VkPipelineLayout layout = getPipelineLayout();
	VkDescriptorSet boundNeo[2]={};
	const VkDeviceSize vertexOffset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &header->vbo, &vertexOffset);
	vkCmdBindIndexBuffer(commandBuffer, header->ibo, 0, VK_INDEX_TYPE_UINT16);

	if(boneOffset != nil){
		VkDescriptorSet boneSet = getBoneDescriptor();
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		                        getPipelineLayout(), 2, 1, &boneSet,
		                        1, boneOffset);
	}

	// Opaque meshes first, blended meshes second. File order interleaves them,
	// and a blended triangle drawn before the opaque mesh behind it has only
	// the background to blend against -- the classic see-through-body artifact.
	// Vehicle alpha atomics frequently mix opaque bodywork and transparent side
	// glass.  Keep depth writes for the opaque pass, but do not let that glass
	// hide VR hands, held weapons and tracers rendered later in the frame.
	for(int32 blendPass = 0; blendPass < 2; blendPass++){
	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++, inst++){
		Material *material = inst->material;
		// Glass shares atomics with opaque doors/bodywork. Preserve those
		// meshes and texture cutouts while hiding translucent glass materials.
		if(hideVehicleWindows && material && material->color.alpha < 255)
			continue;
		Raster *raster = material && material->texture ?
			material->texture->raster : nil;
		const bool textureAlpha = rasterHasAlpha(raster);

		const bool wantsBlend =
			inst->vertexAlpha || (material && material->color.alpha != 0xFF) ||
			textureAlpha;
		if(wantsBlend != (blendPass == 1))
			continue;

		if(material && material->texture){
			// A world texture that asks for plain LINEAR was authored when
			// nothing carried a mip chain. It has one now, generated or out
			// of the dictionary, and the sampler holds a LINEAR request at
			// level zero -- which is why a leaf mask is sampled at full
			// resolution from any distance and sparkles. The interface keeps
			// that clamp: fonts and the radar draw through Im2D, not here.
			// Only for a chain this backend built. Those are filtered with
			// the colour weighted by alpha; the ones baked into the model
			// packs are not, so their levels carry a dark rim around every
			// shape in the mask and reading them turns foliage and neon into
			// a lattice. Lift this once the packs are rebuilt with the fixed
			// txdcompress.
			uint32 textureFilter = material->texture->getFilter();
			if(textureFilter == Texture::LINEAR && raster != nil &&
			   rasterNumLevels(raster) > 1 &&
			   rasterHasGeneratedMips(raster))
				textureFilter = Texture::LINEARMIPLINEAR;
			SetRenderState(TEXTUREFILTER, textureFilter);
			SetRenderState(TEXTUREADDRESSU, material->texture->getAddressU());
			SetRenderState(TEXTUREADDRESSV, material->texture->getAddressV());
		}
		SetRenderState(VERTEXALPHA,
			inst->vertexAlpha ||
			(material && material->color.alpha != 0xFF) ||
			textureAlpha ? 1 : 0);

		const uint32 savedZWrite = gstate.zWriteEnabled;
		// The PS2 alpha rule, which reVC asks for and this backend used to
		// ignore. A blended mesh that writes depth goes twice: the solid
		// half claims the depth buffer, the faint half only blends. Without
		// it the invisible border around a masked element writes depth for
		// colour it never contributes, and whatever stands behind it is cut
		// away -- a railing carving its own outline out of a wall.
		// The class both rules act on: a blended mesh that writes depth
		// and is not a vehicle -- masked world geometry, which the game
		// marks by asking for the PS2 rule in the first place.
		const bool masked = gstate.gsAlphaTest != 0 && wantsBlend &&
			savedZWrite != 0 && !vehicleAlphaPass;
		// Alpha to coverage was tried here and taken out again: it turns
		// the mask into per-sample coverage, and at two or four samples
		// that is a dither pattern, not a soft edge. On a neon tube or a
		// leaf it reads as a lattice. Multisampling still earns its place
		// on the geometry it was made for -- railings, poles, wires.
		const bool vehicleSplit = splitVehicleAlpha(vehicleAlphaPass != 0, wantsBlend, savedZWrite != 0);
		const bool gsSplit = vehicleSplit || (masked && getPs2AlphaTestEnabled());
		const float splitReference = vehicleSplit ? vehicleOpaqueReference() : gstate.gsAlphaTestRef/255.0f;
		// Leaf masks are the highest frequency in the game and the thing
		// that sparkles at distance. Nothing spatial can resolve detail
		// finer than a pixel, so the honest answer is to stop asking for
		// it: bias the mask down the mip chain and let it soften, which is
		// what a temporal filter does for this on desktop.
		//
		// Keyed off the whole blended mask class, not the depth-writing
		// half of it. Foliage comes through the alpha list, which draws
		// back to front with depth writes off, so it never was in the
		// class the PS2 rule carved out -- the fences and the neon were,
		// which is why they answered and the leaves did not.
		const bool blendedMask = wantsBlend && !vehicleAlphaPass &&
			gstate.alphaTestFunction != ALPHAALWAYS;
		gstate.mipLodBias = blendedMask ? getMaskedMipBias() : 0;
		const uint32 alphaPasses=selectAtomicAlphaPasses(inst,material,raster,geometry,
			shader,boneOffset,gsSplit,vehicleSplit,textureAlpha,splitReference);
		const int firstGsPass=(alphaPasses&UniformAlphaPolicy::Solid)?0:1;
		const bool opaqueWorldPipeline=useOpaqueWorldPipeline(inst,material,raster,
			geometry,shader,boneOffset,gsSplit,wantsBlend,textureAlpha);
		// Sampler/raster state is identical across this mesh's split halves.
		// Resolve lazily, after a pipeline succeeds, and never reuse over a mesh.
		VkDescriptorSet textureSet = VK_NULL_HANDLE;
		bool textureResolved = false;
		bool emittedEdges=false;
		for(int gsPass = 0; gsPass < (gsSplit ? 2 : 1); gsPass++){
		// Skip before state changes/binds: the complementary shader test is
		// guaranteed to discard every fragment, including motion outputs.
		if(!(alphaPasses&(1u<<gsPass)))continue;
		if((vehicleAlphaPass && wantsBlend && !vehicleSplit) || (gsSplit && gsPass == 1))
			gstate.zWriteEnabled = 0;
		const uint32 savedAlphaFunction = gstate.alphaTestFunction;
		if(vehicleSplit) gstate.alphaTestFunction = ALPHAGREATEREQUAL;
		else if(opaqueWorldPipeline) gstate.alphaTestFunction = ALPHAALWAYS;
		const bool profileDepthWrite = gstate.zWriteEnabled != 0;
		const bool profileAlphaTest = gstate.alphaTestFunction != ALPHAALWAYS;
        const uint32 savedCull=gstate.cullMode;
        bool backfaceMesh=false;
        if(gWorldBackfaceEnabled){
            ++gWorldBackfaceStats.candidates;
            backfaceMesh=backfaceAtomic && savedCull==CULLNONE && !wantsBlend && !gsSplit &&
                !inst->vertexAlpha && !textureAlpha && material && material->color.alpha==255 &&
                gstate.zTestEnabled && profileDepthWrite && !gstate.vertexAlphaEnabled &&
                MatFX::getEffects(material)==MatFX::NOTHING;
            if(backfaceMesh)gstate.cullMode=CULLBACK;
            else ++gWorldBackfaceStats.rejectState;
        }
		VkPipeline pipeline = getPipeline(shader,
		                                  (VkPrimitiveTopology)header->primType);
		VkPipeline instancePipeline=VK_NULL_HANDLE;
		if(bufferStatic && !wantsBlend && !gsSplit && gstate.zTestEnabled && !gstate.vertexAlphaEnabled && profileDepthWrite && !profileAlphaTest){
			instancePipeline=getPipeline(SHADER_WORLD_INSTANCED,(VkPrimitiveTopology)header->primType);
			if(!instancePipeline)++gStaticInstanceStats.pipelineMiss;
		}
        gstate.cullMode=savedCull;
		gstate.zWriteEnabled = savedZWrite;
		gstate.alphaTestFunction = savedAlphaFunction;
		if(pipeline == VK_NULL_HANDLE)
			continue;
		if(!instanceSubmission && bindings.PipelineBind(pipeline))
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		if(!cacheBindings || !textureResolved){
			textureSet = getTextureDescriptor(raster);
			textureResolved = true;
			bindings.TextureLookup();
		}
		if(!instanceSubmission && bindings.TextureBind(layout,textureSet))
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			                        layout, 1, 1, &textureSet, 0, nil);

		VkDescriptorSet neoSets[2]={};
		uint32 neoFlags=0;
		if(gvk.neoEnabled){
			Raster *lightmap=nil,*gloss=nil;
			MatFX *fx=material?MatFX::get(material):nil;
			if(fx && geometry->numTexCoordSets>1 && fx->getEffects(material)==MatFX::DUAL){
				auto tex=fx->getDualTexture();if(tex && tex->raster){lightmap=tex->raster;neoFlags|=2;}
			}
			if(material && material->texture){
				char name[64];snprintf(name,sizeof(name),"%s_gloss",material->texture->name);
				auto tex=gvk.neoTextures->find(name);if(tex && tex->raster){gloss=tex->raster;neoFlags|=4;}
			}
			if(fx && fx->getEffectIndex(MatFX::ENVMAP)>=0 && (geometry->flags&Geometry::NORMALS) &&
			   !gvk.im3dWaterDraw){
				neoFlags|=8;
				const float32 spec=fminf(fmaxf(material->surfaceProps.specular,0.0f),1.0f);
				neoFlags|=uint32(spec*63.0f+0.5f)<<5;
				auto mask=gvk.neoTextures->find("carreflectionmask");
				if(mask && mask->raster)gloss=mask->raster;
			}
			if(gPedestrianRim.Eligible(shader==SHADER_SKIN,
			    (geometry->flags&Geometry::LIGHT)!=0,
			    (geometry->flags&Geometry::NORMALS)!=0)) neoFlags|=16;
			neoSets[0]=getTextureDescriptor(lightmap);neoSets[1]=getTextureDescriptor(gloss);
			if(neoSets[0]!=boundNeo[0] || neoSets[1]!=boundNeo[1]){
				vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,4,2,neoSets,0,nil);
				boundNeo[0]=neoSets[0];boundNeo[1]=neoSets[1];
			}
		}
		push.ambientLight[3]=(gstate.fogEnabled?1.0f:0.0f)+float32(neoFlags);
		if(material && (geometry->flags & Geometry::MODULATE)){
			push.materialColour[0] = material->color.red / 255.0f;
			push.materialColour[1] = material->color.green / 255.0f;
			push.materialColour[2] = material->color.blue / 255.0f;
			push.materialColour[3] = material->color.alpha / 255.0f;
			push.surfaceProps[0] = material->surfaceProps.ambient;
			push.surfaceProps[1] = material->surfaceProps.diffuse;
		}else if(material){
			// No MODULATE flag: the material colour is ignored, exactly as
			// gl3's setMaterial substitutes white. Surface properties still
			// apply -- they scale the lighting, not the colour.
			push.materialColour[0] = push.materialColour[1] =
			push.materialColour[2] = push.materialColour[3] = 1.0f;
			push.surfaceProps[0] = material->surfaceProps.ambient;
			push.surfaceProps[1] = material->surfaceProps.diffuse;
		}else{
			push.materialColour[0] = push.materialColour[1] =
			push.materialColour[2] = push.materialColour[3] = 1.0f;
			push.surfaceProps[0] = 1.0f;
			push.surfaceProps[1] = 1.0f;
		}
		if(neoFlags&2) push.materialColour[0]=push.materialColour[1]=push.materialColour[2]=1.0f;
		// The specular coefficient never reached the shaders, so the slot
		// carries the dynamic-light state for this draw instead, packed as an
		// exact small integer: bits 0-7 the surface light mask, bits 8-15 the
		// glow mask, bit 16 whether the mesh brought real normals -- the
		// per-pixel lights pick between the vertex normal and a facet normal
		// from derivatives with that one -- bits 17-22 the material's MatFX
		// env coefficient for the reflection, quantised to 6 bits, and
		// bit 23 the occupied vehicle, which keeps the panorama layer only.
		uint32 envBits = 0;
		if(carReflectionsActive() && material != nil &&
		   (geometry->flags & Geometry::NORMALS)){
			MatFX *matfx = MatFX::get(material);
			if(matfx != nil){
				const int32 slot = matfx->getEffectIndex(MatFX::ENVMAP);
				if(slot >= 0){
					// The game quarters every coefficient "for PC"
					// (SetDefaultEnvironmentMapCB); the PS2 build kept them
					// as authored, and so does this path.
					float32 coefficient =
						matfx->fx[slot].env.coefficient*4.0f;
					if(coefficient > 1.0f) coefficient = 1.0f;
					if(coefficient > 0.0f)
						envBits = (uint32)(coefficient*63.0f + 0.5f);
					Texture *envTex = matfx->fx[slot].env.tex;
					if(envTex != nil && envTex->raster != nil)
						gvk.envRaster = envTex->raster;
				}
			}
		}
#if defined(LIBERTYVR_DEV_TOOLS) && LIBERTYVR_DEV_TOOLS
		// The visualizer replaces the packed mask with a bare colour index.
		// It cannot share the float: the normal bits already reach 2^24
		// (water), and anything above that loses integer precision.
		if(gvk.debugVizActive)
			push.surfaceProps[2] = (float32)gvk.debugTint;
		else
#endif
			push.surfaceProps[2] = (float32)(lightMask |
				((geometry->flags & Geometry::NORMALS) ? 0x10000u : 0u) |
				envBits << 17 | ownVehicleBit | waterBit);
		push.surfaceProps[3] = gsSplit ?
			(gsPass == 0 ? splitReference : -splitReference) :
			gstate.alphaTestRef/255.0f;

		// Never defer commands across objects or reorder geometry. Compatible
		// adjacent index ranges share every per-atomic value and the exact
		// texture/sampler and material state used to build this push block.
		uint32 batchMeshes=1,drawIndices=inst->numIndex;
		const bool batchState = batchAtomic && !wantsBlend && !gsSplit &&
			!inst->vertexAlpha && !textureAlpha && profileDepthWrite &&
			gstate.zTestEnabled && !profileAlphaTest && !gvk.im3dWaterDraw &&
			sameOpaqueBatchMaterial(material,material);
		if(measureBatch){
			if(batchState) ++gOpaqueMeshBatchStats.eligible;
			else ++gOpaqueMeshBatchStats.rejectedState;
		}
		if(batchState){
			while(i+batchMeshes<header->numMeshes && batchMeshes<OpaqueMeshBatch::MaxMeshes){
				const InstanceData &next=inst[batchMeshes];
				if(measureBatch) ++gOpaqueMeshBatchStats.checks;
				if(next.vertexAlpha || !sameOpaqueBatchMaterial(material,next.material)){
					if(measureBatch) ++gOpaqueMeshBatchStats.rejectedMaterial;
					break;
				}
				if(!OpaqueMeshBatch::CanAppend(inst->offset,drawIndices,next.offset,next.numIndex,batchMeshes)){
					if(measureBatch) ++gOpaqueMeshBatchStats.rejectedRange;
					break;
				}
				drawIndices+=next.numIndex;
				++batchMeshes;
			}
		}
		if(measureBatch && batchMeshes>1){
			gOpaqueMeshBatchStats.merged+=batchMeshes;
			gOpaqueMeshBatchStats.savedDraws+=batchMeshes-1;
		}
		// Sampled feasibility audit only: ignore transform in the grouping key,
		// but require exact geometry range, pipeline, descriptor and lighting.
		// No GPU command/state is changed by this measurement.
		if(auditSampling){
			const auto auditStart=std::chrono::steady_clock::now();
			const auto &recording=gvk.frames[gvk.activeFrame].gpuProfile;
			const auto &owner=recording.geometry.entity;
			const bool worldPhase=recording.active==GpuPassProfile::Roads ||
				recording.active==GpuPassProfile::World || recording.active==GpuPassProfile::Fading;
			if(owner.entityClass!=0 || owner.model<0)
				++gStaticReuse.stats.rejectedOwner;
			else if(!staticAtomic || !worldPhase || gvk.wristPanelOffscreen ||
				wantsBlend || gsSplit || inst->vertexAlpha || textureAlpha ||
				!profileDepthWrite || !gstate.zTestEnabled || profileAlphaTest ||
				gvk.im3dWaterDraw || !sameOpaqueBatchMaterial(material,material))
				++gStaticReuse.stats.rejectedState;
			else{
				bool finite=true;
				for(float value:push.model)if(!isfinite(value))finite=false;
				if(!finite)++gStaticReuse.stats.rejectedTransform;
				else{
					StaticReuseAudit::Key key;
					key.geometry=(uint64)(uintptr_t)header;
					key.pipeline=(uint64)(uintptr_t)pipeline;
					key.texture=(uint64)(uintptr_t)textureSet;
					key.first=inst->offset/2;key.count=drawIndices;key.model=owner.model;
					static_assert(sizeof(PushConstants)==128,"Audit non-transform push layout");
					memcpy(key.state,push.materialColour,sizeof(key.state));
					const float position[3]={ltm->pos.x,ltm->pos.y,ltm->pos.z};
					gStaticReuse.Observe(key,(uint64)(uintptr_t)atomic,position);
				}
			}
			gStaticReuse.stats.cpuMs+=std::chrono::duration<double,std::milli>(
				std::chrono::steady_clock::now()-auditStart).count();
		}
        if(backfaceMesh){++gWorldBackfaceStats.draws;gWorldBackfaceStats.indices+=drawIndices;}
		const uint32 profileFlags=(gsPass==firstGsPass?DrawProfile::Mesh:DrawProfile::Extra) |
            (wantsBlend?DrawProfile::HasAlpha:0) |
            (textureAlpha?DrawProfile::HasTextureAlpha:0) |
            (inst->vertexAlpha?DrawProfile::HasVertexAlpha:0) |
            (material&&material->color.alpha!=255?DrawProfile::HasMaterialAlpha:0) |
            (profileAlphaTest?DrawProfile::HasAlphaTest:0) |
            (profileDepthWrite?DrawProfile::WritesDepth:0) |
            (gsSplit&&!vehicleSplit?DrawProfile::Ps2:0) |
            (vehicleSplit?DrawProfile::Vehicle:0) |
            (boneOffset?DrawProfile::Skin:0);
		const bool bufferDraw=bufferStatic && material && !MatFX::get(material) &&
			drawIndices && drawIndices%3==0 && isfinite(material->surfaceProps.ambient) &&
			isfinite(material->surfaceProps.diffuse);
		if(gStaticInstancesEnabled && !instanceSubmission)++gStaticInstanceStats.rejectState;
		if(instanceSubmission){
			if(bufferDraw){
				if(gInstanceCount==StaticInstances::Window)flushStaticInstances();
				StaticInstanceDraw &d=gInstanceDraws[gInstanceCount++];
				d.pipeline=pipeline;d.instanced=instancePipeline;d.texture=textureSet;
				d.vertices=header->vbo;d.indices=header->ibo;d.first=inst->offset/2;d.count=drawIndices;
				d.flags=profileFlags;d.push=push;d.owner=gInstanceOwner;d.object=(uint64)(uintptr_t)atomic;
				d.position[0]=ltm->pos.x;d.position[1]=ltm->pos.y;d.position[2]=ltm->pos.z;d.bounds=instanceBounds;
				d.opaque=!wantsBlend && !gsSplit && gstate.zTestEnabled && !gstate.vertexAlphaEnabled && profileDepthWrite && !profileAlphaTest;
				++gStaticInstanceStats.queued;gStaticInstanceStats.sourceIndices+=drawIndices;
			}else{
				++gStaticInstanceStats.rejectState;flushStaticInstances();
				// A flush may have changed every graphics binding. Restore this
				// draw fully; bone set 2 is untouched by the instanced emitter.
				vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
				vkCmdBindVertexBuffers(commandBuffer,0,1,&header->vbo,&vertexOffset);
				vkCmdBindIndexBuffer(commandBuffer,header->ibo,0,VK_INDEX_TYPE_UINT16);
				vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,1,&textureSet,0,nil);
			}
		}
		if(instanceSubmission && bufferDraw){
			i+=batchMeshes-1;inst+=batchMeshes-1;continue;
		}
		vkCmdPushConstants(commandBuffer, getPipelineLayout(),
		                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		                   0, sizeof(push), &push);

		capturePhysicalScopeDraw(pipeline, textureSet, push, header->vbo, 0,
			header->ibo, 0, drawIndices, inst->offset / 2, boneOffset, atomic, neoSets);
        if(DebugVisualization::Get().mesh != DebugVisualization::Wire || DebugVisualization::IsOverdraw()){
            countGpuProfileDraw(drawIndices, header->primType,profileFlags);
            vkCmdDrawIndexed(commandBuffer, drawIndices, 1, inst->offset / 2, 0, 0);
        }
        if(!emittedEdges && DebugVisualization::Get().mesh != DebugVisualization::Solid && !DebugVisualization::IsOverdraw()){
            DrawDebugEdges(commandBuffer,header,inst,shader,push,textureSet,boneOffset,neoSets,profileFlags);
            emittedEdges=true;
            vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        }
		i+=batchMeshes-1;
		inst+=batchMeshes-1;
		}
	}
	}
	if(measureBindings) gAtomicSubmissionStats.Add(bindings.stats);
}

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	if(!gvk.inFrame || header->vbo == VK_NULL_HANDLE ||
	   header->ibo == VK_NULL_HANDLE)
		return;
    // Reject before pipeline/descriptor binds, mesh iteration or draw emission.
    // Entity attribution is maintained even when per-draw GPU sampling is off.
    if(getWorldOcclusionEnabled()&&!dynamicObjectPass&&!vehicleAlphaPass&&
       gInstanceOwner.entityClass==0&&gInstanceOwner.model>=0&&
       !(gInstanceOwner.flags&DrawProfile::Distance)&&isWorldOccluded(atomic))return;
	drawAtomicMeshes(atomic, header, SHADER_WORLD, nil);
}

bool32
getOpaqueOrderInterval(Atomic *atomic, OpaqueOrder::Interval &depth, bool32 staticBuilding)
{
	++gOpaqueOrderStats.candidates;
	using namespace OpaqueOrder;
	if(!gOpaqueOrderEnabled || !gvk.inFrame || !atomic)
		return Reject(gOpaqueOrderStats,Inactive);
	if(!staticBuilding) return Reject(gOpaqueOrderStats,StaticEntity);
	if(!IsDefaultCallback(atomic->renderCB,Atomic::defaultRenderCB,gOpaqueOrderDefaultWrapper))
		return Reject(gOpaqueOrderStats,Callback);
	if(dynamicObjectPass || vehicleAlphaPass ||
	   !gstate.zTestEnabled || !gstate.zWriteEnabled || gstate.alphaTestRef>=254)
		return Reject(gOpaqueOrderStats,PassState);
	rw::ObjPipeline *base=atomic->getPipeline();
	if(!base || base->platform!=PLATFORM_VULKAN) return Reject(gOpaqueOrderStats,OpaqueOrder::Pipeline);
	ObjPipeline *pipe=(ObjPipeline*)base;
	if(pipe->instanceCB!=defaultInstanceCB || pipe->renderCB!=defaultRenderCB)
		return Reject(gOpaqueOrderStats,OpaqueOrder::Pipeline);
	rw::Geometry *geo=atomic->geometry;
	if(!geo || geo->numMorphTargets!=1 || geo->lockedSinceInst ||
	   (geo->flags&rw::Geometry::NATIVE) || !geo->meshHeader)
		return Reject(gOpaqueOrderStats,OpaqueOrder::Geometry);
	// LIGHT enables per-atomic lighting; it does not imply animation or motion.
	// The game has already proved a static building, and the default pipeline
	// resolves lighting per draw. Preserve that flag and all lighting inputs.
	if(!geo->instData || geo->instData->platform!=PLATFORM_VULKAN)
		return Reject(gOpaqueOrderStats,Instance);
	InstanceDataHeader *header=(InstanceDataHeader*)geo->instData;
	if(!header->orderBoundsValid || !header->inst || header->numMeshes==0 ||
	   header->numMeshes>64 || header->serialNumber!=geo->meshHeader->serialNum)
		return Reject(gOpaqueOrderStats,Instance);
	for(uint32 i=0;i<header->numMeshes;++i){
		const InstanceData &inst=header->inst[i];
		Material *m=inst.material;
		if(inst.vertexAlpha || !m || m->color.alpha!=255)
			return Reject(gOpaqueOrderStats,MeshAlpha);
		if(MatFX::getEffects(m)!=MatFX::NOTHING) return Reject(gOpaqueOrderStats,MatFx);
		Raster *r=m->texture ? m->texture->raster : nil;
		if(r && (r->platform!=PLATFORM_VULKAN || (r->type&0xF)!=Raster::TEXTURE ||
		   (r->parent && r->parent!=r) || rasterHasAlpha(r))) return Reject(gOpaqueOrderStats,OpaqueOrder::Texture);
		if(m->texture){
			const uint32 u=m->texture->getAddressU(),v=m->texture->getAddressV();
			if((u!=rw::Texture::WRAP&&u!=rw::Texture::MIRROR&&u!=rw::Texture::CLAMP)||
			   (v!=rw::Texture::WRAP&&v!=rw::Texture::MIRROR&&v!=rw::Texture::CLAMP))
				return Reject(gOpaqueOrderStats,Address);
		}
	}
	if(!atomic->getFrame()) return Reject(gOpaqueOrderStats,Projection);
	Matrix *ltm=atomic->getFrame()->getLTM();
	float object[16]={ltm->right.x,ltm->right.y,ltm->right.z,0,
		ltm->up.x,ltm->up.y,ltm->up.z,0,ltm->at.x,ltm->at.y,ltm->at.z,0,
		ltm->pos.x,ltm->pos.y,ltm->pos.z,1};
	float model[16],clipMatrix[16];
	multiplyMatrix(model,gvk.worldToPlay,object);
	const SceneData *scene=getSceneData();
	for(int eye=0;eye<2;++eye){
		multiplyMatrix(clipMatrix,scene->viewProj[eye],model);
		depth.nearDepth[eye]=1;depth.farDepth[eye]=0;
		for(int corner=0;corner<8;++corner){
			float p[3];for(int axis=0;axis<3;++axis)
				p[axis]=(corner&(1<<axis)) ? header->orderMax[axis] : header->orderMin[axis];
			const float w=clipMatrix[3]*p[0]+clipMatrix[7]*p[1]+clipMatrix[11]*p[2]+clipMatrix[15];
			const float z=clipMatrix[2]*p[0]+clipMatrix[6]*p[1]+clipMatrix[10]*p[2]+clipMatrix[14];
			if(!isfinite(w)||!isfinite(z)||w<=0.001f) return Reject(gOpaqueOrderStats,Projection);
			const float d=z/w;
			if(!isfinite(d)||d<0||d>1) return Reject(gOpaqueOrderStats,Projection);
			depth.nearDepth[eye]=fminf(depth.nearDepth[eye],d);
			depth.farDepth[eye]=fmaxf(depth.farDepth[eye],d);
		}
	}
	++gOpaqueOrderStats.eligible;
	return 1;
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_VULKAN);
	this->impl.instance = vulkan::instance;
	this->impl.uninstance = vulkan::uninstance;
	this->impl.render = vulkan::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}

ObjPipeline *
ObjPipeline::create(void)
{
	ObjPipeline *pipe = rwNewT(ObjPipeline, 1, MEMDUR_GLOBAL);
	pipe->init();
	return pipe;
}

ObjPipeline *
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

}
}

#else

namespace rw {
namespace vulkan {
void *destroyNativeGeometryData(void *object, int32, int32) { return object; }
}
}

#endif
