#include "../../../overlay/src/audio/QuestRadioStream.h"
#include <cassert>
#include <string>
#include <vector>

int main()
{
    char path[768];
    std::vector<std::string> attempts;
    std::string available;
    auto resolve = [&](char *out, size_t capacity, const char *relative) {
        attempts.emplace_back(relative);
        if(available != relative) return false;
        snprintf(out, capacity, "/gamedata/%s", relative);
        return true;
    };
    available = "AUDIO\\HEAD.m4a";
    assert(QuestResolveRadioStream(path, sizeof(path), "AUDIO\\HEAD.WAV", true, resolve));
    assert(attempts.size() == 1 && std::string(path) == "/gamedata/AUDIO\\HEAD.m4a");
    attempts.clear(); available = "AUDIO\\HEAD.WAV";
    assert(QuestResolveRadioStream(path, sizeof(path), "AUDIO\\HEAD.WAV", true, resolve));
    assert(attempts.size() == 2 && attempts.back() == available);
    attempts.clear(); available.clear();
    assert(!QuestResolveRadioStream(path, sizeof(path), "AUDIO\\HEAD.WAV", true, resolve));
    assert(attempts.size() == 2);
    attempts.clear(); available = "AUDIO\\mission.WAV";
    assert(QuestResolveRadioStream(path, sizeof(path), "AUDIO\\mission.WAV", false, resolve));
    assert(attempts.size() == 1 && attempts[0] == available);
    return 0;
}
