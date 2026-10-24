#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../../overlay/src/renderer/QuestBillboard.h"

struct Vertex { float x, y, z, w, u, v; };
struct V { double x, y, z; };
static V sub(V a, V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static V add(V a, V b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static V scale(V a, double s) { return {a.x*s,a.y*s,a.z*s}; }
static double dot(V a, V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static double length(V a) { return std::sqrt(dot(a,a)); }
static V cross(V a, V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static void require(bool b, const char *message)
{
    if(!b){ std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static bool close(double a, double b) { return std::abs(a-b) < 0.0003*(1+std::abs(b)); }
// Independent inverse of the pixel-plane projection in rw_im2d.vert, with
// camera forward +Z and up +Y. Measures geometry, not expected vertex values.
static V unproject(Vertex v, float width, float height, float vx, float vy)
{
    return {(2*v.x/width-1)*vx*v.w,(1-2*v.y/height)*vy*v.w,v.w};
}
int main()
{
    // Hold the source and its texture fixed in world space while changing
    // the camera's yaw, pitch and roll. Compare UV-labelled corners in WORLD
    // space, so a head-locked card cannot pass by merely facing the viewer.
    int rollCases=0;
    for(bool pole : {false,true}){
        V reference[4] = {};
        bool first=true;
        for(double yaw : {0.0,-0.3,0.3})
        for(double pitch : {0.0,-0.3,0.3})
        for(double roll : {0.0,-1.2,-0.6,0.6,1.2,3.141592653589793}){
            if(pole && (yaw!=0 || pitch!=0)) continue;
            const V source=pole ? V{0,10,0} : V{4,2,10};
            const V r0=pole ? V{1,0,0} : V{std::cos(yaw),0,-std::sin(yaw)};
            const V u0=pole ? V{0,0,-1} : V{std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)};
            const V f=pole ? V{0,1,0} : V{std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
            const V r=add(scale(r0,std::cos(roll)),scale(u0,std::sin(roll)));
            const V u=sub(scale(u0,std::cos(roll)),scale(r0,std::sin(roll)));
            const float depth=float(dot(source,f));
            const float cx=500+500*float(dot(source,r))/depth;
            const float cy=500-500*float(dot(source,u))/depth;
            const float up[3]={float(r.y),float(u.y),float(f.y)};
            const float east[3]={float(r.x),float(u.x),float(f.x)};
            Vertex quad[4];
            const int sx[4]={-1,-1,1,1}, sy[4]={1,-1,-1,1};
            for(int i=0;i<4;++i){
                const float dx=0.4f*sx[i], dy=0.6f*sy[i];
                const float x=std::cos(0.7f)*dx-std::sin(0.7f)*dy;
                const float y=std::sin(0.7f)*dx+std::cos(0.7f)*dy;
                quad[i]={cx+x*500/depth,cy-y*500/depth,0.99f,depth,float(i&1),float(i>>1)};
            }
            QuestBillboard::FaceViewer(quad,4,cx,cy,1000,1000,1,1,up,east);
            for(int i=0;i<4;++i){
                const V p=unproject(quad[i],1000,1000,1,1);
                const V world=add(add(scale(r,p.x),scale(u,p.y)),scale(f,p.z));
                if(first) reference[i]=world;
                require(length(sub(world,reference[i]))<0.0001,"texture corner stays fixed in world space under head roll/yaw/pitch");
            }
            first=false;
            ++rollCases;
        }
    }
    std::printf("PASS: %d world-orientation cases including upside-down head roll and zenith fallback.\n",rollCases);
    int cases=0;
    for(float aspect : {0.9f, 1.0f, 1.777777f})
    for(float tangent : {0.7f, 1.2f})
    for(float depth : {1.2f, 2.1f, 20.0f, 100.0f})
    for(float yaw : {-65.0f,-35.0f,0.0f,35.0f,65.0f})
    for(float pitch : {-65.0f,-35.0f,0.0f,35.0f,65.0f})
    for(float rotation : {0.0f,0.6f,1.8f}){
        const float width=1800, height=width/aspect, vx=tangent, vy=vx/aspect;
        const float px=2*vx/width, py=2*vy/height;
        const float cx=width*0.5f+std::tan(yaw*0.01745329252f)/px;
        const float cy=height*0.5f-std::tan(pitch*0.01745329252f)/py;
        const float c=std::cos(rotation), s=std::sin(rotation);
        const float halfWidth=0.04f*depth, halfHeight=0.06f*depth;
        const int sx[4]={-1,-1,1,1}, sy[4]={1,-1,-1,1};
        Vertex quad[4];
        for(int i=0;i<4;++i){
            float x=halfWidth*sx[i], y=halfHeight*sy[i];
            quad[i]={cx+(c*x-s*y)/(px*depth),cy-(s*x+c*y)/(py*depth),
                0.99f,depth,float(i&1),float(i>>1)};
        }
        Vertex batch[6];
        const int indices[6]={0,1,2,3,0,2};
        for(int i=0;i<6;++i) batch[i]=quad[indices[i]];
        QuestBillboard::FaceViewer(quad,4,cx,cy,width,height,vx,vy);
        QuestBillboard::FaceViewer(batch,6,cx,cy,width,height,vx,vy);
        V p[4];
        for(int i=0;i<4;++i){
            p[i]=unproject(quad[i],width,height,vx,vy);
            require(quad[i].z<0,"world depth marker");
            require(quad[i].u==float(i&1) && quad[i].v==float(i>>1),"texture coordinates");
        }
        V centre={(p[0].x+p[2].x)*0.5,(p[0].y+p[2].y)*0.5,(p[0].z+p[2].z)*0.5};
        V target=unproject({cx,cy,0,depth,0,0},width,height,vx,vy);
        require(close(centre.x,target.x)&&close(centre.y,target.y)&&close(centre.z,target.z),"source centre anchored");
        V a=sub(p[1],p[0]), b=sub(p[3],p[0]), n=cross(a,b);
        require(close(length(a),2*halfHeight)&&close(length(b),2*halfWidth),"physical size preserved");
        require(std::abs(dot(a,b))/(length(a)*length(b))<0.0003,"quad remains rectangular");
        require(std::abs(dot(n,centre))/(length(n)*length(centre))>0.99999,"normal faces viewer along source ray");
        for(int i=0;i<6;++i){
            V delta=sub(unproject(batch[i],width,height,vx,vy),p[indices[i]]);
            require(length(delta)<0.0001,"buffered and direct draws agree");
        }
        ++cases;
    }
    Vertex invalid[4]={{0,0,0.9f,1,0,0},{0,0,0.9f,1,0,0},{0,0,0.9f,1,0,0},{0,0,0.9f,1,0,0}};
    QuestBillboard::FaceViewer(invalid,4,0,0,0,100,1,1);
    require(invalid[0].z==0.9f,"invalid viewport leaves input untouched");
    Vertex large[4]={{-500,-1500,0.9f,1.2f,0,0},{-500,2500,0.9f,1.2f,0,1},
        {3500,2500,0.9f,1.2f,1,1},{3500,-1500,0.9f,1.2f,1,0}};
    QuestBillboard::FaceViewer(large,4,1500,500,1000,1000,1,1);
    bool behind=false;
    for(auto vertex : large){
        require(vertex.z<0 && std::isfinite(vertex.x) && std::isfinite(vertex.y),"large quad remains finite");
        if(vertex.w<0) behind=true;
    }
    require(behind,"large near effects reach GPU near-plane clipping");
    std::printf("PASS: %d billboard geometry cases (view angles, roll, aspect, near/far depth, direct/buffered).\n",cases);
}
