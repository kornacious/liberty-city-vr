#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <cstdint>
#define CLOCK_REALTIME 0
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
inline int clock_gettime(int clock, timespec *out) {
    auto ns = clock==CLOCK_REALTIME ? std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() :
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    out->tv_sec = static_cast<time_t>(ns / 1000000000); out->tv_nsec = long(ns % 1000000000); return 0;
}
#endif
inline tm *gmtime_r(const time_t *time, tm *out) { return gmtime_s(out,time)==0 ? out : nullptr; }
inline int mkdir(const char *path, int) { return _mkdir(path); }
inline int lstat(const char *path, struct stat *out) {
    DWORD attributes = GetFileAttributesA(path);
    if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { errno = EINVAL; return -1; }
    return stat(path,out);
}
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode) false
#endif
