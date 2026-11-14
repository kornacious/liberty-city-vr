#pragma once
#include "RenderDebugCore.h"
namespace RenderDebug {
void InitializeWorld();
void ShutdownWorld();
void ResetWorld(const Settings &settings,const Camera &camera);
void UpdateWorld(const Settings &settings,const Camera &camera,double elapsed,double step);
void RenderWorld(const Settings &settings);
void SetView(const Camera &camera,const Settings &settings);
float LodDistanceMultiplier();
}
