#include "Assets.h"
#include "FrameLimiter.h"
#include "History.h"
#include "Preferences.h"
#include "Process.h"
#ifdef _WIN32
// Windows' obsolete memory-model macros collide with RenderWare math locals.
#undef near
#undef far
#endif
#include "imgui/ImGuizmo.h"
#include <chrono>
#include <future>
#include <rw.h>
#include <set>
#include <skeleton.h>

using namespace VehiclePlacement;
using namespace editor;
rw::EngineOpenParams engineOpenParams;
static rw::World *world;
static rw::Camera *camera;
static int nodeOffset;
static rw::Clump *clump;
static std::vector<rw::Clump *> wheelClumps;
static std::vector<rw::TexDictionary *> dictionaries;
static rw::TexDictionary *genericDictionaries[3] = {};
static std::string genericHashes[3];
static Assets assets;
static Legacy legacy, radioLegacy, radioDefaults;
static Store store;
static Pack *editing = &store.personal;
static int layer = 0, set = 0, selected = -1, element = 0, operation = 0;
static Vec seat, eye;
static bool cameraInitialized = false, autoResetCamera = false, vsync = true, invertX = false,
            invertY = false;
static int fpsLimit = 0;
static fs::path preferencesPath;
static std::string imguiSettingsPath, preferencesError, savedPreferences, observedPreferences;
static bool preferencesWritable = true;
static auto preferencesChangedAt = std::chrono::steady_clock::now();
static float panSensitivity = 1.f, rotationSensitivity = .5f;
static float background[3] = {37.f / 255, 43.f / 255, 53.f / 255};
static float yaw = 0, pitch = 0, fov = 80, steering = 0, eyeAboveSeat = 0.68f;
static bool extras[6] = {}, dirty = false, showHud = true, showWheel = true, orbit = false,
            closeRequested = false;
static char gamePath[1024] = "", workPath[1024] = "", search[128] = "", adbPath[1024] = "adb";
#ifdef _WIN32
static char managerPath[1024] = "LibertyManager.exe";
#else
static char managerPath[1024] = "LibertyManager";
#endif
static char importPath[1024] = "";
static std::string message, fingerprintNow;
static std::string deviceOutput;
static bool showDeviceOutput = false, effectivePreview = true, liveSync = false, taskIsLive = false;
static std::string liveObserved, liveSent, liveStatus = "Disabled";
static auto liveChangedAt = std::chrono::steady_clock::now();
static std::map<std::string, std::string> reviews;
static std::set<std::string> staleReviews;
static std::set<std::string> selection;
static History history;
static Values clipboard;
static std::future<std::string> deviceTask;
static int deviceIndex = 0, conflictPreference = 0;
static int smokeFrames = 0, smokeCount = 0;
static bool smokeOk = true;
static bool modelTextures = false;
static bool modelWheels = true;
static bool smokeAll = false;
static int smokeIndex = 0, smokeFailures = 0;

static Preferences capturePreferences() {
    Preferences p;
    p.paths["GameFolder"] = gamePath;
    p.paths["WorkspaceFolder"] = workPath;
    p.paths["Manager"] = managerPath;
    p.paths["ADB"] = adbPath;
    for (auto key : {"GameFolder", "WorkspaceFolder"})
        if (!p.paths[key].empty()) {
            std::error_code error;
            auto path = fs::absolute(fs::u8path(p.paths[key]), error);
            if (!error)
                p.paths[key] = path.lexically_normal().string();
        }
    p.set("EffectivePreview", effectivePreview);
    p.set("AutoResetCamera", autoResetCamera);
    p.set("Orbit", orbit);
    p.set("VSync", vsync);
    p.set("FPSLimit", fpsLimit);
    p.set("InvertX", invertX);
    p.set("InvertY", invertY);
    p.set("PanSensitivity", panSensitivity);
    p.set("RotationSensitivity", rotationSensitivity);
    p.set("FieldOfView", fov);
    p.set("EyeAboveSeat", eyeAboveSeat);
    p.set("BackgroundR", background[0]);
    p.set("BackgroundG", background[1]);
    p.set("BackgroundB", background[2]);
    p.set("PreviewHUD", showHud);
    p.set("PreviewWheel", showWheel);
    p.set("Modelset", set);
    p.set("EditLayer", layer);
    p.set("Element", element);
    p.set("Gizmo", operation);
    p.set("CameraInitialized", cameraInitialized);
    p.set("CameraX", eye.x);
    p.set("CameraY", eye.y);
    p.set("CameraZ", eye.z);
    p.set("CameraYaw", yaw);
    p.set("CameraPitch", pitch);
    return p;
}
static void applyPreferences(const Preferences &p) {
    if (!p.paths.at("GameFolder").empty())
        std::snprintf(gamePath, sizeof(gamePath), "%s", p.paths.at("GameFolder").c_str());
    if (!p.paths.at("WorkspaceFolder").empty())
        std::snprintf(workPath, sizeof(workPath), "%s", p.paths.at("WorkspaceFolder").c_str());
    if (!p.paths.at("Manager").empty())
        std::snprintf(managerPath, sizeof(managerPath), "%s", p.paths.at("Manager").c_str());
    if (!p.paths.at("ADB").empty())
        std::snprintf(adbPath, sizeof(adbPath), "%s", p.paths.at("ADB").c_str());
    effectivePreview = (bool)p.get("EffectivePreview");
    autoResetCamera = (bool)p.get("AutoResetCamera");
    orbit = (bool)p.get("Orbit");
    vsync = (bool)p.get("VSync");
    fpsLimit = (int)p.get("FPSLimit");
    invertX = (bool)p.get("InvertX");
    invertY = (bool)p.get("InvertY");
    panSensitivity = (float)p.get("PanSensitivity");
    rotationSensitivity = (float)p.get("RotationSensitivity");
    fov = (float)p.get("FieldOfView");
    eyeAboveSeat = (float)p.get("EyeAboveSeat");
    background[0] = (float)p.get("BackgroundR");
    background[1] = (float)p.get("BackgroundG");
    background[2] = (float)p.get("BackgroundB");
    showHud = (bool)p.get("PreviewHUD");
    showWheel = (bool)p.get("PreviewWheel");
    set = editor::canonicalModelSet((int)p.get("Modelset"));
    layer = (int)p.get("EditLayer");
    element = (int)p.get("Element");
    operation = (int)p.get("Gizmo");
    cameraInitialized = (bool)p.get("CameraInitialized");
    eye.x = (float)p.get("CameraX");
    eye.y = (float)p.get("CameraY");
    eye.z = (float)p.get("CameraZ");
    yaw = (float)p.get("CameraYaw");
    pitch = (float)p.get("CameraPitch");
    editing = layer ? &store.templates : &store.personal;
}
static void savePreferences(bool force = false) {
    if (smokeFrames || preferencesPath.empty() || (!preferencesWritable && !force))
        return;
    auto p = capturePreferences();
    auto text = p.text();
    auto now = std::chrono::steady_clock::now();
    if (text != observedPreferences) {
        observedPreferences = text;
        preferencesChangedAt = now;
    }
    if (text == savedPreferences ||
        (!force && now - preferencesChangedAt < std::chrono::milliseconds(500)))
        return;
    if (p.save(preferencesPath, preferencesError)) {
        savedPreferences = text;
        preferencesWritable = true;
    } else
        preferencesWritable = false;
}
static const char *node(rw::Frame *f) {
    return (const char *)f + nodeOffset;
}
static void *nodeCtor(void *o, rw::int32 off, rw::int32) {
    *((char *)o + off) = 0;
    return o;
}
static void *nodeCopy(void *d, void *s, rw::int32 off, rw::int32) {
    std::memcpy((char *)d + off, (char *)s + off, 64);
    return d;
}
static rw::Stream *nodeRead(rw::Stream *s, rw::int32 n, void *o, rw::int32 off, rw::int32) {
    if (n < 0)
        return nullptr;
    int take = std::min(n, 63);
    s->read8((char *)o + off, take);
    *((char *)o + off + take) = 0;
    if (n > take)
        s->seek(n - take);
    return s;
}
static Vec vec(rw::V3d v) {
    return Vec(v.x, v.y, v.z);
}
static rw::V3d rwv(Vec v) {
    return rw::makeV3d(v.x, v.y, v.z);
}
static rw::Texture *textureFind(const char *name) {
    for (auto i = dictionaries.rbegin(); i != dictionaries.rend(); ++i) {
        auto t = (*i)->find(name);
        if (t)
            return t;
    }
    return genericDictionaries[set] ? genericDictionaries[set]->find(name) : nullptr;
}
static void clearModel() {
    modelTextures = false;
    modelWheels = true;
    if (clump) {
        world->removeClump(clump);
        clump->destroy();
        clump = nullptr;
    }
    for (auto c : wheelClumps)
        c->destroy();
    wheelClumps.clear();
    rw::TexDictionary::setCurrent(nullptr);
    for (auto d : dictionaries)
        d->destroy();
    dictionaries.clear();
}
static rw::TexDictionary *loadTxd(std::vector<uint8_t> b) {
    if (b.empty())
        return nullptr;
    rw::StreamMemory stream;
    stream.open(b.data(), (rw::uint32)b.size());
    if (!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, nullptr, nullptr))
        return nullptr;
    auto d = rw::TexDictionary::streamRead(&stream);
    if (!d)
        return nullptr;
    FORLIST(link, d->textures) {
        auto t = rw::Texture::fromDict(link);
        if (t->raster)
            t->raster = rw::Raster::convertTexToCurrentPlatform(t->raster);
        t->setFilter(rw::Texture::LINEARMIPLINEAR);
    }
    dictionaries.push_back(d);
    return d;
}
static rw::Clump *loadDff(std::vector<uint8_t> &b) {
    if (b.empty())
        return nullptr;
    rw::StreamMemory stream;
    stream.open(b.data(), (rw::uint32)b.size());
    if (!rw::findChunk(&stream, rw::ID_CLUMP, nullptr, nullptr))
        return nullptr;
    return rw::Clump::streamRead(&stream);
}
static rw::Frame *findFrame(rw::Frame *f, const std::string &name) {
    if (lower(node(f)) == name)
        return f;
    for (auto c = f->child; c; c = c->next) {
        auto r = findFrame(c, name);
        if (r)
            return r;
    }
    return nullptr;
}
static Values values(bool gamePreview = false) {
    if (selected < 0)
        return {};
    auto &v = assets.vehicles[selected];
    auto result = legacy.resolve(set, v);
    radioLegacy.radio(v.id, result, radioDefaults);
    auto name = section(set, v.id);
    if (!layer || (gamePreview && effectivePreview))
        result = VehiclePlacement::resolve(store, set, v.id, result);
    // Template authoring previews the template without personal overrides.
    else
        for (auto &f : result) {
            float value = f.second;
            if (store.templates.get(name, f.first, value) || store.bundled.get(name, f.first, value))
                f.second = value;
        }
    float linked;
    bool explicitAnchor = ((!layer || (gamePreview && effectivePreview)) &&
                           store.personal.get(name, "DashFollowWheel", linked)) ||
                          store.templates.get(name, "DashFollowWheel", linked) ||
                          store.bundled.get(name, "DashFollowWheel", linked);
    if (!explicitAnchor && legacy.entries.empty()) {
        auto frame = dashboard(wheel(seat, v.boat(), result), result);
        auto angles = relativeAngles(frame, Basis());
        result["DashFollowWheel"] = 0;
        result["DashX"] = frame.position.x * 100;
        result["DashY"] = frame.position.y * 100;
        result["DashZ"] = frame.position.z * 100;
        result["DashPitch"] = angles.x;
        result["DashYaw"] = angles.y;
        result["DashRoll"] = angles.z;
    }
    return result;
}
static void remember() {
    history.capture(*editing, reviews);
    dirty = true;
}
static std::string currentSection() {
    return selected < 0 ? "" : section(set, assets.vehicles[selected].id);
}
static void setValue(const std::string &key, float value) {
    float lo, hi;
    if (!limits(key, lo, hi))
        return;
    float linked;
    bool explicitAnchor = (!layer && store.personal.get(currentSection(), "DashFollowWheel", linked)) ||
                          store.templates.get(currentSection(), "DashFollowWheel", linked) ||
                          store.bundled.get(currentSection(), "DashFollowWheel", linked);
    Values before;
    if (!explicitAnchor && legacy.entries.empty() && selected >= 0)
        before = values();
    auto &entry = editing->entries[currentSection()];
    if (!before.empty()) {
        const char *fields[] = {"DashFollowWheel", "DashX",   "DashY",   "DashZ",
                                "DashPitch",       "DashYaw", "DashRoll"};
        for (auto field : fields)
            entry[field] = before[field];
    }
    entry[key] = std::max(lo, std::min(hi, value));
    reviews.erase(currentSection());
}
static void driver() {
    if (selected < 0)
        return;
    auto &v = assets.vehicles[selected];
    auto p = values(true);
    eye = seat;
    if (!v.boat())
        eye.x = -eye.x;
    eye.y += .12f + p["SeatDistance"] * .01f;
    eye.z += eyeAboveSeat + p["SeatHeight"] * .01f;
    yaw = pitch = 0;
    orbit = false;
    cameraInitialized = true;
}
static void loadModel() {
    clearModel();
    fingerprintNow.clear();
    if (selected < 0)
        return;
    auto &v = assets.vehicles[selected];
    try {
        int source = 0, txSource = 0;
        auto dff = assets.read(set, v.name + ".dff", &source),
             txd = assets.read(set, v.txd + ".txd", &txSource);
        std::vector<std::vector<uint8_t>> hashes = {dff, txd};
        if (!genericDictionaries[set]) {
            auto generic = assets.read(set, "generic.txd");
            auto misc = assets.read(set, "misc.txd");
            genericHashes[set] = editor::fingerprint({generic, misc});
            genericDictionaries[set] = loadTxd(generic);
            if (genericDictionaries[set])
                dictionaries.pop_back();
            // default.dat merges MISC.TXD into the shared game dictionary;
            // it contains the stock tyre and rim textures used by both profiles.
            if (auto shared = loadTxd(misc)) {
                dictionaries.pop_back();
                if (!genericDictionaries[set])
                    genericDictionaries[set] = shared;
                else {
                    std::vector<rw::Texture *> textures;
                    FORLIST(link, shared->textures)
                        textures.push_back(rw::Texture::fromDict(link));
                    for (auto texture : textures)
                        if (!genericDictionaries[set]->find(texture->name))
                            genericDictionaries[set]->add(texture);
                    shared->destroy();
                }
            }
        }
        modelTextures = loadTxd(txd) != nullptr;
        clump = loadDff(dff);
        if (!clump) {
            message = "Vehicle model is missing or unsupported.";
            return;
        }
        world->addClump(clump);
        auto front = findFrame(clump->getFrame(), "ped_frontseat");
        if (!front) {
            message = "Vehicle has no driver seat marker; calibration is disabled.";
            return;
        }
        seat = vec(front->getLTM()->pos);
        auto colours = assets.vehicleColors.find(v.name);
        FORLIST(link, clump->atomics) {
            auto a = rw::Atomic::fromClump(link);
            if (!a->geometry)
                continue;
            auto &mats = a->geometry->matList;
            for (int i = 0; i < mats.numMaterials; i++) {
                auto m = mats.materials[i];
                int channel = -1;
                if (m->color.red == 0x3c && m->color.green == 0xff && m->color.blue == 0)
                    channel = 0;
                if (m->color.red == 0xff && m->color.green == 0 && m->color.blue == 0xaf)
                    channel = 1;
                if (channel >= 0) {
                    int index = colours == assets.vehicleColors.end() ? 0
                                : channel                             ? colours->second.second
                                                                      : colours->second.first;
                    auto c = index >= 0 && index < (int)assets.colors.size()
                                 ? assets.colors[index]
                                 : std::array<int, 3>{220, 220, 220};
                    m->color.red = (rw::uint8)c[0];
                    m->color.green = (rw::uint8)c[1];
                    m->color.blue = (rw::uint8)c[2];
                }
            }
        }
        // GTA III classic cars share wheel models, attached to six wheel dummies.
        auto obj = assets.objects.find(v.wheel);
        if (v.type == "car" && obj != assets.objects.end()) {
            auto wheelAssets = assets.wheelData(set, v.wheel);
            auto &wb = wheelAssets.first, &wt = wheelAssets.second;
            hashes.push_back(wb);
            hashes.push_back(wt);
            loadTxd(wt);
            const char *dummies[] = {"wheel_rf_dummy", "wheel_rb_dummy", "wheel_rm_dummy",
                                     "wheel_lf_dummy", "wheel_lb_dummy", "wheel_lm_dummy"};
            for (auto name : dummies) {
                auto frame = findFrame(clump->getFrame(), name);
                if (!frame || frame->objectList.count())
                    continue;
                auto w = loadDff(wb);
                if (!w) {
                    modelWheels = false;
                    continue;
                }
                auto link = w->atomics.link.next;
                while (link != w->atomics.end()) {
                    auto a = rw::Atomic::fromClump(link);
                    link = link->next;
                    std::string name = lower(node(a->getFrame()));
                    const std::string &wanted = obj->second.first;
                    if (name != wanted && name != wanted + "_l0" && name != wanted + "_hi") {
                        w->removeAtomic(a);
                        a->destroy();
                    } else {
                        if (a->geometry) {
                            const auto &materials = a->geometry->matList;
                            for (int i = 0; i < materials.numMaterials; ++i) {
                                auto texture = materials.materials[i]->texture;
                                if (texture && (!texture->raster || texture->raster->width == 0 ||
                                                texture->raster->height == 0))
                                    modelWheels = false;
                            }
                        }
                        // The production loader detaches each wheel atomic
                        // from the container hierarchy before instancing it.
                        auto frame = rw::Frame::create();
                        w->getFrame()->addChild(frame);
                        a->setFrame(frame);
                    }
                }
                if (!w->countAtomics()) {
                    modelWheels = false;
                    w->destroy();
                    continue;
                }
                wheelClumps.push_back(w);
                rw::Matrix m = *frame->getLTM();
                rw::V3d scale = rw::makeV3d(v.wheelScale, v.wheelScale, v.wheelScale);
                m.scale(&scale, rw::COMBINEPRECONCAT);
                w->getFrame()->transform(&m, rw::COMBINEREPLACE);
                FORLIST(link, w->atomics) {
                    auto a = rw::Atomic::fromClump(link);
                    if (a->getFrame() != w->getFrame()) {
                        a->getFrame()->matrix.setIdentity();
                        a->getFrame()->updateObjects();
                    }
                }
            }
        }
        fingerprintNow = editor::fingerprint(hashes) + genericHashes[set];
        if (!cameraInitialized || autoResetCamera)
            driver();
        message = std::string("Geometry: ") + editor::modelSetName(source) +
                  "; textures: " + editor::modelSetName(txSource);
        if (set && source == 0)
            message += " (Classic geometry fallback)";
        if (!modelTextures)
            message += "; vehicle textures are missing or unsupported";
        if (!modelWheels)
            message += "; shared wheel geometry is missing or unsupported";
    } catch (const std::exception &e) {
        message = e.what();
    }
}
static void checkReviews() {
    if (assets.vehicles.empty())
        return;
    std::string generics[3];
    for (auto it = reviews.begin(); it != reviews.end();) {
        const std::string &s = it->first;
        int selectedSet = s.substr(0, s.find(':')) == "xbox"     ? 2
                          : s.substr(0, s.find(':')) == "modern" ? 1
                                                                 : 0;
        int id = std::atoi(s.c_str() + s.find(':') + 1);
        auto car = std::find_if(assets.vehicles.begin(), assets.vehicles.end(),
                                [&](const Vehicle &v) { return v.id == id; });
        std::string hash;
        if (car != assets.vehicles.end() && assets.available[selectedSet]) {
            std::vector<std::vector<uint8_t>> parts = {assets.read(selectedSet, car->name + ".dff"),
                                                       assets.read(selectedSet, car->txd + ".txd")};
            auto obj = assets.objects.find(car->wheel);
            if (car->type == "car" && obj != assets.objects.end()) {
                auto wheels = assets.wheelData(selectedSet, car->wheel);
                parts.push_back(std::move(wheels.first));
                parts.push_back(std::move(wheels.second));
            }
            if (generics[selectedSet].empty())
                generics[selectedSet] = editor::fingerprint({assets.read(selectedSet, "generic.txd"),
                                                          assets.read(selectedSet, "misc.txd")});
            hash = editor::fingerprint(parts) + generics[selectedSet];
        }
        if (hash != it->second) {
            staleReviews.insert(s);
            it = reviews.erase(it);
            dirty = true;
        } else
            ++it;
    }
}
static void readReviews() {
    reviews.clear();
    staleReviews.clear();
    std::ifstream in(fs::path(workPath) / "vehicle_placements.review.txt");
    std::string a, b;
    while (in >> a >> b)
        if (validSection(a))
            reviews[a] = b;
    checkReviews();
}
static bool saveReviews() {
    auto path = fs::path(workPath) / "vehicle_placements.review.txt";
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";
    std::ofstream out(temporary);
    for (auto &v : reviews)
        out << v.first << ' ' << v.second << '\n';
    out.close();
    if (!out)
        return false;
    std::error_code error;
    bool existing = fs::exists(path, error);
    if (error)
        return false;
    if (existing) {
        fs::remove(backup, error);
        error.clear();
        fs::rename(path, backup, error);
        if (error)
            return false;
    }
    fs::rename(temporary, path, error);
    if (error) {
        if (existing) {
            std::error_code ignored;
            fs::rename(backup, path, ignored);
        }
        return false;
    }
    return true;
}
static void openWorkspace() {
    if (!*workPath) {
        message = "Choose an editor workspace folder.";
        return;
    }
    try {
        fs::create_directories(workPath);
        Store next;
        std::string error;
        const char *files[] = {"vehicle_placements.bundled.ini", "vehicle_placements.template.ini",
                               "vehicle_placements.personal.ini"};
        Pack *packs[] = {&next.bundled, &next.templates, &next.personal};
        for (int i = 0; i < 3; i++) {
            auto p = fs::path(workPath) / files[i];
            if (fs::exists(p) && !packs[i]->load(p.string(), error))
                throw std::runtime_error(error);
        }
        store = next;
        editing = layer ? &store.templates : &store.personal;
        legacy.load(fs::path(workPath) / "vr_settings.ini");
        radioLegacy.load(fs::path(workPath) / "radio_board.ini");
        radioDefaults.load(fs::path(workPath) / "radio_board_defaults.ini");
        dirty = false;
        readReviews();
        history.clear();
        message = "Workspace loaded. Legacy Classic/Modern settings are shared fallbacks.";
    } catch (const std::exception &e) {
        message = e.what();
    }
}
static bool save() {
    if (!*workPath) {
        message = "Choose a workspace first.";
        return false;
    }
    std::string error;
    if (!store.personal.save((fs::path(workPath) / "vehicle_placements.personal.ini").string(), error) ||
        !store.templates.save((fs::path(workPath) / "vehicle_placements.template.ini").string(),
                              error)) {
        message = error;
        return false;
    }
    if (!saveReviews()) {
        message = "Placement packs saved, but review metadata could not be saved.";
        return false;
    }
    dirty = false;
    message =
        "Saved. Template pack can also be distributed as release defaults (rename to .bundled.ini).";
    return true;
}
static bool visible(rw::Atomic *a) {
    std::string n = lower(node(a->getFrame()));
    if (n.find("_vlo") != n.npos || n.find("_lo") != n.npos || n.find("_dam") != n.npos ||
        n.find("_l1") != n.npos || n.find("_l2") != n.npos)
        return false;
    for (auto f = a->getFrame(); f; f = f->getParent()) {
        std::string name = lower(node(f));
        if (name.size() >= 6 && name.substr(0, 5) == "extra" && name[5] >= '1' && name[5] <= '6')
            return extras[name[5] - '1'];
    }
    return true;
}
static void renderModel() {
    if (!clump)
        return;
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
    rw::SetRenderState(rw::ZTESTENABLE, 1);
    rw::SetRenderState(rw::ZWRITEENABLE, 1);
    FORLIST(link, clump->atomics) {
        auto a = rw::Atomic::fromClump(link);
        if (visible(a))
            a->render();
    }
    for (auto c : wheelClumps) {
        FORLIST(link, c->atomics) {
            auto a = rw::Atomic::fromClump(link);
            if (visible(a))
                a->render();
        }
    }
}
static rw::RWDEVICE::Im3DVertex vertex(Vec p, rw::RGBA c, float u = 0, float v = 0) {
    rw::RWDEVICE::Im3DVertex a;
    a.position = rwv(p);
    a.setColor(c.red, c.green, c.blue, c.alpha);
    a.setU(u);
    a.setV(v);
    return a;
}
static void lines(const std::vector<Vec> &points, rw::RGBA color) {
    std::vector<rw::RWDEVICE::Im3DVertex> verts;
    for (auto p : points)
        verts.push_back(vertex(p, color));
    if (verts.empty())
        return;
    rw::SetRenderStatePtr(rw::TEXTURERASTER, nullptr);
    rw::im3d::Transform(verts.data(), (int)verts.size(), nullptr,
                        rw::im3d::VERTEXXYZ | rw::im3d::VERTEXRGBA);
    rw::im3d::RenderPrimitive(rw::PRIMTYPELINELIST);
    rw::im3d::End();
}
static void quad(Vec tl, Vec tr, Vec br, Vec bl, rw::RGBA color, rw::Raster *texture = nullptr,
                 float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    rw::RWDEVICE::Im3DVertex vertices[] = {vertex(tl, color, u0, v0), vertex(tr, color, u1, v0),
                                           vertex(br, color, u1, v1), vertex(tl, color, u0, v0),
                                           vertex(br, color, u1, v1), vertex(bl, color, u0, v1)};
    rw::SetRenderStatePtr(rw::TEXTURERASTER, texture);
    rw::im3d::Transform(vertices, 6, nullptr, rw::im3d::EVERYTHING);
    rw::im3d::RenderPrimitive(rw::PRIMTYPETRILIST);
    rw::im3d::End();
}
static void panelText(const Basis &p, float width, float height, const char *text) {
    auto font = ImGui::GetFont();
    auto texture = (rw::Texture *)ImGui::GetIO().Fonts->TexID;
    if (!texture)
        return;
    float scale = width / (float)std::max<size_t>(std::strlen(text) * 8, 40), x = -width * .44f;
    for (const char *c = text; *c; c++) {
        auto glyph = font->FindGlyph((unsigned char)*c);
        if (!glyph)
            continue;
        Vec origin = p.position - p.forward * .0005f;
        auto pt = [&](float gx, float gy) {
            return origin + p.right * (x + gx * scale) + p.up * (height * .35f - gy * scale);
        };
        quad(pt(glyph->X0, glyph->Y0), pt(glyph->X1, glyph->Y0), pt(glyph->X1, glyph->Y1),
             pt(glyph->X0, glyph->Y1), rw::makeRGBA(100, 255, 180, 255), texture->raster, glyph->U0,
             glyph->V0, glyph->U1, glyph->V1);
        x += glyph->AdvanceX * scale;
    }
}
static void renderPlacement() {
    if (selected < 0 || !clump || fingerprintNow.empty())
        return;
    auto &car = assets.vehicles[selected];
    if (!car.supported())
        return;
    auto v = values(true);
    Basis w = wheel(seat, car.boat(), v);
    rw::SetRenderState(rw::ZTESTENABLE, 1);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::SetRenderState(rw::VERTEXALPHA, 1);
    rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
    if (showWheel && v["WheelVisible"]) {
        std::vector<Vec> points;
        for (int i = 0; i < 96; i++) {
            float a = i * 6.2831853f / 96, b = (i + 1) * 6.2831853f / 96;
            points.push_back(w.position +
                             (w.right * std::cos(a) + w.up * std::sin(a)) * (v["WheelRadius"] * .01f));
            points.push_back(w.position +
                             (w.right * std::cos(b) + w.up * std::sin(b)) * (v["WheelRadius"] * .01f));
        }
        for (int i = 0; i < 3; i++) {
            float a = (steering + i * 120) * .0174532925f;
            points.push_back(w.position);
            points.push_back(w.position +
                             (w.right * std::cos(a) + w.up * std::sin(a)) * (v["WheelRadius"] * .01f));
        }
        lines(points, rw::makeRGBA(255, 180, 40, 255));
    }
    if (showHud) {
        auto d = dashboard(w, v);
        static const float widths[] = {.115f, .130f, .055f, .070f};
        const char *text[] = {"N  +  MAP", "100  $000500", "12:30", "17 / 120"};
        for (int n = 0; n < 5; n++) {
            Basis p = n == 4 ? radio(seat, assets.vehicles[selected].boat(), v) : panel(d, v, n);
            float width = n == 4 ? .40f * v["RadioSize"] * .01f
                                 : widths[n] * v[std::string(panelName(n)) + "Size"] * .01f *
                                       v["DashSize"] * .01f,
                  height = width * (n == 0   ? 1
                                    : n == 4 ? .5f
                                             : .42f);
            quad(p.position - p.right * (width / 2) + p.up * (height / 2),
                 p.position + p.right * (width / 2) + p.up * (height / 2),
                 p.position + p.right * (width / 2) - p.up * (height / 2),
                 p.position - p.right * (width / 2) - p.up * (height / 2), rw::makeRGBA(8, 20, 23, 230));
            panelText(p, width, height, n == 4 ? "RADIO   MUSIC   <<  >  >>" : text[n]);
        }
    }
    rw::SetRenderState(rw::ZWRITEENABLE, 1);
}
static std::string status(int s, const Vehicle &car) {
    auto name = section(s, car.id);
    if (!car.supported())
        return "unsupported";
    if (s && !assets.available[s])
        return "missing set";
    if (assets.archives[s].index.count(car.name + ".dff") == 0 &&
        assets.archives[0].index.count(car.name + ".dff") == 0)
        return "missing";
    if (staleReviews.count(name))
        return "asset changed";
    if (reviews.count(name)) {
        if (name == currentSection() && !fingerprintNow.empty() && reviews[name] != fingerprintNow)
            return "asset changed";
        return "reviewed";
    }
    auto personal = store.personal.entries.find(name), templates = store.templates.entries.find(name);
    if ((personal != store.personal.entries.end() && !personal->second.empty()) ||
        (templates != store.templates.entries.end() && !templates->second.empty()))
        return "edited";
    return "inherited";
}
static void editField(const char *key, const char *label, Values &v) {
    float f = v[key], lo, hi;
    if (!limits(key, lo, hi))
        return;
    ImGui::PushID(key);
    int source = 0;
    if (!layer)
        store.value(set, assets.vehicles[selected].id, key, 0, &source);
    else {
        float ignored;
        if (store.templates.get(currentSection(), key, ignored))
            source = 2;
        else if (store.bundled.get(currentSection(), key, ignored))
            source = 1;
    }
    bool changed = ImGui::SliderFloat(label, &f, lo, hi, "%.2f");
    ImGui::SameLine();
    ImGui::PushItemWidth(72);
    changed = ImGui::InputFloat("##value", &f, 0, 0, 2) || changed;
    ImGui::PopItemWidth();
    if (changed) {
        remember();
        setValue(key, f);
        v[key] = f;
    }
    ImGui::SameLine();
    const char *sources[] = {"legacy", "bundled", "template", "personal"};
    ImGui::TextDisabled("%s", sources[source]);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        remember();
        editing->entries[currentSection()].erase(key);
        reviews.erase(currentSection());
    }
    ImGui::PopID();
}
static void transformFields(const std::string &prefix, Values &v, bool size) {
    const char *suffix[] = {"X", "Y", "Z", "Pitch", "Yaw", "Roll"};
    for (auto a : suffix)
        editField((prefix + a).c_str(), a, v);
    if (size)
        editField((prefix + "Size").c_str(), "Size %", v);
}
static void device(const std::string &action, bool automatic = false) {
    if (deviceTask.valid()) {
        message = "A device operation is already running.";
        return;
    }
    if (!*workPath) {
        message = "Choose a workspace first.";
        return;
    }
    if ((action == "deploy" || action == "preview") && !save())
        return;
    std::vector<std::string> args = {
        managerPath, "--vehicle-bridge", action, "--workspace", fs::absolute(workPath).string(),
        "--adb",    adbPath};
    if (deviceIndex) {
        args.push_back("--device-index");
        args.push_back(std::to_string(deviceIndex));
    }
    if (action == "deploy" || action == "preview") {
        args.push_back("--kind");
        args.push_back(layer ? "template" : "personal");
        args.push_back("--pack");
        args.push_back((fs::absolute(workPath) /
                        (layer ? "vehicle_placements.template.ini" : "vehicle_placements.personal.ini"))
                           .string());
        std::string sections;
        for (auto &s : (automatic ? std::set<std::string>() : selection)) {
            if (!sections.empty())
                sections += ",";
            sections += s;
        }
        if (sections.empty())
            sections = currentSection();
        if (sections.empty()) {
            message = "Select an entry to transfer.";
            return;
        }
        args.push_back("--sections");
        args.push_back(sections);
        if (conflictPreference) {
            args.push_back("--prefer");
            args.push_back(conflictPreference == 1 ? "local" : "remote");
        }
    }
    if (action == "deploy")
        args.push_back("--live");
    taskIsLive = automatic;
    if (automatic)
        liveStatus = "Uploading current entry...";
    message = "Local device operation running...";
    deviceTask =
        std::async(std::launch::async, [args, action] { return action + "\n" + editor::process(args); });
}
static void gizmo() {
    if (layer && effectivePreview && store.personal.entries.count(currentSection()) &&
        !store.personal.entries.at(currentSection()).empty())
        return;
    if (selected < 0 || !clump || fingerprintNow.empty() || !assets.available[set] ||
        !assets.vehicles[selected].supported())
        return;
    auto v = values();
    auto w = wheel(seat, assets.vehicles[selected].boat(), v), d = dashboard(w, v);
    Basis radioParent;
    radioParent.position = seat;
    if (!assets.vehicles[selected].boat())
        radioParent.position.x = -seat.x;
    Basis b = element == 0   ? w
              : element == 1 ? d
              : element == 6 ? radio(seat, assets.vehicles[selected].boat(), v)
                             : panel(d, v, element - 2);
    float m[16] = {
        b.right.x,    b.right.y,    b.right.z,    0, b.up.x,       b.up.y,       b.up.z,       0,
        -b.forward.x, -b.forward.y, -b.forward.z, 0, b.position.x, b.position.y, b.position.z, 1};
    // OpenGL camera matrices are exactly those used to draw the preview.
    ImGuizmo::SetRect(0, 0, (float)sk::globals.width, (float)sk::globals.height);
    ImGuizmo::Manipulate((float *)&camera->devView, (float *)&camera->devProj,
                         operation == 0 ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE, ImGuizmo::WORLD, m);
    if (ImGuizmo::IsUsing()) {
        remember();
        Vec p(m[12], m[13], m[14]);
        if (operation == 0) {
            if (element == 0) {
                Vec base = seat;
                if (!assets.vehicles[selected].boat())
                    base.x = -base.x;
                base = base + Vec(0, .33f, .30f);
                p = (p - base) * 100;
                setValue("WheelX", p.x);
                setValue("WheelY", p.y);
                setValue("WheelZ", p.z);
            } else {
                Basis parent = element == 1   ? (v["DashFollowWheel"] ? w : Basis())
                               : element == 6 ? radioParent
                                              : d;
                Vec delta = p - parent.position;
                float scale = element == 1 || element == 6 ? 1 : v["DashSize"] * .01f;
                p = Vec(dot(delta, parent.right), dot(delta, parent.forward), dot(delta, parent.up)) *
                    (100 / scale);
                std::string prefix = element == 1   ? "Dash"
                                     : element == 6 ? "Radio"
                                                    : panelName(element - 2);
                setValue(prefix + "X", p.x);
                setValue(prefix + "Y", p.y);
                setValue(prefix + "Z", p.z);
            }
        } else {
            // Extract intrinsic X/Z/Y (right/up/forward) Euler angles in parent space.
            Basis parent = element == 0   ? Basis()
                           : element == 1 ? (v["DashFollowWheel"] ? w : Basis())
                           : element == 6 ? radioParent
                                          : d;
            Basis rotated;
            rotated.right = Vec(m[0], m[1], m[2]);
            rotated.forward = Vec(-m[8], -m[9], -m[10]);
            rotated.up = Vec(m[4], m[5], m[6]);
            auto angles = relativeAngles(rotated, parent);
            std::string prefix = element == 0   ? "Wheel"
                                 : element == 1 ? "Dash"
                                 : element == 6 ? "Radio"
                                                : panelName(element - 2);
            setValue(prefix + "Pitch", angles.x);
            setValue(prefix + "Yaw", angles.y);
            setValue(prefix + "Roll", angles.z);
        }
    }
}
static void ui() {
    if (deviceTask.valid() &&
        deviceTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = deviceTask.get();
        if (result.find("import\nImported") == 0)
            openWorkspace();
        deviceOutput = result;
        if (taskIsLive) {
            bool success = result.find("Transfer complete.") != std::string::npos;
            liveStatus = success ? (result.find("Live reload acknowledged") != std::string::npos
                                        ? "Live reload acknowledged by game."
                                        : "Pack saved; game is closed.")
                                 : "Stopped; see transfer error.";
            if (!success)
                liveSync = false;
        }
        showDeviceOutput = !taskIsLive || !liveSync;
        taskIsLive = false;
        message = "Device helper finished. See the transfer preview for changes or errors.";
    }
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(390, 680), ImGuiCond_Once);
    ImGui::Begin("Vehicles and files");
    ImGui::InputText("GTA III folder", gamePath, sizeof(gamePath));
    if (ImGui::Button("Read local game assets")) {
        try {
            Assets next;
            next.open(gamePath);
            clearModel();
            for (int i = 0; i < 3; i++) {
                if (genericDictionaries[i])
                    genericDictionaries[i]->destroy();
                genericDictionaries[i] = nullptr;
                genericHashes[i].clear();
            }
            assets = std::move(next);
            auto first = std::find_if(assets.vehicles.begin(), assets.vehicles.end(),
                                      [](const Vehicle &v) { return v.supported(); });
            selected = first == assets.vehicles.end() ? 0 : (int)(first - assets.vehicles.begin());
            loadModel();
            checkReviews();
            history.clear();
        } catch (const std::exception &e) {
            message = e.what();
        }
    }
    ImGui::InputText("Workspace folder", workPath, sizeof(workPath));
    if (ImGui::Button("Open workspace")) {
        if (dirty)
            ImGui::OpenPopup("Discard workspace changes?");
        else
            openWorkspace();
    }
    if (ImGui::BeginPopupModal("Discard workspace changes?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Unsaved edits will be replaced.");
        if (ImGui::Button("Discard and open")) {
            openWorkspace();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(dirty ? "Save *" : "Save"))
        save();
    if (ImGui::CollapsingHeader("Import a local settings or placement file")) {
        ImGui::InputText("File path", importPath, sizeof(importPath));
        if (ImGui::Button("Import placement pack")) {
            Pack incoming;
            std::string error;
            if (incoming.load(importPath, error)) {
                remember();
                *editing = incoming;
                reviews.clear();
                message = "Imported pack into the active edit layer. Undo restores the previous layer.";
            } else
                message = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Import legacy INI")) {
            try {
                if (!*workPath || !fs::exists(importPath))
                    throw std::runtime_error("Choose an existing legacy INI and a workspace.");
                auto sourceName = lower(fs::path(importPath).filename().string());
                auto destination =
                    fs::absolute(fs::path(workPath) / (sourceName == "radio_board.ini" ||
                                                               sourceName == "radio_board_defaults.ini"
                                                           ? sourceName
                                                           : "vr_settings.ini"));
                auto source = fs::absolute(importPath);
                if (source != destination) {
                    if (fs::exists(destination))
                        fs::copy_file(destination, destination.string() + ".before-import",
                                      fs::copy_options::overwrite_existing);
                    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
                }
                legacy.load(fs::path(workPath) / "vr_settings.ini");
                radioLegacy.load(fs::path(workPath) / "radio_board.ini");
                radioDefaults.load(fs::path(workPath) / "radio_board_defaults.ini");
                reviews.clear();
                history.clear();
                dirty = true;
                message = "Imported legacy fallbacks; Classic and Modern legacy values remain shared.";
            } catch (const std::exception &) {
                message = "Cannot import legacy INI; check the source file and workspace.";
            }
        }
    }
    const char *layers[] = {"Personal overrides", "Template / release defaults"};
    if (ImGui::Combo("Edit layer", &layer, layers, 2)) {
        editing = layer ? &store.templates : &store.personal;
        history.clear();
    }
    if (ImGui::Button("Undo") && history.undo(*editing, reviews))
        dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Redo") && history.redo(*editing, reviews))
        dirty = true;
    const char *sets[] = {"Classic", assets.available[2] ? "Modern" : "Modern (not installed)"};
    int visibleSet = set == 0 ? 0 : 1;
    if (ImGui::Combo("Modelset", &visibleSet, sets, 2)) {
        set = editor::visibleModelSets[visibleSet];
        loadModel();
    }
    ImGui::InputText("Search", search, sizeof(search));
    if (ImGui::Button("Next unreviewed")) {
        bool found = false;
        for (size_t step = 1; step <= assets.vehicles.size() * 2; step++) {
            int index = (std::max(0, selected) + (set == 0 ? 0 : 1) * (int)assets.vehicles.size() + (int)step) %
                        ((int)assets.vehicles.size() * 2);
            int s = editor::visibleModelSets[index / (int)assets.vehicles.size()],
                i = index % (int)assets.vehicles.size();
            if (assets.vehicles[i].supported() && assets.available[s] &&
                !reviews.count(section(s, assets.vehicles[i].id))) {
                set = s;
                selected = i;
                loadModel();
                found = true;
                break;
            }
        }
        if (!found)
            message = "All available supported entries have been reviewed.";
    }
    if (selected >= 0 && !fingerprintNow.empty() && assets.available[set] &&
        assets.vehicles[selected].supported()) {
        ImGui::SameLine();
        if (ImGui::Button("Mark reviewed")) {
            reviews[currentSection()] = fingerprintNow;
            staleReviews.erase(currentSection());
            dirty = true;
        }
    }
    ImGui::TextWrapped("Checklist is per modelset. Tick entries for paste or deployment. Asset "
                       "fingerprints are checked when opening the workspace/game.");
    ImGui::BeginChild("Catalogue", ImVec2(0, 220), true);
    for (size_t i = 0; i < assets.vehicles.size(); i++) {
        auto &car = assets.vehicles[i];
        if (lower(car.name).find(lower(search)) == std::string::npos)
            continue;
        ImGui::PushID((int)i);
        std::string s = section(set, car.id);
        bool checked = selection.count(s) != 0;
        if (ImGui::Checkbox("##select", &checked)) {
            if (checked)
                selection.insert(s);
            else
                selection.erase(s);
        }
        ImGui::SameLine();
        std::string label = car.name + " [" + std::to_string(car.id) + "]  " + status(set, car);
        if (ImGui::Selectable(label.c_str(), selected == (int)i)) {
            selected = (int)i;
            loadModel();
            if (reviews.count(s) && reviews[s] != fingerprintNow) {
                reviews.erase(s);
                dirty = true;
                message += "; geometry/textures changed: review required.";
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button("Copy placement") && selected >= 0)
        clipboard = values();
    ImGui::SameLine();
    if (ImGui::Button("Paste to ticked") && !clipboard.empty()) {
        remember();
        for (auto &s : selection) {
            editing->entries[s] = clipboard;
            reviews.erase(s);
        }
        if (selection.empty())
            editing->entries[currentSection()] = clipboard;
    }
    if (ImGui::Button("Reset current entry to lower layer") && selected >= 0) {
        remember();
        editing->entries.erase(currentSection());
        reviews.erase(currentSection());
    }
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(970, 8), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(460, 760), ImGuiCond_Once);
    ImGui::Begin("Placement");
    if (selected >= 0 && clump && !fingerprintNow.empty() && assets.available[set] &&
        assets.vehicles[selected].supported()) {
        if (layer && store.personal.entries.count(currentSection()) &&
            !store.personal.entries.at(currentSection()).empty()) {
            ImGui::TextWrapped(
                "Personal overrides take priority over this template in the game. The effective preview "
                "includes them; template fields below show the authored layer.");
            if (ImGui::Button("Clear personal overrides and edit personal layer")) {
                history.clear();
                layer = 0;
                editing = &store.personal;
                remember();
                editing->entries.erase(currentSection());
                reviews.erase(currentSection());
            }
        }
        ImGui::Checkbox("Preview effective game values", &effectivePreview);
        auto v = values();
        const char *elements[] = {"Virtual wheel", "Dashboard group",     "Map", "Status", "Clock",
                                  "Ammo",          "Radio / music player"};
        ImGui::Combo("Element", &element, elements, 7);
        const char *operations[] = {"Move gizmo", "Rotate gizmo"};
        ImGui::Combo("Gizmo", &operation, operations, 2);
        ImGui::PushItemWidth(160);
        if (element == 0) {
            transformFields("Wheel", v, false);
            editField("WheelRadius", "Radius cm", v);
            bool visible = v["WheelVisible"] != 0;
            if (ImGui::Checkbox("Wheel visible in game", &visible)) {
                remember();
                setValue("WheelVisible", visible ? 1 : 0);
            }
        } else if (element == 1) {
            bool linked = v["DashFollowWheel"] != 0;
            if (ImGui::Checkbox("Follow neutral wheel", &linked)) {
                remember();
                auto w = wheel(seat, assets.vehicles[selected].boat(), v);
                auto d = dashboard(w, v);
                Basis parent = linked ? w : Basis();
                Vec delta = d.position - parent.position;
                auto angles = relativeAngles(d, parent);
                setValue("DashFollowWheel", linked ? 1 : 0);
                setValue("DashX", dot(delta, parent.right) * 100);
                setValue("DashY", dot(delta, parent.forward) * 100);
                setValue("DashZ", dot(delta, parent.up) * 100);
                setValue("DashPitch", angles.x);
                setValue("DashYaw", angles.y);
                setValue("DashRoll", angles.z);
                v = values();
            }
            transformFields("Dash", v, true);
        } else
            transformFields(element == 6 ? "Radio" : panelName(element - 2), v, true);
        if (ImGui::Button("Create fixed dashboard at wheel")) {
            remember();
            auto w = wheel(seat, assets.vehicles[selected].boat(), v);
            setValue("DashFollowWheel", 0);
            setValue("DashX", w.position.x * 100);
            setValue("DashY", w.position.y * 100);
            setValue("DashZ", w.position.z * 100);
            setValue("DashPitch", v["WheelPitch"]);
            setValue("DashYaw", v["WheelYaw"]);
            setValue("DashRoll", v["WheelRoll"]);
        }
        ImGui::Separator();
        editField("SeatHeight", "Seat height cm", v);
        editField("SeatDistance", "Seat distance cm", v);
        editField("DefaultSeatHeight", "Default height cm", v);
        editField("DefaultSeatDistance", "Default distance cm", v);
        ImGui::PopItemWidth();
    } else
        ImGui::TextWrapped("Choose a supported car or boat from a locally installed modelset with a "
                           "driver seat marker.");
    if (ImGui::Button("Driver view"))
        driver();
    ImGui::SameLine();
    if (ImGui::Button("Exterior")) {
        eye = Vec(-5, -6, 3);
        yaw = .7f;
        pitch = -.2f;
        orbit = true;
    }
    ImGui::Checkbox("Auto reset camera on vehicle / modelset change", &autoResetCamera);
    ImGui::Checkbox("Orbit around vehicle", &orbit);
    ImGui::Checkbox("Invert mouse X", &invertX);
    ImGui::SameLine();
    ImGui::Checkbox("Invert mouse Y", &invertY);
    ImGui::Checkbox("VSync", &vsync);
    if (ImGui::InputInt("FPS cap (0=off)", &fpsLimit))
        fpsLimit = std::max(0, std::min(1000, fpsLimit));
    ImGui::Text("Preview: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Pan sensitivity", &panSensitivity, .1f, 3.f, "%.2fx");
    ImGui::SliderFloat("Look / orbit sensitivity", &rotationSensitivity, .1f, 3.f, "%.2fx");
    ImGui::ColorEdit3("Background", background);
    ImGui::SliderFloat("Field of view", &fov, 35, 115);
    ImGui::SliderFloat("Eye above seat (preview)", &eyeAboveSeat, .3f, 1.f);
    ImGui::TextWrapped(
        "Neutral eye height approximates the seated head animation. Final comfort/reach requires a "
        "headset check. Right drag: look/orbit. Middle drag: pan. Wheel: dolly. WASD: move.");
    ImGui::SliderFloat("Steering preview", &steering, -80, 80);
    if (ImGui::Button("Reset steering"))
        steering = 0;
    if (ImGui::Button("Save application settings"))
        savePreferences(true);
    if (!preferencesError.empty())
        ImGui::TextWrapped("%s", preferencesError.c_str());
    ImGui::Checkbox("Preview HUD", &showHud);
    ImGui::SameLine();
    ImGui::Checkbox("Preview wheel", &showWheel);
    for (int i = 0; i < 6; i++) {
        ImGui::PushID(i);
        std::string label = "Extra " + std::to_string(i + 1);
        ImGui::Checkbox(label.c_str(), &extras[i]);
        if (i % 3 != 2)
            ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(8, 700), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(600, 280), ImGuiCond_Once);
    ImGui::Begin("Quest 3 import and deployment");
    ImGui::InputText("Manager executable", managerPath, sizeof(managerPath));
    ImGui::InputText("ADB executable", adbPath, sizeof(adbPath));
    ImGui::InputInt("Device index (0=auto)", &deviceIndex);
    if (ImGui::Button("Check headset"))
        device("status");
    ImGui::SameLine();
    if (ImGui::Button("Import all placement settings")) {
        if (dirty)
            ImGui::OpenPopup("Import replaces local layers");
        else
            device("import");
    }
    if (ImGui::BeginPopupModal("Import replaces local layers", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save first or discard current edits before import.");
        if (ImGui::Button("Save then import")) {
            if (save())
                device("import");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    const char *prefs[] = {"Stop on conflict", "Use local conflicting fields",
                           "Keep remote conflicting fields"};
    ImGui::Combo("Conflicts", &conflictPreference, prefs, 3);
    if (ImGui::Button("Preview selected changes"))
        device("preview");
    ImGui::SameLine();
    if (ImGui::Button(layer ? "Install template" : "Install personal overrides"))
        ImGui::OpenPopup("Install placement pack?");
    if (ImGui::BeginPopupModal("Install placement pack?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("The destination is reread, merged and backed up. The updated game reloads "
                           "the pack live; acknowledgement is checked while it runs.");
        if (ImGui::Button("Install selected entries")) {
            device("deploy");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::Checkbox("Live sync current entry to headset", &liveSync)) {
        liveObserved.clear();
        liveSent.clear();
        liveStatus = liveSync ? "Waiting to synchronize current entry..." : "Disabled";
    }
    ImGui::TextWrapped("Live sync: %s", liveStatus.c_str());
    ImGui::TextWrapped("Import first. Live sync saves and deploys the current entry/layer after a short "
                       "pause. Conflicts or missing game acknowledgement stop synchronization.");
    if (liveSync && selected >= 0 && clump && assets.available[set] &&
        assets.vehicles[selected].supported()) {
        std::string signature = std::to_string(layer) + currentSection() + editing->text();
        auto now = std::chrono::steady_clock::now();
        if (signature != liveObserved) {
            liveObserved = signature;
            liveChangedAt = now;
        }
        if (!deviceTask.valid() && signature != liveSent &&
            now - liveChangedAt > std::chrono::milliseconds(700)) {
            liveSent = signature;
            device("deploy", true);
            if (!deviceTask.valid()) {
                liveSync = false;
                liveStatus = "Stopped: " + message;
            }
        }
    }
    ImGui::TextWrapped("Save exports the selected layer in the workspace. Deployment requires Python 3 "
                       "and ADB. Only configuration files are transferred.");
    ImGui::End();
    if (deviceTask.valid() && !taskIsLive)
        ImGui::OpenPopup("Device operation");
    if (ImGui::BeginPopupModal("Device operation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!deviceTask.valid())
            ImGui::CloseCurrentPopup();
        else
            ImGui::Text("Working locally with the personal Quest 3 device...");
        ImGui::EndPopup();
    }
    if (showDeviceOutput) {
        ImGui::SetNextWindowPos(ImVec2(410, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(550, 350), ImGuiCond_Once);
        ImGui::Begin("Device transfer preview", &showDeviceOutput);
        ImGui::TextUnformatted(deviceOutput.c_str());
        ImGui::End();
    }
    if (closeRequested) {
        if (dirty)
            ImGui::OpenPopup("Unsaved placements");
        else
            sk::globals.quit = 1;
    }
    if (ImGui::BeginPopupModal("Unsaved placements", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save before closing?");
        if (ImGui::Button("Save and close")) {
            if (save())
                sk::globals.quit = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
            sk::globals.quit = 1;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            closeRequested = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
static void draw(float dt) {
    if (glfwWindowShouldClose(*engineOpenParams.window)) {
        glfwSetWindowShouldClose(*engineOpenParams.window, GLFW_FALSE);
        closeRequested = true;
    }
    ImGui_ImplRW_NewFrame(dt);
    ImGuizmo::BeginFrame();
    auto &io = ImGui::GetIO();
    Vec forward(std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch));
    Vec right = unit(cross(forward, Vec(0, 0, 1))), up = cross(right, forward);
    float mouseX = io.MouseDelta.x * (invertX ? -1.f : 1.f),
          mouseY = io.MouseDelta.y * (invertY ? -1.f : 1.f);
    if (!io.WantCaptureMouse && !ImGuizmo::IsUsing()) {
        if (io.MouseDown[1]) {
            yaw -= mouseX * .005f * rotationSensitivity;
            pitch = std::max(-1.5f, std::min(1.5f, pitch - mouseY * .005f * rotationSensitivity));
            forward =
                Vec(std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch));
            right = unit(cross(forward, Vec(0, 0, 1)));
            up = cross(right, forward);
            if (orbit) {
                float radius = std::max(1.f, std::sqrt(dot(eye, eye)));
                eye = forward * (-radius);
            }
        }
        if (io.MouseDown[2])
            eye = eye + right * (-mouseX * .005f * panSensitivity) +
                  up * (mouseY * .005f * panSensitivity);
        eye = eye + forward * (io.MouseWheel * .15f);
    }
    if (!io.WantCaptureKeyboard) {
        float step = dt * (io.KeyShift ? 4 : 1);
        if (io.KeysDown['W'])
            eye = eye + forward * step;
        if (io.KeysDown['S'])
            eye = eye - forward * step;
        if (io.KeysDown['A'])
            eye = eye - right * step;
        if (io.KeysDown['D'])
            eye = eye + right * step;
    }
    // RenderWare cameras store the screen-left axis in matrix.right.
    auto frame = camera->getFrame();
    frame->matrix.right = rwv(right * (-1));
    frame->matrix.up = rwv(up);
    frame->matrix.at = rwv(forward);
    frame->matrix.pos = rwv(eye);
    frame->matrix.update();
    frame->updateObjects();
    float h = std::tan(fov * .00872664626f);
    rw::V2d vw = {h, h * sk::globals.height / std::max(1.f, (float)sk::globals.width)};
    camera->setViewWindow(&vw);
    auto clear = rw::makeRGBA((int)(background[0] * 255), (int)(background[1] * 255),
                              (int)(background[2] * 255), 255);
    camera->clear(&clear, rw::Camera::CLEARIMAGE | rw::Camera::CLEARZ);
    camera->beginUpdate();
    renderModel();
    renderPlacement();
    ui();
    gizmo();
    ImGui::Render();
    camera->endUpdate();
    camera->showRaster(vsync ? rw::Raster::FLIPWAITVSYNCH : 0);
    savePreferences();
    if (smokeAll) {
        int total = (int)assets.vehicles.size() * 2;
        while (smokeIndex < total) {
            int index = smokeIndex++;
            set = editor::visibleModelSets[index / (int)assets.vehicles.size()];
            selected = index % (int)assets.vehicles.size();
            if (assets.available[set] && assets.vehicles[selected].supported()) {
                loadModel();
                bool ok = clump && modelTextures && modelWheels && !fingerprintNow.empty();
                if (!ok)
                    smokeFailures++;
                std::printf("Preview %s: %s\n", currentSection().c_str(), ok ? "PASS" : "FAIL");
                break;
            }
        }
        if (smokeIndex >= total) {
            std::printf("Preview catalogue validation: failures=%d\n", smokeFailures);
            sk::globals.quit = 1;
        }
    } else if (smokeFrames && ++smokeCount >= smokeFrames)
        sk::globals.quit = 1;
}
sk::EventStatus AppEventHandler(sk::Event event, void *param) {
    using namespace sk;
    if (event == INITIALIZE) {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        globals.windowtitle = "Liberty City VR - Vehicle Placement Editor";
        globals.width = 1440;
        globals.height = 1000;
        auto executable = fs::absolute(args.argv[0]);
#ifdef _WIN32
        wchar_t module[32768];
        DWORD length = GetModuleFileNameW(nullptr, module, 32768);
        if (length && length < 32768)
            executable = fs::path(std::wstring(module, length));
#else
        std::error_code executableError;
        auto module = fs::read_symlink("/proc/self/exe", executableError);
        if (!executableError)
            executable = module;
#endif
        auto executableFolder = executable.parent_path();
        preferencesPath = executableFolder / "vehicle-editor.ini";
        imguiSettingsPath = (executableFolder / "vehicle-editor-ui.ini").string();
        ImGui::GetIO().IniFilename = imguiSettingsPath.c_str();
#ifdef _WIN32
        auto portableManager = executableFolder / "manager" / "LibertyManager.exe";
#else
        auto portableManager = executableFolder / "manager" / "LibertyManager";
#endif
        if (fs::exists(portableManager))
            std::snprintf(managerPath, sizeof(managerPath), "%s", portableManager.string().c_str());
        std::snprintf(workPath, sizeof(workPath), "%s",
                      (executableFolder / "workspace").string().c_str());
        for (const char *variable : {"ANDROID_HOME", "ANDROID_SDK_ROOT"}) {
            const char *sdk = std::getenv(variable);
            if (!sdk || !*sdk)
                continue;
#ifdef _WIN32
            auto adb = fs::path(sdk) / "platform-tools" / "adb.exe";
#else
            auto adb = fs::path(sdk) / "platform-tools" / "adb";
#endif
            if (fs::exists(adb)) {
                std::snprintf(adbPath, sizeof(adbPath), "%s", adb.string().c_str());
                break;
            }
        }
#ifdef _WIN32
        if (std::string(adbPath) == "adb" &&
            fs::exists("C:/LCVRBuild/.android-sdk/platform-tools/adb.exe"))
            std::snprintf(adbPath, sizeof(adbPath), "%s",
                          "C:/LCVRBuild/.android-sdk/platform-tools/adb.exe");
#endif
        if (fs::exists(preferencesPath)) {
            Preferences p;
            if (p.load(preferencesPath, preferencesError)) {
                applyPreferences(p);
                savedPreferences = p.text();
            } else
                preferencesWritable = false;
        }
        for (int i = 1; i < args.argc; i++) {
            std::string a = args.argv[i];
            if (a == "--game" && i + 1 < args.argc)
                std::snprintf(gamePath, sizeof(gamePath), "%s", args.argv[++i]);
            else if (a == "--workspace" && i + 1 < args.argc)
                std::snprintf(workPath, sizeof(workPath), "%s", args.argv[++i]);
            else if (a == "--smoke" && i + 1 < args.argc)
                smokeFrames = std::atoi(args.argv[++i]);
            else if (a == "--smoke-all") {
                smokeAll = true;
                smokeFrames = 1;
            }
        }
        return EVENTPROCESSED;
    }
    if (event == PLUGINATTACH) {
        rw::registerMeshPlugin();
        rw::registerNativeDataPlugin();
        rw::registerAtomicRightsPlugin();
        rw::registerMaterialRightsPlugin();
        rw::xbox::registerVertexFormatPlugin();
        rw::registerSkinPlugin();
        rw::registerHAnimPlugin();
        rw::registerMatFXPlugin();
        rw::registerUVAnimPlugin();
        rw::registerUserDataPlugin();
        rw::ps2::registerADCPlugin();
        nodeOffset = rw::Frame::registerPlugin(64, 0x0253F2FE, nodeCtor, nullptr, nodeCopy);
        rw::Frame::registerPluginStream(0x0253F2FE, nodeRead, nullptr, nullptr);
        return EVENTPROCESSED;
    }
    if (event == RWINITIALIZE) {
        if (!sk::InitRW())
            return EVENTERROR;
        glfwSetScrollCallback(*engineOpenParams.window, [](GLFWwindow *, double, double y) {
            ImGui::GetIO().MouseWheel += (float)y;
        });
        world = rw::World::create();
        camera = sk::CameraCreate(globals.width, globals.height, 1);
        camera->setNearPlane(.01f);
        camera->setFarPlane(200);
        world->addCamera(camera);
        auto ambient = rw::Light::create(rw::Light::AMBIENT);
        ambient->setColor(.65f, .65f, .65f);
        world->addLight(ambient);
        auto light = rw::Light::create(rw::Light::DIRECTIONAL);
        light->setColor(.65f, .65f, .65f);
        light->setFrame(rw::Frame::create());
        rw::V3d axis = {1, 0, 0};
        light->getFrame()->rotate(&axis, 145, rw::COMBINEREPLACE);
        world->addLight(light);
        rw::Texture::findCB = textureFind;
        rw::Texture::setCreateDummies(true);
        ImGui_ImplRW_Init();
        if (*workPath)
            openWorkspace();
        if (*gamePath) {
            try {
                assets.open(gamePath);
                auto first = std::find_if(assets.vehicles.begin(), assets.vehicles.end(),
                                          [](const Vehicle &v) { return v.supported(); });
                selected = first == assets.vehicles.end() ? 0 : (int)(first - assets.vehicles.begin());
                loadModel();
                checkReviews();
            } catch (const std::exception &e) {
                message = e.what();
                smokeOk = false;
            }
        }
        if (smokeFrames) {
            smokeOk = smokeOk && clump && modelTextures && modelWheels && !fingerprintNow.empty();
            std::printf("Vehicle editor smoke: %s; catalogue=%zu\n", smokeOk ? "PASS" : "FAIL",
                        assets.vehicles.size());
            std::fflush(stdout);
        }
        return EVENTPROCESSED;
    }
    ImGuiEventHandler(event, param);
    if (event == IDLE) {
        auto frameStart = std::chrono::steady_clock::now();
        draw(*(float *)param);
        if (fpsLimit > 0 && !smokeFrames && !globals.quit)
            editor::waitForFrame(frameStart, fpsLimit);
        return EVENTPROCESSED;
    }
    if (event == RESIZE && camera) {
        auto r = (rw::Rect *)param;
        if (r->w < 1)
            r->w = 1;
        if (r->h < 1)
            r->h = 1;
        globals.width = r->w;
        globals.height = r->h;
        sk::CameraSize(camera, r);
        return EVENTPROCESSED;
    }
    if (event == QUIT || (event == KEYDOWN && *(int *)param == KEY_ESC)) {
        closeRequested = true;
        return EVENTPROCESSED;
    }
    if (event == RWTERMINATE) {
        if (preferencesWritable)
            savePreferences(true);
        clearModel();
        for (auto d : genericDictionaries)
            if (d)
                d->destroy();
        ImGui_ImplRW_Shutdown();
        sk::TerminateRW();
        // The shared skeleton returns zero after termination. Validation runs
        // must still signal missing models/textures through their process status.
        if (smokeFrames && (!smokeOk || smokeFailures))
            std::exit(EXIT_FAILURE);
        return EVENTPROCESSED;
    }
    return EVENTNOTPROCESSED;
}
