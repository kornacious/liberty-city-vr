// Compile with the assembled src/core include path, with and without __ANDROID__.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using int32 = std::int32_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
struct CVector { float x, y, z; };
static float Sqrt(float value) { return std::sqrt(value); }
enum { NUMCULLZONES=1, NUMATTRIBZONES=1, NUMZONEINDICES=1,
    NUMBUILDINGS=1, NUMTREADABLES=1 };
#include "ZoneCull.h"
int32 CCullZones::CurrentFlags_Player;
int32 CCullZones::CurrentFlags_Camera;
static void Check(bool value) { if (!value) std::abort(); }
int main()
{
    // Cover stairs alone, overlapping zone attributes, and exit/re-entry.
    for (int repeat=0; repeat<3; ++repeat)
        for (int flags=0; flags<256; ++flags) {
            CCullZones::CurrentFlags_Player=flags;
            CCullZones::CurrentFlags_Camera=flags;
#ifdef __ANDROID__
            Check(!CCullZones::CamStairsForPlayer());
#else
            Check(CCullZones::CamStairsForPlayer()==bool(flags & ATTRZONE_STAIRS));
#endif
            Check(CCullZones::CamCloseInForPlayer()==bool(flags & ATTRZONE_CAMCLOSEIN));
            Check(CCullZones::Cam1stPersonForPlayer()==bool(flags & ATTRZONE_1STPERSON));
            Check(CCullZones::NoPolice()==bool(flags & ATTRZONE_NOPOLICE));
            Check(CCullZones::DoINeedToLoadCollision()==bool(flags & ATTRZONE_DOINEEDCOLLISION));
            Check(CCullZones::PlayerNoRain()==bool(flags & ATTRZONE_NORAIN));
            Check(CCullZones::CamNoRain()==bool(flags & ATTRZONE_NORAIN));
            Check(CCullZones::CurrentFlags_Player==flags);
        }
    std::puts("Stair camera: 768 zone combinations passed.");
}
