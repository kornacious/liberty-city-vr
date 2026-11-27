#include "PcvrStartup.h"
#include "QuestUpstatePackage.h"
#include <iostream>
#ifndef _WIN32
#include <dlfcn.h>
#endif

int main(int argc, char **argv) {
    try {
        pcvrstartup::Arguments args(argc, argv);
        const auto app = pcvrstartup::ExecutableDirectory();
        std::string data, user;
        bool explicitData = false, explicitUser = false, checkUpstate = false;
        for (size_t i = 1; i < args.values.size(); ++i) {
            const auto &argument = args.values[i];
            if (argument == "--help") { std::cout << pcvrstartup::GameHelp; return 0; }
            if (argument == "--check-upstate-archive") { checkUpstate = true; continue; }
            if (argument == "--check-assets") continue;
            if (i + 1 >= args.values.size()) throw std::runtime_error("Missing PCVR option value.");
            if (argument == "--game-data") { explicitData = true; data = args.values[++i]; }
            else if (argument == "--user-data") { explicitUser = true; user = args.values[++i]; }
            else if (argument == "--frames" || argument == "--quest-profile") ++i;
            else throw std::runtime_error("Unknown PCVR option.");
        }
        pcvrstartup::Defaults(app, data, user, explicitData, explicitUser);
        if (data.empty() || user.empty()) throw std::runtime_error("PCVR data or user path is missing.");
        const auto marker = std::filesystem::u8path(data) / "mods/upstate-liberty.enabled";
        const bool upstate = std::filesystem::exists(std::filesystem::symlink_status(marker));
        if (upstate) {
            if (!questpackage::SupportsPortableEngine(app) || !questpackage::CanActivate(data))
                throw std::runtime_error("Upstate Liberty could not be verified. Repair or disable it before launching.");
        } else if (checkUpstate) {
            throw std::runtime_error("Enable Upstate Liberty before checking its archive.");
        }
        const auto library = app / (upstate ? questpackage::PortableUpstateLibrary : questpackage::PortableClassicLibrary);
        if (!questpackage::Regular(library)) throw std::runtime_error("PCVR engine library is missing.");
        using Run = int (*)(int, char **);
#ifdef _WIN32
        const auto module = LoadLibraryExW(library.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        const auto run = module ? reinterpret_cast<Run>(GetProcAddress(module, "LibertyVrRun")) : nullptr;
        if (!run) throw std::runtime_error("Cannot load the PCVR engine library (Windows error " + std::to_string(GetLastError()) + ").");
#else
        const auto module = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!module) throw std::runtime_error(dlerror());
        const auto run = reinterpret_cast<Run>(dlsym(module, "LibertyVrRun"));
        if (!run) throw std::runtime_error("Cannot find the PCVR engine entry point.");
#endif
        return run(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "Liberty City VR: " << error.what() << '\n';
#ifdef _WIN32
        if (argc <= 1) MessageBoxA(nullptr, error.what(), "Liberty City VR", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}
