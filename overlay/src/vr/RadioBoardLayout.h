#pragma once
#include <cmath>
namespace RadioBoard {
// Millimetres, degrees and percent. Entire placements override one another;
// fields from different sources are never silently mixed.
struct Placement { int v[7]={350,600,300,0,0,0,100}; bool owned=false; };
inline int ClampValue(int i,int v) { int lo=i<3?-2000:i<6?-180:40; int hi=i<3?2000:i<6?179:250; return v<lo?lo:v>hi?hi:v; }
inline Placement Resolve(const Placement &personalModel,const Placement &personalGlobal,
                         const Placement &bundledModel,const Placement &bundledGlobal) {
    return personalModel.owned?personalModel:personalGlobal.owned?personalGlobal:bundledModel.owned?bundledModel:bundledGlobal;
}
struct Button { float left,top,right,bottom; const char *text; };
static constexpr Button Buttons[]={
    {12,78,126,122,"STATION -"},{134,78,248,122,"STATION +"},
    {264,78,378,122,"VOL -"},{386,78,500,122,"VOL +"},
    {12,132,168,186,"PREVIOUS"},{178,132,334,186,"PAUSE"},{344,132,500,186,"NEXT"},
    {370,202,500,244,"RESCAN"}
};
inline int Hit(float x,float y) { for(int i=0;i<8;++i) if(x>=Buttons[i].left && x<=Buttons[i].right && y>=Buttons[i].top && y<=Buttons[i].bottom) return i; return -1; }
// Metres in the panel basis. Approach from the front, press once, then
// withdraw 3 cm before another press (including sliding to another button).
struct Touch {
    bool armed=false;
    void Reset() { armed=false; }
    int Update(float ox,float oy,float oz,float width,int &hover) {
        hover=-1;
        if(!std::isfinite(ox) || !std::isfinite(oy) || !std::isfinite(oz) || !std::isfinite(width) || width<=0) { Reset(); return -1; }
        if(oz>=.03f) armed=true;
        if(oz<-.025f) { Reset(); return -1; }
        if(oz>.07f) return -1;
        hover=Hit(256+ox*512/width,128-oy*512/width);
        if(oz>.008f || !armed) return -1;
        // Contact anywhere on the glass consumes this poke; sliding cannot click.
        armed=false;
        return hover;
    }
};
// Ray already expressed in the panel basis, normal towards its viewer.
inline bool Intersect(float ox,float oy,float oz,float dx,float dy,float dz,float width,float &x,float &y) {
    if(!std::isfinite(oz) || !std::isfinite(dz) || oz<=0 || dz>=-0.0001f || width<=0) return false;
    float t=-oz/dz; if(t<0 || t>2.5f) return false;
    x=256+(ox+t*dx)*512/width; y=128-(oy+t*dy)*512/width;
    return std::isfinite(x) && std::isfinite(y) && x>=0 && x<=512 && y>=0 && y<=256;
}
}
