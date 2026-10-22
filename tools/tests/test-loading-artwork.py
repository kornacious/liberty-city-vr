#!/usr/bin/env python3
"""Compile the assembled production DoFade against rendering adapters (no game data)."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage_root', type=pathlib.Path)
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
source = (args.stage_root / 'src/core/main.cpp').read_text()
im = (args.stage_root / 'vendor/librw/src/vulkan/vkim.cpp').read_text()
transform_start = im.index('if(shader == SHADER_IM2D && gImmediate2DFullscreen){')
transform_end = im.index('\n\t}', transform_start) + 3
transform = im[transform_start:transform_end]
fade = source[source.index('void\nDoFade(void)'):source.index('\nbool\nRwGrabScreen(')]
adapter = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
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
struct CRGBA { int r=0,g=0,b=0,a=0; };
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
namespace rw { namespace vulkan {
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
 static void DrawRect(const CRect&,const CRGBA&){}
 void Draw(const CRect& rect,const CRGBA& colour,const CRGBA&,const CRGBA&,const CRGBA&){
 assert(fullscreen && rect.left==0 && rect.top==0);
 assert(rect.right==SCREEN_WIDTH && rect.bottom==SCREEN_HEIGHT);
 assert(colour.r==255 && colour.g==255 && colour.b==255);
 ++splashDraws; lastAlpha=colour.a;
 }
};
CSprite2d* LoadSplash(void*){ static CSprite2d sprite; return &sprite; }
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
 for(float w : {1024.f,1832.f,2064.f,2560.f})
  for(float h : {1024.f,1920.f,2208.f}) CheckCoverage(w,h);
 for(bool splash : {false,true}) {
  TheCamera.m_FadeTargetIsSplashScreen=splash;
  for(int alpha=0;alpha<=255;++alpha){
   CDraw::FadeValue=alpha; splashDraws=solidDraws=0; lastAlpha=-1;
   // Letterboxing and reduced brightness must not shrink artwork coverage.
   TheCamera.m_WideScreenOn=true; CMenuManager::m_PrefsBrightness=180;
   DoFade();
   assert(!fullscreen);
   assert(splashDraws==(splash && alpha>0));
   assert(solidDraws==(!splash && alpha>0));
   if(alpha>0) assert(lastAlpha==alpha);
  }
 }
 puts("PASS: 512 production fade cases and 48 screen corners: artwork opacity, solid fades, state reset and full coverage");
}
'''
with tempfile.TemporaryDirectory(prefix='liberty-loading-') as tmp:
    cpp = pathlib.Path(tmp) / 'loading.cpp'
    exe = pathlib.Path(tmp) / 'loading-test'
    cpp.write_text(adapter + fade + matrix + checks)
    subprocess.run([args.cxx, '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
