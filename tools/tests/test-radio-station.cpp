#include <cassert>
#include <cstdint>
#include <cstdio>
#define nil nullptr
#define TRUE true
#define FALSE false
#define FIX_BUGS
#define RADIO_SCROLL_TO_PREV_STATION
using uint8=uint8_t;
using uint32=uint32_t;
using int32=int32_t;
using bool8=bool;
constexpr int HEAD_RADIO=0, USERTRACK=9, POLICE_RADIO=10, RADIO_OFF=11;
constexpr int NUM_RADIOS=10, TOTAL_STREAMED_SOUNDS=20;
constexpr int STREAMED_SOUND_CITY_AMBIENT=11, STREAMED_SOUND_WATER_AMBIENT=12;
constexpr int SOUND_FRONTEND_RADIO_CHANGE=0;
struct CVehicle { int m_nRadioStation=0; bool police=false; } vehicle;
bool inCar=true, hasVehicle=true, available=true, bRadioOff=false;
int32 gNumRetunePresses=0, gRetuneCounter=0;
CVehicle *FindPlayerVehicle() { return hasVehicle ? &vehicle : nullptr; }
namespace QuestMusic { bool Available() { return available; } }
namespace QuestRadio { CVehicle *Vehicle() { return FindPlayerVehicle(); } }
struct Timer { static uint32 GetTimeInMillisecondsPauseMode() { return 100; } };
using CTimer=Timer;
struct Samples {
    int stream=0, starts=0, stops=0;
    uint32 position=1234;
    bool IsMP3RadioChannelAvailable() { return available; }
    uint32 GetStreamedFilePosition() { return position; }
    void SetStreamedVolumeAndPan(int,int,bool) {}
    void StopStreamedFile() { stream=-1; ++stops; }
    bool IsStreamPlaying() { return stream>=0; }
    bool StartStreamedFile(int track,uint32) { stream=track; ++starts; return true; }
} SampleManager;
struct Audio {
    int m_nFrontEndEntity=0;
    void PlayOneShot(int,int,float) {}
    void DoPoliceRadioCrackle() {}
    bool ShouldDuckMissionAudio() { return false; }
} AudioManager;
struct cMusicManager {
    bool m_bIsInitialised=true, m_bSetNextStation=false;
    int m_nRadioInCar=0, m_nNextTrack=0, m_nPlayingTrack=0;
    struct Track { uint32 m_nPosition=0, m_nLastPosCheckTimer=0; } m_aTracks[TOTAL_STREAMED_SOUNDS];
    bool PlayerInCar() { return inCar; }
    bool UsesPoliceRadio(CVehicle *v) { return v->police; }
    uint32 GetTrackStartPos(int track) { return m_aTracks[track].m_nPosition; }
    void SetRadioInCar(uint32);
    uint8 GetNextCarTuning();
    bool ChangeRadioChannel();
    void ServiceRetune();
} MusicManager;
// Upstream tuning stores integer arithmetic back into byte-sized station IDs.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244)
#endif
#include "radio-station-production.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

void Reset(int station) {
    MusicManager=cMusicManager(); vehicle=CVehicle(); SampleManager=Samples();
    vehicle.m_nRadioStation=station;
    MusicManager.m_nNextTrack=MusicManager.m_nPlayingTrack=SampleManager.stream=station;
    gNumRetunePresses=gRetuneCounter=0; bRadioOff=false;
    available=hasVehicle=inCar=true;
}
void Tick() {
    MusicManager.ServiceRetune();
    if(MusicManager.m_nNextTrack<RADIO_OFF) assert(MusicManager.ChangeRadioChannel());
    // Radio Off selects the native ambience branch; it must never start USERTRACK/police.
    else assert(bRadioOff);
    MusicManager.m_nPlayingTrack=MusicManager.m_nNextTrack;
}
int main() {
    Reset(0); QuestRadio::Station(1);
    assert(vehicle.m_nRadioStation==1 && SampleManager.stream==0);
    Tick(); assert(SampleManager.stream==1 && SampleManager.starts==1 && SampleManager.stops==1);
    assert(MusicManager.m_aTracks[0].m_nPosition==1234);
    Tick(); assert(SampleManager.starts==1); // no repeated restart
    QuestRadio::Station(-1); Tick(); assert(SampleManager.stream==0);
    Reset(8); QuestRadio::Station(1); Tick(); assert(SampleManager.stream==USERTRACK);
    QuestRadio::Station(-1); Tick(); assert(SampleManager.stream==8);
    Reset(9); QuestRadio::Station(1); Tick(); assert(vehicle.m_nRadioStation==RADIO_OFF);
    Reset(RADIO_OFF); QuestRadio::Station(1); Tick(); assert(SampleManager.stream==HEAD_RADIO);
    Reset(0); QuestRadio::Station(-1); Tick(); assert(vehicle.m_nRadioStation==RADIO_OFF);
    Reset(RADIO_OFF); available=false; QuestRadio::Station(-1); Tick(); assert(SampleManager.stream==8);
    Reset(8); available=false; QuestRadio::Station(1); Tick(); assert(vehicle.m_nRadioStation==RADIO_OFF);
    Reset(2); gNumRetunePresses=1; gRetuneCounter=30;
    QuestRadio::Station(1); Tick(); assert(SampleManager.stream==3);
    assert(gNumRetunePresses==0 && gRetuneCounter==0);
    Reset(2); QuestRadio::Station(1); QuestRadio::Station(1); QuestRadio::Station(-1);
    Tick(); assert(SampleManager.stream==3);
    Reset(2); vehicle.police=true; QuestRadio::Station(1); assert(!MusicManager.m_bSetNextStation);
    hasVehicle=false; QuestRadio::Station(1); assert(!MusicManager.m_bSetNextStation);
    puts("Production radio station selection, retune dispatch and stream changes passed.");
}
