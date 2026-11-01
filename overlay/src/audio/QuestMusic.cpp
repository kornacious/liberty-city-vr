#include "common.h"
#ifdef AUDIO_ANDROID
#include "QuestMusic.h"
#include "QuestMusicTimeline.h"
#include "QuestMusicPlaylist.h"
#include "platform_android.h"
#include "MusicManager.h"
#include "Timer.h"
#include <atomic>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace QuestMusic {
namespace {
struct Track { std::string name; uint64_t bytes; int64_t modified; uint32_t duration; std::string display; bool failed=false; };
struct Library { std::vector<Track> tracks; Timeline timeline; unsigned rejected=0; };
Library library, pending;
std::mutex publication;
std::thread scanner;
std::atomic<bool> scanning{false}, ready{false}, cancel{false};
bool active=false, userPaused=false, gamePaused=false;
size_t current=0;
unsigned failures=0;
std::string root;
char status[160]="NO CUSTOM MUSIC";
std::string title;
uint32_t lastSave=0, lastMarkerCheck=0;
int64_t rescanStamp=-1;

bool Extension(const std::string &name) {
    auto dot=name.find_last_of('.'); if(dot==std::string::npos) return false;
    std::string ext=name.substr(dot); for(char &c:ext) c=char(tolower((unsigned char)c));
    return ext==".mp3" || ext==".m4a" || ext==".ogg" || ext==".opus" || ext==".wav" || ext==".flac";
}
void Scan() {
    Library result;
    std::map<std::string, Track> cache;
    std::ifstream input(root+"/.index-v1");
    Track t;
    while(input >> std::quoted(t.name) >> t.bytes >> t.modified >> t.duration)
        cache[t.name]=t;
    std::map<std::string, Track> displays;
    std::ifstream display(root+"/.display-v1");
    while(display >> std::quoted(t.name) >> t.bytes >> t.modified >> std::quoted(t.display))
        if(t.name.size()<=200 && t.display.size()<=4096) displays[t.name]=t;
    std::vector<std::string> names;
    if(DIR *dir=opendir(root.c_str())) {
        while(auto e=readdir(dir)) {
            if(cancel) break;
            std::string name=e->d_name;
            if(name.empty() || name[0]=='.' || !Extension(name)) continue;
            // Bound enumeration too, including rejected files. Deterministic
            // selection is applied below for the supported library size.
            if(names.size()<10000) names.push_back(name); else ++result.rejected;
        }
        closedir(dir);
    }
    std::ifstream playlist(root+"/.playlist-v1"); names=OrderedNames(std::move(names),playlist);
    for(const auto &name:names) {
        if(cancel) break;
        struct stat st{}; std::string path=root+"/"+name;
        if(lstat(path.c_str(),&st) || !S_ISREG(st.st_mode) || st.st_size<=0 ||
           uint64_t(st.st_size)>MaxFileBytes || name.size()>200 || name.find('\n')!=std::string::npos) {
            ++result.rejected; continue;
        }
        if(result.tracks.size()>=MaxTracks) { ++result.rejected; continue; }
        uint32_t ms=0;
        auto old=cache.find(name);
        if(old!=cache.end() && old->second.bytes==uint64_t(st.st_size) && old->second.modified==st.st_mtime)
            ms=old->second.duration;
        else ms=platform::audioProbeDuration(path.c_str());
        if(!result.timeline.Add(ms)) { ++result.rejected; LOGW("Custom music rejected: %s (duration/format/limit)",name.c_str()); continue; }
        std::string caption;
        auto metadata=displays.find(name);
        if(metadata!=displays.end() && metadata->second.bytes==uint64_t(st.st_size) && metadata->second.modified==st.st_mtime) caption=metadata->second.display;
        else caption=platform::audioProbeTitle(path.c_str());
        if(caption.empty()) caption=name;
        result.tracks.push_back({name,uint64_t(st.st_size),int64_t(st.st_mtime),ms,caption,false});
    }
    if(!cancel) {
        std::ofstream out(root+"/.index-v1.tmp",std::ios::trunc);
        for(const auto &track:result.tracks)
            out<<std::quoted(track.name)<<' '<<track.bytes<<' '<<track.modified<<' '<<track.duration<<'\n';
        out.close();
        if(out) rename((root+"/.index-v1.tmp").c_str(),(root+"/.index-v1").c_str());
        std::lock_guard<std::mutex> lock(publication);
        pending=std::move(result); ready=true;
    }
    scanning=false;
}
void Save() {
    if(library.tracks.empty() || current>=library.tracks.size()) return;
    std::ofstream out(root+"/.resume-v1.tmp",std::ios::trunc);
    out<<std::quoted(library.tracks[current].name)<<' '<<platform::audioGetStreamPosition(0)<<'\n';
    out.close(); if(out) rename((root+"/.resume-v1.tmp").c_str(),(root+"/.resume-v1").c_str());
}
bool Load(uint32_t local) {
    if(library.tracks.empty()) return false;
    for(size_t tried=0; tried<library.tracks.size(); ++tried) {
        auto &t=library.tracks[current];
        if(!t.failed && platform::audioLoadStream(0,(root+"/"+t.name).c_str(),local,false,!userPaused && !gamePaused)) {
            title=t.display;
            active=true; return true;
        }
        t.failed=true; current=(current+1)%library.tracks.size(); local=0;
    }
    active=false; snprintf(status,sizeof(status),"NO PLAYABLE CUSTOM TRACKS"); return false;
}
}
void Initialise() { root=std::string(platform::gameDataRoot())+"/mp3"; mkdir(root.c_str(),0770); Rescan(); }
void Rescan() {
    if(scanning) return;
    if(scanner.joinable()) scanner.join();
    cancel=false; scanning=true; ready=false;
    snprintf(status,sizeof(status),"SCANNING MUSIC...");
    scanner=std::thread(Scan);
}
void Terminate() { if(active) Save(); active=false; cancel=true; if(scanner.joinable()) scanner.join(); ready=false; library={}; }
bool Available() { for(const auto &t:library.tracks) if(!t.failed) return true; return false; }
uint32_t Length() { return std::max(1u,library.timeline.Length()); }
bool Start(uint32_t position) {
    if(!Available()) return false;
    current=library.timeline.Locate(position); failures=0; return Load(position);
}
void Stop() { if(active) Save(); active=false; gamePaused=false; }
int Position() { return active ? int(library.timeline.Begin(current)+uint32_t(std::max(0,platform::audioGetStreamPosition(0)))) : 0; }
// Completion is owned by Service, not MusicManager's generic radio restart.
// Keep the logical station alive through track transitions and user pauses.
bool Playing() { return active; }
void Pause(bool paused) { gamePaused=paused; if(active) platform::audioPauseStream(0,gamePaused || userPaused); }
void Skip(int direction) {
    if(!active || !Available()) return;
    Save(); current=library.timeline.Step(current,direction,uint32_t(std::max(0,platform::audioGetStreamPosition(0))));
    failures=0; Load(0);
}
void TogglePause() { if(active) { userPaused=!userPaused; platform::audioPauseStream(0,userPaused || gamePaused); } }
bool UserPaused() { return userPaused; }
const char *Title() { return title.c_str(); }
const char *Status() { return status; }
void Service() {
    uint32_t clock=CTimer::GetTimeInMillisecondsPauseMode();
    if(clock-lastMarkerCheck>1000) {
        lastMarkerCheck=clock; struct stat st{};
        if(stat((root+"/.rescan").c_str(),&st)==0) {
            int64_t stamp=int64_t(st.st_mtim.tv_sec)*1000000000+st.st_mtim.tv_nsec;
            if(stamp!=rescanStamp && !scanning && !ready) { rescanStamp=stamp; Rescan(); }
        }
    }
    if(ready.exchange(false)) {
        std::string resumeName; uint32_t resumeMs=0;
        if(active) Save();
        bool restart=active;
        { std::lock_guard<std::mutex> lock(publication); library=std::move(pending); }
        current=0; active=false;
        std::ifstream resume(root+"/.resume-v1"); resume>>std::quoted(resumeName)>>resumeMs;
        for(size_t i=0;i<library.tracks.size();++i) if(library.tracks[i].name==resumeName) { current=i; break; }
        if(!library.tracks.empty()) resumeMs=std::min(resumeMs,library.tracks[current].duration-1);
        uint32_t position=library.tracks.empty()?0:library.timeline.Begin(current)+resumeMs;
        MusicManager.m_aTracks[USERTRACK].m_nLength=Length();
        MusicManager.m_aTracks[USERTRACK].m_nPosition=position;
        MusicManager.m_aTracks[USERTRACK].m_nLastPosCheckTimer=CTimer::GetTimeInMillisecondsPauseMode();
        snprintf(status,sizeof(status),"%zu TRACKS / %u REJECTED",library.tracks.size(),library.rejected);
        if(restart) { platform::audioStopStream(0); Load(resumeMs); }
    }
    if(!active || userPaused || gamePaused) return;
    if(!platform::audioIsStreamPlaying(0)) {
        // A prepare/runtime failure near the beginning is quarantined for this
        // scan. Natural completion advances without disabling the track.
        uint32_t pos=uint32_t(std::max(0,platform::audioGetStreamPosition(0)));
        if(pos+1500<library.tracks[current].duration) {
            library.tracks[current].failed=true;
            LOGW("Custom track decode failed: %s",library.tracks[current].name.c_str());
        }
        if(++failures>library.tracks.size() || !Available()) { active=false; platform::audioStopStream(0); snprintf(status,sizeof(status),"NO PLAYABLE CUSTOM TRACKS"); return; }
        current=(current+1)%library.tracks.size(); Load(0);
    } else {
        // Successful progress resets the retry budget; preparation reports
        // playing but has zero position and cannot reset it indefinitely.
        if(platform::audioGetStreamPosition(0)>1500) failures=0;
        uint32_t now=CTimer::GetTimeInMillisecondsPauseMode();
        if(now-lastSave>10000) { Save(); lastSave=now; }
    }
}
}
#endif
