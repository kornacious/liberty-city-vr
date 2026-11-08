#pragma once
#include <string>
// Original game WAVs can contain IMA/MS ADPCM, which MFPlay cannot decode.
// Decode those locally with Windows ACM, leaving all source assets untouched.
std::string WindowsPlayableAudio(const char *source,const std::string &userRoot);
