#pragma once

#include <cstdio>
#include <cstring>

// Resolve the compressed sibling before the retail path, even when the WAV
// has been removed. The caller restricts this override to native radio IDs.
template <typename Resolve>
bool QuestResolveRadioStream(char *path, size_t capacity, const char *retail,
    bool nativeRadio, Resolve resolve)
{
    if(nativeRadio){
        char compressed[128];
        const char *extension = strrchr(retail, '.');
        if(extension && size_t(extension - retail) + 5 <= sizeof(compressed)){
            snprintf(compressed, sizeof(compressed), "%.*s.m4a", int(extension - retail), retail);
            if(resolve(path, capacity, compressed)) return true;
        }
    }
    return resolve(path, capacity, retail);
}
