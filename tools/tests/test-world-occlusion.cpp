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
    BuildTimeline timeline;timeline.Start(true,100);
    timeline.Switch(BuildGather,200);timeline.Switch(BuildBounds,250);
    timeline.Switch(BuildGather,350);timeline.Switch(BuildSelect,400);timeline.Finish(450);
    assert(timeline.ns[BuildSetup]==100&&timeline.ns[BuildGather]==100&&timeline.ns[BuildBounds]==100&&timeline.ns[BuildSelect]==50);
    uint64_t phaseTotal=0;for(auto value:timeline.ns)phaseTotal+=value;
    assert(phaseTotal==350&&!timeline.active);
    timeline.Start(false,100);timeline.Switch(BuildRaster,200);timeline.Finish(500);
    for(auto value:timeline.ns)assert(value==0);
    timeline.Start(true,100);timeline.Switch(BuildCache,90);timeline.Finish(200);
    phaseTotal=0;for(auto value:timeline.ns)phaseTotal+=value;assert(phaseTotal==100);
    Stats attribution;attribution.contributionCount=5;
    for(unsigned slot=0;slot<5;++slot)attribution.contributions[slot].model=100+int(slot);
    attribution.contributions[0].Record(true,false,0,true);
    attribution.contributions[1].Record(false,true,0,false);
    attribution.contributions[2].Record(true,true,3,false);
    attribution.contributions[2].Record(true,false,0,false);
    attribution.contributions[3].Record(true,false,0,false);
    attribution.contributions[3].Record(true,false,0,true);
    attribution.contributions[4].Record(true,false,0,false);
    attribution.contributions[4].Record(true,false,0,false);
    const auto ranks=TopNoDepthSources(attribution);
    assert(ranks[0]==3&&ranks[1]==4&&ranks[2]==2); // zero-depth, selected, stable slot tie-break
    unsigned selectedTotal=0,attemptedTotal=0,noDepthTotal=0,improvedTotal=0,updatesTotal=0;
    for(const auto &source:attribution.contributions){
        assert(source.attempted==source.noDepth+source.improved&&source.attempted<=source.selected&&source.redundant<=source.noDepth);
        selectedTotal+=source.selected;attemptedTotal+=source.attempted;noDepthTotal+=source.noDepth;
        improvedTotal+=source.improved;updatesTotal+=source.updates;
    }
    assert(selectedTotal==8&&attemptedTotal==7&&noDepthTotal==6&&improvedTotal==1&&updatesTotal==3);
    assert(attribution.contributions[1].budget==1&&attribution.contributions[1].noDepth==0);
    Stats noSources;for(auto slot:TopNoDepthSources(noSources))assert(slot==-1);
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
    PolygonSelection polygonSelection;
    for(unsigned i=0;i<MaxSelectedTriangles+1;++i){
        PolygonCandidate surface;surface.owner=i;surface.priority=i+1;
        surface.eye[0].count=MaxClippedVertices;surface.eye[1].count=3;
        polygonSelection.Add(surface);
    }
    assert(polygonSelection.Count()==MaxSelectedTriangles);
    unsigned polygonVisits=0;
    polygonSelection.Visit([&](const PolygonCandidate &surface){assert(surface.owner!=0);++polygonVisits;});
    assert(polygonVisits==MaxSelectedTriangles); // each polygon occupies one slot
    Triangle invalid;invalid.priority=std::numeric_limits<double>::infinity();selected.Add(invalid);assert(selected.Count()==0);
    Point outside[3]={{60,60,.2},{80,60,.2},{60,80,.2}};assert(Priority(outside)==0);
    Point broad[3]={{0,0,.2},{48,0,.2},{0,48,.2}};assert(Priority(broad)>0);
    // Source selection finds a broad final face that index-stride sampling
    // missed, and keeps vertical orientations despite numerous broad roofs.
    SourceCache cache;cache.Reset();unsigned reads=0;
    cache.Build(302,[&](unsigned face,Point (&points)[3]){
        ++reads;const double size=face==299?1000:double(face+1);
        points[0]={0,0,0};points[1]={size,0,0};points[2]={0,size,0};
        if(face==300){points[1]={0,10,0};points[2]={0,0,10};}
        if(face==301){points[1]={10,0,0};points[2]={0,0,10};}
        return true;
    });
    assert(cache.ready&&reads==302&&cache.count==SourceFacesPerAxis+2);
    bool found[3]={};
    for(unsigned i=0;i<cache.count;++i){assert(cache.faces[i]<302);if(cache.faces[i]>=299)found[cache.faces[i]-299]=true;}
    assert(found[0]&&found[1]&&found[2]);
    cache.Reset();assert(!cache.ready&&cache.count==0); // reinstance invalidation
    reads=0;
    cache.Build(100000,[&](unsigned face,Point (&points)[3]){
        assert(face<100000);++reads;
        points[0]={0,0,0};points[1]={1,0,0};points[2]={0,1,0};return true;
    });
    assert(reads==MaxCacheScan&&cache.count==SourceFacesPerAxis);
    cache.Reset();cache.Build(0,[](unsigned,Point (&)[3]){assert(false);return false;});
    assert(cache.ready&&cache.count==0);
    cache.Reset();cache.Build(8,[](unsigned face,Point (&points)[3]){
        points[0]=points[1]=points[2]={0,0,0};
        if(face%2)points[0].x=std::numeric_limits<double>::quiet_NaN();return true;
    });
    assert(cache.ready&&cache.count==0); // no malformed/degenerate source faces
    cache.Reset();cache.Build(5,[](unsigned,Point (&)[3]){return false;});
    assert(cache.ready&&cache.count==0); // invalid index/reader failure
    // Every eligible object receives one sample before any gets a second.
    unsigned samples[MaxSourceObjects]={};unsigned sequence=0;
    const unsigned used=Distribute(MaxSourceObjects,[](unsigned){return SourceFacesPerAxis*3;},
        [&](unsigned object,unsigned rank){assert(rank==sequence/MaxSourceObjects);assert(object==sequence%MaxSourceObjects);++sequence;++samples[object];});
    assert(used==MaxTriangles&&sequence==MaxTriangles);
    for(auto n:samples)assert(n==MaxTriangles/MaxSourceObjects);
    unsigned sparse[3]={};
    assert(Distribute(3,[](unsigned object){return object==1?0u:2u;},
        [&](unsigned object,unsigned rank){assert(object!=1&&rank<2);++sparse[object];})==4);
    assert(sparse[0]==2&&sparse[1]==0&&sparse[2]==2);
    // Current-view source footprints: clip to each viewport and avoid spending
    // source samples on tiny, thin, offscreen or unprojectable bounds.
    double view[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    double low[3]={-.5,-.5,.2},high[3]={.5,.5,.8};
    auto footprint=ProjectedFootprint(view,low,high);
    assert(footprint.valid&&footprint.area==576);
    view[12]=4;auto otherEye=ProjectedFootprint(view,low,high);
    assert(otherEye.valid&&otherEye.area==0); // per-eye disocclusion
    view[12]=0;
    low[0]=low[1]=0;high[0]=high[1]=.01;
    footprint=ProjectedFootprint(view,low,high);assert(footprint.valid&&footprint.area==0);
    high[0]=.04;high[1]=1; // tall but thinner than two grid cells
    assert(ProjectedFootprint(view,low,high).area==0);
    high[0]=high[1]=.125; // 3x3 footprint is below 16-cell area threshold
    assert(ProjectedFootprint(view,low,high).area==0);
    high[0]=high[1]=.25;assert(ProjectedFootprint(view,low,high).area==36);
    low[0]=low[1]=-100;high[0]=high[1]=100;
    assert(ProjectedFootprint(view,low,high).area==Size*Size);
    low[2]=0;assert(!ProjectedFootprint(view,low,high).valid);
    low[2]=.2;high[2]=1;assert(!ProjectedFootprint(view,low,high).valid);
    high[2]=.8;view[15]=-1;assert(ProjectedFootprint(view,low,high).area==0);view[15]=1;
    low[0]=std::numeric_limits<double>::quiet_NaN();assert(!ProjectedFootprint(view,low,high).valid);
    low[0]=101;assert(!ProjectedFootprint(view,low,high).valid);
    // Near-plane crossings remain useful scheduling candidates, but the box
    // itself never enters the coverage map. Fully behind/far sources are skipped.
    double identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    double nearLow[3]={-1,-1,-.5},nearHigh[3]={1,1,.5};
    auto nearFootprint=ProjectedFootprint(identity,nearLow,nearHigh);
    assert(!nearFootprint.valid && nearFootprint.area==Size*Size);
    nearHigh[2]=-.1;assert(ProjectedFootprint(identity,nearLow,nearHigh).area==0);
    // A nearby wall extending beyond the viewport now supplies real depth.
    Point wall[3]={{-3,-3,.2},{3,-3,.2},{0,3,.2}};
    const auto clippedWall=ClipTriangle(identity,wall);
    assert(clippedWall.changed && clippedWall.count>0);
    Grid wallGrid;Stats wallStats;
    for(unsigned i=0;i<clippedWall.count;++i){const auto &t=clippedWall.triangles[i];wallGrid.Raster(t[0],t[1],t[2],wallStats);}
    assert(wallGrid.Coverage()>Size*Size*.8);
    assert(wallGrid.Hidden(box(9,12,9,12,.8),wallStats));
    assert(!wallGrid.Hidden(box(9,12,9,12,.1),wallStats));
    const auto wallPolygon=ClipPolygon(identity,wall);
    Grid polygonWall;Stats polygonWork;polygonWall.RasterPolygon(wallPolygon,polygonWork);
    assert(polygonWall.Coverage()>wallGrid.Coverage()); // no artificial fan seams
    assert(polygonWork.rasterTests<wallStats.rasterTests);
    assert(polygonWork.edgeTests<=MaxRasterEdges);
    // Independent original-surface oracle: clipping may lose boundary/seam
    // cells, but must never write a cell absent from the real triangle, or put
    // it closer than that surface. Include near/far and side-plane crossings.
    std::mt19937 clippingRandom(27851);
    std::uniform_real_distribution<double> xy(-3,3),z(-.5,1.5);
    unsigned clippedCovered=0;
    for(unsigned iteration=0;iteration<600;++iteration){
        Point testSurface[3];for(auto &samplePoint:testSurface)samplePoint={xy(clippingRandom),xy(clippingRandom),z(clippingRandom)};
        Point screen[3];for(unsigned i=0;i<3;++i)screen[i]={(testSurface[i].x+1)*Size*.5,(testSurface[i].y+1)*Size*.5,testSurface[i].z};
        const auto expected=reference(screen[0],screen[1],screen[2]);
        const auto clipped=ClipTriangle(identity,testSurface);
        assert(clipped.count<=MaxClippedTriangles);
        Grid actual;Stats work;
        for(unsigned i=0;i<clipped.count;++i){
            const auto &t=clipped.triangles[i];
            for(auto clippedPoint:t)assert(Finite(clippedPoint)&&clippedPoint.x>=-1e-8&&clippedPoint.x<=Size+1e-8&&clippedPoint.y>=-1e-8&&clippedPoint.y<=Size+1e-8);
            actual.Raster(t[0],t[1],t[2],work);
        }
        Grid polygonGrid;Stats polygonStats;
        polygonGrid.RasterPolygon(ClipPolygon(identity,testSurface),polygonStats);
        assert(polygonStats.edgeTests<=MaxRasterEdges&&polygonStats.rasterTests<=MaxRasterTests);
        for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
            Stats probe;
            if(!actual.Hidden(box(x,x+1,y,y+1,.999999),probe))continue;
            ++clippedCovered;const double expectedDepth=expected[y*Size+x];
            assert(expectedDepth<1); // no invented coverage across an opening
            Stats frontProbe;
            assert(!actual.Hidden(box(x,x+1,y,y+1,expectedDepth-DepthGuard),frontProbe));
        }
        for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x){
            const double stored=polygonGrid.DepthAt(x,y);
            if(stored>=1)continue;
            const double expectedDepth=expected[y*Size+x];
            assert(expectedDepth<1&&stored+1e-10>=expectedDepth);
        }
    }
    assert(clippedCovered>10000);
    Point malformed[3]={{0,0,.5},{1,0,.5},{0,1,std::numeric_limits<double>::quiet_NaN()}};
    assert(ClipTriangle(identity,malformed).count==0);
    double behindEye[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,-1};
    assert(ClipTriangle(behindEye,wall).count==0);
    Polygon concave;concave.count=5;
    concave.vertices[0]={2,2,.2};concave.vertices[1]={40,2,.2};concave.vertices[2]={15,15,.2};
    concave.vertices[3]={40,40,.2};concave.vertices[4]={2,40,.2};
    Grid invalidPolygon;Stats invalidWork;invalidPolygon.RasterPolygon(concave,invalidWork);
    assert(invalidPolygon.Coverage()==0); // never form a convex hull over a hole
    Polygon nonplanar=wallPolygon;nonplanar.vertices[1].z+=.1;
    invalidPolygon.RasterPolygon(nonplanar,invalidWork);assert(invalidPolygon.Coverage()==0);
    Stats edgeBudget;edgeBudget.edgeTests=MaxRasterEdges;
    invalidPolygon.RasterPolygon(wallPolygon,edgeBudget);assert(invalidPolygon.Coverage()==0);
    Stereo previewGrid;previewGrid.eye[0]=polygonWall;
    const auto preview=previewGrid.MakePreview(true),unavailable=previewGrid.MakePreview(false);
    unsigned displayCells=0;
    for(unsigned i=0;i<Size*Size;++i){displayCells+=preview.eye[0][i]!=0;assert(preview.eye[1][i]==0&&unavailable.eye[0][i]==0);}
    assert(preview.valid&&!unavailable.valid&&displayCells==polygonWall.Coverage());
    assert(PreviewValue(.1)>PreviewValue(.9)&&PreviewValue(1)==0);
    // Perspective near/eye-plane crossing: reconstruct clipped vertices and
    // prove they still lie on the original triangle, rather than a filled cap.
    double perspective[16]={1,0,0,0,0,1,0,0,0,0,1,1,0,0,-.1,0};
    Point crossing[3]={{-.5,-.5,-.5},{1,-.5,2},{-.5,1,2}};
    auto nearClip=ClipTriangle(perspective,crossing);
    assert(nearClip.changed&&nearClip.count>0);
    auto subtract=[](Point a,Point b){return Point{a.x-b.x,a.y-b.y,a.z-b.z};};
    auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const auto edgeA=subtract(crossing[1],crossing[0]),edgeB=subtract(crossing[2],crossing[0]);
    const double aa=dot(edgeA,edgeA),ab=dot(edgeA,edgeB),bb=dot(edgeB,edgeB),denominator=aa*bb-ab*ab;
    for(unsigned part=0;part<nearClip.count;++part)for(auto projected:nearClip.triangles[part]){
        const double distance=.1/(1-projected.z);
        Point recovered{(projected.x/(Size*.5)-1)*distance,(projected.y/(Size*.5)-1)*distance,distance};
        const auto relative=subtract(recovered,crossing[0]);
        const double u=(bb*dot(relative,edgeA)-ab*dot(relative,edgeB))/denominator;
        const double v=(aa*dot(relative,edgeB)-ab*dot(relative,edgeA))/denominator;
        assert(u>=-1e-7&&v>=-1e-7&&u+v<=1+1e-7);
        Point onSurface{crossing[0].x+u*edgeA.x+v*edgeB.x,crossing[0].y+u*edgeA.y+v*edgeB.y,crossing[0].z+u*edgeA.z+v*edgeB.z};
        const auto error=subtract(recovered,onSurface);assert(dot(error,error)<1e-12);
    }
    // Face ranges exclude adjacent transparent triangles and incomplete ranges.
    MeshRange opaque{3,6};
    assert(!ContainsFace(opaque,0)&&ContainsFace(opaque,1)&&ContainsFace(opaque,2)&&!ContainsFace(opaque,3));
    assert(!ContainsFace({4,1},1)&&!ContainsFace({0,0},0));
    SourceCache mixed{};mixed.Reset();
    mixed.Build(4,[&](unsigned face,Point (&p)[3]){
        if(!ContainsFace(opaque,face))return false;
        p[0]={0,0,0};p[1]={10,0,0};p[2]={0,10,0};return true;
    });
    assert(mixed.count==2);
    for(unsigned i=0;i<mixed.count;++i)assert(ContainsFace(opaque,mixed.faces[i]));
    // Sparse opaque meshes must not disappear from a uniformly sampled large
    // transparent object. Scan only the eligible face domain, retain real IDs.
    unsigned opaqueReads=0;
    mixed.BuildMapped(2,[&](unsigned face,Point (&points)[3]){
        assert(face==9000||face==9900);++opaqueReads;
        points[0]={0,0,0};points[1]={10,0,0};points[2]={0,10,0};return true;
    },[](unsigned rank){return rank==0?9000u:9900u;});
    assert(opaqueReads==2&&mixed.count==2);
    for(unsigned i=0;i<mixed.count;++i)assert(mixed.faces[i]==9000||mixed.faces[i]==9900);
    // Depth reuse must reproduce the detailed raster path, including holes,
    // closer surfaces, partial tiles and stale (farther-only) tile summaries.
    auto rectangle=[](double left,double right,double bottom,double top,double base,double slope=0){
        Polygon out;out.count=4;
        out.vertices[0]={left,bottom,base+slope*left};out.vertices[1]={right,bottom,base+slope*right};
        out.vertices[2]={right,top,base+slope*right};out.vertices[3]={left,top,base+slope*left};return out;
    };
    auto sameDepth=[](const Grid &left,const Grid &right){
        for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x)
            assert(std::abs(left.DepthAt(x,y)-right.DepthAt(x,y))<1e-12);
    };
    Grid reused,detailed;Stats initialReuse,initialDetail;
    const auto frontWall=rectangle(0,48,0,48,.2),farPanel=rectangle(8,40,8,40,.7);
    reused.RasterPolygon(frontWall,initialReuse);detailed.RasterPolygon(frontWall,initialDetail,false,false);
    Stats skipWork,fullWork;
    reused.RasterPolygon(farPanel,skipWork);detailed.RasterPolygon(farPanel,fullWork,false,false);
    sameDepth(reused,detailed);
    assert(skipWork.redundantEyes==1&&skipWork.edgeTests==0&&skipWork.rasterTests==0);
    assert(skipWork.redundancyTests>0&&fullWork.edgeTests>1000);
    Stats duplicateWork,duplicateDetail;
    reused.RasterPolygon(frontWall,duplicateWork);detailed.RasterPolygon(frontWall,duplicateDetail,false,false);
    sameDepth(reused,detailed);
    assert(duplicateWork.depthSkippedCells>0&&duplicateWork.edgeTests<duplicateDetail.edgeTests);
    // A partly covered tile cannot conceal an opening or a nearer occluder.
    reused.Clear();detailed.Clear();
    for(const auto &panel:{rectangle(0,23,0,48,.2),rectangle(25,48,0,48,.2),rectangle(20,28,8,40,.7),rectangle(16,32,16,32,.1)}){
        Stats fast,slow;reused.RasterPolygon(panel,fast);detailed.RasterPolygon(panel,slow,false,false);sameDepth(reused,detailed);
    }
    assert(reused.DepthAt(24,10)<1&&reused.DepthAt(24,24)<.2);
    std::mt19937 reuseRandom(14071);
    std::uniform_real_distribution<double> position(1,40),extent(2,7),distance(.05,.85),depthSlope(-.0005,.0005);
    uint64_t savedEdges=0,tileCells=0,cellSkips=0;
    for(unsigned iteration=0;iteration<400;++iteration){
        const double left=position(reuseRandom),bottom=position(reuseRandom);
        auto panel=rectangle(left,left+extent(reuseRandom),bottom,bottom+extent(reuseRandom),distance(reuseRandom),depthSlope(reuseRandom));
        if(iteration%2)std::reverse(panel.vertices,panel.vertices+panel.count);
        Stats fast,slow;reused.RasterPolygon(panel,fast);detailed.RasterPolygon(panel,slow,false,false);sameDepth(reused,detailed);
        assert(fast.redundancyTests<=MaxRedundancyTests&&fast.edgeTests<=slow.edgeTests);
        savedEdges+=slow.edgeTests-fast.edgeTests;tileCells+=fast.tileSkippedCells;cellSkips+=fast.depthSkippedCells;
    }
    assert(savedEdges>1000&&tileCells>0&&cellSkips>0);
    Stats exhausted,exhaustedBaseline;exhausted.redundancyTests=MaxRedundancyTests;
    reused.RasterPolygon(farPanel,exhausted);detailed.RasterPolygon(farPanel,exhaustedBaseline,false,false);sameDepth(reused,detailed);
    assert(exhausted.redundancyTests==MaxRedundancyTests&&exhausted.redundancyBudgetStops==1);
    reused.Clear();detailed.Clear();
    Stats afterClear,clearBaseline;reused.RasterPolygon(farPanel,afterClear);detailed.RasterPolygon(farPanel,clearBaseline,false,false);sameDepth(reused,detailed);
    assert(afterClear.redundantEyes==0&&reused.Coverage()>0);
    std::printf("Depth reuse: %llu edge checks saved; %llu tile cells and %llu exact-depth cells skipped; baseline depths identical.\n",(unsigned long long)savedEdges,(unsigned long long)tileCells,(unsigned long long)cellSkips);
    // Isolate span certification from depth reuse: same inputs and exact depth
    // values, with the prior per-cell edge path used as the reference.
    Grid spanWall,cellWall;Stats spanWallWork,cellWallWork;
    spanWall.RasterPolygon(frontWall,spanWallWork,true,true);
    cellWall.RasterPolygon(frontWall,cellWallWork,true,false);sameDepth(spanWall,cellWall);
    assert(spanWallWork.spanRows>0&&spanWallWork.spanCells>1000);
    assert(spanWallWork.edgeTests*2<cellWallWork.edgeTests);
    assert(spanWallWork.spanEdgeTests<=spanWallWork.edgeTests);
    std::mt19937 spanRandom(438913);
    std::uniform_real_distribution<double> spanPosition(-3,3),spanDepth(-.5,1.5);
    int64_t spanSaved=0;uint64_t certifiedCells=0,referenceEdges=0;
    for(unsigned iteration=0;iteration<1200;++iteration){
        Point spanSurface[3];
        for(auto &spanPoint:spanSurface)spanPoint={spanPosition(spanRandom),spanPosition(spanRandom),spanDepth(spanRandom)};
        auto spanPolygon=ClipPolygon(iteration%2?perspective:identity,spanSurface);
        if(iteration%3==0)std::reverse(spanPolygon.vertices,spanPolygon.vertices+spanPolygon.count);
        Grid spanActual,spanReference;Stats spanActualWork,spanReferenceWork;
        spanActual.RasterPolygon(spanPolygon,spanActualWork,true,true);
        spanReference.RasterPolygon(spanPolygon,spanReferenceWork,true,false);
        sameDepth(spanActual,spanReference);
        assert(spanActualWork.edgeTests<=MaxRasterEdges&&spanActualWork.rasterTests<=MaxRasterTests);
        assert(spanActualWork.spanEdgeTests<=spanActualWork.edgeTests);
        spanSaved+=int64_t(spanReferenceWork.edgeTests)-spanActualWork.edgeTests;referenceEdges+=spanReferenceWork.edgeTests;certifiedCells+=spanActualWork.spanCells;
    }
    assert(spanSaved>1000&&certifiedCells>10000);
    // Silhouette-margin threshold, narrow spans, reversed winding and rounding
    // uncertainty must retain exactly the former boundary-cell classification.
    for(double offset:{-.048-1e-12,-.048,-.048+1e-12,1.001,1.999999999999})
    for(double width:{.01,3.0,7.0,31.0}){
        auto boundaryPanel=rectangle(offset,offset+width,1.01,46.99,.3,.0002);
        Grid spanBoundary,cellBoundary;Stats spanBoundaryWork,cellBoundaryWork;
        spanBoundary.RasterPolygon(boundaryPanel,spanBoundaryWork,true,true);
        cellBoundary.RasterPolygon(boundaryPanel,cellBoundaryWork,true,false);sameDepth(spanBoundary,cellBoundary);
    }
    Grid spanSequence,cellSequence;Stats spanFrameWork,cellFrameWork;
    for(unsigned iteration=0;iteration<64;++iteration){
        auto sequencePanel=rectangle(0,48,0,48,.9-.01*iteration,.0001);
        spanSequence.RasterPolygon(sequencePanel,spanFrameWork,true,true);
        cellSequence.RasterPolygon(sequencePanel,cellFrameWork,true,false);
    }
    assert(spanFrameWork.edgeTests<=MaxRasterEdges&&spanFrameWork.rasterTests<=MaxRasterTests&&spanFrameWork.rasterRows<=MaxRasterRows);
    for(unsigned row=0;row<Size;++row)for(unsigned col=0;col<Size;++col)
        assert(spanSequence.DepthAt(col,row)<=cellSequence.DepthAt(col,row)+1e-12);
    std::printf("Row spans: %llu of %llu reference edge tests saved; %llu certified cells; exact per-polygon depths preserved.\n",(unsigned long long)spanSaved,(unsigned long long)referenceEdges,(unsigned long long)certifiedCells);
    // Differential oracle retained from the former full seven-plane clipper.
    ClipProfile fastRoutes;uint64_t referencePlanes=0,clipComparisons=0;
    auto compareClip=[&](const double *clipMatrix,const Point (&surface)[3],bool rasterCheck){
        uint32_t referenceVisits=0;
        const auto fast=ClipPolygon(clipMatrix,surface,&fastRoutes);
        const auto old=ClipPolygonReference(clipMatrix,surface,&referenceVisits);
        ++clipComparisons;referencePlanes+=referenceVisits;
        assert(fast.count==old.count);
        if(fast.count)assert(fast.changed==old.changed);
        for(unsigned i=0;i<fast.count;++i){
            assert(std::abs(fast.vertices[i].x-old.vertices[i].x)<1e-9);
            assert(std::abs(fast.vertices[i].y-old.vertices[i].y)<1e-9);
            assert(std::abs(fast.vertices[i].z-old.vertices[i].z)<1e-12);
        }
        if(rasterCheck){
            Grid fastGrid,oldGrid;Stats fastWork,oldWork;
            fastGrid.RasterPolygon(fast,fastWork);oldGrid.RasterPolygon(old,oldWork);sameDepth(fastGrid,oldGrid);
        }
    };
    Point insideSurface[3]={{-.2,-.2,.3},{.2,-.2,.3},{0,.2,.3}};
    Point outsideSurface[3]={{2,-.2,.3},{3,-.2,.3},{2,.2,.3}};
    Point enclosingSurface[3]={{-2,-2,.3},{2,-2,.3},{0,2,.3}};
    ClipProfile insideRoute,outsideRoute,crossingRoute,boundaryRoute,badRoute;
    assert(ClipPolygon(identity,insideSurface,&insideRoute).count==3&&insideRoute.inside==1&&insideRoute.planeVisits==0);
    assert(ClipPolygon(identity,outsideSurface,&outsideRoute).count==0&&outsideRoute.outside==1&&outsideRoute.planeVisits==0);
    assert(ClipPolygon(identity,enclosingSurface,&crossingRoute).count>=3&&crossingRoute.crossing==1);
    // Vertices outside different planes can still enclose visible geometry.
    compareClip(identity,enclosingSurface,true);compareClip(identity,insideSurface,true);compareClip(identity,outsideSurface,true);
    for(double boundaryOffset:{-1e-14,0.0,1e-14}){
        Point boundarySurface[3]={{-1+boundaryOffset,-.2,.3},{.2,-.2,.3},{0,.2,.3}};
        assert(ClipPolygon(identity,boundarySurface,&boundaryRoute).count>0);
        compareClip(identity,boundarySurface,true);
    }
    assert(boundaryRoute.boundary==3);
    for(double eyeW:{.001-1e-15,.001,.001+1e-15}){
        double eyeBoundary[16]={.001,0,0,0,0,.001,0,0,0,0,.0005,0,0,0,0,eyeW};
        compareClip(eyeBoundary,insideSurface,true);
    }
    assert(ClipPolygon(identity,malformed,&badRoute).count==0&&badRoute.invalid==1);
    compareClip(identity,malformed,true);compareClip(behindEye,insideSurface,true);
    std::mt19937 clipRandom(744183);
    std::uniform_real_distribution<double> clipXY(-4,4),clipZ(-1,3);
    for(unsigned iteration=0;iteration<12000;++iteration){
        Point clipSurface[3];for(auto &clipPoint:clipSurface)clipPoint={clipXY(clipRandom),clipXY(clipRandom),clipZ(clipRandom)};
        double oblique[16]={1,0,0,.1,0,1,0,-.07,0,0,1,1,.03,-.05,-.1,.2};
        const double *clipMatrix=iteration%3==0?identity:iteration%3==1?perspective:oblique;
        compareClip(clipMatrix,clipSurface,iteration%12==0);
    }
    double largeClip[16]={1e150,0,0,0,0,1e150,0,0,0,0,1e150,0,0,0,0,1e150};
    compareClip(largeClip,insideSurface,true);
    assert(uint64_t(fastRoutes.inside)+fastRoutes.outside+fastRoutes.crossing+fastRoutes.boundary+fastRoutes.invalid==clipComparisons);
    assert(fastRoutes.inside>0&&fastRoutes.outside>0&&fastRoutes.crossing>0&&fastRoutes.boundary>0&&fastRoutes.invalid>0);
    assert(fastRoutes.planeVisits<referencePlanes);
    std::printf("Clip paths: %llu differential cases; polygon-plane traversals %u versus %llu; reference coverage/depth preserved.\n",(unsigned long long)clipComparisons,fastRoutes.planeVisits,(unsigned long long)referencePlanes);
    std::puts("PASS: clip fast paths and boundary fallback, phase/source accounting, certified spans, depth reuse, holes, bounded work, previews and 3.6 million triangle checks.");
}
