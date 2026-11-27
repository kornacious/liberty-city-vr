#pragma once

#include "RuntimeLog.h"
#include <stdint.h>
#include <string>

#define MIAMIVR_LOG_TAG "LibertyVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  MIAMIVR_LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  MIAMIVR_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MIAMIVR_LOG_TAG, __VA_ARGS__)

struct ANativeActivity;

namespace platform {

// Absolute path of the app-specific external directory, e.g.
// /sdcard/Android/data/com.libertyvr.quest/files. Readable and writable with no
// runtime permission, which is where the original GTA III data is staged and
// where re3.ini / vr_settings.ini live on this platform.
// Pipes stdout and stderr into logcat. The game reports through printf all
// over -- CdStream traces, casepath failures, TXD load errors -- and none of
// it is visible on Android otherwise.
void redirectStdioToLog(void);

void setStorageRoot(const char *path);
void setNativeActivity(ANativeActivity *activity);

// Stall diagnostics.
//
// printf tracing is not usable here: it is slow enough to change the timing of
// what it is meant to observe, and a hang that appears with tracing removed and
// disappears with it added cannot be chased that way. A checkpoint is a single
// relaxed pointer store, cheap enough to leave in a hot path, and the watchdog
// only says anything when the same checkpoint has been current for too long.
void setCheckpoint(const char *label);
void startStallWatchdog(void);
const char *storageRoot(void);

// <storageRoot>/gamedata -- the original game directory contents.
const char *gameDataRoot(void);
#ifdef LIBERTYVR_PORTABLE_ENGINE
const char *configurationRoot(void);
#else
inline const char *configurationRoot(void) { return gameDataRoot(); }
#endif

// Joins a path under gameDataRoot() into a caller-supplied buffer, normalising
// the backslashes that the game source uses throughout ("DATA\\GTA_VC.DAT").
// Returns buffer.
char *resolveGamePath(char *buffer, size_t bufferSize, const char *relative);

// Case-insensitive resolution against the real on-device filenames. The game
// asks for "DATA\\GTA_VC.DAT" and "models/gta3.img" interchangeably, and ext4
// is case sensitive, so a direct open would fail for most of the data set.
// Returns false when nothing matches.
bool resolveGamePathCaseInsensitive(char *buffer, size_t bufferSize, const char *relative);

// Android MediaPlayer bridge used by the streamed-audio half of the native
// audio backend.  SFX are mixed through AAudio; long MP3/ADF tracks stay in the
// platform decoder so radio and cutscenes do not consume game-thread time.
uint32_t audioProbeDuration(const char *absolutePath);
std::string audioProbeTitle(const char *absolutePath);
bool audioLoadStream(int stream, const char *absolutePath, int positionMs,
	bool loop, bool autoStart);
void audioPlayStream(int stream);
void audioPauseStream(int stream, bool pause);
void audioStopStream(int stream);
void audioSetStreamVolume(int stream, float left, float right);
void audioSetStreamLoop(int stream, bool loop);
int audioGetStreamPosition(int stream);
int audioGetStreamDuration(int stream);
bool audioIsStreamPlaying(int stream);

} // namespace platform

// ---------------------------------------------------------------------------
// Win32 profile-API shim.
//
// The VR layer stores every setting and all per-weapon calibration through
// GetPrivateProfileIntA / WritePrivateProfileStringA (47 and 6 call sites in
// OpenXRVR.cpp alone). Reimplementing that contract is far cheaper than
// rewriting those call sites, and it keeps vr_settings.ini byte-compatible
// with the desktop build so a calibration file can move between them.
// ---------------------------------------------------------------------------
#ifndef _WIN32
extern "C" {

int32_t GetPrivateProfileIntA(const char *section, const char *key,
                              int32_t defaultValue, const char *fileName);

uint32_t GetPrivateProfileStringA(const char *section, const char *key,
                                  const char *defaultValue, char *out,
                                  uint32_t outSize, const char *fileName);

int32_t WritePrivateProfileStringA(const char *section, const char *key,
                                   const char *value, const char *fileName);

}

#endif

#if (defined(__ANDROID__) || (defined(LIBERTYVR_WINDOWS) || defined(LIBERTYVR_DESKTOP))) && !defined(LCVR_PROFILE_API_IMPLEMENTATION)
#include "MatchedProfile.h"
#include "WorldSession.h"
#include <string>
inline std::string LcvrSettingsPath(const char *file) {
    if(!file || file[0]=='/' || file[0]=='\\' || (file[0] && file[1]==':')) return file?file:"";
    if(WorldSession::IsViewer()) {
        std::string relative=file;for(char &c:relative)if(c=='\\')c='/';
        return std::string(platform::storageRoot())+"/"+relative;
    }
    char path[2048];platform::resolveGamePath(path,sizeof(path),file);return path;
}
inline int32_t LcvrProfileInt(const char *section,const char *key,int32_t fallback,const char *file) {
    if(WorldSession::IsViewer() && section && std::string(section)=="VR") {
        auto found=WorldSession::settings.find(key);if(found!=WorldSession::settings.end())return found->second;
    }
    const auto path=LcvrSettingsPath(file);
    return MatchedProfile::Read(section,key,(int32_t)GetPrivateProfileIntA(section,key,fallback,path.c_str()));
}
inline int32_t LcvrProfileWrite(const char *section,const char *key,const char *value,const char *file) {
    const auto path=LcvrSettingsPath(file);
    return MatchedProfile::Write(section,key,value) ? WritePrivateProfileStringA(section,key,value,path.c_str()) : 1;
}
inline uint32_t LcvrProfileString(const char *section,const char *key,const char *fallback,char *out,uint32_t size,const char *file) {
    const auto path=LcvrSettingsPath(file);return GetPrivateProfileStringA(section,key,fallback,out,size,path.c_str());
}
#define GetPrivateProfileIntA LcvrProfileInt
#define WritePrivateProfileStringA LcvrProfileWrite
#define GetPrivateProfileStringA LcvrProfileString
#endif
