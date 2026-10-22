#pragma once

#include <math.h>
#include <openxr/openxr.h>

// Fit an axis-aligned head-space panel inside both eye frusta. A square
// inscribed at the panel depth is conservative even with asymmetric/canted
// views and IPD; every point in it satisfies all four planes of both eyes.
inline bool
FitMinimalProfiler(const XrView eyes[2], XrCompositionLayerQuad &panel)
{
	const float depth = 1.5f;
	float extent = depth*tanf(30.0f*0.01745329252f);
	for(int eye = 0; eye < 2; ++eye){
		const XrView &v = eyes[eye];
		const XrVector3f normals[4] = {
			{1, 0, tanf(v.fov.angleLeft)},
			{-1, 0, -tanf(v.fov.angleRight)},
			{0, 1, tanf(v.fov.angleDown)},
			{0, -1, -tanf(v.fov.angleUp)} };
		const XrQuaternionf &q = v.pose.orientation;
		for(const XrVector3f &n : normals){
			// Rotate an inward eye-space plane normal into head space.
			const XrVector3f t = {2*(q.y*n.z-q.z*n.y),
				2*(q.z*n.x-q.x*n.z), 2*(q.x*n.y-q.y*n.x)};
			const XrVector3f r = {n.x+q.w*t.x+q.y*t.z-q.z*t.y,
				n.y+q.w*t.y+q.z*t.x-q.x*t.z,
				n.z+q.w*t.z+q.x*t.y-q.y*t.x};
			const float available = -depth*r.z-r.x*v.pose.position.x-
				r.y*v.pose.position.y-r.z*v.pose.position.z;
			const float slope = fabsf(r.x)+fabsf(r.y);
			if(!isfinite(available) || available <= 0.0f || slope <= 0.0f)
				return false;
			extent = fminf(extent, available/slope);
		}
	}
	// Leave a margin from the visible limits and scale down for narrow FOVs.
	extent *= 0.9f;
	if(!isfinite(extent) || extent <= 0.0f)
		return false;
	panel.size.width = fminf(0.48f, extent);
	panel.size.height = panel.size.width*(64.0f/512.0f);
	panel.pose = {};
	panel.pose.orientation.w = 1.0f;
	panel.pose.position = {extent-panel.size.width*0.5f,
		extent-panel.size.height*0.5f, -depth};
	return true;
}
