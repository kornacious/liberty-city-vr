#pragma once
#include <string>
#include <atomic>
namespace platform {
void installNativeDiagnostics(std::atomic<const char*> *checkpoint);
bool configureDesktop(const std::string &data,const std::string &user);
void shutdownDesktop();
bool exitRequested();
void requestExit();
}
