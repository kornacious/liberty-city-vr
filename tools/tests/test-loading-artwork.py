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
#define SCREEN_WIDTH 1832.0f
#define SCREEN_HEIGHT 1920.0f
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
bool fullscreen=false;
int splashDraws=0, solidDraws=0, lastAlpha=0;
float displayAspect=16.f/9.f;
int textureWidth=512,textureHeight=256,barDraws=0;
struct RwRaster {};
struct RwTexture {};
struct RwTexDictionary {};
RwRaster* RwTextureGetRaster(RwTexture*){static RwRaster r;return &r;}
int RwRasterGetWidth(RwRaster*){return textureWidth;}
int RwRasterGetHeight(RwRaster*){return textureHeight;}
namespace rw { namespace vulkan {
float getImmediate2DDisplayAspect(){return displayAspect;}
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
  if(fullscreen){
   assert(rect.left==0 && rect.top==0 && rect.right==SCREEN_WIDTH && rect.bottom==SCREEN_HEIGHT);
   assert(colour.r==0 && colour.g==0 && colour.b==0);
   assert(colour.a==CDraw::FadeValue);++barDraws;
  }
 }
 void Draw(const CRect& rect,const CRGBA& colour,const CRGBA&,const CRGBA&,const CRGBA&){
 assert(fullscreen && rect.left>=0 && rect.top>=0);
 assert(rect.right<=SCREEN_WIDTH && rect.bottom<=SCREEN_HEIGHT);
 float w=rect.right-rect.left,h=rect.bottom-rect.top;
 float nativeAspect=textureWidth>0 && textureHeight>0 ? float(textureWidth)/textureHeight : 2.f;
 assert(std::abs((w/SCREEN_WIDTH)/(h/SCREEN_HEIGHT)*displayAspect-nativeAspect)<0.00001f);
 assert(rect.left<0.0001f || rect.top<0.0001f); // largest fit, no unnecessary margin
 assert(std::abs(rect.left+rect.right-SCREEN_WIDTH)<0.001f);
 assert(std::abs(rect.top+rect.bottom-SCREEN_HEIGHT)<0.001f);
 assert(colour.r==255 && colour.g==255 && colour.b==255);
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
struct { float model[16]; } push;
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
 for(const char* name : {"loadsc0","mainsc1","mainsc2","splash2","splash3","loadsc24"})
  assert(LoadSplash(name)==&splash);
 assert(textureLoads==1); // different requests retain the same cached cyan texture
 assert(LoadSplash(nullptr)==&splash && textureLoads==1);
 for(float w : {1024.f,1832.f,2064.f,2560.f})
  for(float h : {1024.f,1920.f,2208.f}) CheckCoverage(w,h);
 for(float aspect : {16.f/9.f,1832.f/1920.f,1.f,4.f/3.f,2.4f})
 for(int shape : {0,1,2,3}) {
 displayAspect=aspect;
 textureWidth=shape==0 ? 512 : shape==1 ? 256 : shape==2 ? 1024 : 0;
 textureHeight=shape==0 ? 256 : shape==1 ? 512 : shape==2 ? 1024 : 0;
 for(bool isSplash : {false,true}) {
  TheCamera.m_FadeTargetIsSplashScreen=isSplash;
  for(int alpha=0;alpha<=255;++alpha){
   CDraw::FadeValue=alpha; splashDraws=solidDraws=barDraws=0; lastAlpha=-1;
   // Cutscene bars and reduced brightness cannot distort the fitted image.
   TheCamera.m_WideScreenOn=true; CMenuManager::m_PrefsBrightness=180;
   DoFade();
   assert(!fullscreen);
   assert(splashDraws==(isSplash && alpha>0));
   assert(barDraws==splashDraws);
   assert(solidDraws==(!isSplash && alpha>0));
   if(alpha>0) assert(lastAlpha==alpha);
  }
 }
 }
 puts("PASS: 10240 production fade cases, 48 screen corners and cyan texture selection/cache: native aspect, maximal fit, no cropping, black bars, opacity and state reset");
}
'''
with tempfile.TemporaryDirectory(prefix='liberty-loading-') as tmp:
    cpp = pathlib.Path(tmp) / 'loading.cpp'
    exe = pathlib.Path(tmp) / 'loading-test'
    cpp.write_text(adapter + loader + helper + fade + matrix + checks)
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
