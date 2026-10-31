#include "../../librw/src/vulkan/WorldBackface.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace WorldBackface;
int main(){
    std::vector<Point> p={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    std::vector<uint16_t> ix={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    auto check=[&](){return ClosedOutward(unsigned(p.size()),unsigned(ix.size()),ix.data(),[&](unsigned i){return p[i];});};
    assert(check());
    auto original=ix;
    ix.resize(ix.size()-3);assert(!check()); // open building
    ix=original;std::swap(ix[1],ix[2]);assert(!check()); // inconsistent winding
    ix=original;for(unsigned i=0;i<ix.size();i+=3)std::swap(ix[i+1],ix[i+2]);
    assert(!check()); // inward shell/interior geometry
    ix=original;ix.insert(ix.end(),original.begin(),original.begin()+3);assert(!check()); // non-manifold
    ix=original;ix[2]=ix[1];assert(!check());
    ix=original;ix[0]=99;assert(!check()); // malformed index
    ix=original;p[0][0]=std::numeric_limits<double>::quiet_NaN();assert(!check());p[0][0]=0;
    // Separate UV/material seams must not make a closed opaque object fail.
    std::vector<Point> seam;for(auto i:ix)seam.push_back(p[i]);
    const auto cube=p;p=seam;for(unsigned i=0;i<ix.size();++i)ix[i]=uint16_t(i);
    assert(check());
    // Two shells must each be outward; a large outward shell cannot hide
    // an inverted component just because the combined volume is positive.
    p=cube;ix=original;
    for(auto point:cube){point[0]+=3;p.push_back(point);}
    for(auto i:original)ix.push_back(uint16_t(i+8));assert(check());
    for(unsigned i=36;i<ix.size();i+=3)std::swap(ix[i+1],ix[i+2]);assert(!check());
    p=cube;ix=original;
    for(auto &point:p)for(double &v:point)v+=1000000;assert(check()); // stable translated volume
    assert(!ClosedOutward(4,MaxIndices+3,ix.data(),[&](unsigned i){return p[i];}));
    float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    assert(PositiveTransform(m));m[0]=-1;assert(!PositiveTransform(m));
    m[0]=0;assert(!PositiveTransform(m));m[0]=1;m[4]=1000000;assert(!PositiveTransform(m));
    m[4]=0;m[7]=std::numeric_limits<float>::quiet_NaN();assert(!PositiveTransform(m));
    const float centre[3]={0,0,0},inside[3]={0,0,0},edge[3]={1.25f,0,0},outside[3]={2,0,0};
    assert(!ViewerOutside(inside,centre,1));assert(!ViewerOutside(edge,centre,1));
    assert(ViewerOutside(outside,centre,1));assert(!ViewerOutside(outside,centre,-1));
    std::puts("PASS: closed outward shells, exact seam welding, open/two-sided and malformed rejection, component winding, transforms and stereo viewer margin.");
}
