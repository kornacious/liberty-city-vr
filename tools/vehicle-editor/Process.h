#pragma once
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace editor {
// Argument arrays only: user-selected paths never pass through a command shell.
inline std::string process(const std::vector<std::string> &args) {
#ifdef _WIN32
    auto wide = [](const std::string &s) {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, 0, 0);
        std::wstring w(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        w.resize(n - 1);
        return w;
    };
    std::wstring command;
    for (auto &a : args) {
        command += L'"';
        int slashes = 0;
        for (wchar_t c : wide(a)) {
            if (c == L'\\') {
                slashes++;
                continue;
            }
            command.append(slashes * (c == L'"' ? 2 : 1), L'\\');
            slashes = 0;
            if (c == L'"')
                command += L'\\';
            command += c;
        }
        command.append(slashes * 2, L'\\');
        command += L"\" ";
    }
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
    HANDLE read = 0, write = 0;
    if (!CreatePipe(&read, &write, &sa, 0))
        return "Cannot start local device helper";
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write;
    si.hStdError = write;
    HANDLE input =
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, 0);
    si.hStdInput = input;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(0, &command[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi);
    CloseHandle(write);
    CloseHandle(input);
    if (!ok) {
        CloseHandle(read);
        return "Cannot start Python helper. Set the Python executable path.";
    }
    std::string result;
    char buffer[4096];
    DWORD n;
    while (ReadFile(read, buffer, sizeof(buffer), &n, 0) && n) {
        if (result.size() < 1024 * 1024)
            result.append(buffer, n);
    }
    CloseHandle(read);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
#else
    int pipes[2];
    if (pipe(pipes))
        return "Cannot start local device helper";
    pid_t pid;
    std::vector<char *> argv;
    for (auto &a : args)
        argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(0);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    int error = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), ::environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipes[1]);
    if (error) {
        close(pipes[0]);
        return "Cannot start Python helper; check the executable path.";
    }
    std::string result;
    char buffer[4096];
    ssize_t n;
    for (;;) {
        n = read(pipes[0], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        if (result.size() < 1024 * 1024)
            result.append(buffer, n);
    }
    close(pipes[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    return result.empty() && status ? "Cannot start Python helper; check the executable path." : result;
#endif
}
} // namespace editor
