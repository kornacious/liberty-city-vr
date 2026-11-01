"""Compile production cutscene camera discovery/selection with synthetic actors."""
from pathlib import Path
import os, shutil, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'overlay/src/skel/android/android.cpp').read_text()
start = source.index('static RpHAnimHierarchy*\nGetPosedHeadHierarchy(')
end = source.index('// A small per-model calibration table.', start)
production = source[start:end]
render_start = source.index('\t\tCVector forward = TheCamera.Cams[TheCamera.ActiveCam].Front;', source.index('}else if(cinematicCutscene){'))
render_end = source.index('\t\tif(usingSelectedActorHead){', render_start)
render = source[render_start:render_end]
fixture = r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#define nil nullptr
#define NUMCUTSCENEOBJECTS 8
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
template<class T> T Min(T a,T b){return a<b?a:b;}
struct CVector { float x=0,y=0,z=0; CVector()=default; CVector(float a,float b,float c):x(a),y(b),z(c){} float MagnitudeSqr(){return x*x+y*y+z*z;} void Normalise(){float n=std::sqrt(MagnitudeSqr()); x/=n;y/=n;z/=n;} };
struct RwMatrix { CVector pos,up{0,1,0}; };
struct RwFrame { RwMatrix matrix; };
struct RpHAnimHierarchy { bool hasHead=true,hasMatrices=true; RwMatrix matrix; };
struct AnimBlendFrameData { RwFrame *frame=nullptr; };
struct RpClump { int type=1; bool skin=false,initialized=true; RpHAnimHierarchy *hier=nullptr; AnimBlendFrameData *head=nullptr; };
constexpr int rpCLUMP=1,BONE_head=2;
int RwObjectGetType(RpClump *c){return c->type;}
bool IsClumpSkinned(RpClump *c){return c->skin;}
RpHAnimHierarchy *GetAnimHierarchyFromSkinClump(RpClump *c){return c->hier;}
int RpHAnimIDGetIndex(RpHAnimHierarchy *h,int){return h->hasHead?0:-1;}
RwMatrix *RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy *h){return h->hasMatrices?&h->matrix:nullptr;}
bool RpAnimBlendClumpIsInitialized(RpClump *c){return c->initialized;}
AnimBlendFrameData *RpAnimBlendClumpFindFrame(RpClump *c,const char *name){assert(!std::strcmp(name,"Shead"));return c->head;}
RwMatrix *RwFrameGetLTM(RwFrame *f){return &f->matrix;}
CVector *RwMatrixGetPos(RwMatrix *m){return &m->pos;}
CVector *RwMatrixGetUp(RwMatrix *m){return &m->up;}
struct CVehicle;
struct CEntity { RpClump *m_rwObject=nullptr; CVector GetForward(){return {1,0,0};} };
struct CPlayerPed: CEntity { bool bInVehicle=false; CVehicle *m_pMyVehicle=nullptr; };
struct CVehicle { CEntity *pDriver=nullptr,*pPassengers[2]={}; };
using CPed=CEntity;
static CPlayerPed player;
CPlayerPed *FindPlayerPed(){return &player;}
struct CCutsceneMgr { static inline CEntity *objects[8]={}; static inline int count=0; static inline bool running=true,processing=false; static int GetNumCutsceneObjects(){return count;} static CEntity *GetCutsceneObject(int i){return objects[i];} static bool IsRunning(){return running;} static bool IsCutsceneProcessing(){return processing;} };
static CEntity *gCutsceneCameraActor=nullptr;
namespace androidgame { int VrCutsceneCameraCount(); static int camera=0; int VrCutsceneCamera(){return camera;} }
struct Camera { struct Cam {CVector Front{0,1,0};} Cams[1]; int ActiveCam=0; CVector GetPosition(){return {10,20,30};} } TheCamera;
'''
checks = r'''
static CEntity *renderCamera(){
RENDER
return usingSelectedActorHead ? gCutsceneCameraActor : nullptr;
}
int main(){
 RwFrame head; head.matrix.pos={1,2,3};
 AnimBlendFrameData frame{&head}; RpClump original; original.head=&frame;
 CEntity claude,other,prop; claude.m_rwObject=&original;
 RpHAnimHierarchy hierarchy; hierarchy.matrix.pos={4,5,6};
 RpClump skinned; skinned.skin=true; skinned.hier=&hierarchy; other.m_rwObject=&skinned;
 RpClump noHead; prop.m_rwObject=&noHead;
 player.m_rwObject=&original;
 CCutsceneMgr::objects[0]=&claude; CCutsceneMgr::objects[1]=&prop; CCutsceneMgr::objects[2]=&other; CCutsceneMgr::count=3;
 assert(androidgame::VrCutsceneCameraCount()==3);
 CVector position,direction;
 assert(GetCutsceneActorHead(1,&position,&direction)); assert(gCutsceneCameraActor==&claude); assert(position.x==1 && position.z==3);
 assert(GetCutsceneActorHead(2,&position,&direction)); assert(gCutsceneCameraActor==&other); assert(position.x==4);
 androidgame::camera=2; assert(renderCamera()==&other);
 androidgame::camera=1; assert(renderCamera()==&claude);
 androidgame::camera=0; assert(renderCamera()==nullptr); assert(gCutsceneCameraActor==nullptr);
 assert(!GetCutsceneActorHead(9,&position,&direction)); assert(!GetCutsceneActorHead(-1,&position,&direction));
 original.initialized=false; assert(androidgame::VrCutsceneCameraCount()==2); original.initialized=true;
 hierarchy.hasHead=false; assert(androidgame::VrCutsceneCameraCount()==2); hierarchy.hasHead=true;
 hierarchy.hasMatrices=false; assert(androidgame::VrCutsceneCameraCount()==2); hierarchy.hasMatrices=true;
 original.head=nullptr; assert(!HasAnimatedHead(&claude)); original.head=&frame;
 assert(!HasAnimatedHead(nullptr)); CEntity unloaded; assert(!HasAnimatedHead(&unloaded));
 head.matrix.up={0,0,1}; assert(GetAnimatedPedHeadPose(&claude,&position,&direction)); assert(direction.x==1 && direction.z==0);
 CCutsceneMgr::count=0; assert(androidgame::VrCutsceneCameraCount()==1); // Never use the parked player in formal scenes.
 CCutsceneMgr::running=false; CCutsceneMgr::processing=true; assert(androidgame::VrCutsceneCameraCount()==1);
 CCutsceneMgr::processing=false; assert(androidgame::VrCutsceneCameraCount()==2);
 CVehicle car; car.pDriver=&player; car.pPassengers[0]=&other; player.bInVehicle=true;player.m_pMyVehicle=&car;
 assert(androidgame::VrCutsceneCameraCount()==3); // Player is not duplicated as driver.
 puts("Cutscene cameras: original/skinned heads, selection, director, streaming and world actors passed.");
}
'''.replace('RENDER',render)
with tempfile.TemporaryDirectory(prefix='lcvr-cutscene-') as temp:
 temp=Path(temp); cpp=temp/'test.cpp'; cpp.write_text(fixture+production+checks)
 env=os.environ.copy()
 if os.name=='nt':
  if not shutil.which('cl'):
   vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
   install=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
   vcvars=Path(install)/'VC/Auxiliary/Build/vcvars64.bat'
   output=subprocess.check_output(f'"{vcvars}" >nul && set',shell=True,text=True)
   for line in output.splitlines():
    key,sep,value=line.partition('=')
    if sep and key:
     for existing in list(env):
      if existing.lower()==key.lower(): del env[existing]
     env[key]=value
  exe=temp/'test.exe'
  compiler=shutil.which('cl',path=next(v for k,v in env.items() if k.lower()=='path'))
  command=[compiler,'/nologo','/std:c++17','/EHsc',str(cpp),'/Fe:'+str(exe)]
 else:
  exe=temp/'test';command=[os.environ.get('CXX','c++'),'-std=c++17',str(cpp),'-o',str(exe)]
 subprocess.run(command,cwd=temp,env=env,check=True)
 subprocess.run([str(exe)],check=True)
