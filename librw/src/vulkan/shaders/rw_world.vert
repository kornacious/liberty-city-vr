#version 450
#extension GL_EXT_multiview : require

#include "rw_common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColour;
layout(location = 3) in vec2 inTexCoord;
layout(location = 8) in vec2 inTexCoord1;
#ifdef RW_STATIC_INSTANCING
layout(location = 4) in vec4 instance0;
layout(location = 5) in vec4 instance1;
layout(location = 6) in vec4 instance2;
layout(location = 7) in vec4 instance3;
#endif
#ifdef RW_STATIC_ARRAY
layout(location = 4) in vec4 inMaterialColour;
layout(location = 5) in vec2 inSurface;
layout(location = 6) in uint inTextureLayer;
layout(location = 7) flat out uint fragTextureLayer;
#endif
#ifdef RW_STATIC_ARRAY_CROSS
struct CrossArrayObject { mat4 model; vec4 ambient; vec4 directional; };
layout(set = 2, binding = 0, std140) uniform CrossArrayObjects {
	CrossArrayObject objects[128]; // CrossObjectPageSize
} crossArray;
#endif

layout(location = 0) out vec4 fragColour;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec2 fragFog;
layout(location = 3) flat out vec2 fragDynamicMotion;
layout(location = 4) out vec3 fragWorldPos;
layout(location = 5) out vec3 fragNormal;
layout(location = 6) out vec2 fragTexCoord1;

// Lighting is evaluated per vertex, matching what the original fixed-function
// RenderWare pipeline did. Moving it to the fragment stage would look slightly
// better and cost noticeably more on a tiler at 1680x1760 per eye.
void main()
{
#ifdef RW_STATIC_ARRAY_CROSS
	CrossArrayObject object = crossArray.objects[uint(gl_InstanceIndex) + (inTextureLayer >> 6)];
	mat4 model = object.model;
	vec4 rootMotion = vec4(model[0].w, model[1].w, model[2].w, model[3].w);
	model[0].w = 0.0;
	model[1].w = 0.0;
	model[2].w = 0.0;
	model[3].w = 1.0;
	vec4 ambientLight = object.ambient;
	vec4 lightDirColour = object.directional;
#else
#ifdef RW_STATIC_INSTANCING
	mat4 model = mat4(instance0,instance1,instance2,instance3);
#else
	mat4 model = RwModelMatrix();
#endif
	vec4 rootMotion = RwRootScreenMotion();
	vec4 ambientLight = push.ambientLight;
	vec4 lightDirColour = push.lightDirColour;
#endif
	vec4 world = model * vec4(inPosition, 1.0);
	gl_Position = scene.viewProj[gl_ViewIndex] * world;
	fragDynamicMotion = gl_ViewIndex == 0 ? rootMotion.xy : rootMotion.zw;

	// RenderWare's fixed-function sum: prelight, plus ambient scaled by the
	// material's ambient coefficient, plus the directional term -- additive,
	// not multiplied. Multiplying makes night scenes black: dark prelight
	// times anything stays dark, where the original adds the ambient on top.
	//
	// push.model carries the game camera as well as the object transform, so
	// the normal lands in play space; the light direction was rotated into the
	// same space on the CPU, keeping the dot product honest.
	vec3 normal = normalize(mat3(model) * inNormal);
#ifdef RW_STATIC_ARRAY
	vec4 materialColour = inMaterialColour;
	vec2 surface = inSurface;
	fragTextureLayer = inTextureLayer & 63u;
#else
	vec4 materialColour = push.materialColour;
	vec2 surface = push.surfaceProps.xy;
#endif
	vec3 colour = inColour.rgb + ambientLight.rgb * surface.x;
	vec4 dirColour = unpackUnorm4x8(floatBitsToUint(lightDirColour.w));
	if(scene.neoControl.x>0.5 && (int(ambientLight.w)&8)!=0) dirColour.rgb=scene.neoDiffuse.rgb*scene.neoSpec.a;
	colour += dirColour.rgb *
	          max(dot(normal, -lightDirColour.xyz), 0.0) *
	          surface.y;

	if(scene.neoControl.x > 0.5 && (int(ambientLight.w) & 16) != 0){
		colour+=RwNeoRimLight(normal);
	}
	fragColour = vec4(clamp(colour, 0.0, 1.0), inColour.a) * materialColour;
    if(RW_VISUALIZATION != 0 && scene.debugVisualization.x==4.0) fragColour.rgb=inColour.rgb;
    if(RW_VISUALIZATION != 0 && scene.debugVisualization.x==6.0) fragColour.rgb=clamp(colour,0.0,1.0);
	fragTexCoord = inTexCoord;
	fragTexCoord1 = inTexCoord1;
	// For the per-pixel dynamic lights. Prelit world sectors carry a dummy
	// normal; surfaceProps.z tells the fragment stage whether this one is real.
	if(RW_WORLD_EFFECTS != 0 || scene.neoControl.x > 0.5 || RW_VISUALIZATION != 0){
		fragWorldPos = world.xyz;
		fragNormal = normal;
	}

	// Fog by distance from the eye rather than by depth along the view axis.
	// Planar depth makes the fog a wall perpendicular to the gaze, and in a
	// headset that wall turns with the head: the mist appears to follow the
	// player instead of sitting still in the world.
	vec3 viewOffset = world.xyz - scene.im2dParams.yzw;
	float fogDistance = length(viewOffset);
	float fogFactor = (scene.fogParams.w * float(int(ambientLight.w) & 1)) > 0.5 ?
		clamp((scene.fogParams.y - fogDistance) * scene.fogParams.z, 0.0, 1.0) :
		1.0;
	float elevation = fogDistance > 1e-4 ? (viewOffset.y / fogDistance) : 0.0;
	float skyBlend = clamp(elevation * 4.0 + 0.1, 0.0, 1.0);
	fragFog = vec2(fogFactor, skyBlend);
}
