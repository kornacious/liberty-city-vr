#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace WorldOcclusion {
constexpr unsigned Size=48,MaxOccluders=24,MaxTriangles=1024,PerObjectTriangles=64;
constexpr unsigned MaxRasterTests=8192,MaxQueryTests=16384;
constexpr unsigned MaxRasterRows=8192,MaxSelectedTriangles=256;
constexpr double DepthGuard=0.00002,ScreenGuard=0.002;
struct Stats {
    uint32_t builds=0,occluderCandidates=0,occluders=0,triangles=0,cells=0;
    uint32_t candidates=0,rejected=0,visible=0,invalid=0,budget=0;
    uint32_t leftCells=0,rightCells=0,rasterTests=0,queryTests=0;
    uint64_t indices=0,vertices=0;
    double buildMs=0,queryMs=0;
    uint32_t rasterRows=0,selectedTriangles=0;
};
struct Point {double x=0,y=0,z=0;}; // screen x/y in cells, Vulkan depth [0,1]
struct Box {double left=0,right=0,bottom=0,top=0,nearDepth=0;bool valid=false;};
inline bool Finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.z>0&&p.z<1;}
// Projection fails open for near/far-plane crossings and uncertain coordinates.
inline bool Project(const double *matrix,const double *p,Point &out){
    double c[4]={};
    for(unsigned row=0;row<4;++row)c[row]=matrix[row]*p[0]+matrix[4+row]*p[1]+matrix[8+row]*p[2]+matrix[12+row];
    for(double v:c)if(!std::isfinite(v))return false;
    if(c[3]<=0.001||c[2]<=0||c[2]>=c[3])return false;
    out={(c[0]/c[3]+1)*Size*0.5,(c[1]/c[3]+1)*Size*0.5,c[2]/c[3]};
    return Finite(out);
}
inline Box Bounds(const Point *corners){
    Box b;
    b.left=b.bottom=1e100;b.right=b.top=-1e100;b.nearDepth=1;
    for(unsigned i=0;i<8;++i){
        if(!Finite(corners[i]))return b;
        b.left=std::min(b.left,corners[i].x);b.right=std::max(b.right,corners[i].x);
        b.bottom=std::min(b.bottom,corners[i].y);b.top=std::max(b.top,corners[i].y);
        b.nearDepth=std::min(b.nearDepth,corners[i].z);
    }
    const double pad=ScreenGuard*Size*0.5;
    b.left-=pad;b.right+=pad;b.bottom-=pad;b.top+=pad;
    b.valid=b.left<b.right&&b.bottom<b.top&&b.right>0&&b.top>0&&b.left<Size&&b.bottom<Size;
    return b;
}
// A scheduling estimate only, never a coverage/depth proof. Favor broad
// projected faces that intersect the viewport over thin or offscreen faces.
inline double Priority(const Point (&p)[3]){
    for(auto v:p)if(!Finite(v))return 0;
    const double width=std::max({p[0].x,p[1].x,p[2].x})-std::min({p[0].x,p[1].x,p[2].x});
    const double height=std::max({p[0].y,p[1].y,p[2].y})-std::min({p[0].y,p[1].y,p[2].y});
    const double clippedWidth=std::min(double(Size),std::max({p[0].x,p[1].x,p[2].x}))-
        std::max(0.0,std::min({p[0].x,p[1].x,p[2].x}));
    const double clippedHeight=std::min(double(Size),std::max({p[0].y,p[1].y,p[2].y}))-
        std::max(0.0,std::min({p[0].y,p[1].y,p[2].y}));
    if(width<=1||height<=1||clippedWidth<=1||clippedHeight<=1)return 0;
    const double area=std::abs((p[1].x-p[0].x)*(p[2].y-p[0].y)-(p[1].y-p[0].y)*(p[2].x-p[0].x))*.5;
    const double score=clippedWidth*clippedHeight*std::min(1.0,area/(width*height));
    return std::isfinite(score)?score:0;
}
struct Triangle {
    Point eye[2][3];
    bool valid[2]={false,false};
    unsigned owner=0;
    double priority=0;
};
class Selection {
    std::array<Triangle,MaxSelectedTriangles> triangles;
    unsigned count=0;
    static bool Greater(const Triangle &a,const Triangle &b){return a.priority>b.priority;}
public:
    void Clear(){count=0;}
    unsigned Count()const{return count;}
    void Add(const Triangle &t){
        if(!std::isfinite(t.priority)||t.priority<=0)return;
        if(count==triangles.size()){
            if(t.priority<=triangles[0].priority)return;
            std::pop_heap(triangles.begin(),triangles.begin()+count,Greater);--count;
        }
        triangles[count++]=t;
        std::push_heap(triangles.begin(),triangles.begin()+count,Greater);
    }
    template<class F> void Visit(F visit){
        std::sort_heap(triangles.begin(),triangles.begin()+count,Greater);
        for(unsigned i=0;i<count;++i)visit(triangles[i]);
        // Sorted storage no longer has heap order. The next build starts empty.
        count=0;
    }
};
class Grid {
    std::array<double,Size*Size> depth;
public:
    Grid(){Clear();}
    void Clear(){depth.fill(1);}
    unsigned Coverage()const {unsigned n=0;for(double d:depth)if(d<1)++n;return n;}
    // Inner-conservative rasterization: a cell is written only when the whole
    // cell lies inside one actual triangle, including a silhouette margin.
    // Its stored depth is a far bound; coverage from separate partial triangles
    // is never combined to fill a cell or bridge a window/fence hole.
    void Raster(Point a,Point b,Point c,Stats &s){
        if(!Finite(a)||!Finite(b)||!Finite(c))return;
        double area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(!std::isfinite(area)||std::abs(area)<1e-9)return;
        if(area<0){std::swap(b,c);area=-area;}
        const double minX=std::max(0.0,std::min({a.x,b.x,c.x}));
        const double maxX=std::min(double(Size),std::max({a.x,b.x,c.x}));
        const double minY=std::max(0.0,std::min({a.y,b.y,c.y}));
        const double maxY=std::min(double(Size),std::max({a.y,b.y,c.y}));
        if(minX>=maxX||minY>=maxY)return;
        const double dx=((b.z-a.z)*(c.y-a.y)-(c.z-a.z)*(b.y-a.y))/area;
        const double dy=((b.x-a.x)*(c.z-a.z)-(c.x-a.x)*(b.z-a.z))/area;
        Point p[3]={a,b,c};
        struct Edge {double ex,ey,constant,threshold;};
        Edge edges[3];
        for(unsigned e=0;e<3;++e){
            const auto &v=p[e];const auto &w=p[(e+1)%3];
            const double ex=w.x-v.x,ey=w.y-v.y;
            edges[e]={ex,ey,-ex*v.y+ey*v.x,
                ScreenGuard*Size*.5*std::sqrt(ex*ex+ey*ey)+1e-9};
        }
        for(unsigned y=unsigned(std::floor(minY));y<unsigned(std::ceil(maxY));++y){
            if(s.rasterRows>=MaxRasterRows){++s.budget;return;}
            ++s.rasterRows;
            double left=std::floor(minX),right=std::ceil(maxX)-1;
            for(const auto &e:edges){
                // Minimum edge function over a whole cell is affine in x.
                const double constant=e.ex*(e.ex>=0?double(y):double(y+1))+e.constant-
                    (e.ey>=0?e.ey:0);
                const double coefficient=-e.ey;
                if(coefficient>0)left=std::max(left,std::floor((e.threshold-constant)/coefficient)-1);
                else if(coefficient<0)right=std::min(right,std::ceil((e.threshold-constant)/coefficient)+1);
                else if(constant<=e.threshold){right=-1;break;}
            }
            if(left>right)continue;
            // Bounds above deliberately include extra cells. Recheck all exact
            // inequalities before writing; solving spans never proves coverage.
            for(unsigned x=unsigned(left);x<=unsigned(right);++x){
                if(s.rasterTests>=MaxRasterTests){++s.budget;return;}
                ++s.rasterTests;
                bool covered=true;
                for(unsigned e=0;e<3;++e){
                    const auto &v=p[e];const auto &w=p[(e+1)%3];
                    const double ex=w.x-v.x,ey=w.y-v.y;
                    const double edge=ex*((ex>=0?double(y):double(y+1))-v.y)-
                        ey*((ey>=0?double(x+1):double(x))-v.x);
                    if(edge<=edges[e].threshold){covered=false;break;}
                }
                if(!covered)continue;
                const double far=a.z+dx*((dx>=0?double(x+1):double(x))-a.x)+
                    dy*((dy>=0?double(y+1):double(y))-a.y)+DepthGuard;
                if(!std::isfinite(far)||far<=0||far>=1)continue;
                auto &d=depth[y*Size+x];
                if(far<d){d=far;++s.cells;}
            }
        }
    }
    bool Hidden(const Box &b,Stats &s)const {
        if(!b.valid||!std::isfinite(b.nearDepth)||b.nearDepth<=0||b.nearDepth>=1)return false;
        for(double v:{b.left,b.right,b.bottom,b.top})if(!std::isfinite(v))return false;
        if(b.left>=b.right||b.bottom>=b.top||b.right<=0||b.top<=0||b.left>=Size||b.bottom>=Size)return false;
        const unsigned x0=unsigned(std::floor(std::max(0.0,b.left))),y0=unsigned(std::floor(std::max(0.0,b.bottom)));
        const unsigned x1=unsigned(std::ceil(std::min(double(Size),b.right))),y1=unsigned(std::ceil(std::min(double(Size),b.top)));
        for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x){
            if(s.queryTests>=MaxQueryTests){++s.budget;return false;}
            ++s.queryTests;
            if(b.nearDepth<=depth[y*Size+x]+DepthGuard)return false;
        }
        return true;
    }
};
class Stereo {
public:
    Grid eye[2];
    void Clear(){eye[0].Clear();eye[1].Clear();}
    bool Hidden(const Box (&b)[2],Stats &s)const {return eye[0].Hidden(b[0],s)&&eye[1].Hidden(b[1],s);}
};
}
