#!/usr/bin/env python3
"""Compile and exercise the production music service with a fake media backend.
Only generated text fixtures are used; no game assets or headset are needed.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='radio-test-') as d:
    work = Path(d)
    (work/'common.h').write_text('#pragma once\n#include <cstdint>\n#include <cstdio>\n')
    (work/'platform_android.h').write_text('''#pragma once
#include <cstdint>
#define LOGW(...) ((void)0)
namespace platform {
const char *gameDataRoot(); uint32_t audioProbeDuration(const char*);
bool audioLoadStream(int,const char*,int,bool,bool);
int audioGetStreamPosition(int); bool audioIsStreamPlaying(int);
void audioPauseStream(int,bool); void audioStopStream(int);
}
''')
    (work/'MusicManager.h').write_text('''#pragma once
#include <cstdint>
constexpr int USERTRACK=9;
struct FakeMusicManager { struct Track { uint32_t m_nLength=0,m_nPosition=0,m_nLastPosCheckTimer=0; } m_aTracks[20]; };
extern FakeMusicManager MusicManager;
''')
    (work/'Timer.h').write_text('''#pragma once
#include <cstdint>
extern uint32_t clockMs;
struct CTimer { static uint32_t GetTimeInMillisecondsPauseMode() { return clockMs; } };
''')
    (work/'main.cpp').write_text(r'''
#include "QuestMusic.h"
#include "MusicManager.h"
#include "platform_android.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <filesystem>
FakeMusicManager MusicManager;
uint32_t clockMs=0;
std::string root,path; int position=0; bool playing=false, paused=false;
std::atomic<int> probes{0};
namespace platform {
const char *gameDataRoot() { return root.c_str(); }
uint32_t audioProbeDuration(const char *p) { ++probes; std::ifstream f(p); uint32_t n=0; f>>n; return n; }
bool audioLoadStream(int,const char *p,int pos,bool loop,bool start) { assert(!loop); path=p; position=pos; playing=start; return true; }
int audioGetStreamPosition(int) { return position; }
bool audioIsStreamPlaying(int) { return playing; }
void audioPauseStream(int,bool p) { paused=p; playing=!p; }
void audioStopStream(int) { playing=false; }
}
void waitScan() {
    for(int i=0;i<1000 && std::string(QuestMusic::Status()).find("SCANNING")!=std::string::npos;++i) {
        QuestMusic::Service(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(std::string(QuestMusic::Status()).find("SCANNING")==std::string::npos);
}
int main(int argc,char **argv) {
    root=argv[1]; std::filesystem::create_directories(root+"/mp3");
    std::ofstream(root+"/mp3/a.mp3")<<"10000";
    std::ofstream(root+"/mp3/b.m4a")<<"20000";
    std::ofstream(root+"/mp3/bad.ogg")<<"CORRUPT";
    QuestMusic::Initialise(); waitScan();
    assert(QuestMusic::Available() && QuestMusic::Length()==30000);
    assert(MusicManager.m_aTracks[USERTRACK].m_nLength==30000);
    assert(std::string(QuestMusic::Status())=="2 TRACKS / 1 REJECTED");
    assert(QuestMusic::Start(12000)); assert(path.find("b.m4a")!=std::string::npos && position==2000);
    assert(QuestMusic::Position()==12000);
    position=4000; QuestMusic::Skip(-1); assert(path.find("b.m4a")!=std::string::npos && position==0);
    QuestMusic::Skip(-1); assert(path.find("a.mp3")!=std::string::npos);
    QuestMusic::TogglePause(); assert(paused && QuestMusic::Playing());
    QuestMusic::Pause(true); QuestMusic::Pause(false); assert(paused); // user pause survives game resume
    QuestMusic::TogglePause(); assert(!paused && QuestMusic::Playing());
    // Completion is advanced by the service regardless of MusicManager order.
    playing=false; position=10000; assert(QuestMusic::Playing()); QuestMusic::Service();
    assert(path.find("b.m4a")!=std::string::npos && position==0);
    position=6500; QuestMusic::Stop(); platform::audioStopStream(0);
    int before=probes; QuestMusic::Rescan(); waitScan();
    assert(probes==before+1); // only the previously rejected file is re-probed
    auto resume=MusicManager.m_aTracks[USERTRACK].m_nPosition;
    assert(resume==16500); assert(QuestMusic::Start(resume) && position==6500);
    // Preparation/runtime failure is quarantined and skipped, not retried forever.
    playing=false; position=0; QuestMusic::Service(); assert(path.find("a.mp3")!=std::string::npos);
    playing=false; position=0; QuestMusic::Service(); assert(!QuestMusic::Available() && !QuestMusic::Playing());
    QuestMusic::Terminate();
    // A scan can be cancelled during shutdown without publishing into dead state.
    QuestMusic::Initialise(); QuestMusic::Terminate();
    puts("Production custom-music service: scan/cache, resume, skips, pause, completion and bounded failure recovery passed.");
}
''')
    exe=work/'test'
    subprocess.run(['c++','-std=c++17','-pthread','-DAUDIO_ANDROID','-I'+str(work),'-I'+str(root/'overlay/src/audio'),str(work/'main.cpp'),str(root/'overlay/src/audio/QuestMusic.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(work/'generated-music')],check=True)
