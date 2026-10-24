#pragma once
#include "VehiclePlacement.h"
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <vector>

namespace editor {
namespace fs = std::filesystem;
inline std::string lower(std::string s) {
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}
inline fs::path casePath(fs::path root, std::string relative) {
    std::replace(relative.begin(), relative.end(), '\\', '/');
    for (auto &part : fs::path(relative)) {
        std::string name = part.string();
        if (name == ".." || part.is_absolute())
            throw std::runtime_error("Unsafe asset path");
        fs::path direct = root / part;
        if (fs::exists(direct)) {
            root = direct;
            continue;
        }
        bool found = false;
        if (fs::is_directory(root))
            for (auto &e : fs::directory_iterator(root))
                if (lower(e.path().filename().string()) == lower(name)) {
                    root = e.path();
                    found = true;
                    break;
                }
        if (!found)
            return direct;
    }
    return root;
}
inline std::vector<uint8_t> bytes(fs::path p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        return {};
    auto n = f.tellg();
    if (n < 0 || n > 256 * 1024 * 1024)
        throw std::runtime_error("Asset exceeds 256 MiB limit");
    std::vector<uint8_t> b((size_t)n);
    f.seekg(0);
    if (n && !f.read((char *)b.data(), n))
        throw std::runtime_error("Truncated asset");
    return b;
}
inline uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
struct Archive {
    fs::path img;
    std::map<std::string, std::pair<uint64_t, uint64_t>> index;
    void open(fs::path dir) {
        img = casePath(dir, "gta3.img");
        auto b = bytes(casePath(dir, "gta3.dir"));
        index.clear();
        if (b.size() % 32)
            throw std::runtime_error("Invalid IMG directory");
        if (!fs::exists(img))
            return;
        auto size = fs::file_size(img);
        for (size_t i = 0; i < b.size(); i += 32) {
            uint64_t off = uint64_t(u32(&b[i])) * 2048, n = uint64_t(u32(&b[i + 4])) * 2048;
            std::string name((char *)&b[i + 8], 24);
            name = name.substr(0, name.find('\0'));
            if (off > size || n > size - off || n > 256 * 1024 * 1024)
                throw std::runtime_error("IMG entry outside archive");
            index[lower(name)] = {off, n};
        }
    }
    std::vector<uint8_t> read(const std::string &name) const {
        auto e = index.find(lower(name));
        if (e == index.end())
            return {};
        std::ifstream f(img, std::ios::binary);
        std::vector<uint8_t> b((size_t)e->second.second);
        f.seekg(e->second.first);
        if (!f.read((char *)b.data(), b.size()))
            throw std::runtime_error("Cannot read IMG entry");
        return b;
    }
};
struct Vehicle {
    int id = 0, wheel = -1;
    float wheelScale = 1;
    std::string name, txd, type;
    bool supported() const {
        return (type == "car" || type == "boat") && id != 131 && id != 141;
    }
    bool boat() const {
        return type == "boat";
    }
};
struct Assets {
    fs::path root;
    Archive archives[3];
    bool available[3] = {false, false, false};
    std::vector<Vehicle> vehicles;
    std::map<int, std::pair<std::string, std::string>> objects;
    std::vector<std::array<int, 3>> colors;
    std::map<std::string, std::pair<int, int>> vehicleColors;
    void ide(fs::path path) {
        std::ifstream f(path);
        std::string line, section;
        while (std::getline(f, line)) {
            line = line.substr(0, line.find('#'));
            line = VehiclePlacement::trim(line);
            if (line.empty())
                continue;
            if (line.find(',') == line.npos) {
                section = lower(line);
                continue;
            }
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream in(line);
            if (section == "cars") {
                Vehicle v;
                // GTA III has no animation-group column here. Boats, trains
                // and helicopters stop after component rules; cars/bikes also
                // provide the wheel/steering ID and scale columns.
                std::string handling, label, klass, freq, level, rules;
                if (!(in >> v.id >> v.name >> v.txd >> v.type >> handling >> label >> klass >> freq >>
                      level >> rules))
                    continue;
                v.name = lower(v.name);
                v.txd = lower(v.txd);
                v.type = lower(v.type);
                if ((v.type == "car" || v.type == "bike") && !(in >> v.wheel >> v.wheelScale))
                    continue;
                auto old = std::find_if(vehicles.begin(), vehicles.end(),
                                        [&](const Vehicle &a) { return a.id == v.id; });
                if (old == vehicles.end())
                    vehicles.push_back(v);
                else
                    *old = v;
            } else if (section == "objs") {
                int id;
                std::string name, txd;
                if (in >> id >> name >> txd)
                    objects[id] = {lower(name), lower(txd)};
            }
        }
    }
    void open(fs::path path) {
        root = fs::absolute(path);
        vehicles.clear();
        objects.clear();
        colors.clear();
        vehicleColors.clear();
        ide(casePath(root, "data/default.ide"));
        std::ifstream dat(casePath(root, "data/gta3.dat"));
        std::string line;
        while (std::getline(dat, line)) {
            std::istringstream in(line);
            std::string type, path;
            if (in >> type >> path && lower(type) == "ide")
                ide(casePath(root, path));
        }
        if (vehicles.empty())
            throw std::runtime_error("No GTA III vehicle definitions found");
        std::ifstream carcols(casePath(root, "data/carcols.dat"));
        std::string colorSection;
        while (std::getline(carcols, line)) {
            line = VehiclePlacement::trim(line.substr(0, line.find('#')));
            if (line.empty())
                continue;
            std::replace(line.begin(), line.end(), ',', ' ');
            if (line == "col" || line == "car" || line == "end") {
                colorSection = line;
                continue;
            }
            std::istringstream in(line);
            if (colorSection == "col") {
                std::array<int, 3> rgb;
                if (in >> rgb[0] >> rgb[1] >> rgb[2])
                    colors.push_back(rgb);
            }
            if (colorSection == "car") {
                std::string name;
                int a, b;
                if (in >> name >> a >> b)
                    vehicleColors[lower(name)] = {a, b};
            }
        }
        for (int set = 0; set < 3; set++) {
            fs::path p =
                set == 0 ? root
                         : casePath(root, std::string("modelsets/") + (set == 1 ? "modern" : "xbox"));
            archives[set].open(casePath(p, "models"));
            available[set] = !archives[set].index.empty();
        }
        std::sort(vehicles.begin(), vehicles.end(),
                  [](const Vehicle &a, const Vehicle &b) { return a.name < b.name; });
    }
    std::vector<uint8_t> read(int set, const std::string &name, int *source = nullptr) const {
        if (set > 0) {
            fs::path p = casePath(root, std::string("modelsets/") + (set == 1 ? "modern" : "xbox") +
                                            "/models/" + name);
            auto b = bytes(p);
            if (b.empty())
                b = bytes(casePath(root, std::string("modelsets/") + (set == 1 ? "modern" : "xbox") +
                                             "/models/generic/" + name));
            if (b.empty())
                b = archives[set].read(name);
            if (!b.empty()) {
                if (source)
                    *source = set;
                return b;
            }
        }
        if (source)
            *source = 0;
        auto b = bytes(casePath(root, "models/" + name));
        if (b.empty())
            b = archives[0].read(name);
        if (b.empty())
            b = bytes(casePath(root, "models/generic/" + name));
        return b;
    }
    std::vector<uint8_t> common(int set, const std::string &name) const {
        // MODELFILE assets use loose models/generic paths in re3; they are not
        // individual streamed vehicle entries in gta3.img.
        if (set > 0) {
            auto data = bytes(casePath(root, std::string("modelsets/") + (set == 1 ? "modern" : "xbox") +
                                                 "/models/generic/" + name));
            if (!data.empty())
                return data;
        }
        return bytes(casePath(root, "models/generic/" + name));
    }
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> wheelData(int set, int wheel) const {
        auto object = objects.find(wheel);
        if (object == objects.end())
            return {};
        auto geometry = common(set, "wheels.dff");
        if (geometry.empty())
            geometry = read(set, object->second.first + ".dff");
        auto texture = common(set, "wheels.txd");
        if (texture.empty() && object->second.second != "generic")
            texture = read(set, object->second.second + ".txd");
        return {std::move(geometry), std::move(texture)};
    }
};
inline std::string fingerprint(const std::vector<std::vector<uint8_t>> &parts) {
    uint64_t h = 14695981039346656037ull;
    for (auto &b : parts) {
        for (auto c : b) {
            h ^= c;
            h *= 1099511628211ull;
        }
        h ^= b.size();
        h *= 1099511628211ull;
    }
    std::ostringstream o;
    o << std::hex << h;
    return o.str();
}
// A permissive reader for the existing game's INI; never writes that file.
struct Legacy {
    std::map<std::string, std::map<std::string, float>> entries;
    void load(fs::path path) {
        entries.clear();
        std::ifstream f(path);
        std::string line, s;
        while (std::getline(f, line)) {
            line = VehiclePlacement::trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;
            if (line[0] == '[' && line.back() == ']') {
                s = line.substr(1, line.size() - 2);
                continue;
            }
            auto p = line.find('=');
            if (p == line.npos)
                continue;
            char *end = nullptr;
            std::string value = VehiclePlacement::trim(line.substr(p + 1));
            float n = std::strtof(value.c_str(), &end);
            if (end != value.c_str() && !*end && std::isfinite(n))
                entries[s][VehiclePlacement::trim(line.substr(0, p))] = n;
        }
    }
    float value(const std::string &s, const std::string &k, float fallback) const {
        auto a = entries.find(s);
        if (a == entries.end())
            return fallback;
        auto b = a->second.find(k);
        return b == a->second.end() ? fallback : b->second;
    }
    VehiclePlacement::Values resolve(int set, const Vehicle &car) const {
        using namespace VehiclePlacement;
        Values v = legacyDefaults(set, car.id, car.boat());
        std::string cat = car.boat() ? "Boat" : "Car";
        const char *fields[] = {"WheelX",
                                "WheelY",
                                "WheelZ",
                                "WheelRadius",
                                "WheelPitch",
                                "WheelYaw",
                                "WheelRoll",
                                "SeatHeight",
                                "SeatDistance",
                                "DefaultSeatHeight",
                                "DefaultSeatDistance",
                                "WheelVisible"};
        const char *keys[] = {"WheelCenterXCm",      "WheelCenterYCm",        "WheelCenterZCm",
                              "WheelRadiusV2Cm",     "WheelPitchHalfDeg",     "WheelYawHalfDeg",
                              "WheelRollHalfDeg",    "SeatHeightCm",          "SeatDistanceCm",
                              "DefaultSeatHeightCm", "DefaultSeatDistanceCm", "VirtualWheelVisibility"};
        for (int i = 0; i < 12; i++) {
            float factor = i >= 4 && i <= 6 ? .5f : 1.f;
            std::string base = "VehicleView_" + std::to_string(car.id),
                        modern = "VehicleViewModern_" + std::to_string(car.id);
            float model = value(base, keys[i], 99999);
            if (set == 1)
                model = value(modern, keys[i], model);
            if (i == 11) {
                if (model != 99999)
                    v[fields[i]] = model == 0 ? 0.f : 1.f;
                continue;
            }
            float category = value("VR", cat + keys[i], 99999);
            if (set == 1)
                category = value("VR", "Modern" + cat + keys[i], category);
            Values categoryDefaults = legacyDefaults(set, -1, car.boat());
            float c = categoryDefaults[fields[i]];
            if (i == 9 || i == 10) {
                float fallback =
                    i == 9 ? (set == 2 && !car.boat() ? 0 : value("VR", "DrivingYOffsetCm", 15)) : 0;
                category = value(
                    "VR", std::string("Default") + cat + (i == 9 ? "SeatHeightCm" : "SeatDistanceCm"),
                    fallback);
            }
            if (i == 7)
                category =
                    value("VR", set == 1 ? "Modern" + cat + "SeatHeightCm" : cat + "SeatHeightCm",
                          value("VR", cat + "SeatHeightCm", value("VR", "DrivingYOffsetCm", category)));
            if (i == 3) {
                if (category != 99999)
                    c = std::max(8.f, std::min(40.f, category));
                if (model != 99999)
                    v[fields[i]] = model > 0 ? std::max(8.f, std::min(40.f, model)) : c;
                else if (v[fields[i]] == categoryDefaults[fields[i]])
                    v[fields[i]] = c;
                continue;
            }
            if (category != 99999) {
                v[fields[i]] += category * factor - c;
                c = category * factor;
            }
            if (model != 99999)
                v[fields[i]] = c + model * factor;
        }
        const char *names[] = {"Map", "Status", "Clock", "Ammo"};
        const char *axis[] = {"Across", "Along", "Lift", "Pitch", "Yaw", "Roll", "Size"};
        const char *out[] = {"X", "Y", "Z", "Pitch", "Yaw", "Roll", "Size"};
        for (auto p : names)
            for (int a = 0; a < 7; a++) {
                std::string prefix = std::string("Wrist") + (std::string(p) == "Map" ? "" : p);
                bool inner = value("VR", prefix + "Underside", std::string(p) == "Clock" ? 0 : 1) != 0;
                std::string key = prefix + "Car" + (inner ? "Inner" : "Outer") + axis[a];
                // Legacy dashboard ignores wrist rotations.
                if (value("VR", "VehicleHudAnchorVersion", 0) >= 1 &&
                    value("VR", prefix + "CarCustom", 0) != 0)
                    v[std::string(p) + out[a]] =
                        a >= 3 && a <= 5
                            ? 0
                            : value("VR", key, v[std::string(p) + out[a]] * (a == 6 ? 1 : 10)) *
                                  (a == 6 ? 1 : .1f);
            }
        return v;
    }
};
} // namespace editor
