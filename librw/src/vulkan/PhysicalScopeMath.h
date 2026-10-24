#pragma once
#include <math.h>
#include <string.h>

namespace PhysicalScopeMath {
inline bool sphereVisible(const float m[16], const float sphere[4])
{
	// The captured main-view render list is broad. Reject objects outside the
	// much narrower optic before replaying their meshes on the GPU.
	for(int plane = 0; plane < 5; plane++){
		float p[4];
		for(int i = 0; i < 4; i++)
			p[i] = plane == 4 ? m[4*i+2] :
				m[4*i+3]+(plane%2 == 0 ? 1.0f : -1.0f)*m[4*i+plane/2];
		const float distance = p[0]*sphere[0]+p[1]*sphere[1]+p[2]*sphere[2]+p[3];
		const float length = sqrtf(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
		if(distance < -sphere[3]*length) return false;
	}
	return true;
}
inline float dot(const float a[3], const float b[3])
{
	return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
inline bool normalise(float v[3])
{
	const float length = sqrtf(dot(v, v));
	if(!isfinite(length) || length < 0.0001f) return false;
	for(int i = 0; i < 3; i++) v[i] /= length;
	return true;
}
// Positive forward, Vulkan Y down and depth 0..1; infinite far plane.
// The model matrices already include worldToPlay, so build the optic in the
// same play space. This preserves room-scale translation and weapon roll.
inline bool projection(float out[16], const float fold[16],
    const float position[3], const float right[3], const float up[3],
    const float forward[3], float verticalFov, float aspect)
{
	if(!isfinite(verticalFov) || verticalFov < 0.001f || verticalFov > 3.0f ||
	   !isfinite(aspect) || aspect < 0.1f) return false;
	float p[3], r[3], u[3], f[3];
	for(int i = 0; i < 3; i++){
		p[i] = fold[i]*position[0]+fold[4+i]*position[1]+
		       fold[8+i]*position[2]+fold[12+i];
		r[i] = fold[i]*right[0]+fold[4+i]*right[1]+fold[8+i]*right[2];
		u[i] = fold[i]*up[0]+fold[4+i]*up[1]+fold[8+i]*up[2];
		f[i] = fold[i]*forward[0]+fold[4+i]*forward[1]+fold[8+i]*forward[2];
		if(!isfinite(p[i])) return false;
	}
	if(!normalise(r) || !normalise(u) || !normalise(f)) return false;
	const float sy = 1.0f/tanf(verticalFov*0.5f), sx = sy/aspect;
	memset(out, 0, 16*sizeof(float));
	for(int i = 0; i < 3; i++){
		out[4*i] = r[i]*sx;
		out[4*i+1] = -u[i]*sy;
		out[4*i+2] = out[4*i+3] = f[i];
	}
	out[12] = -dot(r, p)*sx;
	out[13] = dot(u, p)*sy;
	out[14] = -dot(f, p)-0.05f;
	out[15] = -dot(f, p);
	return true;
}
}
