#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace RadioTitle {
// The dashboard's 5x7 font has ASCII glyphs. Decode whole UTF-8 characters,
// transliterating common Latin accents rather than showing one '?' per byte.
inline std::string Glyphs(const std::string &text) {
    std::string out;
    for(size_t i=0;i<text.size();) {
        unsigned c=(unsigned char)text[i++];
        if(c>=128) {
            unsigned extra=c<224?1:c<240?2:3; unsigned value=c&((1u<<(6-extra))-1);
            while(extra-- && i<text.size()) value=(value<<6)|((unsigned char)text[i++]&63);
            c=value;
            if(c>=192 && c<=197) c='A'; else if(c>=224 && c<=229) c='a';
            else if(c==199 || c==231) c=c==199?'C':'c';
            else if(c>=200 && c<=203) c='E'; else if(c>=232 && c<=235) c='e';
            else if(c>=204 && c<=207) c='I'; else if(c>=236 && c<=239) c='i';
            else if(c==209 || c==241) c=c==209?'N':'n';
            else if(c>=210 && c<=214) c='O'; else if(c>=242 && c<=246) c='o';
            else if(c>=217 && c<=220) c='U'; else if(c>=249 && c<=252) c='u';
            else c='?';
        }
        out.push_back(c>=32 && c<=126?char(c):' ');
    }
    return out;
}
inline float LeftToRight(double seconds, float textWidth, float left, float right) {
    if(textWidth<=right-left) return (left+right-textWidth)*.5f;
    const double travel=(textWidth+right-left)/40.0;
    const double phase=std::fmod(std::max(0.0,seconds),travel+3.0);
    return float(left-textWidth+40.0*std::min(phase,travel));
}
}
