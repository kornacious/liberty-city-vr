#include "common.h"

#include "FileMgr.h"
#include "GameRenderCommand.h"
#include "Timecycle.h"
#include "rtpng.h"
#include "VRHandModel.h"

#include <string.h>
#include <vector>

// The source hand meshes and poses are from UltimateXR by VRMADA, licensed
// under the MIT License: https://github.com/VRMADA/ultimatexr-unity
namespace VRHandModel
{
namespace
{
enum {
	HAND_COUNT = 2,
	POSE_COUNT = 4,
	MAX_VERTEX_COUNT = 65535,
	MAX_INDEX_COUNT = 1000000
};

struct DiskHeader
{
	char magic[4];
	uint32 version;
	uint32 vertexCount;
	uint32 indexCount;
};

struct DiskVertex
{
	float position[POSE_COUNT][3];
	float normal[POSE_COUNT][3];
	float u;
	float v;
};

struct HandVertex
{
	CVector position[POSE_COUNT];
	CVector normal[POSE_COUNT];
	float u;
	float v;
};

struct HandMesh
{
	bool attempted;
	bool loaded;
	size_t indexTip=0;
	std::vector<HandVertex> vertices;
	std::vector<RwImVertexIndex> indices;
	std::vector<RwIm3DVertex> renderVertices;

	HandMesh(void) : attempted(false), loaded(false) {}
};

HandMesh gHands[HAND_COUNT];
RwTexture *gAlbedo;
bool gAlbedoAttempted;

static_assert(sizeof(DiskHeader) == 16, "Unexpected UXRH header layout");
static_assert(sizeof(DiskVertex) == sizeof(float)*26, "Unexpected UXRH vertex layout");

float
Clamp01(float value)
{
	if(value < 0.0f) return 0.0f;
	if(value > 1.0f) return 1.0f;
	return value;
}

bool
ReadExact(int file, void *destination, size_t size)
{
	uint8 *bytes = (uint8*)destination;
	size_t offset = 0;
	while(offset < size){
		const size_t read = CFileMgr::Read(file, (char*)bytes+offset,
			(ssize_t)(size-offset));
		if(read == 0)
			return false;
		offset += read;
	}
	return true;
}

bool
LoadHand(int hand)
{
	if(hand < 0 || hand >= HAND_COUNT)
		return false;
	HandMesh &mesh = gHands[hand];
	if(mesh.attempted)
		return mesh.loaded;
	mesh.attempted = true;

	const char *path = hand == 0 ?
		"models\\vrhands\\BigHandLeft.uxrh" :
		"models\\vrhands\\BigHandRight.uxrh";
	const int file = CFileMgr::OpenFile(path, "rb");
	if(file == 0)
		return false;

	DiskHeader header;
	bool ok = ReadExact(file, &header, sizeof(header));
	if(!ok || memcmp(header.magic, "UXRH", 4) != 0 || header.version != 1 ||
	   header.vertexCount == 0 || header.vertexCount > MAX_VERTEX_COUNT ||
	   header.indexCount == 0 || header.indexCount > MAX_INDEX_COUNT ||
	   header.indexCount % 3 != 0){
		CFileMgr::CloseFile(file);
		return false;
	}

	mesh.vertices.resize(header.vertexCount);
	mesh.indices.resize(header.indexCount);
	mesh.renderVertices.resize(header.vertexCount);
	for(uint32 i = 0; i < header.vertexCount && ok; i++){
		DiskVertex vertex;
		ok = ReadExact(file, &vertex, sizeof(vertex));
		if(!ok)
			break;
		for(int pose = 0; pose < POSE_COUNT; pose++){
			mesh.vertices[i].position[pose] = CVector(
				vertex.position[pose][0], vertex.position[pose][1], vertex.position[pose][2]);
			mesh.vertices[i].normal[pose] = CVector(
				vertex.normal[pose][0], vertex.normal[pose][1], vertex.normal[pose][2]);
		}
		mesh.vertices[i].u = vertex.u;
		mesh.vertices[i].v = vertex.v;
	}
	if(ok)
		ok = ReadExact(file, &mesh.indices[0], mesh.indices.size()*sizeof(mesh.indices[0]));
	CFileMgr::CloseFile(file);
	if(ok){
		for(size_t i = 0; i < mesh.indices.size(); i++){
			if(mesh.indices[i] >= mesh.vertices.size()){
				ok = false;
				break;
			}
		}
	}
	if(!ok){
		mesh.vertices.clear();
		mesh.indices.clear();
		mesh.renderVertices.clear();
		return false;
	}
	mesh.loaded = true;
	// Pose 1 closes the other fingers but leaves the index extended. Its most
	// distal surface vertex is the touch point, shared by every animated pose.
	mesh.indexTip=0;
	for(size_t i=1; i<mesh.vertices.size(); ++i)
		if(mesh.vertices[i].position[1].x>mesh.vertices[mesh.indexTip].position[1].x)
			mesh.indexTip=i;
	return true;
}

bool
LoadAlbedo(void)
{
	if(gAlbedoAttempted)
		return gAlbedo != nil;
	gAlbedoAttempted = true;
	RwImage *image = RtPNGImageRead("models\\vrhands\\ClaudeHandsAlbedo.png");
	if(image == nil)
		image = RtPNGImageRead("models\\vrhands\\BigHandsAlbedo.png");
	if(image == nil)
		return false;

	int32 width, height, depth, format;
	RwImageFindRasterFormat(image, rwRASTERTYPETEXTURE,
		&width, &height, &depth, &format);
	RwRaster *raster = RwRasterCreate(width, height, depth, format);
	if(raster == nil || RwRasterSetFromImage(raster, image) == nil){
		if(raster) RwRasterDestroy(raster);
		RwImageDestroy(image);
		return false;
	}
	RwImageDestroy(image);
	gAlbedo = RwTextureCreate(raster);
	if(gAlbedo == nil){
		RwRasterDestroy(raster);
		return false;
	}
	RwTextureSetFilterMode(gAlbedo, rwFILTERLINEAR);
	return true;
}

CVector
InterpolatePose(const CVector pose[POSE_COUNT], float grip, float trigger)
{
	const float openWeight = (1.0f-grip)*(1.0f-trigger);
	const float gripWeight = grip*(1.0f-trigger);
	const float triggerWeight = (1.0f-grip)*trigger;
	const float bothWeight = grip*trigger;
	return pose[0]*openWeight + pose[1]*gripWeight +
		pose[2]*triggerWeight + pose[3]*bothWeight;
}

uint8
ShadeChannel(float shade)
{
	int value = (int)(shade*255.0f + 0.5f);
	if(value < 0) value = 0;
	if(value > 255) value = 255;
	return (uint8)value;
}
}

void
Shutdown(void)
{
	if(gAlbedo){
		RwTextureDestroy(gAlbedo);
		gAlbedo = nil;
	}
	gAlbedoAttempted = false;
	for(int hand = 0; hand < HAND_COUNT; hand++){
		HandMesh &mesh = gHands[hand];
		mesh.attempted = false;
		mesh.loaded = false;
		std::vector<HandVertex>().swap(mesh.vertices);
		std::vector<RwImVertexIndex>().swap(mesh.indices);
		std::vector<RwIm3DVertex>().swap(mesh.renderVertices);
	}
}

bool
IndexTip(int hand, const CVector &position, const CVector &right,
    const CVector &up, const CVector &forward, float grip, float trigger, CVector *tip)
{
    if(!tip || hand<0 || hand>=HAND_COUNT) return false;
    grip=Clamp01(grip); trigger=Clamp01(trigger);
    if(LoadHand(hand) && LoadAlbedo()) {
        const HandMesh &mesh=gHands[hand];
        const CVector local=InterpolatePose(mesh.vertices[mesh.indexTip].position,grip,trigger);
        *tip=position+forward*(local.x-.055f)+right*local.z-up*local.y;
    } else {
        // Same two index segments as the procedural fallback in main.cpp.
        const float a=.10f+trigger*1.05f,b=.18f+trigger*2.00f;
        *tip=position+forward*.090f-up*.005f+right*.030f+
            (forward*cosf(a)-up*sinf(a))*.045f+(forward*cosf(b)-up*sinf(b))*.033f;
    }
    return true;
}

bool
Render(int hand, const CVector &position, const CVector &right,
	const CVector &up, const CVector &forward, float grip, float trigger)
{
	if(hand < 0 || hand >= HAND_COUNT || !LoadHand(hand) || !LoadAlbedo())
		return false;
	HandMesh &mesh = gHands[hand];
	grip = Clamp01(grip);
	trigger = Clamp01(trigger);

	CVector lightDirection(-0.35f, -0.25f, 0.90f);
	const int32 sunIdx = CTimeCycle::m_CurrentStoredValue & 15;
	const CVector &sunDir = CTimeCycle::m_VectorToSun[sunIdx];
	if(sunDir.MagnitudeSqr() > 0.001f){
		lightDirection = sunDir + CVector(0.0f, 0.0f, 0.65f);
	}
	lightDirection.Normalise();

	// Blend world timecycle color with a readable floor so the hands match
	// Claude's jacket and weapons across day, night, and rain without glowing.
	const float tcAmbR = 0.56f + 0.44f*Clamp01(CTimeCycle::GetAmbientRed()*1.35f + 0.18f);
	const float tcAmbG = 0.56f + 0.44f*Clamp01(CTimeCycle::GetAmbientGreen()*1.35f + 0.18f);
	const float tcAmbB = 0.56f + 0.44f*Clamp01(CTimeCycle::GetAmbientBlue()*1.35f + 0.18f);
	const float tcDirR = 0.50f + 0.50f*Clamp01(CTimeCycle::GetDirectionalRed()*1.25f + 0.20f);
	const float tcDirG = 0.50f + 0.50f*Clamp01(CTimeCycle::GetDirectionalGreen()*1.25f + 0.20f);
	const float tcDirB = 0.50f + 0.50f*Clamp01(CTimeCycle::GetDirectionalBlue()*1.25f + 0.20f);

	for(size_t i = 0; i < mesh.vertices.size(); i++){
		const HandVertex &source = mesh.vertices[i];
		const CVector localPosition = InterpolatePose(source.position, grip, trigger);
		CVector localNormal = InterpolatePose(source.normal, grip, trigger);
		if(localNormal.MagnitudeSqr() > 0.000001f)
			localNormal.Normalise();

		// UltimateXR's baked model uses +X along the fingers, +Y along the
		// palm normal and +Z across the palm. Position the wrist 5.5 cm behind
		// the OpenXR controller origin to match the former procedural hand.
		const CVector worldPosition = position +
			forward*(localPosition.x-0.055f) +
			right*localPosition.z - up*localPosition.y;
		CVector worldNormal = forward*localNormal.x +
			right*localNormal.z - up*localNormal.y;
		if(worldNormal.MagnitudeSqr() > 0.000001f)
			worldNormal.Normalise();
		const float diffuse = DotProduct(worldNormal, lightDirection);
		const float posDiffuse = diffuse > 0.0f ? diffuse : 0.0f;
		const float backFill = diffuse < 0.0f ? (-diffuse * 0.12f) : 0.0f;
		const float baseShade = 0.70f + 0.30f*posDiffuse + backFill;

		// Soft shadow where the wrist tucks into Claude's black leather jacket cuff.
		const float cuffT = Clamp01((localPosition.x - 0.002f) / 0.024f);
		const float cuffSmooth = cuffT * cuffT * (3.0f - 2.0f * cuffT);
		const float cuffShade = 0.22f + 0.78f * cuffSmooth;

		const float shadeR = baseShade * cuffShade * (0.72f*tcAmbR + 0.28f*tcDirR);
		const float shadeG = baseShade * cuffShade * (0.72f*tcAmbG + 0.28f*tcDirG);
		const float shadeB = baseShade * cuffShade * (0.72f*tcAmbB + 0.28f*tcDirB);

		RwIm3DVertex &destination = mesh.renderVertices[i];
		RwIm3DVertexSetPos(&destination, worldPosition.x, worldPosition.y, worldPosition.z);
		RwIm3DVertexSetU(&destination, source.u);
		RwIm3DVertexSetV(&destination, source.v);
		RwIm3DVertexSetRGBA(&destination,
			ShadeChannel(shadeR), ShadeChannel(shadeG), ShadeChannel(shadeB), 255);
	}

	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(gAlbedo));
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
	bool rendered = false;
	if(GameRender::BeginImmediate3D(&mesh.renderVertices[0], (uint32)mesh.renderVertices.size(), nil,
	   rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA|rwIM3D_VERTEXUV,
	   GameRender::IMMEDIATE3D_DRAW_VR_HAND)){
		rendered = GameRender::SubmitImmediate3DIndexed(rwPRIMTYPETRILIST,
			&mesh.indices[0], (int32)mesh.indices.size());
		GameRender::EndImmediate3D();
	}
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	return rendered;
}
}
