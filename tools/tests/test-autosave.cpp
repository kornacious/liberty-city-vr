#define _CRT_SECURE_NO_WARNINGS
#define __ANDROID__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include "../../overlay/src/save/AutosaveTransaction.h"

using int32 = int32_t;
using uint32 = uint32_t;
using uint8 = uint8_t;
constexpr int AUTOSAVE_SLOT = -1, SLOT_COUNT = 8;
constexpr uint32 SIZE_OF_ONE_GAME_IN_BYTES = 201729;
char LoadFileName[256], DefaultPCSaveFileName[260];
uint8 work_buff[16384];
int CheckSum;
constexpr int SAVESTATUS_SUCCESSFUL = 0;
struct { int nErrorCode = 0; } PcSaveHelper;
enum eLevelName { LEVEL_GENERIC };
eLevelName m_LevelToLoad;
void SaveLog(const char *, ...) {}
#define SAVELOG SaveLog
int32 align4bytes(int32 n) { return (n + 3) & ~3; }
using Bytes = std::vector<uint8>;
std::map<std::string, Bytes> files;
struct Cursor { std::string path; size_t offset; };
std::map<int, Cursor> handles;
int nextHandle = 1, checks = 0;
void Require(bool value) { ++checks; if(!value) { std::fprintf(stderr, "Failed assertion %d\n", checks); std::exit(1); } }
struct CFileMgr {
    static int OpenFile(const char *path, const char *) {
        if(!files.count(path)) return 0;
        handles[nextHandle] = {path, 0};
        return nextHandle++;
    }
};
bool CloseFile(int file) { return handles.erase(file) == 1; }
bool ReadDataFromFile(int file, uint8 *out, size_t count) {
    auto &cursor = handles.at(file);
    const auto &data = files.at(cursor.path);
    if(count > data.size() - cursor.offset) {
        PcSaveHelper.nErrorCode = 1;
        CloseFile(file);
        return false;
    }
    std::memcpy(out, data.data() + cursor.offset, count);
    cursor.offset += count;
    return true;
}
#include "autosave-production.inc"

Bytes Serialized(uint8 seed) {
    Bytes result;
    uint32 checksum = 0, remaining = uint32(align4bytes(SIZE_OF_ONE_GAME_IN_BYTES - 4));
    while(remaining) {
        uint32 length = std::min(remaining, uint32(sizeof(work_buff)));
        const auto *header = reinterpret_cast<const uint8 *>(&length);
        result.insert(result.end(), header, header + 4);
        for(int i = 0; i < 4; ++i) checksum += header[i];
        for(uint32 i = 0; i < length; ++i) { result.push_back(seed); checksum += seed; }
        remaining -= length;
    }
    const auto *tail = reinterpret_cast<const uint8 *>(&checksum);
    result.insert(result.end(), tail, tail + 4);
    return result;
}

// Each operation can fail independently. The operation after every completed
// step also checks recovery, modelling interruption at that boundary.
struct Operations {
    int step = 0, failAt = 0;
    Bytes current, old;
    bool recoverable = true;
    bool Gate() {
        if(recoverable) Require(files["autosave.b"] == old || files["autosave.b.bak"] == old || files["autosave.b"] == current);
        for(int i = 1; i <= 8; ++i) Require(files[std::to_string(i) + ".b"] == Bytes{uint8(i)});
        return ++step != failAt;
    }
    bool Write(const char *path) {
        bool ok = Gate(); files[path] = ok ? current : Bytes{0}; return ok;
    }
    bool Sync(const char *) { return Gate(); }
    bool Validate(const char *path) {
        if(!Gate()) return false;
        char name[256]; return CheckSaveFile(path, name, AUTOSAVE_SLOT);
    }
    bool Exists(const char *path) { return files.count(path) != 0; }
    bool Copy(const char *src, const char *dst) {
        bool ok = Gate(); files[dst] = ok ? files.at(src) : Bytes{0}; return ok;
    }
    bool Replace(const char *src, const char *dst) {
        if(!Gate()) return false;
        files[dst] = files.at(src); files.erase(src); return true;
    }
    bool SyncDirectory(const char *) { return Gate(); }
    void Remove(const char *path) { files.erase(path); }
};
void Reset(const Bytes &old) {
    Require(handles.empty()); files.clear();
    files["autosave.b"] = old;
    for(int i = 1; i <= 8; ++i) files[std::to_string(i) + ".b"] = {uint8(i)};
}
int main() {
    const Bytes old = Serialized(1), current = Serialized(2), older = Serialized(3);
    Reset(old);
    Operations success{0, 0, current, old};
    Require(AutosaveTransaction::Save(success, "autosave.b"));
    Require(files["autosave.b"] == current && files["autosave.b.bak"] == old);
    const int steps = success.step;
    for(int fault = 1; fault <= steps; ++fault) {
        Reset(old); files["autosave.b.bak"] = older;
        Operations ops{0, fault, current, old};
        // A failed validation of the existing destination is treated as a
        // corrupt primary: preserve the existing recovery copy and replace it.
        const bool saved = AutosaveTransaction::Save(ops, "autosave.b");
        Require(saved == (fault == 4));
        Require(files["autosave.b"] == old || files["autosave.b.bak"] == old ||
                (files["autosave.b"] == current && files["autosave.b.bak"] == older));
        Require(!files.count("autosave.b.tmp") && !files.count("autosave.b.bak.tmp"));
        Require(handles.empty());
    }
    char name[256];
    Reset(old); files["autosave.b.bak"] = older; files["autosave.b"] = {0};
    Require(CheckDataNotCorrupt(AUTOSAVE_SLOT, name) && std::string(name) == "autosave.b.bak");
    Require(PcSaveHelper.nErrorCode == SAVESTATUS_SUCCESSFUL);
    Operations recover{0, 0, current, old, false};
    Require(AutosaveTransaction::Save(recover, "autosave.b"));
    Require(files["autosave.b.bak"] == older && files["autosave.b"] == current);
    files.erase("autosave.b");
    Require(CheckDataNotCorrupt(AUTOSAVE_SLOT, name));
    files.erase("autosave.b.bak");
    Require(!CheckDataNotCorrupt(AUTOSAVE_SLOT, name));
    Operations first{0, 0, current, old, false};
    Require(AutosaveTransaction::Save(first, "autosave.b"));
    Require(files["autosave.b"] == current && !files.count("autosave.b.bak"));
    // Legacy saves (including old autosaves) remain loadable in manual slot 1.
    files["1.b"] = older;
    Require(CheckDataNotCorrupt(0, name) && std::string(name) == "1.b");
    files["1.b"] = {0};
    Require(!CheckDataNotCorrupt(0, name)); // never fall back to an unrelated save
    Require(files["autosave.b"] == current);
    for(int32 length : {-1, 0, INT32_MAX, 16385}) {
        files["autosave.b"] = current;
        std::memcpy(files["autosave.b"].data(), &length, 4);
        Require(!CheckDataNotCorrupt(AUTOSAVE_SLOT, name));
    }
    files["autosave.b"] = current; files["autosave.b"].pop_back();
    Require(!CheckDataNotCorrupt(AUTOSAVE_SLOT, name));
    files["autosave.b"] = current; files["autosave.b"].back() ^= 1;
    Require(!CheckDataNotCorrupt(AUTOSAVE_SLOT, name));
    for(int i = 0; i < 8; ++i) {
        Require(GetSaveFileName(i, name, sizeof(name)) && std::string(name) == std::to_string(i + 1) + ".b");
    }
    Require(GetSaveFileName(AUTOSAVE_SLOT, name, sizeof(name)) && std::string(name) == "autosave.b");
    Require(!GetSaveFileName(-2, name, sizeof(name)));
    Require(!GetSaveFileName(9, name, sizeof(name)));
    std::memset(DefaultPCSaveFileName, 'x', 250); DefaultPCSaveFileName[250] = 0;
    Require(!GetSaveFileName(AUTOSAVE_SLOT, name, sizeof(name)));
    Operations longPath{0, 0, current, old, false};
    Require(!AutosaveTransaction::Save(longPath, std::string(259, 'x').c_str()) && longPath.step == 0);
    Require(handles.empty());
    std::printf("Autosave: %d assertions passed, %d injected failure boundaries; no game assets used.\n", checks, steps);
}
