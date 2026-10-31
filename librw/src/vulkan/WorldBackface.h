#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <new>
#include <vector>

namespace WorldBackface {
struct Stats {
    uint32_t candidates=0,draws=0,rejectState=0,rejectGeometry=0,rejectView=0,rejectTransform=0,certifications=0;
    uint64_t indices=0;
    double certifyMs=0;
};
constexpr unsigned MaxVertices=8192,MaxIndices=24576;
using Point=std::array<double,3>;
inline Point Sub(Point a,Point b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline Point Cross(Point a,Point b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline double Dot(Point a,Point b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool PositiveTransform(const float *m){
    for(unsigned i=0;i<16;++i)if(!std::isfinite(m[i]))return false;
    Point a={m[0],m[1],m[2]},b={m[4],m[5],m[6]},c={m[8],m[9],m[10]};
    const double scale=std::sqrt(Dot(a,a)*Dot(b,b)*Dot(c,c));
    return scale>1e-12 && Dot(a,Cross(b,c))>scale*1e-5;
}
inline bool ViewerOutside(const float *eye,const float *centre,float radius){
    if(!std::isfinite(radius)||radius<=0)return false;
    double squared=0;
    for(unsigned i=0;i<3;++i){
        if(!std::isfinite(eye[i])||!std::isfinite(centre[i]))return false;
        const double d=double(eye[i])-centre[i];squared+=d*d;
    }
    // A conservative head-centred allowance covers both eye positions.
    return squared>double(radius+0.25f)*(radius+0.25f);
}
// Exact-position welding joins UV/material seams without closing actual gaps.
// Every component must be a closed, consistently oriented, outward shell.
// Open, inverted, degenerate, non-manifold and excessively large meshes fail
// closed. This is a certificate of topology/winding, not a simplification.
template<class Position>
bool ClosedOutward(unsigned vertices,unsigned count,const uint16_t *indices,Position position){
    if(!indices||vertices<4||vertices>MaxVertices||count<12||count>MaxIndices||count%3)return false;
    std::map<Point,unsigned> welded;
    std::vector<unsigned> remap(vertices);
    std::vector<Point> points;
    Point lo{},hi{};
    for(unsigned i=0;i<vertices;++i){
        const Point p=position(i);
        for(double v:p)if(!std::isfinite(v))return false;
        if(!i)lo=hi=p;
        for(unsigned a=0;a<3;++a){lo[a]=std::min(lo[a],p[a]);hi[a]=std::max(hi[a],p[a]);}
        auto inserted=welded.emplace(p,unsigned(points.size()));
        if(inserted.second)points.push_back(p);
        remap[i]=inserted.first->second;
    }
    const double extent=std::max({hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]});
    if(extent<=1e-6)return false;
    const unsigned faces=count/3;
    std::vector<unsigned> parent(faces);
    for(unsigned i=0;i<faces;++i)parent[i]=i;
    auto root=[&](unsigned a){while(parent[a]!=a){parent[a]=parent[parent[a]];a=parent[a];}return a;};
    struct Edge {unsigned face=0,count=0;int direction=0;};
    std::map<std::pair<unsigned,unsigned>,Edge> edges;
    std::vector<double> volume(faces);
    for(unsigned f=0;f<faces;++f){
        unsigned v[3];
        for(unsigned k=0;k<3;++k){if(indices[f*3+k]>=vertices)return false;v[k]=remap[indices[f*3+k]];}
        if(v[0]==v[1]||v[1]==v[2]||v[0]==v[2])return false;
        const Point a=points[v[0]],b=points[v[1]],c=points[v[2]];
        const Point normal=Cross(Sub(b,a),Sub(c,a));
        if(Dot(normal,normal)<=extent*extent*extent*extent*1e-20)return false;
        volume[f]=Dot(Sub(a,lo),Cross(Sub(b,lo),Sub(c,lo)));
        for(unsigned k=0;k<3;++k){
            const unsigned x=v[k],y=v[(k+1)%3];
            Edge &e=edges[{std::min(x,y),std::max(x,y)}];
            if(!e.count)e.face=f;
            else parent[root(f)]=root(e.face);
            if(++e.count>2)return false;
            e.direction+=x<y?1:-1;
        }
    }
    for(const auto &entry:edges)if(entry.second.count!=2||entry.second.direction)return false;
    std::vector<double> sums(faces);
    for(unsigned f=0;f<faces;++f)sums[root(f)]+=volume[f];
    const double tolerance=extent*extent*extent*1e-9;
    for(unsigned f=0;f<faces;++f)if(root(f)==f&&sums[f]<=tolerance)return false;
    return true;
}
}
