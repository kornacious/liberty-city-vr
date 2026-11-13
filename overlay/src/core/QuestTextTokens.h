#pragma once
#include <cstddef>
#include <cstring>

namespace questtext {
// Xbox GXT action names differ from the PC controller configuration names.
inline const char *ControllerAction(const char *name)
{
    if(!std::strcmp(name,"CYCLE_CAMERA_MODE")) return "CAMERA_CHANGE_VIEW_ALL_SITUATIONS";
    if(!std::strcmp(name,"CAR_GUN")) return "VEHICLE_FIREWEAPON";
    return name;
}

template<class Char>
inline bool Delimiter(Char c, bool japanese = false)
{
    return c == '~' || (japanese && c == (0x8000 | '~'));
}

// Returns either the closing delimiter or the string terminator. Never scan
// beyond NUL when a modded/truncated message contains an unfinished token.
template<class Char>
inline Char *TokenEnd(Char *s, bool japanese = false)
{
    while(*s && !Delimiter(*s,japanese)) ++s;
    return s;
}

// The engine's help/message buffers contain 256 UTF-16 code units. Expand into
// a separate bounded buffer, consuming unknown actions as a whole. Otherwise
// stripping just ~k~~ leaves a stray trailing ~ for the font scanner.
template<std::size_t Capacity, class Char, class Lookup>
void ExpandControls(Char *text, Lookup lookup, bool japanese = false)
{
    static_assert(Capacity > 1,"A message needs a terminator");
    if(!text) return;
    std::size_t length=0;
    while(length<Capacity-1 && text[length]) ++length;
    text[length]=0;
    Char output[Capacity]{};
    std::size_t used=0;
    auto append=[&](const Char *value,std::size_t count) {
        if(count>Capacity-1-used) return false;
        for(std::size_t n=0;n<count;++n) output[used++]=value[n];
        return true;
    };
    for(std::size_t i=0;i<length;) {
        const auto code=[&](std::size_t n) { return unsigned(text[n]) & (japanese?0x7fff:0xffff); };
        if(i+3<length && code(i)=='~' && code(i+1)=='k' && code(i+2)=='~' && code(i+3)=='~') {
            std::size_t end=i+4;
            while(end<length && !Delimiter(text[end],japanese)) ++end;
            char action[Capacity]{};
            for(std::size_t n=i+4;n<end;++n) action[n-i-4]=code(n)<128?char(code(n)):'?';
            Char keys[Capacity]{};
            if(end<length && lookup(ControllerAction(action),keys,Capacity)) {
                std::size_t count=0;while(count<Capacity-1 && keys[count]) ++count;
                // A binding label must not introduce an unfinished font token.
                bool complete=true;
                for(std::size_t n=0;n<count;++n) if(Delimiter(keys[n],japanese)) {
                    do {++n;} while(n<count && !Delimiter(keys[n],japanese));
                    if(n==count) complete=false;
                }
                if(complete && !append(keys,count)) break;
                if(!complete) { for(std::size_t n=0;n<count;++n) if(!Delimiter(keys[n],japanese) && !append(keys+n,1)) break; }
            } else {
                // Keep an unknown action readable, without exposing token syntax.
                for(std::size_t n=0;action[n];++n) {
                    Char c=action[n]=='_'?' ':action[n];
                    if(!append(&c,1)) break;
                }
            }
            i=end<length?end+1:end;
        } else if(Delimiter(text[i],japanese)) {
            std::size_t end=i+1;
            while(end<length && !Delimiter(text[end],japanese)) ++end;
            if(end==length) { ++i; continue; }
            if(!append(text+i,end-i+1)) break;
            i=end+1;
        } else {
            if(!append(text+i,1)) break;
            ++i;
        }
    }
    output[used]=0;
    for(std::size_t n=0;n<Capacity;++n) text[n]=output[n];
}
}
