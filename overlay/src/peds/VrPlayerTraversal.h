#pragma once
class CPlayerPed;
namespace VrPlayerTraversal {
#ifdef __ANDROID__
bool Flight();
bool Noclip();
bool TerrainQuery();
bool TerrainModel();
class TerrainModelScope {
    bool previous;
public:
    explicit TerrainModelScope(const char *name);
    ~TerrainModelScope();
};
bool ToggleFlight();
bool ToggleNoclip();
bool Process(CPlayerPed *player);
void Reset(CPlayerPed *player);
#else
inline bool Flight() { return false; }
inline bool Noclip() { return false; }
inline bool TerrainQuery() { return false; }
inline bool TerrainModel() { return false; }
class TerrainModelScope { public: explicit TerrainModelScope(const char *) {} };
inline bool ToggleFlight() { return false; }
inline bool ToggleNoclip() { return false; }
inline bool Process(CPlayerPed *) { return false; }
inline void Reset(CPlayerPed *) {}
#endif
}
