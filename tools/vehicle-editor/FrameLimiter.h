#pragma once
#include <algorithm>
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace editor {
inline void waitForFrame(std::chrono::steady_clock::time_point start, int fps) {
    if (fps <= 0)
        return;
    auto deadline = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                std::chrono::duration<double>(1.0 / fps));
#ifdef _WIN32
    // Sleep's coarse Windows timer can turn a 30 FPS cap into ~21 FPS. A private
    // high-resolution waitable timer avoids changing the system timer period.
    struct Timer {
        HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_MODIFY_STATE | SYNCHRONIZE);
        ~Timer() {
            if (handle)
                CloseHandle(handle);
        }
    };
    static Timer timer;
    if (timer.handle) {
        auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return;
        LARGE_INTEGER due;
        due.QuadPart = -std::max<long long>(
            1, std::chrono::duration_cast<std::chrono::duration<long long, std::ratio<1, 10000000>>>(
                   remaining)
                   .count());
        if (SetWaitableTimer(timer.handle, &due, 0, nullptr, nullptr, FALSE)) {
            if (WaitForSingleObject(timer.handle, INFINITE) == WAIT_OBJECT_0)
                return;
        }
    }
#endif
    std::this_thread::sleep_until(deadline);
}
} // namespace editor
