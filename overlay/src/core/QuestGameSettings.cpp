#include "common.h"
#include "QuestGameSettings.h"
#include "Frontend.h"
#include "DMAudio.h"
#include "FileMgr.h"
#include "PlayerSkin.h"
#include "World.h"
#include "MusicManager.h"
#include "Renderer.h"
#include "crossplatform.h"
#include "platform_android.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

extern int32 gNumRetunePresses;
extern int32 gRetuneCounter;

namespace QuestGameSettings {
static bool gFrontendPreviewActive, gFrontendPreviewPlaying;
static int gFrontendPreviewStation = -1;
static int Volume(int value) { return value < 0 ? 0 : value > 127 ? 127 : value; }
static const char *const stations[] = { "HEAD RADIO", "DOUBLE CLEF", "K-JAH", "RISE FM",
    "LIPS 106", "GAME FM", "MSX FM", "FLASHBACK", "CHATTERBOX", "CUSTOM SOUNDTRACKS", "POLICE RADIO", "RADIO OFF" };

int RadioStation() { return DMAudio.GetRadioInCar(); }
bool CustomRadioAvailable() { return DMAudio.IsMP3RadioChannelAvailable(); }

void ServiceFrontendAudioFrame()
{
    // The original CMenuManager::Process services audio before game load.
    // Its shared-settings early return must retain that work. Once loaded,
    // the ordinary game idle path already services audio exactly once.
    if(FrontEndMenuManager.m_bGameNotLoaded && !FrontEndMenuManager.m_bWantToRestart)
        DMAudio.Service();
}

void AudioText(int row, char *text, int size)
{
    const CMenuManager &menu = FrontEndMenuManager;
    if(row == Music) snprintf(text, size, "MUSIC VOLUME  < %d%% >", menu.m_PrefsMusicVolume*100/127);
    else if(row == Effects) snprintf(text, size, "SFX VOLUME  < %d%% >", menu.m_PrefsSfxVolume*100/127);
    else if(row == Station) {
        const int station = RadioStation();
        snprintf(text, size, "RADIO STATION  < %s >", stations[station >= 0 && station < 12 ? station : 11]);
    } else snprintf(text, size, "%s", row == RadioBoard ? "RADIO BOARD AND CUSTOM MUSIC  < OPEN >" :
        row == AudioDefaults ? "RESTORE AUDIO DEFAULTS" : "BACK TO SETTINGS");
}

void AudioAction(int row, int direction)
{
    CMenuManager &menu = FrontEndMenuManager;
    if(row == Music || row == AudioDefaults) {
        SetMusicVolume(row == AudioDefaults ? 102 : menu.m_PrefsMusicVolume+direction*8);
    }
    if(row == Effects || row == AudioDefaults) {
        menu.m_PrefsSfxVolume = row == AudioDefaults ? 102 : Volume(menu.m_PrefsSfxVolume+direction*8);
        DMAudio.SetEffectsMasterVolume(menu.m_PrefsSfxVolume);
    }
    if(row == Station) {
        CycleRadioStation(direction);
        if(gFrontendPreviewActive) ServiceFrontendAudioPreview(true);
    }
    if(row == Effects || row == AudioDefaults) menu.SaveSettings();
}

void ServiceFrontendAudioPreview(bool active)
{
    gFrontendPreviewActive = active;
    const int station = RadioStation();
    const bool playable = active && station >= 0 && station < 10 &&
        (station != 9 || DMAudio.IsMP3RadioChannelAvailable());
    if(!playable) {
        if(gFrontendPreviewPlaying) DMAudio.StopFrontEndTrack();
        gFrontendPreviewPlaying = false;
        gFrontendPreviewStation = -1;
        return;
    }
    if(!gFrontendPreviewPlaying || station != gFrontendPreviewStation) {
        DMAudio.PlayFrontEndTrack(station, true);
        gFrontendPreviewStation = station;
        gFrontendPreviewPlaying = true;
    }
}

void SetMusicVolume(int volume)
{
    FrontEndMenuManager.m_PrefsMusicVolume = Volume(volume);
    DMAudio.SetMusicMasterVolume(FrontEndMenuManager.m_PrefsMusicVolume);
    FrontEndMenuManager.SaveSettings();
}

void CycleRadioStation(int direction)
{
    int station = DMAudio.GetRadioInCar();
    if(station < 0 || station > 11) station = 11;
    do { station = (station+(direction < 0 ? -1 : 1)+12)%12; }
    while(station == 10 || (station == 9 && !DMAudio.IsMP3RadioChannelAvailable()));
    FrontEndMenuManager.m_PrefsRadioStation = station;
    DMAudio.SetRadioInCar(station);
    // Tuning alone stores the selection; the game music service also needs
    // the same retune request used by the physical VR radio board.
    gNumRetunePresses = gRetuneCounter = 0;
    MusicManager.m_bSetNextStation = true;
    FrontEndMenuManager.SaveSettings();
}

void AdjustDrawDistance(int direction)
{
    const float value = FrontEndMenuManager.m_PrefsLOD + (direction < 0 ? -0.05f : 0.05f);
    FrontEndMenuManager.m_PrefsLOD = std::max(0.8f, std::min(1.8f, value));
    CRenderer::ms_lodDistScale = FrontEndMenuManager.m_PrefsLOD;
    FrontEndMenuManager.SaveSettings();
}

void ToggleCinemaBorders()
{
    FrontEndMenuManager.m_PrefsCutsceneBorders = !FrontEndMenuManager.m_PrefsCutsceneBorders;
    FrontEndMenuManager.SaveSettings();
}

static const char *const languages[] = { "ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH" };
static const char *const textFiles[] = { "TEXT/AMERICAN.GXT", "TEXT/FRENCH.GXT", "TEXT/GERMAN.GXT", "TEXT/ITALIAN.GXT", "TEXT/SPANISH.GXT" };
void LanguageText(int row, char *text, int size)
{
    const int language = FrontEndMenuManager.m_PrefsLanguage;
    if(row == Language) snprintf(text, size, "GAME LANGUAGE  < %s >", language >= 0 && language < 5 ? languages[language] : "CUSTOM");
    else if(row == Subtitles) snprintf(text, size, "SUBTITLES  < %s >", FrontEndMenuManager.m_PrefsShowSubtitles ? "ON" : "OFF");
    else snprintf(text, size, "BACK TO SETTINGS");
}
void LanguageAction(int row, int direction)
{
    CMenuManager &menu = FrontEndMenuManager;
    if(row == Subtitles) menu.m_PrefsShowSubtitles = !menu.m_PrefsShowSubtitles;
    else if(row == Language) {
        int language = menu.m_PrefsLanguage;
        if(language < 0 || language >= 5) language = 0;
        for(int attempt = 0; attempt < 5; ++attempt) {
            language = (language+direction+5)%5;
            const int file = CFileMgr::OpenFile(textFiles[language], "rb");
            if(!file) continue;
            CFileMgr::CloseFile(file);
            if(language != menu.m_PrefsLanguage) {
                menu.m_PrefsLanguage = language;
                menu.m_bFrontEnd_ReloadObrTxtGxt = true;
                menu.InitialiseChangedLanguageSettings();
            }
            break;
        }
    } else return;
    menu.SaveSettings();
}

const char *SkinName()
{
    return strcmp(FrontEndMenuManager.m_PrefsSkinFile, DEFAULT_SKIN_NAME) == 0 ? "DEFAULT" : FrontEndMenuManager.m_PrefsSkinFile;
}
void CycleSkin(int direction)
{
    std::vector<std::string> skins;
    WIN32_FIND_DATA found;
    char pattern[2048];platform::resolveGamePath(pattern,sizeof(pattern),"skins/*.bmp");
    HANDLE handle = FindFirstFile(pattern, &found);
    if(handle != INVALID_HANDLE_VALUE) {
        do {
            const size_t length = strlen(found.cFileName);
            if(length > 4 && length-4 < sizeof(FrontEndMenuManager.m_PrefsSkinFile))
                skins.push_back(std::string(found.cFileName, length-4));
        } while(FindNextFile(handle, &found));
        FindClose(handle);
    }
    std::sort(skins.begin(), skins.end());
    skins.erase(std::unique(skins.begin(), skins.end()), skins.end());
    skins.insert(skins.begin(), DEFAULT_SKIN_NAME);
    int selected = 0;
    for(size_t i = 0; i < skins.size(); ++i)
        if(skins[i] == FrontEndMenuManager.m_PrefsSkinFile) selected = (int)i;
    selected = (selected+direction+(int)skins.size())%(int)skins.size();
    strcpy(FrontEndMenuManager.m_PrefsSkinFile, skins[selected].c_str());
    // Before a game is loaded, the saved preference is applied by player init.
    if(!FrontEndMenuManager.m_bGameNotLoaded)
        CWorld::Players[0].SetPlayerSkin(FrontEndMenuManager.m_PrefsSkinFile);
    FrontEndMenuManager.SaveSettings();
}
}
