"""Compile production car-window gates and vehicle menu visibility without game data."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
driving = (root / 'overlay/src/vr/QuestDrivingVR.cpp').read_text()
menu = (root / 'overlay/src/skel/android/vrdebug.cpp').read_text()
pipe = (root / 'librw/src/vulkan/vkpipe.cpp').read_text()

def block(text, marker):
    start = text.index(marker)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def function(text, signature):
    return block(text, signature) + '\n'

fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#define nil nullptr
using bool32 = int;
enum { VR_DRIVING_DEFAULT, VR_DRIVING_IMMERSIVE, VR_DRIVING_MOTION, GS_PLAYING_GAME };
struct CPlayerPed { bool dead=false; bool DyingOrDead(){return dead;} } player;
struct CVehicle { bool car=true; CPlayerPed *pDriver=&player; bool IsCar(){return car;} bool IsVehicle(){return true;} } car, other;
static CVehicle *active=&car;
static bool thirdPerson=false, gVrFirstPersonActive=true;
static int drivingType=VR_DRIVING_IMMERSIVE, gGameState=GS_PLAYING_GAME;
struct Frontend { bool m_bGameNotLoaded=false,m_bWantToRestart=false,m_bWantToLoad=false; } FrontEndMenuManager;
struct CGame { static inline bool playingIntro=false; };
struct CCutsceneMgr { static inline bool running=false,processing=false; static bool IsRunning(){return running;} static bool IsCutsceneProcessing(){return processing;} };
struct Camera { bool m_WideScreenOn=false; } TheCamera;
static void LoadDrivingSettings(){}
static CVehicle *FindPlayerVehicle(){return active;}
static CPlayerPed *FindPlayerPed(){return &player;}
static CVehicle *GetActivePlayerCar(){return active && active->IsCar()?active:nil;}
static bool IsQuestVehicleThirdPerson(){return thirdPerson;}
static int GetDrivingTypeForVehicle(CVehicle*){return drivingType;}
static bool gImmersiveCarWindowsVisible=true;
static int saved=-1;
static void SaveSetting(const char *key,int value){assert(!strcmp(key,"ImmersiveCarWindowsVisible"));saved=value;}
static int gVrVehicleSubpage=0,kind=0;
static bool seat=true,defaultSeat=false;
namespace OculusVR {
int GetQuestVehicleKind(){return kind;}
bool IsQuestVehicleKindThirdPerson(){return thirdPerson;}
bool IsQuestVehicleKindDrivingDefault(){return drivingType==VR_DRIVING_DEFAULT;}
const char *GetQuestVehicleKindDrivingTypeName(){return drivingType==VR_DRIVING_IMMERSIVE?"IMMERSIVE":"OTHER";}
bool HasQuestDefaultVehicleViewOffsetTarget(){return defaultSeat;}
bool HasQuestVehicleSeatCalibrationTarget(){return seat;}
}
'''
production = ''.join(function(driving, signature) for signature in [
    'IsDrivingEnvironmentActive()', 'IsImmersiveEnvironmentActive()',
    'IsImmersiveCarActive(CVehicle *expected = nil)',
    'bool AreQuestImmersiveCarWindowsVisible()',
    'void ToggleQuestImmersiveCarWindowsVisible()',
    'bool ShouldHideQuestCarWindows(CVehicle *vehicle)'])
# The first three definitions put their return types on the preceding line.
for signature in ['IsDrivingEnvironmentActive()', 'IsImmersiveEnvironmentActive()', 'IsImmersiveCarActive(CVehicle *expected = nil)']:
    production = production.replace(signature, 'bool '+signature, 1)
production += block(menu, 'enum eVrVehicleMenuItem') + ';\n'
vehicle_visibility = block(menu, 'if(page == VR_MENU_PAGE_VEHICLE)')
production += 'bool visible(int item){int page=0; const int VR_MENU_PAGE_VEHICLE=0;'+vehicle_visibility+'return false;}\n'
production += function(pipe, 'bool32 setHideVehicleWindows(bool32 enabled)')
production += function(pipe, 'bool32 getHideVehicleWindows()')
# Execute the exact mesh rejection condition used before Vulkan submission.
mesh_guard = re.search(r'if\(hideVehicleWindows && material && material->color.alpha < 255\)\s*continue;', pipe)
assert mesh_guard
production += 'struct Material { struct {int alpha;} color; }; bool meshVisible(Material *material){for(int i=0;i<1;++i){'+mesh_guard[0]+'return true;}return false;}\n'
fixture += 'static bool32 hideVehicleWindows;\n'

if len(sys.argv) > 1:
    stage = Path(sys.argv[1])
    renderer = (stage / 'src/renderer/Renderer.cpp').read_text()
    visibility = (stage / 'src/rw/VisibilityPlugins.cpp').read_text()
    begin = re.search(r'const bool32 savedHideWindows = rw::vulkan::setHideVehicleWindows\([\s\S]*?;', renderer)[0]
    end = re.search(r'rw::vulkan::setHideVehicleWindows\(savedHideWindows\);', renderer)[0]
    windscreen = re.search(r'if\(isWindscreen && rw::vulkan::getHideVehicleWindows\(\)\)\s*continue;', visibility)[0]
    production += 'namespace OculusVR {using ::ShouldHideQuestCarWindows;} namespace rw {namespace vulkan {using ::setHideVehicleWindows;using ::getHideVehicleWindows;}}\n'
    production += 'static bool glassDrawn,bodyDrawn,windscreenDrawn;void render(CVehicle *e){'+begin+'Material glass{{96}},body{{255}};glassDrawn=meshVisible(&glass);bodyDrawn=meshVisible(&body);windscreenDrawn=false;for(int i=0;i<1;++i){bool isWindscreen=true;'+windscreen+'windscreenDrawn=true;}'+end+'}\n'
    scope_checks = r"""
 thirdPerson=false;drivingType=VR_DRIVING_IMMERSIVE;ToggleQuestImmersiveCarWindowsVisible();
 render(&car);assert(!glassDrawn && bodyDrawn && !windscreenDrawn && !getHideVehicleWindows());
 render(&other);assert(glassDrawn && bodyDrawn && windscreenDrawn && !getHideVehicleWindows());
 thirdPerson=true;render(&car);assert(glassDrawn && bodyDrawn && windscreenDrawn);
 """
else:
    scope_checks = ''

checks = r'''
int main(){
 assert(AreQuestImmersiveCarWindowsVisible());
 assert(!ShouldHideQuestCarWindows(&car));
 ToggleQuestImmersiveCarWindowsVisible(); assert(saved==0);
 assert(ShouldHideQuestCarWindows(&car));
 assert(!ShouldHideQuestCarWindows(&other)); assert(!ShouldHideQuestCarWindows(nullptr));
 thirdPerson=true; assert(!ShouldHideQuestCarWindows(&car)); thirdPerson=false;
 for(int mode:{VR_DRIVING_DEFAULT,VR_DRIVING_MOTION}){drivingType=mode;assert(!ShouldHideQuestCarWindows(&car));}
 drivingType=VR_DRIVING_IMMERSIVE;
 gVrFirstPersonActive=false; assert(!ShouldHideQuestCarWindows(&car)); gVrFirstPersonActive=true;
 CCutsceneMgr::running=true; assert(!ShouldHideQuestCarWindows(&car)); CCutsceneMgr::running=false;
 player.dead=true; assert(!ShouldHideQuestCarWindows(&car)); player.dead=false;
 car.pDriver=nullptr; assert(!ShouldHideQuestCarWindows(&car)); car.pDriver=&player;
 car.car=false; assert(!ShouldHideQuestCarWindows(&car)); car.car=true;
 active=nullptr; assert(!ShouldHideQuestCarWindows(&car)); active=&car;
 ToggleQuestImmersiveCarWindowsVisible(); assert(saved==1 && !ShouldHideQuestCarWindows(&car));
 // Every kind, mode, view and seat context must fit on both pages.
 int contexts=0,maxRows=0;
 for(kind=0;kind<3;++kind) for(drivingType=0;drivingType<3;++drivingType)
 for(int third=0;third<2;++third) for(int seating=0;seating<3;++seating){
  thirdPerson=third;seat=seating!=0;defaultSeat=seating==2;
  for(gVrVehicleSubpage=0;gVrVehicleSubpage<2;++gVrVehicleSubpage){
   int rows=0;
   for(int item=0;item<VR_VEHICLE_ITEM_COUNT;++item)if(visible(item))++rows;
   assert(rows>2);assert(158+(rows-1)*32+18<694);
   if(rows>maxRows)maxRows=rows;
   assert(visible(VR_VEHICLE_PAGE)&&visible(VR_VEHICLE_BACK)&&visible(VR_VEHICLE_KIND));
   assert(visible(VR_VEHICLE_WINDOWS_VISIBLE)==(gVrVehicleSubpage==0 && kind==0 && !third && drivingType==VR_DRIVING_IMMERSIVE));
   ++contexts;
  }
 }
 Material glass{{96}},body{{255}};
 assert(meshVisible(&glass)&&meshVisible(&body));
 assert(setHideVehicleWindows(1)==0);assert(getHideVehicleWindows());
 assert(!meshVisible(&glass)&&meshVisible(&body)&&meshVisible(nullptr));
 for(int alpha=0;alpha<255;++alpha){glass.color.alpha=alpha;assert(!meshVisible(&glass));}
 assert(setHideVehicleWindows(0)==1);assert(meshVisible(&glass));
 printf("Car windows: immersive/third-person/motion gates, persistence toggle, glass/body separation and %d menu contexts passed (maximum %d rows).\n",contexts,maxRows);
}
'''
# initializer_list is used by the mode boundary cases.
checks = checks.replace(' Material glass{{96}},body{{255}};', scope_checks+' Material glass{{96}},body{{255}};')
fixture = '#include <initializer_list>\n' + fixture
with tempfile.TemporaryDirectory(prefix='lcvr-car-windows-') as temp:
    temp=Path(temp); cpp=temp/'test.cpp';cpp.write_text(fixture+production+checks)
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
