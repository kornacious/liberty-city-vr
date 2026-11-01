#pragma once
#include <algorithm>
#include <istream>
#include <set>
#include <string>
#include <vector>

namespace QuestMusic {
inline std::vector<std::string> OrderedNames(std::vector<std::string> names, std::istream &playlist) {
    std::sort(names.begin(), names.end());
    std::set<std::string> remaining(names.begin(), names.end());
    std::vector<std::string> ordered;
    std::string name; size_t lines=0;
    while(lines++<1000 && std::getline(playlist,name)) {
        if(!name.empty() && name.back()=='\r') name.pop_back();
        if(name.size()<=200 && remaining.erase(name)) ordered.push_back(name);
    }
    ordered.insert(ordered.end(),remaining.begin(),remaining.end()); return ordered;
}
}
