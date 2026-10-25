#include "Assets.h"
#include "History.h"
#include "Preferences.h"
#include <iostream>
#include <stdexcept>
using namespace VehiclePlacement;
static void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void near(Vec a, Vec b) {
    check(dot(a - b, a - b) < 1e-9f, "transform mismatch");
}
int main() {
    try {
        // Radio keeps its seat-relative frame independent of wheel/dashboard/steering.
        auto radioValues = legacyDefaults(0, 95, false);
        auto rb = radio(Vec(1, 2, 3), false, radioValues);
        near(rb.position, Vec(-.65f, 2.6f, 3.3f));
        near(radio(Vec(1, 2, 3), true, radioValues).position, Vec(1.35f, 2.6f, 3.3f));
        Vec radioRight(1, 0, 0), u(0, 0, 1), n(0, -1, 0);
        auto oldRotate = [](Vec &a, Vec &b, float degrees) {
            float t = degrees * .01745329252f;
            Vec old = a;
            a = a * std::cos(t) + b * std::sin(t);
            b = b * std::cos(t) - old * std::sin(t);
        };
        oldRotate(u, n, 23);
        oldRotate(n, radioRight, -37);
        oldRotate(radioRight, u, 51);
        auto turned = radio(Vec(), false, Vec(), Vec(23, -37, -51));
        near(turned.right, radioRight);
        near(turned.up, u);
        near(turned.forward, n * (-1));
        editor::Legacy oldRadio, oldDefaults;
        oldDefaults.entries["RadioBoard"]["Version"] = 1;
        oldDefaults.entries["Global"] = {{"Enabled", 1}, {"Xmm", 450}, {"RollDeg", 22}};
        oldRadio.entries["RadioBoard"]["Version"] = 1;
        oldRadio.entries["Global"] = {{"Enabled", 1}, {"Xmm", 500}};
        oldRadio.entries["Vehicle.95"] = {{"Enabled", 1}, {"Xmm", 600}, {"RollDeg", 31}};
        oldRadio.radio(95, radioValues, oldDefaults);
        check(radioValues["RadioX"] == 60 && radioValues["RadioRoll"] == -31,
              "radio model precedence and units");
        oldRadio.radio(96, radioValues, oldDefaults);
        check(radioValues["RadioX"] == 50, "radio global fallback");
        oldRadio.entries["Global"]["Enabled"] = 0;
        oldRadio.radio(96, radioValues, oldDefaults);
        check(radioValues["RadioX"] == 45 && radioValues["RadioRoll"] == -22, "radio bundled fallback");
        Pack radioPack;
        std::string radioError;
        radioPack.entries["xbox:95"] = {
            {"RadioX", 35}, {"RadioPitch", 45}, {"RadioRoll", -180}, {"RadioSize", 125}};
        std::istringstream radioInput(radioPack.text());
        check(radioPack.parse(radioInput, radioError), "radio pack roundtrip");
        editor::Preferences prefs;
        prefs.paths["GameFolder"] = "D:/GTA III = local; assets";
        prefs.paths["WorkspaceFolder"] = "/home/player/placements";
        prefs.set("InvertX", 1);
        prefs.set("FPSLimit", 144);
        prefs.set("VSync", 0);
        prefs.set("AutoResetCamera", 1);
        prefs.set("CameraX", -5.25f);
        editor::Preferences loaded;
        std::string prefError;
        std::istringstream prefInput(prefs.text());
        check(loaded.parse(prefInput, prefError) && loaded.text() == prefs.text(),
              "application preferences roundtrip");
        for (auto invalid :
             {"[Application]\nVersion=2\n", "[Application]\nVersion=1\nFPSLimit=-1\n",
              "[Application]\nVersion=1\nInvertY=.5\n", "[Application]\nVersion=1\nPanSensitivity=nan\n",
              "[Application]\nVersion=1\nFPSLimit=1001\n"}) {
            auto before = loaded.text();
            std::istringstream input(invalid);
            check(!loaded.parse(input, prefError) && loaded.text() == before,
                  "invalid application settings preserve current state");
        }
        std::istringstream partial("[Application]\nVersion=1\nFPSLimit=60\n");
        check(loaded.parse(partial, prefError) && loaded.get("FPSLimit") == 60 &&
                  loaded.get("RotationSensitivity") == .5f,
              "partial application settings inherit defaults");
        Store s;
        auto key = section(2, 95);
        s.bundled.entries[key]["WheelX"] = 1;
        s.templates.entries[key]["WheelX"] = 2;
        s.personal.entries[key]["WheelX"] = 3;
        int source = 0;
        check(s.value(2, 95, "WheelX", 0, &source) == 3 && source == 3, "personal precedence");
        s.personal.entries[key].erase("WheelX");
        check(s.value(2, 95, "WheelX", 0) == 2, "reset inheritance");
        check(s.value(0, 95, "WheelX", 7) == 7, "modelset isolation");
        check(s.value(2, 96, "WheelX", 7) == 7, "vehicle isolation");
        check(s.value(2, 95, "WheelY", 8) == 8, "partial override");
        Pack p;
        std::string error;
        std::istringstream in(s.templates.text());
        check(p.parse(in, error) && p.entries == s.templates.entries, "round trip");
        std::istringstream bom(std::string("\xef\xbb\xbf") + s.templates.text());
        check(p.parse(bom, error), "Windows UTF-8 BOM import");
        const char *bad[] = {"[Placement]\nVersion=2\n",
                             "[xbox:95]\nWheelX=2\n",
                             "[Placement]\nVersion=1\n[xbox:95]\nWheelX=nan\n",
                             "[Placement]\nVersion=1\n[xbox:95]\nWheelRadius=0\n",
                             "[Placement]\nVersion=1\n[xbox:95]\nWheelVisible=.5\n",
                             "[Placement]\nVersion=1\n[xbox:95]\nWheelX=2\nWheelX=3\n"};
        for (auto text : bad) {
            auto original = p.entries;
            std::istringstream f(text);
            check(!p.parse(f, error) && p.entries == original, "malformed pack must be transactional");
        }
        std::istringstream duplicate(
            "[Placement]\nVersion=1\n[xbox:95]\nWheelX=1\n[xbox:95]\nWheelY=2\n");
        check(!p.parse(duplicate, error), "duplicate sections match device parser rejection");
        Values v = legacyDefaults(0, 95, false);
        Vec seat(.4f, .2f, .1f);
        auto w = wheel(seat, false, v);
        near(w.position, Vec(-.41f, .57f, .48f));
        near(wheel(seat, true, v).position, Vec(.39f, .57f, .48f));
        v["WheelPitch"] = 23;
        v["WheelYaw"] = -11;
        v["WheelRoll"] = 19;
        w = wheel(seat, false, v);
        check(std::abs(dot(w.right, w.forward)) < 1e-6f, "orthogonal wheel axes");
        near(cross(w.right, w.forward), w.up);
        auto angles = relativeAngles(w, Basis());
        near(angles, Vec(23, -11, 19));
        Basis recovered;
        recovered.turn(angles.x, angles.y, angles.z);
        near(recovered.right, w.right);
        near(recovered.forward, w.forward);
        v["DashFollowWheel"] = 0;
        v["DashX"] = 10;
        v["DashY"] = 20;
        v["DashZ"] = 30;
        auto d = dashboard(w, v);
        near(d.position, Vec(.1f, .2f, .3f));
        v["WheelX"] += 25;
        near(dashboard(wheel(seat, false, v), v).position, d.position);
        v["DashFollowWheel"] = 1;
        check(dot(dashboard(w, v).position - d.position, dashboard(w, v).position - d.position) > .01f,
              "linked anchor");
        v["DashPitch"] = 12;
        v["DashYaw"] = -19;
        v["DashRoll"] = 7;
        auto linked = dashboard(w, v);
        auto absoluteAngles = relativeAngles(linked, Basis());
        v["DashFollowWheel"] = 0;
        v["DashX"] = linked.position.x * 100;
        v["DashY"] = linked.position.y * 100;
        v["DashZ"] = linked.position.z * 100;
        v["DashPitch"] = absoluteAngles.x;
        v["DashYaw"] = absoluteAngles.y;
        v["DashRoll"] = absoluteAngles.z;
        auto fixed = dashboard(w, v);
        near(fixed.position, linked.position);
        near(fixed.right, linked.right);
        near(fixed.forward, linked.forward);
        v["MapPitch"] = 30;
        auto b = panel(d, v, 0);
        near(b.forward, rotate(d.forward, d.right, 30));
        float r[3] = {1, 0, 0}, up[3] = {0, 0, 1}, normal[3] = {0, -1, 0};
        panelAxes(r, up, normal, 90, 0, 0);
        near(Vec(up[0], up[1], up[2]), Vec(0, -1, 0));
        near(Vec(normal[0], normal[1], normal[2]), Vec(0, 0, -1));
        v["DashSize"] = 200;
        auto scaled = panel(d, v, 0);
        near(scaled.position - d.position, (b.position - d.position) * 2);
        auto root = editor::fs::temp_directory_path() / "lcvr-placement-unit";
        editor::fs::create_directories(root);
        {
            std::ofstream f(root / "vehicles.ide");
            f << "cars\n"
                 "95, testcar, testcar, car, HANDLE, LABEL, executive, 10, 7, 0, 164, 0.7\n"
                 "142, testboat, testboat, boat, HANDLE, LABEL, ignore, 1, 7, 0\n"
                 "124, testtrain, testtrain, train, HANDLE, LABEL, ignore, 1, 7, 0\n"
                 "125, testheli, testheli, heli, HANDLE, LABEL, ignore, 1, 7, 0\n"
                 "140, testplane, testplane, plane, HANDLE, LABEL, ignore, 1, 7, 0, 0\nend\n"
                 "objs\n164, wheel_alloy, generic, 2, 20, 70, 0\nend\n";
        }
        editor::Assets inventory;
        inventory.ide(root / "vehicles.ide");
        check(inventory.vehicles.size() == 5,
              "GTA III short boat/aircraft definitions remain in catalogue");
        check(inventory.vehicles[0].wheel == 164 &&
                  std::abs(inventory.vehicles[0].wheelScale - .7f) < 1e-6f,
              "GTA III wheel ID and scale columns match production loader");
        check(inventory.vehicles[1].supported() && inventory.vehicles[1].boat(),
              "boat calibration coverage");
        check(!inventory.vehicles[2].supported() && !inventory.vehicles[3].supported() &&
                  !inventory.vehicles[4].supported(),
              "unsupported train/aircraft stay visible");
        inventory.root = root;
        auto baseWheels = root / "models" / "generic",
             xboxWheels = root / "modelsets" / "xbox" / "models" / "generic";
        editor::fs::create_directories(baseWheels);
        editor::fs::create_directories(xboxWheels);
        {
            std::ofstream f(baseWheels / "WHEELS.DFF");
            f << "base-wheel-container";
        }
        {
            std::ofstream f(baseWheels / "wheels.txd");
            f << "base-wheel-textures";
        }
        {
            std::ofstream f(xboxWheels / "wheels.dff");
            f << "xbox-wheel-container";
        }
        auto baseWheelData = inventory.wheelData(0, 164), xboxWheelData = inventory.wheelData(2, 164);
        check(std::string(baseWheelData.first.begin(), baseWheelData.first.end()) ==
                  "base-wheel-container",
              "shared GTA III wheels container resolves case-insensitively");
        check(std::string(xboxWheelData.first.begin(), xboxWheelData.first.end()) ==
                      "xbox-wheel-container" &&
                  xboxWheelData.second == baseWheelData.second,
              "modelset common wheels and texture fallback match loose MODELFILE assets");
        check(p.save((root / "pack.ini").string(), error), "atomic save");
        p.entries[key]["WheelY"] = 10;
        check(p.save((root / "pack.ini").string(), error), "replace save");
        check(editor::fs::exists(root / "pack.ini.bak"), "backup");
        {
            std::ofstream f(root / "legacy.ini");
            f << "[VR]\nCarWheelCenterXCm=12\n[VehicleView_95]\nWheelCenterXCm=3\n[VehicleViewModern_95]"
                 "\nWheelCenterXCm=8\n";
        }
        editor::Legacy legacy;
        legacy.load(root / "legacy.ini");
        editor::Vehicle car;
        car.id = 95;
        car.type = "car";
        check(legacy.resolve(0, car)["WheelX"] == 15, "legacy classic");
        check(legacy.resolve(2, car)["WheelX"] == 15, "legacy xbox ambiguity preserved");
        check(legacy.resolve(1, car)["WheelX"] == 20, "legacy modern override");
        legacy.entries["VR"]["ModernCarWheelRadiusV2Cm"] = 30;
        car.id = 92;
        check(legacy.resolve(1, car)["WheelRadius"] == 21,
              "model wheel radius overrides edited category radius");
        car.id = 95;
        check(legacy.resolve(1, car)["WheelRadius"] == 30,
              "unconfigured model inherits category radius");
        legacy.entries["VR"]["DrivingYOffsetCm"] = 25;
        check(legacy.resolve(0, car)["DefaultSeatHeight"] == 25, "legacy default view height");
        check(legacy.resolve(2, car)["DefaultSeatHeight"] == 0, "Xbox car neutral default view");
        car.type = "boat";
        check(legacy.resolve(2, car)["DefaultSeatHeight"] == 25,
              "Xbox boat keeps independent default view fallback");
        auto hash = editor::fingerprint({{1, 2, 3}, {4}});
        check(hash != editor::fingerprint({{1, 2, 3}, {5}}), "asset-change detection");
        editor::History history;
        editor::Reviews reviewed = {{"classic:95", "asset-a"}, {"xbox:95", "asset-b"}};
        Pack doc;
        doc.entries["classic:95"]["WheelX"] = 2;
        history.capture(doc, reviewed);
        doc.entries["classic:95"]["WheelX"] = 3;
        reviewed.erase("classic:95");
        history.capture(doc, reviewed);
        doc.entries["xbox:95"]["WheelX"] = 4;
        reviewed.erase("xbox:95");
        check(history.undo(doc, reviewed) && reviewed.count("xbox:95") == 1 &&
                  reviewed.count("classic:95") == 0,
              "undo restores review metadata for correct vehicle");
        check(history.undo(doc, reviewed) && reviewed.count("classic:95") == 1 &&
                  doc.entries["classic:95"]["WheelX"] == 2,
              "undo restores reviewed placement");
        check(history.redo(doc, reviewed) && reviewed.count("classic:95") == 0,
              "redo invalidates modified review");
        history.capture(doc, reviewed);
        check(!history.redo(doc, reviewed), "branching edit clears redo");
        LiveStore live;
        live.root = root.string();
        Pack liveTemplate, livePersonal;
        liveTemplate.entries["xbox:106"]["RadioRoll"] = 61.64f;
        livePersonal.entries["xbox:106"]["RadioRoll"] = -90;
        check(liveTemplate.save((root / "vehicle_placements.template.ini").string(), error),
              "live template seed");
        check(livePersonal.save((root / "vehicle_placements.personal.ini").string(), error),
              "live personal seed");
        live.poll(true);
        check(live.store.value(2, 106, "RadioRoll", 0) == -90, "live effective personal masks template");
        auto revision = live.revision;
        livePersonal.entries["xbox:106"]["RadioRoll"] = 23.5f;
        check(livePersonal.save((root / "vehicle_placements.personal.ini").string(), error),
              "live update save");
        live.poll(true);
        check(live.revision > revision && live.store.value(2, 106, "RadioRoll", 0) == 23.5f,
              "live hot reload update");
        {
            std::ofstream bad(root / "vehicle_placements.personal.ini");
            bad << "[Placement]\nVersion=99\n";
        }
        live.poll(true);
        check(live.store.value(2, 106, "RadioRoll", 0) == 23.5f,
              "malformed live update retains previous generation");
        editor::fs::remove(root / "vehicle_placements.personal.ini");
        {
            std::ofstream request(root / "vehicle_placements.reload");
            request << "0123456789abcdef0123456789abcdef";
        }
        live.poll(true);
        check(std::fabs(live.store.value(2, 106, "RadioRoll", 0) - 61.64f) < .001f,
              "deleted personal pack reveals template live");
        {
            std::ifstream status(root / "vehicle_placements.live.ini");
            std::ostringstream text;
            text << status.rdbuf();
            check(text.str().find("Request=0123456789abcdef0123456789abcdef") != std::string::npos,
                  "live request acknowledged");
        }
        for (auto name : {"vehicle_placements.template.ini", "vehicle_placements.personal.ini.bak",
                          "vehicle_placements.live.ini", "vehicle_placements.reload"})
            editor::fs::remove(root / name);
        prefs.set("FieldOfView", 115.00001f);
        check(prefs.get("FieldOfView") == 115, "slider boundary roundoff clamped before saving");
        auto prefFile = root / "vehicle-editor.ini";
        check(prefs.save(prefFile, prefError), "application settings save");
        prefs.set("FPSLimit", 30);
        check(prefs.save(prefFile, prefError) && loaded.load(prefFile, prefError) &&
                  loaded.get("FPSLimit") == 30,
              "application settings file reload");
        check(loaded.load(root / "vehicle-editor.ini.bak", prefError) && loaded.get("FPSLimit") == 144,
              "application settings backup preserves prior preferences");
        editor::fs::remove(prefFile);
        editor::fs::remove(root / "vehicle-editor.ini.bak");
        editor::fs::remove(root / "pack.ini");
        editor::fs::remove(root / "pack.ini.bak");
        editor::fs::remove(root / "legacy.ini");
        editor::fs::remove(root / "vehicles.ide");
        for (auto path :
             {baseWheels / "WHEELS.DFF", baseWheels / "wheels.txd", xboxWheels / "wheels.dff"})
            editor::fs::remove(path);
        for (auto path : {baseWheels, root / "models", xboxWheels, xboxWheels.parent_path(),
                          xboxWheels.parent_path().parent_path(), root / "modelsets"})
            editor::fs::remove(path);
        editor::fs::remove(root);
        std::cout << "Placement/data tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
