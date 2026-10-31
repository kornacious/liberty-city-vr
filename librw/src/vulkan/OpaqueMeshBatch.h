#pragma once
#include <cstdint>
#include <limits>

namespace OpaqueMeshBatch {
// Concatenate triangle-list ranges only. Strip/fan boundaries would create
// extra primitives. The bound limits work without changing visibility units.
constexpr uint32_t MaxMeshes=32;
struct Stats {
    uint32_t eligible=0,checks=0,merged=0,savedDraws=0;
    uint32_t rejectedState=0,rejectedMaterial=0,rejectedRange=0;
};
inline bool CanAppend(uint64_t firstByte,uint32_t count,uint64_t nextByte,
                      uint32_t nextCount,uint32_t meshes){
    return meshes<MaxMeshes && count && nextCount && !(firstByte%2) &&
        !(nextByte%2) && !(count%3) && !(nextCount%3) &&
        firstByte/2<=std::numeric_limits<uint32_t>::max() &&
        nextByte/2<=std::numeric_limits<uint32_t>::max() &&
        firstByte<=std::numeric_limits<uint64_t>::max()-uint64_t(count)*2 &&
        firstByte+uint64_t(count)*2==nextByte &&
        nextCount<=std::numeric_limits<uint32_t>::max()-count;
}
}
