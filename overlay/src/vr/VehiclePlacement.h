#pragma once
// Shared by the standalone editor and Quest. No RenderWare, Android or XR types.
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace VehiclePlacement {
struct Vec {
    float x, y, z;
    Vec(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec operator+(Vec b) const {
        return Vec(x + b.x, y + b.y, z + b.z);
    }
    Vec operator-(Vec b) const {
        return Vec(x - b.x, y - b.y, z - b.z);
    }
    Vec operator*(float s) const {
        return Vec(x * s, y * s, z * s);
    }
};
inline float dot(Vec a, Vec b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec cross(Vec a, Vec b) {
    return Vec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline Vec unit(Vec v) {
    float n = std::sqrt(dot(v, v));
    return n > 1e-7f ? v * (1 / n) : Vec();
}
inline Vec rotate(Vec v, Vec axis, float degrees) {
    axis = unit(axis);
    float a = degrees * 0.017453292519943295f, c = std::cos(a), s = std::sin(a);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1 - c));
}
struct Basis {
    Vec right, forward, up, position;
    Basis() : right(1, 0, 0), forward(0, 1, 0), up(0, 0, 1) {}
    Vec vector(Vec v) const {
        return right * v.x + forward * v.y + up * v.z;
    }
    Vec point(Vec v) const {
        return position + vector(v);
    }
    void turn(float pitch, float yaw, float roll) {
        forward = rotate(forward, right, pitch);
        up = rotate(up, right, pitch);
        right = rotate(right, up, yaw);
        forward = rotate(forward, up, yaw);
        right = rotate(right, forward, roll);
        up = rotate(up, forward, roll);
    }
};
inline void panelAxes(float right[3], float up[3], float normal[3], float pitch, float yaw, float roll) {
    Basis b;
    b.right = Vec(right[0], right[1], right[2]);
    b.up = Vec(up[0], up[1], up[2]);
    b.forward = Vec(-normal[0], -normal[1], -normal[2]);
    b.turn(pitch, yaw, roll);
    right[0] = b.right.x;
    right[1] = b.right.y;
    right[2] = b.right.z;
    up[0] = b.up.x;
    up[1] = b.up.y;
    up[2] = b.up.z;
    normal[0] = -b.forward.x;
    normal[1] = -b.forward.y;
    normal[2] = -b.forward.z;
}
inline Vec relativeAngles(const Basis &b, const Basis &parent) {
    Vec f(dot(b.forward, parent.right), dot(b.forward, parent.forward), dot(b.forward, parent.up));
    Vec r(dot(b.right, parent.right), dot(b.right, parent.forward), dot(b.right, parent.up));
    float y = std::asin(std::max(-1.f, std::min(1.f, -f.x))), x = std::atan2(f.z, f.y);
    Basis neutral;
    neutral.turn(x * 57.2957795f, y * 57.2957795f, 0);
    return Vec(x * 57.2957795f, y * 57.2957795f,
               std::atan2(-dot(r, neutral.up), dot(r, neutral.right)) * 57.2957795f);
}
inline std::string trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
typedef std::map<std::string, float> Values;
inline std::string section(int set, int model) {
    return std::string(set == 2   ? "xbox"
                       : set == 1 ? "modern"
                                  : "classic") +
           ":" + std::to_string(model);
}
inline bool limits(const std::string &key, float &lo, float &hi) {
    lo = -200;
    hi = 200;
    if (key == "WheelRadius") {
        lo = 8;
        hi = 40;
        return true;
    }
    if (key == "WheelVisible" || key == "DashFollowWheel") {
        lo = 0;
        hi = 1;
        return true;
    }
    if (key == "SeatHeight" || key == "DefaultSeatHeight") {
        lo = -200;
        hi = 250;
        return true;
    }
    if (key == "DashSize" || key.find("Size") != key.npos) {
        lo = 40;
        hi = 250;
        return key == "DashSize" || key == "MapSize" || key == "StatusSize" || key == "ClockSize" ||
               key == "AmmoSize" || key == "RadioSize" || key == "SpeedometerSize";
    }
    static const char *prefix[] = {"Wheel", "Dash", "Map", "Status", "Clock", "Ammo", "Radio", "Speedometer"};
    for (auto p : prefix) {
        std::string k = p;
        if (key == k + "Pitch" || key == k + "Yaw" || key == k + "Roll") {
            lo = k == "Wheel" ? -90.f : -180.f;
            hi = -lo;
            return true;
        }
        if (key == k + "X" || key == k + "Y" || key == k + "Z")
            return true;
    }
    return key == "SeatHeight" || key == "SeatDistance" || key == "DefaultSeatHeight" ||
           key == "DefaultSeatDistance";
}
inline bool validSection(const std::string &s) {
    auto p = s.find(':');
    if (p == s.npos ||
        (s.substr(0, p) != "classic" && s.substr(0, p) != "modern" && s.substr(0, p) != "xbox"))
        return false;
    std::string id = s.substr(p + 1);
    return !id.empty() && id.find_first_not_of("0123456789") == id.npos && id.size() < 6;
}
struct Pack {
    std::map<std::string, Values> entries;
    bool parse(std::istream &in, std::string &error) {
        Pack next;
        std::string line, sec;
        std::set<std::string> sections;
        bool schema = false;
        int count = 0;
        size_t bytes = 0;
        while (std::getline(in, line)) {
            bytes += line.size() + 1;
            if (++count > 100000 || line.size() > 4096 || bytes > 4 * 1024 * 1024) {
                error = "Placement pack exceeds limits";
                return false;
            }
            if (count == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0)
                line.erase(0, 3);
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;
            if (line[0] == '[' && line.back() == ']') {
                sec = line.substr(1, line.size() - 2);
                if ((sec != "Placement" && !validSection(sec)) || !sections.insert(sec).second) {
                    error = "Invalid or duplicate placement section";
                    return false;
                }
                continue;
            }
            auto p = line.find('=');
            if (p == line.npos) {
                error = "Invalid placement assignment";
                return false;
            }
            std::string key = trim(line.substr(0, p)), value = trim(line.substr(p + 1));
            if (sec == "Placement" && key == "Version") {
                if (schema || value != "1") {
                    error = "Unsupported or duplicate placement version";
                    return false;
                }
                schema = true;
                continue;
            }
            float lo, hi;
            char *end = 0;
            float f = std::strtof(value.c_str(), &end);
            if (!validSection(sec) || !limits(key, lo, hi) || end == value.c_str() || *end ||
                !std::isfinite(f) || f < lo || f > hi ||
                ((key == "WheelVisible" || key == "DashFollowWheel") && f != 0 && f != 1)) {
                error = "Invalid placement field or range at line " + std::to_string(count);
                return false;
            }
            if (next.entries[sec].count(key)) {
                error = "Duplicate placement field";
                return false;
            }
            next.entries[sec][key] = f;
        }
        if (!schema) {
            error = "Missing placement version";
            return false;
        }
        entries.swap(next.entries);
        error.clear();
        return true;
    }
    bool load(const std::string &path, std::string &error) {
        std::ifstream f(path.c_str());
        if (!f) {
            error = "Cannot open placement pack";
            return false;
        }
        return parse(f, error);
    }
    std::string text() const {
        std::ostringstream o;
        o << "[Placement]\nVersion=1\n" << std::setprecision(9);
        for (auto &e : entries) {
            if (e.second.empty())
                continue;
            o << "\n[" << e.first << "]\n";
            for (auto &v : e.second)
                o << v.first << "=" << v.second << "\n";
        }
        return o.str();
    }
    bool save(const std::string &path, std::string &error) const {
        std::string tmp = path + ".tmp", bak = path + ".bak";
        {
            std::ofstream f(tmp.c_str(), std::ios::binary | std::ios::trunc);
            f << text();
            f.close();
            if (!f) {
                error = "Cannot write placement pack";
                return false;
            }
        }
        Pack check;
        if (!check.load(tmp, error))
            return false;
        std::ifstream old(path.c_str());
        bool exists = old.good();
        old.close();
        if (exists) {
            std::remove(bak.c_str());
            if (std::rename(path.c_str(), bak.c_str())) {
                error = "Cannot back up placement pack";
                return false;
            }
        }
        if (std::rename(tmp.c_str(), path.c_str())) {
            if (exists)
                std::rename(bak.c_str(), path.c_str());
            error = "Cannot replace placement pack";
            return false;
        }
        error.clear();
        return true;
    }
    bool get(const std::string &s, const std::string &k, float &value) const {
        auto e = entries.find(s);
        if (e == entries.end())
            return false;
        auto v = e->second.find(k);
        if (v == e->second.end())
            return false;
        value = v->second;
        return true;
    }
};
struct Store {
    Pack bundled, templates, personal;
    float value(int set, int model, const std::string &key, float fallback, int *source = 0) const {
        std::string s = section(set, model);
        const Pack *layers[] = {&personal, &templates, &bundled};
        for (int i = 0; i < 3; i++)
            if (layers[i]->get(s, key, fallback)) {
                if (source)
                    *source = 3 - i;
                return fallback;
            }
        if (source)
            *source = 0;
        return fallback;
    }
    void load(const std::string &root) {
        std::string error;
        bundled.load(root + "/vehicle_placements.bundled.ini", error);
        templates.load(root + "/vehicle_placements.template.ini", error);
        personal.load(root + "/vehicle_placements.personal.ini", error);
    }
};
// Poll only completed, validated files. Malformed updates retain the last good
// generation; removed packs reveal the layer beneath them.
inline uint64_t contentHash(const std::string &text) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}
struct LiveStore {
    Store store;
    std::string root = ".", acknowledged;
    std::string personalRoot = ".";
    uint64_t revision = 0;
    std::string hashes[3];
    std::chrono::steady_clock::time_point lastPoll{};
    bool poll(bool force = false) {
        auto now = std::chrono::steady_clock::now();
        if (!force && now - lastPoll < std::chrono::milliseconds(250))
            return false;
        lastPoll = now;
        const char *names[] = {"bundled", "template", "personal"};
        Pack incoming[3];
        std::string nextHash[3];
        for (int i = 0; i < 3; i++) {
            std::string path = (i == 0 ? root : personalRoot) + "/vehicle_placements." + names[i] + ".ini", raw, error;
            struct stat info;
            if (stat(path.c_str(), &info) != 0) {
                if (errno != ENOENT)
                    return false;
            } else {
                if (info.st_size > 4 * 1024 * 1024)
                    return false;
                std::ifstream file(path, std::ios::binary);
                if (!file)
                    return false;
                std::ostringstream bytes;
                bytes << file.rdbuf();
                if (file.bad())
                    return false;
                raw = bytes.str();
                std::istringstream input(raw);
                if (!incoming[i].parse(input, error))
                    return false;
            }
            nextHash[i] = std::to_string(contentHash(raw));
        }
        bool changed = store.bundled.text() != incoming[0].text() ||
                       store.templates.text() != incoming[1].text() ||
                       store.personal.text() != incoming[2].text();
        if (changed) {
            store.bundled = incoming[0];
            store.templates = incoming[1];
            store.personal = incoming[2];
            ++revision;
        }
        std::string request;
        {
            std::ifstream file(personalRoot + "/vehicle_placements.reload");
            std::getline(file, request);
        }
        if (request.size() != 32 || request.find_first_not_of("0123456789abcdef") != request.npos)
            request.clear();
        bool acknowledge = request != acknowledged;
        for (int i = 0; i < 3; i++)
            acknowledge = acknowledge || hashes[i] != nextHash[i];
        if (acknowledge) {
            const std::string path = personalRoot + "/vehicle_placements.live.ini", temp = path + ".tmp";
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << "[LivePlacement]\nVersion=1\nRequest=" << request << "\n";
            for (int i = 0; i < 3; i++)
                out << names[i] << "=" << nextHash[i] << "\n";
            out.close();
            if (out) {
#ifdef _WIN32
                std::remove(path.c_str());
#endif
                if (std::rename(temp.c_str(), path.c_str()) == 0) {
                    acknowledged = request;
                    for (int i = 0; i < 3; i++)
                        hashes[i] = nextHash[i];
                }
            }
        }
        return changed;
    }
};
inline LiveStore &liveRuntime() {
    static LiveStore live;
    return live;
}
inline void configureRuntime(const std::string &root, const std::string &personalRoot = {}) {
    auto &live = liveRuntime();
    live.root = root;
    live.personalRoot = personalRoot.empty() ? root : personalRoot;
    live.lastPoll = {};
    live.poll(true);
}
inline Store &runtime() {
    auto &live = liveRuntime();
    live.poll();
    return live.store;
}
inline std::string personalPath() {
    return liveRuntime().personalRoot + "/vehicle_placements.personal.ini";
}
inline bool savePersonal(int set, int model, const std::string &key, float value) {
    Store &s = runtime();
    Pack next = s.personal;
    next.entries[section(set, model)][key] = value;
    std::string error;
    if (!next.save(personalPath(), error))
        return false;
    s.personal = next;
    return true;
}
inline const char *panelName(int n) {
    static const char *names[] = {"Map", "Status", "Clock", "Ammo"};
    return names[n < 0 || n > 3 ? 0 : n];
}
inline Values legacyDefaults(int set, int model, bool boat) {
    // Effective category + model values, matching QuestDrivingVR's shipped tables.
    Values v = {{"WheelX", set == 0 ? -1.f : 0.f},
                {"WheelY", set == 0 ? 4.f : 0.f},
                {"WheelZ", set == 0 ? 8.f : 0.f},
                {"WheelRadius", set == 1 ? 17.f : 18.f},
                {"WheelPitch", 0},
                {"WheelYaw", 0},
                {"WheelRoll", 0},
                {"WheelVisible", 1},
                {"SeatHeight", boat ? (set == 1 ? 10.f : 15.f) : (set == 1 ? 5.f : 0.f)},
                {"SeatDistance", 0},
                {"DefaultSeatHeight", set == 2 && !boat ? 0.f : 15.f},
                {"DefaultSeatDistance", 0},
                {"DashFollowWheel", 1},
                {"DashX", 0},
                {"DashY", 0},
                {"DashZ", 0},
                {"DashPitch", 0},
                {"DashYaw", 0},
                {"DashRoll", 0},
                {"DashSize", 100}};
    if (boat) {
        v["WheelX"] = v["WheelY"] = v["WheelZ"] = 0;
        v["WheelRadius"] = 18;
    }
    // GTA III IDs: Stinger 92, Infernus 101, Banshee 119, Speeder 142.
    if (set == 1 && model == 92) {
        v["SeatDistance"] += 8;
        v["SeatHeight"] += 5;
        v["WheelY"] += 19;
        v["WheelZ"] += 9;
        v["WheelRadius"] = 21;
        v["WheelPitch"] = -21;
        v["WheelVisible"] = 0;
    }
    if (set == 1 && model == 101) {
        v["WheelX"] += 1;
        v["WheelY"] -= 5;
        v["WheelZ"] += 4;
        v["WheelPitch"] = -18;
        v["WheelVisible"] = 0;
    }
    if (set == 1 && model == 119) {
        v["SeatHeight"] += 6;
        v["WheelY"] += 2;
        v["WheelZ"] += 12;
        v["WheelRadius"] = 18;
        v["WheelPitch"] = -25;
        v["WheelVisible"] = 0;
    }
    if (model == 142) {
        v["WheelX"] += 3;
        v["WheelY"] += 4;
        v["WheelZ"] += set == 1 ? 9 : 12;
        v["WheelRadius"] = set == 1 ? 16 : 14;
        v["WheelPitch"] = set == 1 ? -13 : -11;
        v["WheelYaw"] = set == 1 ? 3 : 0;
        v["WheelVisible"] = 0;
    }
    const float panels[4][4] = {{-9.6f, -7.4f, 2.3f, 55},
                                {7.6f, -7.4f, 3.3f, 100},
                                {9.7f, -7.4f, -.9f, 100},
                                {-9.6f, -7.4f, -2.f, 70}};
    for (int i = 0; i < 4; i++) {
        std::string p = panelName(i);
        v[p + "X"] = panels[i][0];
        v[p + "Y"] = panels[i][1];
        v[p + "Z"] = panels[i][2];
        v[p + "Size"] = panels[i][3];
        v[p + "Pitch"] = v[p + "Yaw"] = v[p + "Roll"] = 0;
    }
    v["RadioX"] = 35;
    v["RadioY"] = 60;
    v["RadioZ"] = 30;
    v["RadioPitch"] = v["RadioYaw"] = v["RadioRoll"] = 0;
    v["RadioSize"] = 100;
    // One bottom-centre pivot for both speedometer styles, in dashboard space.
    v["SpeedometerX"] = 0;
    v["SpeedometerY"] = 8;
    v["SpeedometerZ"] = 22;
    v["SpeedometerPitch"] = v["SpeedometerYaw"] = v["SpeedometerRoll"] = 0;
    v["SpeedometerSize"] = 100;
    return v;
}
inline Basis radio(Vec seat, bool boat, Vec offset, Vec angles) {
    Basis b;
    if (!boat)
        seat.x = -seat.x;
    b.position = seat + offset * .01f;
    b.turn(angles.x, angles.y, angles.z);
    return b;
}
inline Basis radio(Vec seat, bool boat, const Values &v) {
    return radio(seat, boat, Vec(v.at("RadioX"), v.at("RadioY"), v.at("RadioZ")),
                 Vec(v.at("RadioPitch"), v.at("RadioYaw"), v.at("RadioRoll")));
}
inline Values resolve(const Store &s, int set, int model, Values fallback) {
    for (auto &v : fallback)
        v.second = s.value(set, model, v.first, v.second);
    return fallback;
}
struct WheelSpec {
    Vec offset, angles;
    float radius = 18;
};
struct DashboardSpec {
    bool followWheel = true;
    Vec offset, angles;
};
inline WheelSpec resolveWheel(const Store &s, int set, int model, WheelSpec v) {
    v.offset.x = s.value(set, model, "WheelX", v.offset.x);
    v.offset.y = s.value(set, model, "WheelY", v.offset.y);
    v.offset.z = s.value(set, model, "WheelZ", v.offset.z);
    v.angles.x = s.value(set, model, "WheelPitch", v.angles.x);
    v.angles.y = s.value(set, model, "WheelYaw", v.angles.y);
    v.angles.z = s.value(set, model, "WheelRoll", v.angles.z);
    v.radius = s.value(set, model, "WheelRadius", v.radius);
    return v;
}
inline DashboardSpec resolveDashboard(const Store &s, int set, int model) {
    DashboardSpec v;
    v.followWheel = s.value(set, model, "DashFollowWheel", 1) != 0;
    v.offset = Vec(s.value(set, model, "DashX", 0), s.value(set, model, "DashY", 0),
                   s.value(set, model, "DashZ", 0));
    v.angles = Vec(s.value(set, model, "DashPitch", 0), s.value(set, model, "DashYaw", 0),
                   s.value(set, model, "DashRoll", 0));
    return v;
}
inline Basis wheel(Vec frontSeat, bool boat, const WheelSpec &v) {
    Basis b;
    if (!boat)
        frontSeat.x = -frontSeat.x;
    b.position = frontSeat + Vec(v.offset.x * .01f, .33f + v.offset.y * .01f, .30f + v.offset.z * .01f);
    b.turn(v.angles.x, v.angles.y, v.angles.z);
    return b;
}
inline Basis wheel(Vec frontSeat, bool boat, const Values &v) {
    WheelSpec spec;
    spec.offset = Vec(v.at("WheelX"), v.at("WheelY"), v.at("WheelZ"));
    spec.angles = Vec(v.at("WheelPitch"), v.at("WheelYaw"), v.at("WheelRoll"));
    spec.radius = v.at("WheelRadius");
    return wheel(frontSeat, boat, spec);
}
inline Basis dashboard(const Basis &wheel, const DashboardSpec &v) {
    Basis b;
    if (v.followWheel)
        b = wheel;
    b.position = b.point(v.offset * .01f);
    b.turn(v.angles.x, v.angles.y, v.angles.z);
    return b;
}
inline Basis dashboard(const Basis &wheel, const Values &v) {
    DashboardSpec spec;
    spec.followWheel = v.at("DashFollowWheel") != 0;
    spec.offset = Vec(v.at("DashX"), v.at("DashY"), v.at("DashZ"));
    spec.angles = Vec(v.at("DashPitch"), v.at("DashYaw"), v.at("DashRoll"));
    return dashboard(wheel, spec);
}
inline Basis namedPanel(const Basis &anchor, const Values &v, const std::string &p) {
    Basis b = anchor;
    b.position = anchor.point(Vec(v.at(p + "X"), v.at(p + "Y"), v.at(p + "Z")) *
                              (.01f * v.at("DashSize") * .01f));
    b.turn(v.at(p + "Pitch"), v.at(p + "Yaw"), v.at(p + "Roll"));
    return b;
}
inline Basis panel(const Basis &anchor, const Values &v, int n) {
    return namedPanel(anchor, v, panelName(n));
}
} // namespace VehiclePlacement
