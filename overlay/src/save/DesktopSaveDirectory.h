#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include "QuestSaveCompatibility.h"

namespace desktopsave {
// Earlier desktop builds left the save prefix empty and wrote in the active
// content directory. Copy only missing slots; never change the originals.
inline std::string Prepare(const std::string &userDirectory,
                           const std::string &contentDirectory, bool upstate)
{
    namespace fs = std::filesystem;
    auto destination = fs::u8path(userDirectory) / "userfiles";
    if(upstate) destination /= "upstate-liberty-1.1.2";
    const auto prefix = (destination / "GTA3sf").u8string();
    // DefaultPCSaveFileName has 260 bytes; leave room for "autosave.b" + NUL.
    if(prefix.size() + sizeof("autosave.b") > 260)
        throw std::runtime_error("Desktop save directory path is too long.");
    fs::create_directories(destination);
    const auto marker = destination / ".legacy-content-saves-migrated";
    if(fs::exists(marker)) return prefix;
    for(int slot = 1; slot <= 9; ++slot) {
        const std::string name = slot == 9 ? "autosave.b" : std::to_string(slot) + ".b";
        const auto source = fs::u8path(contentDirectory) / name;
        const auto target = destination / ("GTA3sf" + name);
        if(fs::is_regular_file(source) && !fs::exists(target)) {
            std::ifstream file(source, std::ios::binary);
            const auto profile = questsave::Inspect([&](uint8_t *bytes, size_t count) {
                file.read(reinterpret_cast<char *>(bytes), count);
                return size_t(file.gcount());
            });
            if(profile != questsave::Profile::Unknown &&
               (profile == questsave::Profile::Upstate) != upstate) continue;
            fs::copy_file(source, target, fs::copy_options::none);
        }
    }
    // Import once so deleting a slot does not resurrect its legacy original.
    std::ofstream completed(marker);
    completed << "Legacy content saves copied; originals preserved.\n";
    completed.close();
    if(!completed) throw std::runtime_error("Cannot record desktop save migration.");
    return prefix;
}
}
