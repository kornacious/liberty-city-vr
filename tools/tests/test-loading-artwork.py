#!/usr/bin/env python3
"""Exercise production splash selection, layout and fades (no game data)."""
import argparse
import os
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage_root', type=pathlib.Path)
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
source = (args.stage_root / 'src/core/main.cpp').read_text()
helper = source[source.index('static void\nDrawQuestLoadingArtwork('):source.index('\nvoid\nDoFade(void)')]
# Drop the Android guard's trailing #endif; the adapter defines that platform.
helper = helper[:helper.rindex('#endif')]
loader = source[source.index('CSprite2d*\nLoadSplash('):source.index('\nvoid\nDestroySplashScreen(')]
im = (args.stage_root / 'vendor/librw/src/vulkan/vkim.cpp').read_text()
transform_start = im.index('if(shader == SHADER_IM2D && gImmediate2DFullscreen){')
transform_end = im.index('\n\t}', transform_start) + 3
transform = im[transform_start:transform_end]
vertex = (args.stage_root / 'vendor/librw/src/vulkan/shaders/rw_im2d.vert').read_text()
projection = vertex[vertex.index('\tgl_Position = scene.viewProj'):vertex.index('\tfragColour =')]
# GLSL swizzle syntax has no C++ equivalent; keep the projection/branch logic.
projection = projection.replace('plane.xy', 'vec2(plane.x, plane.y)')
plane_header = (args.stage_root / 'vendor/librw/src/vulkan/vkloading.h').read_text().replace('#pragma once', '')
fade = source[source.index('void\nDoFade(void)'):source.index('\nbool\nRwGrabScreen(')]
adapter = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#define RW_VULKAN
#define __ANDROID__
#define FIX_BUGS
#define nil nullptr
float screenWidth=1832.f,screenHeight=1920.f;
#define SCREEN_WIDTH screenWidth
#define SCREEN_HEIGHT screenHeight
#define SCREEN_SCALE_Y(x) (x)
#define FADE_IN 1
#define FADE_OUT 0
using std::min;
#define Min std::min
struct CRGBA { int r=0,g=0,b=0,a=0;
 CRGBA()=default; CRGBA(int r,int g,int b,int a):r(r),g(g),b(b),a(a){} };
struct CRect { float left=0,right=0,top=0,bottom=0;
 CRect()=default; CRect(float l,float t,float r,float b):left(l),right(r),top(t),bottom(b){} };
struct CTimer { static bool GetIsPaused(){return false;} static unsigned GetTimeInMilliseconds(){return 0;} };
bool JustLoadedDontFadeInYet=false, StillToFadeOut=false;
unsigned TimeStartedCountingForFade=0, TimeToStayFadedBeforeFadeOut=0;
struct CDraw { inline static int FadeValue=0,FadeRed=255,FadeGreen=255,FadeBlue=255; };
struct CMenuManager { inline static int m_PrefsBrightness=256; };
struct { bool m_bMenuActive=false; } FrontEndMenuManager;
struct { bool m_FadeTargetIsSplashScreen=false,m_WideScreenOn=false;
 float m_ScreenReductionPercentage=0;
 void Fade(float,int){} void ProcessFade(){} void ProcessMusicFade(){} void SetFadeColour(int,int,int){}
} TheCamera;
bool fullscreen=false,loadingArtwork=false;
int splashDraws=0, solidDraws=0, lastAlpha=0;
float displayAspect=16.f/9.f;
int textureWidth=512,textureHeight=256,barDraws=0;
using RwUInt32=unsigned;
enum {rwRENDERSTATESRCBLEND,rwRENDERSTATEDESTBLEND,rwRENDERSTATEVERTEXALPHAENABLE};
enum {rwBLENDSRCALPHA=1,rwBLENDINVSRCALPHA=2,rwBLENDONE=3};
#define TRUE 1
RwUInt32 renderStates[]={rwBLENDSRCALPHA,rwBLENDINVSRCALPHA,0};
void RwRenderStateGet(int state,RwUInt32* value){*value=renderStates[state];}
void RwRenderStateSet(int state,void* value){renderStates[state]=(RwUInt32)(size_t)value;}
float backdropPixel=0.6f,artworkPixel=0.6f;
float Blend(float colour,float destination,int alpha){
 if(!renderStates[rwRENDERSTATEVERTEXALPHAENABLE])return colour;
 float a=alpha/255.f;
 float src=renderStates[rwRENDERSTATESRCBLEND]==rwBLENDONE?1.f:a;
 float dst=renderStates[rwRENDERSTATEDESTBLEND]==rwBLENDONE?1.f:1.f-a;
 return colour*src+destination*dst;
}
struct RwRaster {};
struct RwTexture {};
struct RwTexDictionary {};
RwRaster* RwTextureGetRaster(RwTexture*){static RwRaster r;return &r;}
int RwRasterGetWidth(RwRaster*){return textureWidth;}
int RwRasterGetHeight(RwRaster*){return textureHeight;}
namespace rw { namespace vulkan {
void setImmediate2DLoadingArtwork(bool enable){
 assert(!fullscreen);loadingArtwork=enable;
}
void setScreenFade(int r,int g,int b,int a){
 assert(r==CDraw::FadeRed && g==CDraw::FadeGreen && b==CDraw::FadeBlue);
 ++solidDraws; lastAlpha=a;
}
void setImmediate2DFullscreen(bool enable,float w,float h){
 if(enable) assert(w==SCREEN_WIDTH && h==SCREEN_HEIGHT);
 fullscreen=enable;
}
} }
struct CSprite2d {
 RwTexture *m_pTexture=nullptr;
 void Delete(){m_pTexture=nullptr;}
 void SetTexture(const char* name){
  assert(std::strcmp(name,"splash1")==0);static RwTexture t;m_pTexture=&t;
 }
 static void DrawRect(const CRect& rect,const CRGBA& colour){
  // Production DrawRect selects vertex-alpha blending from its opacity.
  renderStates[rwRENDERSTATEVERTEXALPHAENABLE]=colour.a!=255;
  if(fullscreen){
   assert(rect.left==0 && rect.top==0 && rect.right==SCREEN_WIDTH && rect.bottom==SCREEN_HEIGHT);
   assert(colour.r==0 && colour.g==0 && colour.b==0);
   assert(colour.a==CDraw::FadeValue);++barDraws;
   backdropPixel=Blend(0.f,backdropPixel,colour.a);
   artworkPixel=Blend(0.f,artworkPixel,colour.a);
  }
 }
 void Draw(const CRect& rect,const CRGBA& colour,const CRGBA&,const CRGBA&,const CRGBA&){
 assert(loadingArtwork && !fullscreen && rect.left>=0 && rect.top>=0);
 assert(rect.right<=SCREEN_WIDTH && rect.bottom<=SCREEN_HEIGHT);
 float w=rect.right-rect.left,h=rect.bottom-rect.top;
 float nativeAspect=textureWidth>0 && textureHeight>0 ? float(textureWidth)/textureHeight : 2.f;
 assert(std::abs((w/SCREEN_WIDTH)/(h/SCREEN_HEIGHT)*displayAspect-nativeAspect)<0.00001f);
 assert(rect.left<0.0001f || rect.top<0.0001f); // largest fit, no unnecessary margin
 assert(std::abs(rect.left+rect.right-SCREEN_WIDTH)<0.001f);
 assert(std::abs(rect.top+rect.bottom-SCREEN_HEIGHT)<0.001f);
 assert(colour.r==255 && colour.g==255 && colour.b==255);
 artworkPixel=Blend(0.25f,artworkPixel,colour.a);
 float a=colour.a/255.f;
 assert(std::abs(backdropPixel-0.6f*(1.f-a))<0.00001f);
 assert(std::abs(artworkPixel-(0.25f*a+0.6f*(1.f-a)))<0.00001f);
 ++splashDraws; lastAlpha=colour.a;
 }
};
CSprite2d splash;
int splashTxdId=-1,textureLoads=0;
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary*,const char* name){
 assert(std::strcmp(name,"splash1")==0);return splash.m_pTexture;
}
struct CTxdStore {
 struct Slot { RwTexDictionary *texDict=nullptr; };
 static int AddTxdSlot(const char*){return 0;}
 static Slot* GetSlot(int){static Slot s;return &s;}
 static void RemoveTxd(int){GetSlot(0)->texDict=nullptr;}
 static void LoadTxd(int,const char* file){
  assert(std::strcmp(file,"splash1.txd")==0);++textureLoads;
  static RwTexDictionary t;GetSlot(0)->texDict=&t;
 }
 static void AddRef(int){} static void PushCurrentTxd(){}
 static void SetCurrentTxd(int){} static void PopCurrentTxd(){}
};
struct CFileMgr {static void SetDir(const char*){}};
CSprite2d* LoadSplash(const char*);
'''
matrix = r'''
#include <cstring>
struct vec2 {float x,y;vec2(float x,float y):x(x),y(y){}};
struct vec3 {float x,y,z;};
struct vec4 {
 float x=0,y=0,z=0,w=0;
 vec4()=default;
 vec4(vec3 v,float w):x(v.x),y(v.y),z(v.z),w(w){}
 vec4(vec2 v,float z,float w):x(v.x),y(v.y),z(z),w(w){}
};
struct Matrix {
 float m[16]={};
 vec4 operator*(const vec4& v)const{
  vec4 r;float *p=&r.x;const float *q=&v.x;
  for(int row=0;row<4;row++)for(int col=0;col<4;col++)p[row]+=m[col*4+row]*q[col];
  return r;
 }
};
struct { float model[16]; vec4 surfaceProps; } push;
struct {Matrix viewProj[2];} scene;
vec4 gl_Position;
int gl_ViewIndex;
float max(float a,double b){return std::max(a,float(b));}
void ProjectArtwork(vec3 world,vec4 plane){
 vec4 inPosition;inPosition.z=0.25f;
''' + projection + r'''
}
void CheckStereo(){
 int cases=0;
 for(float yaw : {-2.f,-1.f,0.f,1.f,2.f})
 for(float roll : {-0.4f,0.f,0.4f})
 for(float translation : {-0.4f,0.f,0.4f}) {
  float c=std::cos(yaw),s=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
  // Orthonormal head basis: yaw followed by roll. X/Y are shared by both eyes.
  float basis[3][3]={{c*cr,sr,-s*cr},{-c*sr,cr,s*sr},{s,0,c}};
  for(float width : {1024.f,1832.f,2560.f}) {
   float height=1920.f,source[16]={},output[16];source[15]=1;
   for(int i=0;i<3;i++) {
    source[i]=basis[0][i]*4.f/width;
    source[4+i]=basis[1][i]*-3.f/height;
    source[12+i]=(i==0?translation:0.f)-basis[2][i]*2.f-
      source[i]*width*0.5f-source[4+i]*height*0.5f;
   }
   rw::vulkan::buildLoadingScreenPlane(output,source,width,height,false);
   float xLen=0,yLen=0;
   for(int i=0;i<3;i++){
    xLen+=output[i]*output[i];yLen+=output[4+i]*output[4+i];
    float centre=output[12+i]+output[i]*width*0.5f+output[4+i]*height*0.5f;
    assert(std::abs(centre-((i==0?translation:0.f)-basis[2][i]*2.f))<0.00001f);
   }
   assert(std::abs(std::sqrt(xLen)*width-2.56f)<0.00001f);
   assert(std::abs(std::sqrt(yLen)*height-1.44f)<0.00001f);
   float theater[16];rw::vulkan::buildLoadingScreenPlane(theater,source,width,height,true);
   assert(std::memcmp(theater,source,sizeof(source))==0);
   for(float u : {0.f,0.5f,1.f})for(float v : {0.f,0.5f,1.f}){
    vec3 world;
    float *wp=&world.x;
    for(int i=0;i<3;i++)wp[i]=output[12+i]+output[i]*width*u+output[4+i]*height*v;
    float firstNdc=0;
    for(int eye=0;eye<2;eye++){
     const float eyeX=eye==0?-0.032f:0.032f;
     const float offset=eye==0?-0.18f:0.18f;
     Matrix &m=scene.viewProj[eye];std::memset(m.m,0,sizeof(m.m));
     for(int i=0;i<3;i++){
      m.m[i*4]=basis[0][i]+offset*basis[2][i];
      m.m[i*4+1]=-basis[1][i];
      m.m[i*4+3]=-basis[2][i];
     }
     m.m[12]=-translation*(basis[0][0]+offset*basis[2][0])-eyeX;
     m.m[13]=translation*basis[1][0];m.m[15]=translation*basis[2][0];
     gl_ViewIndex=eye;push.surfaceProps.x=3;push.surfaceProps.z=0;
     vec4 plane(world,1.f);ProjectArtwork(world,plane);
     assert(std::abs(gl_Position.w-2.f)<0.00001f);
     float ndc=gl_Position.x/gl_Position.w;
     float reconstructedX=ndc*2.f+2.f*offset+eyeX;
     assert(std::abs(reconstructedX-(u-0.5f)*2.56f)<0.00001f);
     assert(std::abs(gl_Position.y/gl_Position.w-(v-0.5f)*0.72f)<0.00001f);
     assert(std::abs(gl_Position.z/gl_Position.w-0.00001f)<0.000001f);
     assert(std::abs(ndc)<=1.f); // both eye frustums contain the whole plane
     if(eye==0)firstNdc=ndc;else assert(std::abs(ndc-firstNdc)>0.1f);
     ++cases;
    }
   }
  }
 }
 printf("PASS: %d stereo projections: shared plane, asymmetric eyes, yaw/roll/translation, stable size, no clipping and overlay depth\n",cases);
}
void identity(float* m){std::memset(m,0,16*sizeof(float));m[0]=m[5]=m[10]=m[15]=1;}
void CheckCoverage(float width,float height){
 const int SHADER_IM2D=1, shader=1;
 const bool gImmediate2DFullscreen=true;
 const float gFullscreenWidth=width,gFullscreenHeight=height;
''' + transform + r'''
 for(int x : {0,1}) for(int y : {0,1}) {
  const float clipX=push.model[0]*x*width+push.model[4]*y*height+push.model[12];
  const float clipY=push.model[1]*x*width+push.model[5]*y*height+push.model[13];
  assert(std::abs(clipX-(2*x-1))<0.00001f);
  assert(std::abs(clipY-(2*y-1))<0.00001f);
 }
}
'''
checks = r''' 
int main(){
 CheckStereo();
 for(const char* name : {"loadsc0","mainsc1","mainsc2","splash2","splash3","loadsc24"})
  assert(LoadSplash(name)==&splash);
 assert(textureLoads==1); // different requests retain the same cached cyan texture
 assert(LoadSplash(nullptr)==&splash && textureLoads==1);
 for(float w : {1024.f,1832.f,2064.f,2560.f})
  for(float h : {1024.f,1920.f,2208.f}) CheckCoverage(w,h);
 for(float width : {1024.f,1832.f,2064.f,2560.f,3200.f})
 for(int shape : {0,1,2,3}) {
 screenWidth=width;
 textureWidth=shape==0 ? 512 : shape==1 ? 256 : shape==2 ? 1024 : 0;
 textureHeight=shape==0 ? 256 : shape==1 ? 512 : shape==2 ? 1024 : 0;
 for(bool isSplash : {false,true}) {
  TheCamera.m_FadeTargetIsSplashScreen=isSplash;
  for(int alpha=0;alpha<=255;++alpha){
   CDraw::FadeValue=alpha; splashDraws=solidDraws=barDraws=0; lastAlpha=-1;
   backdropPixel=artworkPixel=0.6f;
   RwUInt32 oldSrc=shape&1?rwBLENDONE:rwBLENDSRCALPHA;
   RwUInt32 oldDst=shape&2?rwBLENDONE:rwBLENDINVSRCALPHA;
   renderStates[0]=oldSrc;renderStates[1]=oldDst;renderStates[2]=0;
   // Cutscene bars and reduced brightness cannot distort the fitted image.
   TheCamera.m_WideScreenOn=true; CMenuManager::m_PrefsBrightness=180;
   DoFade();
   assert(!fullscreen && !loadingArtwork);
   assert(splashDraws==(isSplash && alpha>0));
   assert(barDraws==splashDraws);
   assert(renderStates[0]==oldSrc && renderStates[1]==oldDst);
   if(isSplash && alpha>0)assert(renderStates[2]==1);
   assert(solidDraws==(!isSplash && alpha>0));
   if(alpha>0) assert(lastAlpha==alpha);
  }
 }
 }
 puts("PASS: 10240 production fade cases, 48 screen corners and cyan texture selection/cache: native aspect, no cropping, artwork/background group opacity, blend-state restoration and scope reset");
}
'''
with tempfile.TemporaryDirectory(prefix='liberty-loading-') as tmp:
    cpp = pathlib.Path(tmp) / 'loading.cpp'
    exe = pathlib.Path(tmp) / 'loading-test'
    cpp.write_text(plane_header + adapter + loader + helper + fade + matrix + checks)
    if args.cxx == 'msvc':
        vswhere = pathlib.Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vc = max((pathlib.Path(install) / 'VC/Tools/MSVC').iterdir(), key=lambda p: tuple(map(int, p.name.split('.'))))
        sdk = pathlib.Path(os.environ['ProgramFiles(x86)']) / 'Windows Kits/10'
        version = max((sdk / 'Include').iterdir(), key=lambda p: tuple(map(int, p.name.split('.')))).name
        env = os.environ.copy()
        env['INCLUDE'] = ';'.join(map(str, [vc/'include', sdk/'Include'/version/'ucrt', sdk/'Include'/version/'um', sdk/'Include'/version/'shared']))
        env['LIB'] = ';'.join(map(str, [vc/'lib/x64', sdk/'Lib'/version/'ucrt/x64', sdk/'Lib'/version/'um/x64']))
        exe = exe.with_suffix('.exe')
        subprocess.run([str(vc/'bin/Hostx64/x64/cl.exe'), '/nologo', '/EHsc', '/std:c++17', '/MT', '/O2', str(cpp), '/Fo'+str(cpp.with_suffix('.obj')), '/Fe'+str(exe)], env=env, check=True)
    else:
        subprocess.run([args.cxx, '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
