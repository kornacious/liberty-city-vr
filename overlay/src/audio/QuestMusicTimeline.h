#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace QuestMusic {
constexpr uint64_t MaxFileBytes = 512ull*1024*1024;
constexpr uint32_t MaxDuration = 24u*24*60*60*1000; // below signed-ms boundary
constexpr size_t MaxTracks = 1000;
struct Timeline {
    std::vector<uint32_t> ends;
    bool Add(uint32_t ms) {
        uint64_t total = uint64_t(Length())+ms;
        if(!ms || ends.size() >= MaxTracks || total > MaxDuration) return false;
        ends.push_back(uint32_t(total)); return true;
    }
    uint32_t Length() const { return ends.empty() ? 0 : ends.back(); }
    size_t Locate(uint32_t &ms) const {
        if(ends.empty()) { ms=0; return 0; }
        ms %= Length();
        size_t i=std::upper_bound(ends.begin(), ends.end(), ms)-ends.begin();
        ms -= Begin(i); return i;
    }
    uint32_t Begin(size_t i) const { return i ? ends[i-1] : 0; }
    size_t Step(size_t i, int direction, uint32_t local) const {
        if(ends.empty()) return 0;
        if(direction<0 && local>3000) return i;
        return direction<0 ? (i+ends.size()-1)%ends.size() : (i+1)%ends.size();
    }
};
}
