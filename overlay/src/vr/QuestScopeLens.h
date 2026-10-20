#pragma once
#include <math.h>
#include <vector>

// Fit the rear optical cap from CPU mesh geometry, so a replacement rifle
// does not inherit the tiny lens coordinates of a different weapon model.
// All inputs and outputs are model-local; no retail geometry is embedded.
namespace QuestScopeLens {
struct Vec {
	float x, y, z;
	Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
	Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
	Vec operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec cross(Vec a, Vec b)
{
	return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool normalise(Vec &a)
{
	const float len = sqrtf(dot(a,a));
	if(!isfinite(len) || len < 0.00001f) return false;
	a = a*(1.0f/len);
	return true;
}
struct Lens { Vec centre, right, up, forward; float width, height; };
struct Plane {
	Vec forward, right, up;
	float depth, minX, maxX, minY, maxY, area;
	int triangles;
};
template<class Vertex, class Triangle>
bool fit(const Vertex *vertices, int vertexCount, const Triangle *triangles,
    int triangleCount, Vec forward, Vec up, Vec muzzle, Lens &out)
{
	if(!vertices || !triangles || vertexCount < 3 || triangleCount < 2 ||
	   !normalise(forward)) return false;
	up = up-forward*dot(up,forward);
	if(!normalise(up)) return false;
	Vec right = cross(forward,up);
	if(!normalise(right)) return false;
	std::vector<Plane> planes;
	for(int i = 0; i < triangleCount; i++){
		Vec points[3];
		bool valid = true;
		for(int j = 0; j < 3; j++){
			const int index = triangles[i].v[j];
			if(index >= vertexCount){ valid = false; break; }
			points[j] = {vertices[index].x,vertices[index].y,vertices[index].z};
		}
		if(!valid) continue;
		Vec normal = cross(points[1]-points[0],points[2]-points[0]);
		const float twiceArea = sqrtf(dot(normal,normal));
		if(!normalise(normal) || fabsf(dot(normal,forward)) < 0.98f) continue;
		if(dot(normal,forward) < 0.0f) normal = normal*-1.0f;
		const float depth = dot(points[0],normal);
		Plane *plane = nullptr;
		for(Plane &p : planes)
			if(dot(p.forward,normal) > 0.9995f && fabsf(p.depth-depth) < 0.0008f){
				plane = &p; break;
			}
		if(!plane){
			Vec planeUp = up-normal*dot(up,normal);
			if(!normalise(planeUp)) continue;
			Vec planeRight = cross(normal,planeUp);
			normalise(planeRight);
			planes.push_back({normal,planeRight,planeUp,depth,
				1e6f,-1e6f,1e6f,-1e6f,0.0f,0});
			plane = &planes.back();
		}
		plane->area += twiceArea*0.5f;
		plane->triangles++;
		for(Vec p : points){
			const float x = dot(p,plane->right), y = dot(p,plane->up);
			if(x < plane->minX) plane->minX = x;
			if(x > plane->maxX) plane->maxX = x;
			if(y < plane->minY) plane->minY = y;
			if(y > plane->maxY) plane->maxY = y;
		}
	}
	float best = -1e6f;
	bool found = false;
	for(const Plane &p : planes){
		const float w = (p.maxX-p.minX)*0.5f, h = (p.maxY-p.minY)*0.5f;
		const float x = (p.maxX+p.minX)*0.5f, y = (p.maxY+p.minY)*0.5f;
		// A scope is a compact, filled, roughly round cap ABOVE the bore.
		// Stock plates, muzzle ends, sights and scope mounting brackets fail
		// at least one of these tests. Prefer the rear of equal-height caps.
		if(p.triangles < 2 || w < 0.005f || h < 0.005f ||
		   w > 0.075f || h > 0.075f || w/h < 0.65f || w/h > 1.5f ||
		   p.area < 2.0f*w*h || y < dot(muzzle,p.up)+0.012f) continue;
		const Vec centre = p.right*x+p.up*y+p.forward*p.depth;
		const float score = dot(centre,up)*4.0f-dot(centre,forward)*0.15f;
		if(score <= best) continue;
		best = score;
		out = {centre, p.right, p.up, p.forward,
			w*0.90f,h*0.90f};
		found = true;
	}
	return found;
}
}
