#include "../../overlay/src/skel/android/QuestPerfCapture.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

static std::vector<std::string> Cells(const std::string &line)
{
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string cell;
    while(std::getline(input, cell, ',')) result.push_back(cell);
    return result;
}
int main()
{
    using namespace questperf;
    PlayerEvents tracker;
    Player p;
    assert(tracker.Update(p, 0) == 0);
    p.valid = 1; p.health = 100; p.activity = Walking;
    assert(tracker.Update(p, 0) == PlayerAppeared);
    p.armour = 20; tracker.Update(p, 0);
    p.armour = 10;
    assert(tracker.Update(p, 2) == (Damage|WeaponUsed));
    p.health = 0; p.dead = 1;
    assert(tracker.Update(p, 0) == (Damage|Died));
    p.valid = 0;
    assert(tracker.Update(p, 0) == PlayerDisappeared);
    p.valid = 1; p.health = 100; p.dead = 0;
    assert(tracker.Update(p, 0) == (PlayerAppeared|Recovered));
    assert(tracker.Update(p, 0) == 0);
    p.inVehicle = 1;
    assert(tracker.Update(p, 0) == EnteredVehicle);
    p.inVehicle = 0; p.x = 51;
    assert(tracker.Update(p, 0) == (LeftVehicle|Relocated));
    PlayerEvents alreadyDead; p.dead = 1;
    assert(alreadyDead.Update(p, 0) == 0); // capture start is not a new death
    assert(std::string(ActivityName(Walking)) == "walking");
    assert(std::string(ActivityName(999)) == "unavailable");

    const auto root = std::filesystem::temp_directory_path()/
        ("lcvr-perf-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    const std::string stem = (root/"session").string();
    Writer writer;
    assert(!writer.Start((root/"missing"/"session").string(), ""));
    assert(writer.Start(stem, "active_model_set,CLASSIC\n"));
    Frame frame;
    frame.player = p; frame.cpuMs = 0.123456; frame.phases[0] = 0.012345f;
    frame.phaseMask = 1; frame.runtimeGpuMs = -1; frame.events = Damage;
    unsigned accepted = 0;
    constexpr unsigned attempts = 20000;
    for(unsigned i = 1; i <= attempts; ++i) {
        frame.id = i; frame.startNs = uint64_t(i)*10000000;
        if(writer.Submit(frame)) ++accepted;
    }
    writer.Stop(); writer.Stop();
    assert(accepted > 0);
    assert(!writer.Start(stem, "overwrite_attempt,1\n"));
    std::ifstream input(stem+".frames.csv");
    std::string line;
    assert(bool(std::getline(input, line)));
    const auto header = Cells(line);
    uint64_t previous = 0;
    unsigned rows = 0;
    while(std::getline(input, line)) {
        const auto row = Cells(line);
        assert(row.size() == header.size());
        const auto id = std::stoull(row[0]);
        assert(id > previous); previous = id;
        assert(std::stod(row[3]) == 0.123456);
        assert(std::stod(row[11]) == -1); // unknown GPU stays unknown
        assert(std::stod(row[16]) > 0.01234 && std::stod(row[16]) < 0.01235);
        ++rows;
    }
    assert(rows == accepted);
    std::ifstream metadata(stem+".session.csv");
    std::string all((std::istreambuf_iterator<char>(metadata)), std::istreambuf_iterator<char>());
    assert(all.find("active_model_set,CLASSIC\n") != std::string::npos);
    assert(all.find("written_rows,"+std::to_string(rows)+"\n") != std::string::npos);
    assert(all.find("dropped_rows,"+std::to_string(attempts-accepted)+"\n") != std::string::npos);
    assert(all.find("closed,1\n") != std::string::npos);
    assert(all.find("write_error,0\n") != std::string::npos);
    assert(all.find("overwrite_attempt") == std::string::npos);
    std::printf("PASS: player transitions, raw timing precision, CSV schema, bounded writer accounting, shutdown drain and exclusive sessions (%u rows).\n", rows);
}
