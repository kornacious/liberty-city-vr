#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

// STREAMINFO provides exact duration without preparing 195 Android players at
// startup. The importer emits ordinary FLAC with this mandatory first block.
inline uint32_t QuestFlacDuration(const char *path)
{
    unsigned char header[42];
    FILE *file = fopen(path, "rb");
    if(!file) return 0;
    const size_t count = fread(header, 1, sizeof(header), file);
    fclose(file);
    if(count != sizeof(header) || memcmp(header, "fLaC", 4) ||
       (header[4] & 0x7f) != 0 || header[5] || header[6] || header[7] != 34) return 0;
    uint64_t packed = 0;
    for(int i = 18; i < 26; ++i) packed = (packed << 8) | header[i];
    const uint32_t rate = uint32_t(packed >> 44);
    const uint64_t samples = packed & ((uint64_t(1) << 36) - 1);
    const uint64_t ms = rate ? samples * 1000 / rate : 0;
    return ms <= UINT32_MAX ? uint32_t(ms) : 0;
}
