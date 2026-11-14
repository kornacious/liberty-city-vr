#pragma once
#include <string>
#include <vector>
namespace DesktopAudio {
bool Initialize(const std::string &userRoot);
void Shutdown();
void Pump();
std::vector<std::string> Devices();
const std::string &Preference();
void Select(const std::string &name);
const char *Status();
}
