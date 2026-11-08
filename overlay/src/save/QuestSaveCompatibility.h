#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace questsave {
enum class Profile { Unknown, Base, Upstate };
inline uint32_t Word(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}

// Inspect COMPATIBLE_SAVES without deserializing either engine's game tables.
// Identify a foreign configuration only after validating its block stream and
// checksum, so damaged or unrelated files keep the ordinary corruption message.
template<typename Read>
Profile Inspect(Read read) {
    uint32_t sum=0, payload=0, target=0, maximum=0;
    Profile profile=Profile::Unknown;
    unsigned blocks=0;
    while(blocks<24) {
        uint8_t word[4];
        if(read(word,sizeof(word))!=sizeof(word)) return Profile::Unknown;
        const uint32_t size=Word(word);
        if(!size || size>220000) return Profile::Unknown;
        const uint32_t aligned=(size+3)&~3U;
        if(target && (aligned>maximum || aligned>target-payload)) return Profile::Unknown;
        for(uint8_t value:word) sum+=value;
        uint8_t head[192]={};
        uint32_t offset=0;
        while(offset<aligned) {
            uint8_t chunk[4096];
            const size_t count=aligned-offset<sizeof(chunk)?aligned-offset:sizeof(chunk);
            if(read(chunk,count)!=count) return Profile::Unknown;
            for(size_t i=0;i<count;++i) {
                sum+=chunk[i];
                if(offset+i<sizeof(head)) head[offset+i]=chunk[i];
            }
            offset+=uint32_t(count);
        }
        if(blocks==0) {
            if(size<192) return Profile::Unknown;
            const uint32_t identity=Word(head+64)&0xFFFFFF;
            if(identity==201729) { profile=Profile::Base;target=201728;maximum=55000; }
            else if(identity==806916) { profile=Profile::Upstate;target=806912;maximum=220000; }
            else return Profile::Unknown;
            const uint32_t scripts=Word(head+188);
            if(aligned>maximum || !scripts || scripts>maximum || 192+((scripts+3)&~3U)!=size)
                return Profile::Unknown;
        } else if(blocks<20) {
            const uint32_t inner=Word(head);
            if(!inner || inner>maximum || 4+((inner+3)&~3U)!=size) return Profile::Unknown;
            if(blocks==2 && inner!=(profile==Profile::Base?5484U:7480U)) return Profile::Unknown;
        }
        payload+=aligned;
        ++blocks;
        if(payload==target) {
            if(blocks<20 || read(word,sizeof(word))!=sizeof(word) || Word(word)!=sum)
                return Profile::Unknown;
            uint8_t extra;
            return read(&extra,1)==0?profile:Profile::Unknown;
        }
    }
    return Profile::Unknown;
}

inline bool DescribeMismatch(Profile saved, bool upstateEnabled, unsigned slot, char *out, size_t capacity) {
    if(!out || !capacity) return false;
    out[0]=0;
    if(saved==Profile::Unknown || (saved==Profile::Upstate)==upstateEnabled) return false;
    std::snprintf(out,capacity,"Slot %u was saved with %s",slot,
        saved==Profile::Upstate?"Upstate Liberty Mod":"base game.");
    return true;
}
}
