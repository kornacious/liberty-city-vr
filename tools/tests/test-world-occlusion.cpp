#include "../../librw/src/vulkan/WorldOcclusion.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
using namespace WorldOcclusion;
static Box box(double l,double r,double b,double t,double z){return {l,r,b,t,z,true};}
static void fill(Grid &g,Stats &s,double z){g.Raster({-100,-100,z},{200,-100,z},{-100,200,z},s);}
// Exhaustive original bounding-box algorithm as an independent differential
// oracle: scheduling may omit triangles, but spans must not change their proof.
static std::array<double,Size*Size> reference(Point a,Point b,Point c){
    std::array<double,Size*Size> result;result.fill(1);
    double area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    if(std::abs(area)<1e-9)return result;
    if(area<0){std::swap(b,c);area=-area;}
    const double dx=((b.z-a.z)*(c.y-a.y)-(c.z-a.z)*(b.y-a.y))/area;
    const double dy=((b.x-a.x)*(c.z-a.z)-(c.x-a.x)*(b.z-a.z))/area;
    Point p[3]={a,b,c};
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
        bool covered=true;
        for(unsigned e=0;e<3;++e){
            const Point &v=p[e],&w=p[(e+1)%3];
            const double ex=w.x-v.x,ey=w.y-v.y;
            const double edge=ex*((ex>=0?double(y):double(y+1))-v.y)-ey*((ey>=0?double(x+1):double(x))-v.x);
            if(edge<=ScreenGuard*Size*.5*std::sqrt(ex*ex+ey*ey)+1e-9)covered=false;
        }
        if(!covered)continue;
        const double far=a.z+dx*((dx>=0?double(x+1):double(x))-a.x)+dy*((dy>=0?double(y+1):double(y))-a.y)+DepthGuard;
        if(std::isfinite(far)&&far>0&&far<1)result[y*Size+x]=far;
    }
    return result;
}
int main(){
    Stereo grid;Stats s;
    Box b[2]={box(8,12,8,12,.8),box(8,12,8,12,.8)};
    assert(!grid.Hidden(b,s)); // no stale or uninitialized coverage
    s={};fill(grid.eye[0],s,.2);fill(grid.eye[1],s,.2);
    assert(grid.eye[0].Coverage()==Size*Size&&grid.eye[1].Coverage()==Size*Size);
    assert(grid.Hidden(b,s));
    b[0].nearDepth=.1;assert(!grid.Hidden(b,s)); // in front in either eye
    b[0].nearDepth=.8;grid.eye[1].Clear();assert(!grid.Hidden(b,s)); // stereo disocclusion
    s={};fill(grid.eye[1],s,.2);b[1].nearDepth=.20003;assert(!grid.Hidden(b,s)); // depth uncertainty margin
    b[1].nearDepth=.8;b[0].valid=false;assert(!grid.Hidden(b,s));b[0].valid=true;
    // No combining partial triangle coverage into a false solid cell.
    Grid partial;s={};partial.Raster({10.1,10.1,.2},{10.9,10.1,.2},{10.1,10.9,.2},s);
    assert(partial.Coverage()==0);assert(!partial.Hidden(box(10.2,10.8,10.2,10.8,.8),s));
    // A genuine opening between opaque wall pieces stays visible.
    Grid window;s={};
    auto rect=[&](double l,double r,double bot,double top){
        window.Raster({l,bot,.2},{r,bot,.2},{r,top,.2},s);
        window.Raster({l,bot,.2},{r,top,.2},{l,top,.2},s);
    };
    rect(0,48,0,18);rect(0,48,30,48);rect(0,18,18,30);rect(30,48,18,30);
    assert(!window.Hidden(box(22,26,22,26,.8),s));
    // Per-cell far depth must remain conservative on a sloping surface.
    Grid slope;s={};slope.Raster({-1,-1,.1},{49,-1,.6},{-1,49,.1},s);
    assert(!slope.Hidden(box(10,11,10,11,.215),s));
    assert(slope.Hidden(box(10,11,10,11,.4),s));
    // Raster/query work caps fail open, without clearing valid earlier cells.
    Grid budget;s={};s.rasterTests=MaxRasterTests;fill(budget,s,.2);assert(budget.Coverage()==0);
    s={};s.rasterRows=MaxRasterRows;fill(budget,s,.2);assert(budget.Coverage()==0);
    s={};s.queryTests=MaxQueryTests;assert(!grid.Hidden(b,s));
    // Opposite triangle winding has identical coverage for the two-sided path.
    Grid reversed;s={};reversed.Raster({-100,-100,.2},{-100,200,.2},{200,-100,.2},s);
    assert(reversed.Coverage()==Size*Size);
    // Near/far clipping, behind-eye and malformed projection are never hidden.
    double matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    double p[3]={0,0,.5};Point q;
    assert(Project(matrix,p,q)&&q.x==24&&q.y==24&&q.z==.5);
    p[2]=0;assert(!Project(matrix,p,q));p[2]=1;assert(!Project(matrix,p,q));
    p[2]=.5;matrix[15]=-1;assert(!Project(matrix,p,q));matrix[15]=1;
    matrix[0]=std::numeric_limits<double>::quiet_NaN();assert(!Project(matrix,p,q));
    Point corners[8];for(unsigned i=0;i<8;++i)corners[i]={double(10+(i&1)),double(10+((i>>1)&1)),.7};
    auto bounds=Bounds(corners);assert(bounds.valid&&bounds.left<10&&bounds.right>11&&bounds.nearDepth==.7);
    corners[7].z=-.1;assert(!Bounds(corners).valid);
    // Dense oracle: no tile touching/outside a triangle may be classified
    // hidden. Compare every cell's four corners against independent barycentrics.
    Grid triangle;s={};Point a={3.25,4.75,.3},c={38.5,9.5,.3},d={12,42,.3};
    triangle.Raster(a,c,d,s);
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
        Stats test;const bool hidden=triangle.Hidden(box(x+.1,x+.9,y+.1,y+.9,.8),test);
        if(!hidden)continue;
        for(unsigned corner=0;corner<4;++corner){
            const double px=x+(corner&1),py=y+((corner>>1)&1);
            const double area=(c.x-a.x)*(d.y-a.y)-(c.y-a.y)*(d.x-a.x);
            const double u=((px-a.x)*(d.y-a.y)-(py-a.y)*(d.x-a.x))/area;
            const double v=((c.x-a.x)*(py-a.y)-(c.y-a.y)*(px-a.x))/area;
            assert(u>0&&v>0&&u+v<1);
        }
    }
    // Thin screen diagonals used to consume a full viewport bounding rectangle.
    Grid thin;s={};thin.Raster({0,0,.2},{48,48,.2},{48,47.6,.2},s);
    assert(thin.Coverage()==0&&s.rasterTests<300&&s.rasterRows<=Size);
    // Random offscreen, reversed, skinny and sloping triangles: every cell's
    // coverage/depth decision agrees with exhaustive original rasterization.
    std::mt19937 random(0x8172);std::uniform_real_distribution<double> coord(-40,88),depth(.05,.95);
    for(unsigned trial=0;trial<400;++trial){
        Point vertices[3];for(auto &v:vertices)v={coord(random),coord(random),depth(random)};
        Grid spans;Stats work;spans.Raster(vertices[0],vertices[1],vertices[2],work);
        const auto exact=reference(vertices[0],vertices[1],vertices[2]);
        assert(work.rasterTests<MaxRasterTests&&work.rasterRows<=Size);
        for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x)
            for(double z:{.15,.45,.75,.99}){
                Stats query;
                assert(spans.Hidden(box(x+.1,x+.9,y+.1,y+.9,z),query)==(z>exact[y*Size+x]+DepthGuard));
            }
    }
    // Bounded selection retains the highest scores and visits best first.
    Selection selected;
    for(unsigned i=0;i<2*MaxSelectedTriangles;++i){Triangle t;t.priority=i+1;t.owner=i;selected.Add(t);}
    assert(selected.Count()==MaxSelectedTriangles);
    unsigned visited=0;double last=1e100;
    selected.Visit([&](const Triangle &t){assert(t.priority<=last&&t.priority>MaxSelectedTriangles);last=t.priority;++visited;});
    assert(visited==MaxSelectedTriangles&&selected.Count()==0);
    Triangle invalid;invalid.priority=std::numeric_limits<double>::infinity();selected.Add(invalid);assert(selected.Count()==0);
    Point outside[3]={{60,60,.2},{80,60,.2},{60,80,.2}};assert(Priority(outside)==0);
    Point broad[3]={{0,0,.2},{48,0,.2},{0,48,.2}};assert(Priority(broad)>0);
    std::puts("PASS: stereo/opening/depth guards, 3.6 million differential cell-depth checks, bounded priority selection, thin spans and work budgets.");
}
