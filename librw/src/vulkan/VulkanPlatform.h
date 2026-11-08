#pragma once

#include <chrono>
#include <cstdint>

#ifdef __ANDROID__
#include <android/log.h>
#define VKLOG(...) __android_log_print(ANDROID_LOG_INFO, "librw-vk", __VA_ARGS__)
#define VKERR(...) __android_log_print(ANDROID_LOG_ERROR, "librw-vk", __VA_ARGS__)
#else
#include <cstdarg>
#include <cstdio>
namespace VulkanPlatform {
inline void log(bool error, const char *format, ...) {
    FILE *output = error ? stderr : stdout;
    std::fputs("librw-vk: ", output);
    va_list args;
    va_start(args, format);
    std::vfprintf(output, format, args);
    va_end(args);
    std::fputc('\n', output);
}
}
#define VKLOG(...) VulkanPlatform::log(false, __VA_ARGS__)
#define VKERR(...) VulkanPlatform::log(true, __VA_ARGS__)
#endif

namespace VulkanPlatform {
inline std::uint64_t microseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline double milliseconds() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
