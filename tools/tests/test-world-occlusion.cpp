#include "../../librw/src/vulkan/WorldOcclusion.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace WorldOcclusion;
static Box box(double l,double r,double b,double t,double z){return {l,r,b,t,z,true};}
static void fill(Grid &g,Stats &s,double z){g.Raster({-100,-100,z},{200,-100,z},{-100,200,z},s);}
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
    std::puts("PASS: stereo disocclusion, real openings, inner coverage oracle, conservative slope depth, projection failures and work budgets.");
}
