#pragma once
#include "WorldLodPolicy.h"
#include <cstdint>
class CSimpleModelInfo;
namespace VrWorldLod {
using Stats=WorldLodPolicy::Stats;
void Invalidate();
void SetMode(int mode);
void SetFadeTransitions(bool enabled);
bool FadeTransitions();
float FadingDistance(float distance);
void BeginFrame(int mode);
int Mode();
Stats GetStats();
bool Suppress(CSimpleModelInfo *model);
float LargestDistance(CSimpleModelInfo *model);
float NearDistance(CSimpleModelInfo *model);
float ScanRadius(float normalRadius);
// Distance weights, shared by visibility and both rendering paths.
struct Layer { RpAtomic *atomic=nullptr; float weight=0; };
struct Transition { Layer layers[2]; int count=0; bool fading=false; };
Transition GetTransition(CSimpleModelInfo *model,float distance);
RpAtomic *SelectAtomic(CSimpleModelInfo *model,float distance);
}
