"""Exercise the assembled production horn path with synthetic PC/Xbox banks."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

source = Path(sys.argv[1]) / 'src/audio/AudioLogic.cpp'
text = source.read_text()
function = re.search(r'bool8\s+cAudioManager::ProcessVehicleHorn\(.*?\n}', text, re.S)[0]
rows = re.findall(r'\{SFX_\w+, SFX_BANK_\w+, (SFX_CAR_HORN_\w+), (\d+),', text)
assert rows, 'Missing production vehicle settings'
names = list(dict.fromkeys(name for name, _ in rows))
fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#define AUDIO_ANDROID
#define TRUE true
#define FALSE false
#define SQR(x) ((x)*(x))
#define SET_EMITTING_VOLUME(x)
#define SET_LOOP_OFFSETS(x)
#define SET_SOUND_REFLECTION(x)
using uint32=uint32_t; using uint64=uint64_t; using bool8=bool;
template<class T> T Min(T a,T b){return std::min(a,b);}
enum { SFX_BANK_0, MI_MRWHOOP=999, STATUS_PLAYER=1,
       VEHICLE_HORN_MAX_DIST=40, VEHICLE_HORN_VOLUME=80 };
struct Vehicle {
 bool m_bSirenOrAlarm=false; int m_nCarHornTimer=1,m_nCarHornPattern=0;
 int GetModelIndex(){return 0;} int GetStatus(){return STATUS_PLAYER;}
};
struct cVehicleParams {
 float m_fDistance=1; bool m_bDistanceCalculated=true;
 Vehicle *m_pVehicle; int m_nIndex=0;
};
bool HornPattern[8][44]={};
struct {
 uint32 rate=11025;
 uint32 GetSampleBaseFrequency(uint32){return rate;}
} SampleManager;
struct cAudioManager {
 struct {
  int m_nEntityIndex=0,m_nVolume=0,m_nCounter=0,m_nSampleIndex=0,m_nBankIndex=0,m_nPriority=0;
  uint32 m_nFrequency=0,m_nLoopCount=0;
  bool m_bIs2D=false,m_bStatic=false,m_bReverb=false;
  float m_fDistance=1,m_fSpeedMultiplier=0,m_MaxDistance=0;
  int m_nFramesToPlay=0;
 } m_sQueueSample;
 int m_FrameCounter=0, queued=0;
 bool UsesSirenSwitching(int){return false;}
 void CalculateDistance(bool&,float&){}
 int ComputeVolume(int v,int,float){return v;}
 void AddSampleToRequestedQueue(){++queued;}
 bool8 ProcessVehicleHorn(cVehicleParams&);
};
'''
fixture += 'enum {' + ','.join(names) + '};\n'
fixture += 'struct {int m_nHornSample; uint32 m_nHornFrequency;} aVehicleSettings[] = {\n'
fixture += ',\n'.join('{' + name + ',' + rate + '}' for name, rate in rows) + '};\n'
checks = r'''
int main(){
 Vehicle car; cVehicleParams params; params.m_pVehicle=&car;
 cAudioManager audio; int count=0;
 for(uint32 rate : {11025u,32000u}) {
  SampleManager.rate=rate;
  for(unsigned i=0;i<sizeof(aVehicleSettings)/sizeof(aVehicleSettings[0]);++i){
   params.m_nIndex=i;
   auto setting=aVehicleSettings[i]; int queued=audio.queued;
   assert(audio.ProcessVehicleHorn(params)); assert(audio.queued==queued+1);
   uint32 expected=setting.m_nHornFrequency;
   if(setting.m_nHornSample==SFX_CAR_HORN_PORSCHE)
    expected=uint64(expected)*rate/11025;
   assert(audio.m_sQueueSample.m_nFrequency==expected);
   // Preserve per-car tuning, including lower/higher Porsche variants.
   if(setting.m_nHornSample==SFX_CAR_HORN_PORSCHE)
    assert(std::abs(double(expected)/rate-double(setting.m_nHornFrequency)/11025)<1.0/rate);
   ++count;
  }
 }
 car.m_nCarHornTimer=0; int queued=audio.queued;
 assert(audio.ProcessVehicleHorn(params)); assert(audio.queued==queued);
 printf("Horn pitch: %d production vehicle/bank combinations passed.\n",count);
}
'''
with tempfile.TemporaryDirectory(prefix='lcvr-horn-pitch-') as temp:
    temp = Path(temp)
    cpp = temp / 'test.cpp'
    cpp.write_text('#include <cmath>\n' + fixture + function + checks)
    env = os.environ.copy()
    if os.name == 'nt':
        if not shutil.which('cl'):
            vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
            output = subprocess.check_output(f'"{vcvars}" >nul && set', shell=True, text=True)
            for line in output.splitlines():
                key, sep, value = line.partition('=')
                if sep and key:
                    for existing in list(env):
                        if existing.lower() == key.lower(): del env[existing]
                    env[key] = value
        exe = temp / 'test.exe'
        compiler = shutil.which('cl', path=next(v for k, v in env.items() if k.lower() == 'path'))
        command = [compiler, '/nologo', '/std:c++17', '/EHsc', str(cpp), '/Fe:' + str(exe)]
    else:
        exe = temp / 'test'
        command = [os.environ.get('CXX', 'c++'), '-std=c++17', str(cpp), '-o', str(exe)]
    subprocess.run(command, cwd=temp, env=env, check=True)
    subprocess.run([str(exe)], check=True)
