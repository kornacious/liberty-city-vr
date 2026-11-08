#pragma once

// Both settings entry points edit the engine preferences directly. Do not
// mirror these values in vr_settings.ini: the radio board and save/load already
// use the engine's settings store.
namespace QuestGameSettings {
enum AudioRow { Music, Effects, Station, RadioBoard, AudioDefaults, AudioBack, AudioCount };
enum LanguageRow { Language, Subtitles, LanguageBack, LanguageCount };
void AudioText(int row, char *text, int size);
void AudioAction(int row, int direction);
void SetMusicVolume(int volume);
void CycleRadioStation(int direction);
void ServiceFrontendAudioPreview(bool active);
void ServiceFrontendAudioFrame();
int RadioStation();
bool CustomRadioAvailable();
void AdjustDrawDistance(int direction);
void ToggleCinemaBorders();
void LanguageText(int row, char *text, int size);
void LanguageAction(int row, int direction);
const char *SkinName();
void CycleSkin(int direction);
}
