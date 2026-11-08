#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstring>
struct dirent { char d_name[1024]; };
struct DIR { std::vector<std::string> names; size_t index = 0; dirent entry{}; };
inline DIR *opendir(const char *path) {
    auto result = new DIR;
    std::error_code error;
    for(std::filesystem::directory_iterator it(path,error), end; !error && it != end; it.increment(error))
        result->names.push_back(it->path().filename().u8string());
    if(error) { delete result; return nullptr; } return result;
}
inline dirent *readdir(DIR *directory) {
    if(directory->index >= directory->names.size()) return nullptr;
    const auto &name = directory->names[directory->index++];
    if(name.size() >= sizeof(directory->entry.d_name)) return readdir(directory);
    std::memcpy(directory->entry.d_name,name.c_str(),name.size()+1); return &directory->entry;
}
inline int closedir(DIR *directory) { delete directory; return 0; }
