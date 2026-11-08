#pragma once
#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdarg>
#include <cstdio>
enum { ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
// Preserve the existing tagged diagnostics while moving platform delivery out
// of the shared game modules. No Android runtime is linked on Windows.
inline int __android_log_print(int level, const char *tag, const char *format, ...) {
    FILE *out = level >= ANDROID_LOG_WARN ? stderr : stdout;
    std::fprintf(out, "%s: ", tag);
    va_list args; va_start(args, format); int result = std::vfprintf(out, format, args); va_end(args);
    std::fputc('\n', out); std::fflush(out); return result;
}
#endif
