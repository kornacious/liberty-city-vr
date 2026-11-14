#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
using float32 = float;
using uint32 = uint32_t;
#define nil nullptr
#define FORLIST(link, list) for(auto link : list)
struct V3d { float x,y,z; };
struct RGBAf { float red=0,green=0,blue=0,alpha=1; };
static void clamp(RGBAf*) {}
struct Frame { V3d at; Frame *getLTM(){return this;} };
struct Light {
    enum { AMBIENT, DIRECTIONAL, LIGHTATOMICS=1 };
    int type=DIRECTIONAL,flags=LIGHTATOMICS;
    Frame frame{{0.6f,0,-0.8f}};
    RGBAf color{0.5f,0.5f,0.5f,1};
    int getType(){return type;} int getFlags(){return flags;}
    Frame *getFrame(){return &frame;}
    static Light *fromWorld(Light *l){return l;}
};
struct Geometry { enum {LIGHT=1,NORMALS=2}; int flags=0; };
struct Atomic { Geometry *geometry; };
struct WorldLights { Light **directionals; int numDirectionals; RGBAf ambient; };
struct World {
    std::vector<Light*> globalLights;
    void enumerateLights(Atomic *a, WorldLights *out){
        out->numDirectionals=0;out->ambient={0.25f,0.25f,0.25f,1};
        for(auto l:globalLights)
            if((a->geometry->flags&Geometry::NORMALS) && l->type==Light::DIRECTIONAL && (l->flags&Light::LIGHTATOMICS))
                out->directionals[out->numDirectionals++]=l;
    }
};
static struct { World *currentWorld; } engineState, *engine=&engineState;
static struct { float ambientLight[4],lightDirColour[4]; } push;
static struct { bool fogEnabled=false; } gstate;
static struct { bool neoEnabled=true; float worldToPlay[16]={}; } gvk;
static void setup(Geometry *geometry){
    Atomic a{geometry};Atomic *atomic=&a;
#include "neo-gloss-production.inc"
}
static void near(float a,float b){
    if(std::fabs(a-b)>=0.00001f){
        std::fprintf(stderr,"Expected %.6f, got %.6f\n",b,a);std::exit(1);
    }
}
int main(){
    Light ambient;ambient.type=Light::AMBIENT;
    Light excluded;excluded.flags=0;excluded.frame.at={0,1,0};
    // Match LightsCreate: ambient, primary sun, then extra directionals.
    Light sun;World world{{&ambient,&sun,&excluded}};engine->currentWorld=&world;
    // Z-up world to Y-up play space, then rotate body yaw. The light and
    // road-to-eye vector must rotate together: the gloss stays world anchored.
    float baseline=-1;
    const int cases[]={0,Geometry::NORMALS,Geometry::LIGHT,Geometry::LIGHT|Geometry::NORMALS};
    // RenderRoads/RenderFadingInEntities call DeActivateDirectional, which
    // clears the sun's LIGHTATOMICS flag without changing its world direction.
    const int sunStates[]={Light::LIGHTATOMICS,0};
    for(int sunFlags:sunStates){
    sun.flags=sunFlags;
    for(int flags:cases){
        Geometry geometry{flags};
        for(int angle=0;angle<360;angle+=30){
            float yaw=angle*3.14159265f/180.0f, c=std::cos(yaw),s=std::sin(yaw);
            auto m=gvk.worldToPlay;std::memset(m,0,16*sizeof(float));
            m[0]=c;m[2]=s;m[4]=-s;m[6]=c;m[9]=1;m[15]=1;
            setup(&geometry);
            near(push.lightDirColour[0],0.6f*c);near(push.lightDirColour[1],-0.8f);
            near(push.lightDirColour[2],0.6f*s);
            // An oblique road-to-eye vector, rotated by the same body basis.
            // A fixed play-space light makes this highlight change with yaw.
            float hx=-0.4f*c+0.2f*s-push.lightDirColour[0];
            float hy=std::sqrt(0.8f)-push.lightDirColour[1];
            float hz=-0.4f*s-0.2f*c-push.lightDirColour[2];
            float highlight=std::pow(hy/std::sqrt(hx*hx+hy*hy+hz*hz),8.0f);
            if(baseline<0)baseline=highlight;near(highlight,baseline);
            if(!(flags&Geometry::LIGHT)){
                near(push.ambientLight[0],0);near(push.lightDirColour[3],0);
            }else{
                near(push.ambientLight[0],0.25f);
                if((flags&Geometry::NORMALS) && sunFlags) assert(push.lightDirColour[3]!=0);
                else near(push.lightDirColour[3],0);
            }
        }
    }
    }
    Geometry road;
    gvk.neoEnabled=false;setup(&road);near(push.lightDirColour[2],1);near(push.lightDirColour[1],0);
    gvk.neoEnabled=true;world.globalLights={&ambient};
    setup(&road);near(push.lightDirColour[2],1);near(push.lightDirColour[1],0);
    engine->currentWorld=nil;setup(&road);near(push.lightDirColour[2],1);
    std::puts("PASS Neo gloss: disabled road-pass sun stays world anchored across body yaw; diffuse lighting and PC assets preserved");
}
