#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace WorldOcclusion {
constexpr unsigned Size=48,MaxOccluders=24,MaxTriangles=1024;
constexpr unsigned MaxRasterTests=8192,MaxQueryTests=16384;
constexpr unsigned MaxRasterRows=8192,MaxSelectedTriangles=256;
constexpr unsigned MaxRasterEdges=MaxRasterTests*3;
constexpr unsigned TileSize=4,TilesPerRow=Size/TileSize,TileCount=TilesPerRow*TilesPerRow;
constexpr unsigned MaxRedundancyTests=8192;
static_assert(Size%TileSize==0,"whole depth-summary tiles");
constexpr unsigned MaxSourceObjects=128,SourceFacesPerAxis=16,MaxCacheScan=2048,MaxCacheScanPerFrame=4096;
constexpr unsigned MaxFootprintSources=32;
constexpr double MinSourceExtent=2,MinSourceArea=16;
constexpr double DepthGuard=0.00002,ScreenGuard=0.002;
enum BuildPhase { BuildSetup,BuildGather,BuildBounds,BuildMaterials,BuildCache,BuildProjection,BuildSelect,BuildRaster,BuildFinalize,BuildPhaseCount };
inline const char *BuildPhaseName(unsigned phase){
    static const char *names[]={"setup","gather","bounds","materials","cache","projection","selection","raster","finalize"};
    return phase<BuildPhaseCount?names[phase]:"invalid";
}
// Monotonic-clock bookkeeping kept portable for deterministic accounting tests.
struct BuildTimeline {
    bool active=false;BuildPhase phase=BuildSetup;uint64_t last=0;
    std::array<uint64_t,BuildPhaseCount> ns{};
    void Start(bool enabled,uint64_t now){active=enabled;phase=BuildSetup;last=now;ns.fill(0);}
    void Switch(BuildPhase next,uint64_t now){
        if(!active)return;
        if(now>=last){ns[phase]+=now-last;last=now;}
        phase=next;
    }
    void Finish(uint64_t now){Switch(phase,now);active=false;}
};
struct SourceContribution {
    int32_t model=-1;
    uint32_t selected=0,attempted=0,noDepth=0,improved=0,budget=0,redundant=0,updates=0;
    void Record(bool tried,bool limited,uint32_t depthUpdates,bool depthRejected){
        ++selected;if(limited)++budget;
        if(!tried)return;
        ++attempted;updates+=depthUpdates;
        if(depthUpdates)++improved;else ++noDepth;
        if(depthRejected)++redundant;
    }
};
constexpr unsigned TopSourceCount=3;
struct ClipProfile {
    uint32_t inside=0,outside=0,crossing=0,boundary=0,invalid=0,planeVisits=0;
};
struct Stats {
    uint32_t builds=0,occluderCandidates=0,occluders=0,triangles=0,cells=0;
    uint32_t candidates=0,rejected=0,visible=0,invalid=0,budget=0;
    uint32_t leftCells=0,rightCells=0,rasterTests=0,queryTests=0;
    uint64_t indices=0,vertices=0;
    double buildMs=0,queryMs=0;
    ClipProfile clipProfile;
    bool buildPhasesValid=false;uint32_t phaseClockReads=0;
    std::array<double,BuildPhaseCount> buildPhaseMs{};
    uint32_t contributionCount=0;
    std::array<SourceContribution,MaxFootprintSources> contributions{};
    uint32_t rasterRows=0,selectedTriangles=0;
    uint32_t cacheBuilds=0,cacheHits=0,cacheScanned=0,cacheDeferred=0;
    uint32_t sourceObjects=0,sourceProjected=0,sourceProjectionRejected=0,sourceSmall=0,sourceBudgetSkipped=0;
    uint32_t sourceTopSamples=0;
    int32_t sourceTopModel=-1;
    uint32_t footprintCandidates=0,footprintRejected=0,footprintUncertain=0,footprintSelected=0,footprintCapacitySkipped=0;
    uint32_t sourceGeometryRejected=0,sourceNoOpaque=0,meshOpaque=0,meshAlpha=0,meshUnsupported=0;
    uint32_t sourceClipped=0,clipTriangles=0,queryInvalid=0;
    uint32_t polygonCandidates=0,selectedPolygons=0,selectionDropped=0;
    uint32_t rasterBudgetSkipped=0,noDepthPolygons=0,edgeTests=0;
    uint32_t redundancyTests=0,redundancyBudgetStops=0,redundantPolygons=0,redundantEyes=0;
    uint32_t tileSkippedCells=0,depthSkippedCells=0;
    uint32_t spanRows=0,spanCells=0,spanFallbackRows=0,spanEdgeTests=0;
};
inline std::array<int32_t,TopSourceCount> TopNoDepthSources(const Stats &stats){
    std::array<int32_t,TopSourceCount> result;result.fill(-1);
    const unsigned count=std::min(stats.contributionCount,MaxFootprintSources);
    for(unsigned slot=0;slot<count;++slot){
        const auto &candidate=stats.contributions[slot];if(!candidate.noDepth)continue;
        unsigned rank=0;
        for(;rank<TopSourceCount;++rank){
            if(result[rank]<0)break;
            const auto &other=stats.contributions[unsigned(result[rank])];
            if(candidate.noDepth>other.noDepth || (candidate.noDepth==other.noDepth&&candidate.selected>other.selected))break;
        }
        if(rank==TopSourceCount)continue;
        for(unsigned i=TopSourceCount-1;i>rank;--i)result[i]=result[i-1];
        result[rank]=int32_t(slot);
    }
    return result;
}
struct Point { // screen x/y in cells, Vulkan depth [0,1]
    double x,y,z;
    Point(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
};
// Object-space scheduling cache only. Never merges faces or caches visibility.
// An axis quota prevents broad roofs/roads from excluding all wall orientations.
struct SourceCache {
    uint32_t faces[SourceFacesPerAxis*3];
    unsigned count;
    bool ready;
    uint64_t eligibilityKey;
    void Reset(){count=0;ready=false;eligibilityKey=0;}
    template<class Read> void Build(unsigned total,Read read){
        BuildMapped(total,read,[](unsigned face){return face;});
    }
    template<class Read,class Index> void BuildMapped(unsigned total,Read read,Index index){
        struct Ranked {double score=0;unsigned face=0;};
        Ranked best[3][SourceFacesPerAxis];unsigned counts[3]={};
        const unsigned scan=std::min(total,MaxCacheScan);
        for(unsigned i=0;i<scan;++i){
            const unsigned face=index(unsigned(uint64_t(i)*total/scan));
            Point p[3];if(!read(face,p))continue;
            bool finite=true;for(auto v:p)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))finite=false;
            if(!finite)continue;
            const double ax=p[1].x-p[0].x,ay=p[1].y-p[0].y,az=p[1].z-p[0].z;
            const double bx=p[2].x-p[0].x,by=p[2].y-p[0].y,bz=p[2].z-p[0].z;
            const double n[3]={ay*bz-az*by,az*bx-ax*bz,ax*by-ay*bx};
            const double area=n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
            const double edge=std::max({ax*ax+ay*ay+az*az,bx*bx+by*by+bz*bz,
                (ax-bx)*(ax-bx)+(ay-by)*(ay-by)+(az-bz)*(az-bz)});
            const double score=area*std::min(1.0,area/(edge*edge));
            if(!std::isfinite(score)||score<=1e-20)continue;
            unsigned axis=0;for(unsigned j=1;j<3;++j)if(std::abs(n[j])>std::abs(n[axis]))axis=j;
            unsigned pos=0;while(pos<counts[axis]&&best[axis][pos].score>=score)++pos;
            if(pos>=SourceFacesPerAxis)continue;
            const unsigned end=std::min(counts[axis],SourceFacesPerAxis-1);
            for(unsigned j=end;j>pos;--j)best[axis][j]=best[axis][j-1];
            best[axis][pos]={score,face};counts[axis]=std::min(counts[axis]+1,SourceFacesPerAxis);
        }
        count=0;
        for(unsigned rank=0;rank<SourceFacesPerAxis;++rank)for(unsigned axis=0;axis<3;++axis)
            if(rank<counts[axis])faces[count++]=best[axis][rank].face;
        ready=true;
    }
};
// Fair current-frame distribution: one face per eligible object each round.
// A partial final round remains deterministic; unused source slots cost no
// projections. Cache construction and projection have independent work caps.
template<class Count,class Sample> inline unsigned Distribute(unsigned objects,Count count,Sample sample){
    unsigned used=0;
    for(unsigned round=0;round<SourceFacesPerAxis*3;++round)for(unsigned object=0;object<objects;++object){
        if(round>=count(object))continue;
        if(used==MaxTriangles)return used;
        sample(object,round);++used;
    }
    return used;
}
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
struct ClipPoint { double v[4]; };
inline ClipPoint Transform(const double *matrix,Point p){
    ClipPoint out{};
    for(unsigned r=0;r<4;++r)out.v[r]=matrix[r]*p.x+matrix[4+r]*p.y+matrix[8+r]*p.z+matrix[12+r];
    return out;
}
inline double ClipDistance(const ClipPoint &p,unsigned plane){
    // Stay strictly inside the depth interval accepted by the rasterizer.
    switch(plane){
    case 0:return p.v[3]-.001;
    case 1:return p.v[2]-1e-8*p.v[3];
    case 2:return (1-1e-8)*p.v[3]-p.v[2];
    case 3:return p.v[3]+p.v[0];
    case 4:return p.v[3]-p.v[0];
    case 5:return p.v[3]+p.v[1];
    default:return p.v[3]-p.v[1];
    }
}
constexpr unsigned MaxClippedVertices=12,MaxClippedTriangles=MaxClippedVertices-2;
struct Polygon { Point vertices[MaxClippedVertices];unsigned count=0;bool changed=false; };
struct Clipped { Point triangles[MaxClippedTriangles][3];unsigned count=0;bool changed=false; };
// Clip real surface triangles in homogeneous space, never fill a bounding box.
// A capacity/numerical failure discards this surface (fail open).
inline Polygon ClipPolygonReference(const double *matrix,const Point (&points)[3],uint32_t *planeVisits=nullptr){
    Polygon result;
    ClipPoint polygon[MaxClippedVertices],next[MaxClippedVertices];unsigned count=3;
    for(unsigned i=0;i<3;++i){
        polygon[i]=Transform(matrix,points[i]);
        for(double v:polygon[i].v)if(!std::isfinite(v))return result;
    }
    for(unsigned plane=0;plane<7 && count;++plane){
        if(planeVisits)++*planeVisits;
        unsigned size=0;
        auto append=[&](ClipPoint p){if(size==MaxClippedVertices)return false;next[size++]=p;return true;};
        ClipPoint previous=polygon[count-1];double before=ClipDistance(previous,plane);
        for(unsigned i=0;i<count;++i){
            const auto current=polygon[i];const double after=ClipDistance(current,plane);
            if(!std::isfinite(before)||!std::isfinite(after))return Polygon{};
            if((before>=0)!=(after>=0)){
                const double t=before/(before-after);ClipPoint intersection{};
                if(!std::isfinite(t)||t<0||t>1)return Polygon{};
                for(unsigned k=0;k<4;++k)intersection.v[k]=previous.v[k]+t*(current.v[k]-previous.v[k]);
                if(!append(intersection))return Polygon{};
            }
            if(after>=0){if(!append(current))return Polygon{};}else result.changed=true;
            previous=current;before=after;
        }
        count=size;for(unsigned i=0;i<count;++i)polygon[i]=next[i];
    }
    Point projected[MaxClippedVertices];
    for(unsigned i=0;i<count;++i){
        const auto &v=polygon[i].v;
        projected[i]={(v[0]/v[3]+1)*Size*.5,(v[1]/v[3]+1)*Size*.5,v[2]/v[3]};
        if(!Finite(projected[i]))return Polygon{};
    }
    if(count>=3){result.count=count;for(unsigned i=0;i<count;++i)result.vertices[i]=projected[i];}
    return result;
}
// Same thresholds and plane order as the reference. Confidently inside planes
// need no polygon traversal; a common outside plane proves an empty surface.
// Near-boundary inputs retain the full reference path, avoiding rounding-driven
// changes when preceding intersections approach a nominally inside plane.
inline Polygon ClipPolygon(const double *matrix,const Point (&points)[3],ClipProfile *profile=nullptr){
    ClipPoint original[3];double magnitude=1;
    for(unsigned i=0;i<3;++i){
        original[i]=Transform(matrix,points[i]);
        for(double value:original[i].v){
            if(!std::isfinite(value)){if(profile)++profile->invalid;return {};}
            magnitude=std::max(magnitude,std::abs(value));
        }
    }
    const double tolerance=1024*std::numeric_limits<double>::epsilon()*magnitude;
    unsigned crossingMask=0;bool uncertain=false;
    for(unsigned plane=0;plane<7;++plane){
        bool outside=true;
        for(unsigned vertex=0;vertex<3;++vertex){
            const double value=ClipDistance(original[vertex],plane);
            if(!std::isfinite(value)){if(profile)++profile->invalid;return {};}
            if(value>=-tolerance)outside=false;
            if(value<0)crossingMask|=1u<<plane;
            if(std::abs(value)<=tolerance)uncertain=true;
        }
        if(outside){
            if(profile)++profile->outside;
            Polygon empty;empty.changed=true;return empty;
        }
    }
    if(uncertain){
        if(profile)++profile->boundary;
        return ClipPolygonReference(matrix,points,profile?&profile->planeVisits:nullptr);
    }
    if(!crossingMask){
        Polygon result;
        for(unsigned i=0;i<3;++i){
            const auto &v=original[i].v;
            result.vertices[i]={(v[0]/v[3]+1)*Size*.5,(v[1]/v[3]+1)*Size*.5,v[2]/v[3]};
            if(!Finite(result.vertices[i])){if(profile)++profile->invalid;return {};}
        }
        result.count=3;if(profile)++profile->inside;return result;
    }
    ClipPoint polygon[MaxClippedVertices],next[MaxClippedVertices];unsigned count=3;
    for(unsigned i=0;i<3;++i)polygon[i]=original[i];
    bool changed=false;
    for(unsigned plane=0;plane<7 && count;++plane){
        if(!(crossingMask&(1u<<plane)))continue;
        if(profile)++profile->planeVisits;
        unsigned size=0;
        auto append=[&](ClipPoint point){if(size==MaxClippedVertices)return false;next[size++]=point;return true;};
        ClipPoint previous=polygon[count-1];double before=ClipDistance(previous,plane);
        for(unsigned i=0;i<count;++i){
            const auto current=polygon[i];const double after=ClipDistance(current,plane);
            if(!std::isfinite(before)||!std::isfinite(after)){if(profile)++profile->invalid;return {};}
            if((before>=0)!=(after>=0)){
                const double t=before/(before-after);ClipPoint intersection{};
                if(!std::isfinite(t)||t<0||t>1){if(profile)++profile->invalid;return {};}
                for(unsigned k=0;k<4;++k)intersection.v[k]=previous.v[k]+t*(current.v[k]-previous.v[k]);
                if(!append(intersection)){if(profile)++profile->invalid;return {};}
            }
            if(after>=0){if(!append(current)){if(profile)++profile->invalid;return {};}}else changed=true;
            previous=current;before=after;
        }
        count=size;for(unsigned i=0;i<count;++i)polygon[i]=next[i];
    }
    Polygon result;
    for(unsigned i=0;i<count;++i){
        const auto &v=polygon[i].v;
        result.vertices[i]={(v[0]/v[3]+1)*Size*.5,(v[1]/v[3]+1)*Size*.5,v[2]/v[3]};
        if(!Finite(result.vertices[i])){if(profile)++profile->invalid;return {};}
    }
    if(count>=3)result.count=count;
    result.changed=changed;if(profile)++profile->crossing;return result;
}
// Triangle adapter retained for callers comparing the former fan path.
inline Clipped ClipTriangle(const double *matrix,const Point (&points)[3]){
    const auto polygon=ClipPolygon(matrix,points);Clipped result;result.changed=polygon.changed;
    for(unsigned i=1;i+1<polygon.count;++i){
        auto &t=result.triangles[result.count++];t[0]=polygon.vertices[0];t[1]=polygon.vertices[i];t[2]=polygon.vertices[i+1];
    }
    return result;
}
struct MeshRange {uint32_t first=0,count=0;};
inline bool ContainsFace(const MeshRange &range,unsigned face){
    const uint64_t first=uint64_t(face)*3;
    return first>=range.first && first+3<=uint64_t(range.first)+range.count;
}
struct Footprint {
    double area;bool valid;
    Footprint(double area=0,bool valid=false):area(area),valid(valid){}
};
// Scheduling only: these bounds never write depth or hide a rendered object.
// Uncertain near/far crossings receive a full-screen scheduling estimate.
// Only clipped real opaque triangles can subsequently contribute depth.
template<class T> inline Footprint ProjectedFootprint(const double *matrix,const T *low,const T *high){
    for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(low[axis])||!std::isfinite(high[axis])||low[axis]>high[axis])return {};
    ClipPoint clip[8];
    for(unsigned corner=0;corner<8;++corner){
        Point p{double((corner&1)?high[0]:low[0]),double((corner&2)?high[1]:low[1]),double((corner&4)?high[2]:low[2])};
        clip[corner]=Transform(matrix,p);
        for(double value:clip[corner].v)if(!std::isfinite(value))return {};
    }
    for(unsigned plane=0;plane<7;++plane){
        bool outside=true;
        for(const auto &p:clip)if(ClipDistance(p,plane)>=0){outside=false;break;}
        if(outside)return {0,true};
    }
    double left=1e100,right=-1e100,bottom=1e100,top=-1e100;
    for(unsigned corner=0;corner<8;++corner){
        double p[3];for(unsigned axis=0;axis<3;++axis)p[axis]=(corner&(1<<axis))?high[axis]:low[axis];
        Point projected;if(!Project(matrix,p,projected))return {double(Size*Size),false};
        left=std::min(left,projected.x);right=std::max(right,projected.x);
        bottom=std::min(bottom,projected.y);top=std::max(top,projected.y);
    }
    const double width=std::min(double(Size),right)-std::max(0.0,left);
    const double height=std::min(double(Size),top)-std::max(0.0,bottom);
    const double area=width*height;
    return {width>=MinSourceExtent&&height>=MinSourceExtent&&area>=MinSourceArea?area:0,true};
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
struct PolygonCandidate {
    Polygon eye[2];unsigned owner=0;double priority=0;
};
inline double Priority(const Polygon &polygon){
    double score=0;
    for(unsigned i=1;i+1<polygon.count;++i){
        const Point fan[3]={polygon.vertices[0],polygon.vertices[i],polygon.vertices[i+1]};
        score+=Priority(fan);
    }
    return score;
}
template<class Surface> class SurfaceSelection {
    std::array<Surface,MaxSelectedTriangles> triangles;
    unsigned count=0;
    static bool Greater(const Surface &a,const Surface &b){return a.priority>b.priority;}
public:
    void Clear(){count=0;}
    unsigned Count()const{return count;}
    void Add(const Surface &t){
        if(!std::isfinite(t.priority)||t.priority<=0)return;
        if(count==triangles.size()){
            if(t.priority<=triangles[0].priority)return;
            std::pop_heap(triangles.begin(),triangles.begin()+count,Greater);--count;
        }
        triangles[count++]=t;
        std::push_heap(triangles.begin(),triangles.begin()+count,Greater);
    }
    void Sort(){std::sort_heap(triangles.begin(),triangles.begin()+count,Greater);}
    template<class F> void Visit(F visit){Sort();VisitSorted(visit);}
    template<class F> void VisitSorted(F visit){
        for(unsigned i=0;i<count;++i)visit(triangles[i]);
        // Sorted storage no longer has heap order. The next build starts empty.
        count=0;
    }
};
using Selection=SurfaceSelection<Triangle>;
using PolygonSelection=SurfaceSelection<PolygonCandidate>;
class Grid {
    std::array<double,Size*Size> depth;
    // Upper bounds on the farthest stored depth in complete 4x4 tiles.
    // Uncovered tiles stay at 1. Bounds may become stale only toward farther
    // depth as the map improves, which loses skips but cannot invent depth.
    std::array<double,TileCount> tileMax;
    std::array<uint8_t,TileCount> tileUncovered;
    bool TileCannotImprove(unsigned tx,unsigned ty,double lower,Stats &s)const{
        if(s.redundancyTests>=MaxRedundancyTests){s.redundancyBudgetStops=1;return false;}
        ++s.redundancyTests;
        return tileMax[ty*TilesPerRow+tx]<=lower;
    }
    bool MayImprove(double left,double right,double bottom,double top,double lower,Stats &s)const{
        const unsigned x0=unsigned(std::floor(left))/TileSize,y0=unsigned(std::floor(bottom))/TileSize;
        const unsigned x1=(unsigned(std::ceil(right))-1)/TileSize,y1=(unsigned(std::ceil(top))-1)/TileSize;
        for(unsigned ty=y0;ty<=y1;++ty)for(unsigned tx=x0;tx<=x1;++tx)
            if(!TileCannotImprove(tx,ty,lower,s))return true;
        return false;
    }
    void FirstCoverage(unsigned x,unsigned y){
        const unsigned tx=x/TileSize,ty=y/TileSize,tile=ty*TilesPerRow+tx;
        if(--tileUncovered[tile])return;
        double farthest=0;
        for(unsigned row=ty*TileSize;row<(ty+1)*TileSize;++row)
            for(unsigned col=tx*TileSize;col<(tx+1)*TileSize;++col)farthest=std::max(farthest,depth[row*Size+col]);
        tileMax[tile]=farthest;
    }
public:
    Grid(){Clear();}
    void Clear(){depth.fill(1);tileMax.fill(1);tileUncovered.fill(TileSize*TileSize);}
    unsigned Coverage()const {unsigned n=0;for(double d:depth)if(d<1)++n;return n;}
    // Inner-conservative rasterization: a cell is written only when the whole
    // cell lies inside one original surface polygon, including a silhouette margin.
    // Its stored depth is a far bound; coverage from separate partial triangles
    // is never combined to fill a cell or bridge a window/fence hole.
    double DepthAt(unsigned x,unsigned y)const{return x<Size&&y<Size?depth[y*Size+x]:1;}
    void Raster(Point a,Point b,Point c,Stats &s){
        Polygon polygon;polygon.count=3;polygon.vertices[0]=a;polygon.vertices[1]=b;polygon.vertices[2]=c;
        RasterPolygon(polygon,s);
    }
    void RasterPolygon(const Polygon &polygon,Stats &s,bool reuseDepth=true,bool reuseSpans=true){
        if(polygon.count<3||polygon.count>MaxClippedVertices)return;
        const auto *p=polygon.vertices;const unsigned count=polygon.count;
        double minX=Size,maxX=0,minY=Size,maxY=0,area=0,minDepth=1;
        for(unsigned i=0;i<count;++i){
            if(!Finite(p[i]))return;
            minDepth=std::min(minDepth,p[i].z);
            minX=std::min(minX,p[i].x);maxX=std::max(maxX,p[i].x);
            minY=std::min(minY,p[i].y);maxY=std::max(maxY,p[i].y);
            area+=p[i].x*p[(i+1)%count].y-p[(i+1)%count].x*p[i].y;
        }
        minX=std::max(0.0,minX);maxX=std::min(double(Size),maxX);
        minY=std::max(0.0,minY);maxY=std::min(double(Size),maxY);
        if(minX>=maxX||minY>=maxY||!std::isfinite(area)||std::abs(area)<1e-9)return;
        // Any full covered cell's far-depth value is at least this lower bound.
        // This check only discards redundant occluder work; it never writes depth.
        // Leave the depth guard out of this lower bound to allow for rounding.
        const double lowerDepth=minDepth;
        if(reuseDepth&&!MayImprove(minX,maxX,minY,maxY,lowerDepth,s)){++s.redundantEyes;return;}
        // Pick a stable original-surface depth plane; no depth comes from bounds.
        unsigned second=0,third=0;double planeArea=0;
        for(unsigned i=1;i+1<count;++i){
            const double test=(p[i].x-p[0].x)*(p[i+1].y-p[0].y)-(p[i+1].x-p[0].x)*(p[i].y-p[0].y);
            if(std::abs(test)>std::abs(planeArea)){planeArea=test;second=i;third=i+1;}
        }
        if(!std::isfinite(planeArea)||std::abs(planeArea)<1e-9)return;
        const auto a=p[0],b=p[second],c=p[third];
        const double dx=((b.z-a.z)*(c.y-a.y)-(c.z-a.z)*(b.y-a.y))/planeArea;
        const double dy=((b.x-a.x)*(c.z-a.z)-(c.x-a.x)*(b.z-a.z))/planeArea;
        if(!std::isfinite(dx)||!std::isfinite(dy))return;
        double depthError=0;
        for(unsigned i=0;i<count;++i)depthError=std::max(depthError,std::abs(a.z+dx*(p[i].x-a.x)+dy*(p[i].y-a.y)-p[i].z));
        if(depthError>1e-6)return;
        struct Edge {double ex,ey,constant,threshold,roundoff;};
        Edge edges[MaxClippedVertices];unsigned edgeCount=0;
        for(unsigned i=0;i<count;++i){
            const auto v=p[i],w=p[(i+1)%count];const double sign=area>0?1:-1;
            const double ex=(w.x-v.x)*sign,ey=(w.y-v.y)*sign;
            if(ex==0&&ey==0)continue;
            // Invalid/nonconvex input cannot become an occluding convex hull.
            for(unsigned j=0;j<count;++j)if(ex*(p[j].y-v.y)-ey*(p[j].x-v.x)<-1e-8)return;
            const double constant=-ex*v.y+ey*v.x;
            const double threshold=ScreenGuard*Size*.5*std::sqrt(ex*ex+ey*ey)+1e-9;
            const double roundoff=64*std::numeric_limits<double>::epsilon()*
                (std::abs(ex)*(Size+1)+std::abs(ey)*(Size+1)+std::abs(constant)+std::abs(threshold)+1);
            edges[edgeCount++]={ex,ey,constant,threshold,roundoff};
        }
        if(edgeCount<3)return;
        for(unsigned y=unsigned(std::floor(minY));y<unsigned(std::ceil(maxY));++y){
            if(s.rasterRows>=MaxRasterRows){++s.budget;return;}
            ++s.rasterRows;double left=std::floor(minX),right=std::ceil(maxX)-1;
            for(unsigned i=0;i<edgeCount;++i){
                if(s.edgeTests>=MaxRasterEdges){++s.budget;return;}++s.edgeTests;
                const auto &e=edges[i];
                const double constant=e.ex*(e.ex>=0?double(y):double(y+1))+e.constant-(e.ey>=0?e.ey:0);
                const double coefficient=-e.ey;
                if(coefficient>0)left=std::max(left,std::floor((e.threshold-constant)/coefficient)-1);
                else if(coefficient<0)right=std::min(right,std::ceil((e.threshold-constant)/coefficient)+1);
                else if(constant<=e.threshold){right=-1;break;}
            }
            if(left>right)continue;
            const bool spanCandidate=reuseSpans && right-left>=7;
            const unsigned spanLeft=spanCandidate?unsigned(left)+2:0;
            const unsigned spanRight=spanCandidate?unsigned(right)-2:0;
            bool certified=false,spanAttempted=false,fallbackReported=false;
            for(unsigned x=unsigned(left);x<=unsigned(right);){
                if(reuseDepth && (x==unsigned(left)||x%TileSize==0) && TileCannotImprove(x/TileSize,y/TileSize,lowerDepth,s)){
                    const unsigned end=std::min(unsigned(right)+1,(x/TileSize+1)*TileSize);
                    s.tileSkippedCells+=end-x;x=end;continue;
                }
                if(s.rasterTests>=MaxRasterTests){++s.budget;return;}++s.rasterTests;
                const double farDepth=a.z+dx*((dx>=0?double(x+1):double(x))-a.x)+dy*((dy>=0?double(y+1):double(y))-a.y)+DepthGuard+depthError;
                if(!std::isfinite(farDepth)||farDepth<=0||farDepth>=1){++x;continue;}
                auto &d=depth[y*Size+x];
                // Exact proposed depth can be checked before any cell-edge proof.
                // If it cannot improve the stored value, no coverage test is needed.
                if(reuseDepth && d<=farDepth){++s.depthSkippedCells;++x;continue;}
                // Certify lazily: rows already rejected by tile/cell depth
                // need no endpoint proof. Keep uncertain boundary cells on the
                // original path. Each cell-minimum edge value is affine in x;
                // safe endpoints plus rounding allowance prove the interior.
                if(spanCandidate && !spanAttempted && x>=spanLeft && x<=spanRight && spanRight-x>=2){
                    spanAttempted=true;
                    if(MaxRasterEdges-s.edgeTests>=2*edgeCount){
                        certified=true;
                        for(unsigned i=0;i<edgeCount;++i){
                            const auto &e=edges[i];
                            const double rowY=e.ex>=0?double(y):double(y+1);
                            const double l=e.ex*rowY-e.ey*(e.ey>=0?double(spanLeft+1):double(spanLeft))+e.constant;
                            const double r=e.ex*rowY-e.ey*(e.ey>=0?double(spanRight+1):double(spanRight))+e.constant;
                            s.edgeTests+=2;s.spanEdgeTests+=2;
                            if(!std::isfinite(l)||!std::isfinite(r)||!std::isfinite(e.roundoff)||
                               std::min(l,r)<=e.threshold+e.roundoff){certified=false;break;}
                        }
                        if(certified)++s.spanRows;
                    }
                }
                bool covered=true;
                if(certified && x>=spanLeft && x<=spanRight)++s.spanCells;
                else {
                    if(reuseSpans && !fallbackReported){++s.spanFallbackRows;fallbackReported=true;}
                    for(unsigned i=0;i<edgeCount;++i){
                        if(s.edgeTests>=MaxRasterEdges){++s.budget;return;}++s.edgeTests;
                        const auto &e=edges[i];
                        const double edge=e.ex*(e.ex>=0?double(y):double(y+1))-e.ey*(e.ey>=0?double(x+1):double(x))+e.constant;
                        if(edge<=e.threshold){covered=false;break;}
                    }
                }
                if(covered && farDepth<d){
                    const bool first=d>=1;d=farDepth;++s.cells;
                    if(first)FirstCoverage(x,y);
                }
                ++x;
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
struct Preview {
    std::array<uint8_t,Size*Size> eye[2]{};
    bool valid=false;
};
inline uint8_t PreviewValue(double depth){
    if(!std::isfinite(depth)||depth<=0||depth>=1)return 0;
    return uint8_t(1+254*std::sqrt(std::sqrt(1-depth)));
}
class Stereo {
public:
    Grid eye[2];
    void Clear(){eye[0].Clear();eye[1].Clear();}
    bool Hidden(const Box (&b)[2],Stats &s)const {return eye[0].Hidden(b[0],s)&&eye[1].Hidden(b[1],s);}
    Preview MakePreview(bool valid)const{
        Preview out;out.valid=valid;
        if(valid)for(unsigned e=0;e<2;++e)for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x)
            out.eye[e][y*Size+x]=PreviewValue(eye[e].DepthAt(x,y));
        return out;
    }
};
}
