#pragma once
#include <cstddef>
#include <cstdint>

namespace QuestUpstate {
inline bool Active() {
#ifdef LCVR_UPSTATE_ENGINE
    return true;
#else
    return false;
#endif
}
bool Initialize(const char *gameRoot);
const char *Resolve(const char *path, char *out, size_t capacity);
bool IsAssetPath(const char *path);
bool HasTrait(int id, const char *trait);
bool HasNameTrait(const char *name, const char *trait);
uint16_t *Text(const char *key);
bool RuralRubbish();
}
