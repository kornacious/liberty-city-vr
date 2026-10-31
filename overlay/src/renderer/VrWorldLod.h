#pragma once
#include "WorldLodPolicy.h"
#include <cstdint>
class CSimpleModelInfo;
namespace VrWorldLod {
using Stats=WorldLodPolicy::Stats;
void Invalidate();
void SetMode(int mode);
void BeginFrame(int mode);
int Mode();
Stats GetStats();
bool Suppress(CSimpleModelInfo *model);
float LargestDistance(CSimpleModelInfo *model);
float NearDistance(CSimpleModelInfo *model);
float ScanRadius(float normalRadius);
RpAtomic *SelectAtomic(CSimpleModelInfo *model,float distance);
}
