#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

namespace editor {
struct Preferences {
    struct Number {
        float value, low, high;
        bool integral;
    };
    std::map<std::string, std::string> paths = {
        {"GameFolder", ""}, {"WorkspaceFolder", ""}, {"Python", ""}, {"Manager", ""}, {"ADB", ""},
        {"PCVRData", ""}, {"PCVRUser", ""}};
    std::map<std::string, Number> numbers = {{"EffectivePreview", {1, 0, 1, true}},
                                             {"TransferTarget", {0, 0, 1, true}},
                                             {"AutoResetCamera", {0, 0, 1, true}},
                                             {"Orbit", {0, 0, 1, true}},
                                             {"VSync", {1, 0, 1, true}},
                                             {"FPSLimit", {0, 0, 1000, true}},
                                             {"InvertX", {0, 0, 1, true}},
                                             {"InvertY", {0, 0, 1, true}},
                                             {"PanSensitivity", {1, .1f, 3, false}},
                                             {"RotationSensitivity", {.5f, .1f, 3, false}},
                                             {"FieldOfView", {80, 35, 115, false}},
                                             {"EyeAboveSeat", {.68f, .3f, 1, false}},
                                             {"BackgroundR", {37.f / 255, 0, 1, false}},
                                             {"BackgroundG", {43.f / 255, 0, 1, false}},
                                             {"BackgroundB", {53.f / 255, 0, 1, false}},
                                             {"PreviewHUD", {1, 0, 1, true}},
                                             {"PreviewWheel", {1, 0, 1, true}},
                                             {"Modelset", {0, 0, 2, true}},
                                             {"EditLayer", {0, 0, 1, true}},
                                             {"Element", {0, 0, 7, true}},
                                             {"SpeedometerAnalog", {0, 0, 1, true}},
                                             {"SpeedometerMetric", {0, 0, 1, true}},
                                             {"SpeedometerPreviewKmh", {77, 0, 360, false}},
                                             {"Gizmo", {0, 0, 1, true}},
                                             {"CameraInitialized", {0, 0, 1, true}},
                                             {"CameraX", {0, -100000, 100000, false}},
                                             {"CameraY", {0, -100000, 100000, false}},
                                             {"CameraZ", {0, -100000, 100000, false}},
                                             {"CameraYaw", {0, -100000, 100000, false}},
                                             {"CameraPitch", {0, -1.5f, 1.5f, false}}};
    float get(const std::string &key) const {
        return numbers.at(key).value;
    }
    void set(const std::string &key, float value) {
        auto &n = numbers.at(key);
        n.value = std::max(n.low, std::min(n.high, value));
    }
    static std::string trim(const std::string &s) {
        auto first = s.find_first_not_of(" \t\r\n");
        return first == s.npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
    }
    bool parse(std::istream &in, std::string &error) {
        Preferences next;
        std::string line, section;
        bool version = false;
        size_t bytes = 0;
        while (std::getline(in, line)) {
            if ((bytes += line.size() + 1) > 65536) {
                error = "Application settings exceed 64 KiB";
                return false;
            }
            if (bytes == line.size() + 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0)
                line.erase(0, 3);
            line = trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;
            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }
            if (section != "Application")
                continue;
            auto equals = line.find('=');
            if (equals == line.npos) {
                error = "Malformed application setting";
                return false;
            }
            auto key = trim(line.substr(0, equals)), value = trim(line.substr(equals + 1));
            if (key == "Version") {
                if (value != "1") {
                    error = "Unsupported application settings version";
                    return false;
                }
                version = true;
            } else if (next.paths.count(key)) {
                if (value.size() > 1023) {
                    error = "Application path is too long";
                    return false;
                }
                next.paths[key] = value;
            } else if (next.numbers.count(key)) {
                auto &number = next.numbers.at(key);
                std::istringstream stream(value);
                float v;
                char extra;
                if (!(stream >> v) || (stream >> extra) || !std::isfinite(v) || v < number.low ||
                    v > number.high || (number.integral && v != std::floor(v))) {
                    error = "Invalid application setting: " + key;
                    return false;
                }
                number.value = v;
            }
        }
        if (in.bad() || !version) {
            error = "Missing or unreadable application settings version";
            return false;
        }
        *this = next;
        error.clear();
        return true;
    }
    std::string text() const {
        std::ostringstream out;
        out << "[Application]\nVersion=1\n" << std::setprecision(9);
        for (auto &p : paths)
            out << p.first << "=" << p.second << "\n";
        for (auto &n : numbers)
            out << n.first << "=" << n.second.value << "\n";
        return out.str();
    }
    bool load(const std::filesystem::path &path, std::string &error) {
        std::ifstream file(path);
        if (!file) {
            error = "Cannot read application settings";
            return false;
        }
        return parse(file, error);
    }
    bool save(const std::filesystem::path &path, std::string &error) const {
        auto temp = path;
        temp += ".tmp";
        auto backup = path;
        backup += ".bak";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << text();
            out.close();
            if (!out) {
                error = "Cannot write application settings beside the executable";
                return false;
            }
        }
        Preferences check;
        if (!check.load(temp, error))
            return false;
        std::error_code ec;
        bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            error = "Cannot inspect application settings";
            return false;
        }
        if (exists) {
            std::filesystem::remove(backup, ec);
            ec.clear();
            std::filesystem::rename(path, backup, ec);
            if (ec) {
                error = "Cannot back up application settings";
                return false;
            }
        }
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            if (exists) {
                std::error_code ignored;
                std::filesystem::rename(backup, path, ignored);
            }
            error = "Cannot replace application settings";
            return false;
        }
        error.clear();
        return true;
    }
};
} // namespace editor
