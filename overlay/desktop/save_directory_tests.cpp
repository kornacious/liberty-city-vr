#include "DesktopSaveDirectory.h"
#include <chrono>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("lcvr-save-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto data = root / "variant", user = root / "user";
        fs::create_directories(data);
        for(const char *name : {"1.b", "2.b", "8.b", "autosave.b", "9.b", "unrelated.txt"})
            std::ofstream(data / name) << "legacy";
        fs::create_directories(user / "userfiles");
        std::ofstream(user / "userfiles" / "GTA3sf2.b") << "existing";
        const auto prefix = desktopsave::Prepare(user.u8string(), data.u8string(), false);
        std::string existing;
        std::ifstream(fs::u8path(prefix + "2.b")) >> existing;
        if(existing != "existing") throw std::runtime_error("First migration overwrote existing save");
        if(fs::u8path(prefix) != user / "userfiles" / "GTA3sf") throw std::runtime_error("Wrong save root");
        for(const char *name : {"1.b", "8.b", "autosave.b"}) {
            if(!fs::exists(fs::u8path(prefix + name)) || !fs::exists(data / name))
                throw std::runtime_error("Missing migrated slot or original");
        }
        if(fs::exists(fs::u8path(prefix + "9.b")) || fs::exists(user / "userfiles/unrelated.txt"))
            throw std::runtime_error("Unexpected file migrated");
        std::ofstream(fs::u8path(prefix + "1.b")) << "current";
        desktopsave::Prepare(user.u8string(), data.u8string(), false);
        std::string value;
        std::ifstream(fs::u8path(prefix + "1.b")) >> value;
        if(value != "current") throw std::runtime_error("Existing save overwritten");
        fs::remove(fs::u8path(prefix + "8.b"));
        desktopsave::Prepare(user.u8string(), data.u8string(), false);
        if(fs::exists(fs::u8path(prefix + "8.b"))) throw std::runtime_error("Deleted save resurrected");
        const auto upstate = desktopsave::Prepare(user.u8string(), data.u8string(), true);
        if(upstate == prefix || !fs::exists(fs::u8path(upstate + "1.b")))
            throw std::runtime_error("Upstate saves not isolated");
        bool rejected = false;
        try { desktopsave::Prepare(std::string(260, 'x'), data.u8string(), false); }
        catch(const std::runtime_error &) { rejected = true; }
        if(!rejected) throw std::runtime_error("Overlong save prefix accepted");
        // Optional local fixtures verify real saves without modifying originals.
        for(int i = 1; i < argc; ++i) {
            const auto fixtureData = root / ("fixture-" + std::to_string(i));
            const auto fixtureUser = root / ("fixture-user-" + std::to_string(i));
            fs::create_directories(fixtureData);
            fs::copy_file(fs::u8path(argv[i]), fixtureData / "1.b");
            std::ifstream file(fixtureData / "1.b", std::ios::binary);
            const auto profile = questsave::Inspect([&](uint8_t *bytes, size_t count) {
                file.read(reinterpret_cast<char *>(bytes), count);
                return size_t(file.gcount());
            });
            if(profile == questsave::Profile::Unknown) throw std::runtime_error("Invalid local save checksum/format");
            for(bool active : {false, true}) {
                const auto path = desktopsave::Prepare(fixtureUser.u8string(), fixtureData.u8string(), active);
                if(fs::exists(fs::u8path(path + "1.b")) != ((profile == questsave::Profile::Upstate) == active))
                    throw std::runtime_error("Legacy save migrated into wrong engine");
            }
        }
        fs::remove_all(root);
        std::cout << "Persistent desktop save migration passed\n";
        return 0;
    } catch(const std::exception &error) {
        fs::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
