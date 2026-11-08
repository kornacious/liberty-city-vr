#pragma once
#include <string>
namespace platform {
bool configureWindows(const std::string &data, const std::string &user);
bool exitRequested();
void requestExit();
void shutdownWindows();
}
