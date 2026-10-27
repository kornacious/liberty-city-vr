#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "minimal_profiler_layout.h"

int main()
{
	int cases = 0;
	for(float halfFov : {0.12f, 0.4f, 0.8f, 1.1f})
	for(float cant : {-0.15f, 0.0f, 0.15f}){
		XrView eyes[2] = {};
		for(int i = 0; i < 2; ++i){
			const float yaw = (i ? 1 : -1)*cant;
			eyes[i].pose.orientation = {0, sinf(yaw/2), 0, cosf(yaw/2)};
			eyes[i].pose.position = {i ? 0.034f : -0.034f, 0.003f, 0.01f};
			eyes[i].fov = {-halfFov, halfFov*0.9f, halfFov*0.8f, -halfFov};
		}
		for(unsigned row : {0u,1u}){
		XrCompositionLayerQuad panel = {};
		if(!FitMinimalProfiler(eyes, panel, row)){
			// A narrow FOV canted beyond its half-angle has no shared centre.
			assert(halfFov == 0.12f && cant != 0.0f);
			continue;
		}
		assert(panel.pose.position.x > 0 && panel.pose.position.y > 0);
		assert(fabsf(panel.size.height/panel.size.width-0.125f) < 1e-6f);
		if(row){
			XrCompositionLayerQuad fps={};
			assert(FitMinimalProfiler(eyes,fps));
			assert(panel.pose.position.y+panel.size.height/2 < fps.pose.position.y-fps.size.height/2);
		}
		for(const XrView &eye : eyes)
		for(int x : {-1, 1}) for(int y : {-1, 1}){
			const float dx = panel.pose.position.x+x*panel.size.width/2-eye.pose.position.x;
			const float dy = panel.pose.position.y+y*panel.size.height/2-eye.pose.position.y;
			const float dz = panel.pose.position.z-eye.pose.position.z;
			const auto &q = eye.pose.orientation;
			const float ex = (1-2*q.y*q.y)*dx-2*q.y*q.w*dz;
			const float ez = 2*q.y*q.w*dx+(1-2*q.y*q.y)*dz;
			assert(ez < 0);
			assert(ex/-ez > tanf(eye.fov.angleLeft));
			assert(ex/-ez < tanf(eye.fov.angleRight));
			assert(dy/-ez > tanf(eye.fov.angleDown));
			assert(dy/-ez < tanf(eye.fov.angleUp));
		}
		++cases;
		}
	}
	XrView invalid[2] = {};
	XrCompositionLayerQuad panel = {};
	assert(!FitMinimalProfiler(invalid, panel));
	std::printf("Minimal profiler: %d asymmetric/canted binocular layouts fit every corner; invalid FOV rejected.\n", cases);
}
