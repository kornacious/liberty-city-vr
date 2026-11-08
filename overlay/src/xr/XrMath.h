#pragma once
#include <cmath>
#include <openxr/openxr.h>

// Column-major transforms shared by the Quest and Windows Vulkan paths.
namespace XrMath {
struct Mat4 { float m[16]; };

inline Mat4
identity(void)
{
	Mat4 r = {};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

inline Mat4
multiply(const Mat4 &a, const Mat4 &b)
{
	Mat4 r;
	for(int col = 0; col < 4; col++)
		for(int row = 0; row < 4; row++){
			float sum = 0.0f;
			for(int k = 0; k < 4; k++)
				sum += a.m[k*4 + row] * b.m[col*4 + k];
			r.m[col*4 + row] = sum;
		}
	return r;
}

// Asymmetric off-axis projection for a Vulkan clip volume: depth 0..1 and a
// downward Y axis. The four OpenXR angles are signed half-angles from the
// view direction, so tangents cannot be assumed symmetric on Quest optics.
inline Mat4
projectionFromFov(const XrFovf &fov, float nearZ, float farZ)
{
	const float tanLeft = tanf(fov.angleLeft);
	const float tanRight = tanf(fov.angleRight);
	const float tanUp = tanf(fov.angleUp);
	const float tanDown = tanf(fov.angleDown);

	const float tanWidth = tanRight - tanLeft;
	const float tanHeight = tanDown - tanUp;	// flipped for Vulkan

	Mat4 r = {};
	r.m[0]  = 2.0f / tanWidth;
	r.m[5]  = 2.0f / tanHeight;
	r.m[8]  = (tanRight + tanLeft) / tanWidth;
	r.m[9]  = (tanUp + tanDown) / tanHeight;
	if(farZ <= 0.0f || std::isinf(farZ)){
		r.m[10] = -1.0f;
		r.m[11] = -1.0f;
		r.m[14] = -nearZ;
	}else{
		r.m[10] = -farZ / (farZ - nearZ);
		r.m[11] = -1.0f;
		r.m[14] = -(farZ * nearZ) / (farZ - nearZ);
	}
	return r;
}

// Rigid inverse of the view pose: transpose the rotation, then rotate the
// negated translation by it.
inline Mat4
viewFromPose(const XrPosef &pose)
{
	const XrQuaternionf &q = pose.orientation;
	const float x = q.x, y = q.y, z = q.z, w = q.w;

	const float r00 = 1.0f - 2.0f*(y*y + z*z);
	const float r01 = 2.0f*(x*y - z*w);
	const float r02 = 2.0f*(x*z + y*w);
	const float r10 = 2.0f*(x*y + z*w);
	const float r11 = 1.0f - 2.0f*(x*x + z*z);
	const float r12 = 2.0f*(y*z - x*w);
	const float r20 = 2.0f*(x*z - y*w);
	const float r21 = 2.0f*(y*z + x*w);
	const float r22 = 1.0f - 2.0f*(x*x + y*y);

	const float px = pose.position.x, py = pose.position.y, pz = pose.position.z;

	Mat4 r = {};
	// Transposed rotation, written column-major.
	r.m[0] = r00; r.m[1] = r01; r.m[2] = r02;
	r.m[4] = r10; r.m[5] = r11; r.m[6] = r12;
	r.m[8] = r20; r.m[9] = r21; r.m[10] = r22;
	r.m[12] = -(r00*px + r10*py + r20*pz);
	r.m[13] = -(r01*px + r11*py + r21*pz);
	r.m[14] = -(r02*px + r12*py + r22*pz);
	r.m[15] = 1.0f;
	return r;
}

// Rigid head transform from a pose: rotation, then translation.
inline Mat4
worldFromPose(const XrPosef &pose)
{
	const XrQuaternionf &q = pose.orientation;
	const float x = q.x, y = q.y, z = q.z, w = q.w;

	Mat4 r = {};
	r.m[0] = 1.0f - 2.0f*(y*y + z*z);
	r.m[1] = 2.0f*(x*y + z*w);
	r.m[2] = 2.0f*(x*z - y*w);
	r.m[4] = 2.0f*(x*y - z*w);
	r.m[5] = 1.0f - 2.0f*(x*x + z*z);
	r.m[6] = 2.0f*(y*z + x*w);
	r.m[8] = 2.0f*(x*z + y*w);
	r.m[9] = 2.0f*(y*z - x*w);
	r.m[10] = 1.0f - 2.0f*(x*x + y*y);
	r.m[12] = pose.position.x;
	r.m[13] = pose.position.y;
	r.m[14] = pose.position.z;
	r.m[15] = 1.0f;
	return r;
}

// Maps screen pixel coordinates onto a quad of the given size, centred a fixed
// distance ahead of the head. Screen Y grows downward, hence the negative Y
// scale; the plane sits at -Z because that is forward in OpenXR view space.
inline Mat4
im2dPlane(const XrPosef &headPose, float screenWidth, float screenHeight,
          float distance, float planeWidth, float planeHeight)
{
	Mat4 plane = {};
	plane.m[0] = screenWidth > 0.0f ? planeWidth / screenWidth : 0.0f;
	plane.m[5] = screenHeight > 0.0f ? -planeHeight / screenHeight : 0.0f;
	plane.m[10] = 1.0f;
	plane.m[12] = -planeWidth * 0.5f;
	plane.m[13] = planeHeight * 0.5f;
	plane.m[14] = -distance;
	plane.m[15] = 1.0f;

	return multiply(worldFromPose(headPose), plane);
}

inline Mat4
translationScale(float tx, float ty, float tz, float sx, float sy, float sz)
{
	Mat4 r = identity();
	r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
	r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
	return r;
}

} // namespace XrMath
