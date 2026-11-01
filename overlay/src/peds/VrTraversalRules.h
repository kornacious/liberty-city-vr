#pragma once
#include <cmath>
#include <cstring>
#include <initializer_list>
namespace VrTraversal {
struct Motion { float right, forward, up; };
inline Motion Step(float right, float forward, float up, bool boost, float seconds, bool flight)
{
    if(!std::isfinite(right) || !std::isfinite(forward) || !std::isfinite(up) || !std::isfinite(seconds)) return {0,0,0};
    if(!flight) up=0;
    const float length=std::sqrt(right*right+forward*forward+up*up);
    const float scale=(flight ? (boost ? 300.0f : 6.0f) : (boost ? 80.0f : 3.0f)) *
        (seconds < 0 ? 0 : seconds > 0.1f ? 0.1f : seconds) / (length > 1 ? length : 1);
    return {right*scale, forward*scale, up*scale};
}
inline bool OutOfBounds(float x,float y,float z)
{
    return !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        x <= -1950 || x >= 1950 || y <= -1950 || y >= 1950 || z < -80 || z > 1000;
}
inline bool TerrainModelName(const char *name)
{
    if(!name) return false;
    char lower[64];
    unsigned int i=0;
    for(;i<sizeof(lower)-1 && name[i];++i)
        lower[i]=name[i]>='A' && name[i]<='Z' ? char(name[i]+('a'-'A')) : name[i];
    lower[i]=0;
    for(const char *excluded : {"planter","sign","light","barrier","roof","wall","bill"})
        if(std::strstr(lower,excluded)) return false;
    for(const char *token : {"land","lnd","road","rd_","ground","grass","beach","cliff","pave","path","park"})
        if(std::strstr(lower,token)) return true;
    return false;
}
inline bool GroundSurface(int surface,bool terrainModel=false)
{
    // GTA III collision materials: tarmac, grass, gravel, dry mud, pavement,
    // sand and cliff. Default material is accepted only on named ground models.
    return (surface == 0 && terrainModel) || (surface >= 1 && surface <= 5) || surface == 18 || surface == 26;
}
inline bool BypassAll(bool flight,bool noclip) { return flight && noclip; }
inline bool NeedsGround(bool flight,bool noclip) { return !flight && noclip; }
}
