#include "Process.h"
#include <iostream>
int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--echo") {
        for (int i = 2; i < argc; i++)
            std::cout << std::string(argv[i]).size() << ":" << argv[i] << "\n";
        return 0;
    }
    std::vector<std::string> args = {
        argv[0],      "--echo", "spaces in a path", "quote\"and slash\\", "$(literal)",
        "semi;colon", "",       "UTF8-\xc3\xa9"};
    std::string expected;
    for (size_t i = 2; i < args.size(); i++)
        expected += std::to_string(args[i].size()) + ":" + args[i] + "\n";
    auto output = editor::process(args);
#ifdef _WIN32
    // The child's CRT text-mode stdout uses CRLF.
    for (size_t p = 0; (p = output.find("\r\n", p)) != std::string::npos;)
        output.erase(p, 1);
#endif
    if (output != expected) {
        std::cerr << "Device helper argument transport failed\n";
        return 1;
    }
    auto missing = editor::process({"lcvr-deliberately-missing-helper"});
    if (missing.find("Cannot start") == std::string::npos) {
        std::cerr << "Missing helper error was not reported\n";
        return 1;
    }
    std::cout << "Shell-free device helper transport tests passed\n";
    return 0;
}
