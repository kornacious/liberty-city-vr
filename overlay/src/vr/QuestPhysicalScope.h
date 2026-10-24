#pragma once
#include "QuestScopeLens.h"

// Included by the Android tracked-weapon renderer after its RenderWare and
// GameRender declarations. Geometry stays attached to the same model matrix
// as the weapon; the offscreen view follows the authoritative firing ray.
static void
RenderQuestPhysicalScope(int hand, const CMatrix &weapon, RpAtomic *atomic)
{
	CVector muzzle, bore;
	const bool haveAim = OculusVR::GetTrackedWeaponAim(hand,
		WEAPONTYPE_SNIPERRIFLE, &muzzle, &bore);
	CVector local(-0.0250f, -0.1360f, -0.0221f);
	CVector localUp(0.0f,0.0f,1.0f);
	CVector localForward(0.0f,1.0f,0.0f);
	float halfWidth = 0.0068f, halfHeight = 0.0060f;
	if(haveAim){
		struct LensCache {
			RpGeometry *geometry;
			RwV3d *vertices;
			QuestScopeLens::Lens lens;
			bool found;
		};
		static LensCache cache[2] = {};
		RpGeometry *geometry = RpAtomicGetGeometry(atomic);
		RwV3d *vertices = geometry ?
			RpMorphTargetGetVertices(RpGeometryGetMorphTarget(geometry,0)) : nil;
		LensCache &entry = cache[hand];
		if(entry.geometry != geometry || entry.vertices != vertices){
			entry.geometry = geometry;
			entry.vertices = vertices;
			const CVector offset = muzzle-weapon.GetPosition();
			entry.found = geometry && QuestScopeLens::fit(vertices,
				RpGeometryGetNumVertices(geometry), RpGeometryGetTriangles(geometry),
				RpGeometryGetNumTriangles(geometry),
				QuestScopeLens::Vec{DotProduct(bore,weapon.GetRight()),
					DotProduct(bore,weapon.GetForward()),DotProduct(bore,weapon.GetUp())},
				QuestScopeLens::Vec{0.0f,0.0f,1.0f},
				QuestScopeLens::Vec{DotProduct(offset,weapon.GetRight()),
					DotProduct(offset,weapon.GetForward()),DotProduct(offset,weapon.GetUp())},
				entry.lens);
		}
		if(entry.found){
			const QuestScopeLens::Lens &lens = entry.lens;
			local = CVector(lens.centre.x,lens.centre.y,lens.centre.z);
			localUp = CVector(lens.up.x,lens.up.y,lens.up.z);
			localForward = CVector(lens.forward.x,lens.forward.y,lens.forward.z);
			halfWidth = lens.width;
			halfHeight = lens.height;
		}
	}
	const CVector centre = weapon.GetPosition() + weapon.GetRight()*local.x +
		weapon.GetForward()*local.y + weapon.GetUp()*local.z;
	const CVector up = weapon.GetRight()*localUp.x+
		weapon.GetForward()*localUp.y+weapon.GetUp()*localUp.z;
	const CVector back = (weapon.GetRight()*localForward.x+
		weapon.GetForward()*localForward.y+weapon.GetUp()*localForward.z)*-1.0f;
	// Reconstruct screen-right in world space: the tracked weapon can have
	// a mirrored basis, which must not mirror the magnified picture.
	CVector right = CrossProduct(back*-1.0f, up);
	right.Normalise();
	RwRaster *image = nil;
	if(OculusVR::IsTrackedScopeActiveForHand(hand) && haveAim){
		// Use the closest aligned eye rather than the HMD midpoint. This lets
		// either eye aim without forcing the rifle between the player's eyes.
		float bestScore = 1000.0f, eyeDistance = 0.0f;
		for(uint32 eye = 0; eye < 2; eye++){
			float er[3], eu[3], ef[3], ep[3];
			if(!rw::vulkan::getFirstPersonEyeViewFrame(eye, er, eu, ef, ep)) continue;
			CVector delta = CVector(ep[0], ep[1], ep[2])-centre;
			const float distance = DotProduct(delta, back);
			if(distance < 0.04f || distance > 0.65f) continue;
			const float x = DotProduct(delta, right), y = DotProduct(delta, up);
			const float score = (x*x+y*y)/(distance*distance);
			if(score < 0.04f && score < bestScore){
				bestScore = score;
				eyeDistance = distance;
			}
		}
		if(eyeDistance > 0.0f){
			CVector cameraRight = CrossProduct(bore, up);
			if(cameraRight.MagnitudeSqr() > 0.0001f){
				cameraRight.Normalise();
				CVector cameraUp = CrossProduct(cameraRight, bore);
				cameraUp.Normalise();
				// The reticle centre follows the bullet ray, including the
				// muzzle's offset from the optical housing. A small texture
				// still gives real magnification; no crop of the eye image.
				const float zoom = OculusVR::GetTrackedPhysicalScopeZoom();
				const float fov = 2.0f*atanf(halfHeight/(eyeDistance*zoom));
				rw::vulkan::setPhysicalScopeFrame(&muzzle.x, &cameraRight.x,
					&cameraUp.x, &bore.x, fov, halfWidth/halfHeight);
				image = rw::vulkan::getPhysicalScopeRaster();
			}
		}
	}

	// The opaque backing remains when lowered, misaligned or unavailable.
	// The textured fan is inset inside the housing and lifted toward the eye
	// just enough to cover the original mesh cap without depth fighting.
	RwIm3DVertex vertices[34];
	RwImVertexIndex indices[96];
	const CVector lensCentre = centre+back*0.0007f;
	for(int i = 0; i < 34; i++){
		const float angle = (i-1)*(6.28318530718f/32.0f);
		const float x = i == 0 ? 0.0f : cosf(angle);
		const float y = i == 0 ? 0.0f : sinf(angle);
		CVector point = lensCentre+right*(x*halfWidth)+up*(y*halfHeight);
		RwIm3DVertexSetPos(&vertices[i], point.x, point.y, point.z);
		RwIm3DVertexSetU(&vertices[i], 0.5f+0.5f*x);
		RwIm3DVertexSetV(&vertices[i], 0.5f-0.5f*y);
		const uint8 shade = image ? 255 : 12;
		RwIm3DVertexSetRGBA(&vertices[i], shade, shade, shade, 255);
	}
	for(int i = 0; i < 32; i++){
		indices[3*i] = 0;
		indices[3*i+1] = i+1;
		indices[3*i+2] = i+2;
	}
	// Preserve all state touched here; the world renderer continues afterward.
	void *saved[9];
	const RwRenderState states[9] = { rwRENDERSTATETEXTURERASTER,
		rwRENDERSTATEZTESTENABLE, rwRENDERSTATEZWRITEENABLE,
		rwRENDERSTATEVERTEXALPHAENABLE, rwRENDERSTATEFOGENABLE,
		rwRENDERSTATECULLMODE, rwRENDERSTATETEXTUREFILTER,
		rwRENDERSTATETEXTUREADDRESSU, rwRENDERSTATETEXTUREADDRESSV };
	for(int i = 0; i < 9; i++) RwRenderStateGet(states[i], &saved[i]);
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, image);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);
	const GameRender::eImmediate3DDrawRole role = image ?
		GameRender::IMMEDIATE3D_DRAW_VR_SCOPE : GameRender::IMMEDIATE3D_DRAW_VR_HAND;
	if(GameRender::BeginImmediate3D(vertices, 34, nil,
	       rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA|rwIM3D_VERTEXUV, role)){
		GameRender::SubmitImmediate3DIndexed(rwPRIMTYPETRILIST, indices, 96);
		GameRender::EndImmediate3D();
	}
	// Keep the reticle in the same deferred pass, in front of the new image.
	RwIm3DVertex bars[48];
	RwImVertexIndex barIndices[180];
	int vertexCount = 0, indexCount = 0;
	const CVector reticle = lensCentre+back*0.0003f;
	AppendVrHandBox(bars, vertexCount, barIndices, indexCount,
		reticle, right, up, back, halfWidth*0.85f, 0.00012f, 0.0001f, 20, 20, 20);
	AppendVrHandBox(bars, vertexCount, barIndices, indexCount,
		reticle, right, up, back, 0.00012f, halfHeight*0.85f, 0.0001f, 20, 20, 20);
	AppendVrHandBox(bars, vertexCount, barIndices, indexCount,
		reticle+back*0.00015f, right, up, back,
		0.00025f, 0.00025f, 0.0001f, 255, 40, 40);
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	if(GameRender::BeginImmediate3D(bars, vertexCount, nil,
	       rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA, role)){
		GameRender::SubmitImmediate3DIndexed(rwPRIMTYPETRILIST, barIndices, indexCount);
		GameRender::EndImmediate3D();
	}
	for(int i = 0; i < 9; i++) RwRenderStateSet(states[i], saved[i]);
}
