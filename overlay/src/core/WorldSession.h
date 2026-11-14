#pragma once
#include <map>
#include <string>
class CWanted;

// A world may be observed without a player. The normal game remains the default.
// This boundary is shared by streaming, population and the desktop viewer.
namespace WorldSession {
inline bool viewer = false;
inline bool ambient = false;
inline std::map<std::string,int> settings;
// Neutral law-enforcement state for ambient AI when there is no player entity.
CWanted *Wanted();
inline float observation[3] = {-180.f, 180.f, 35.f};
inline bool IsViewer() { return viewer; }
inline void Observe(float x, float y, float z) {
    observation[0]=x; observation[1]=y; observation[2]=z;
}
}
