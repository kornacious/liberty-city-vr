#include <cassert>
#include <cstdio>
#include <initializer_list>
#define nil nullptr
enum { VIS_INVISIBLE, VIS_VISIBLE };
struct CEntity {
    void *m_rwObject;
    bool bIsVisible, monoVisible;
    bool GetIsOnScreen() { return monoVisible; }
    bool GetIsOnScreenComplex() { return monoVisible; }
    bool IsVisible() { return m_rwObject && bIsVisible && monoVisible; }
};

// Compile the actual branch both with and without the shared VR list bypass.
namespace vr {
#define RW_VULKAN
struct CRenderer {
    static bool ms_bVrOmnidirectionalScan;
    static int PlaneVisibility(CEntity *);
    static bool IsEntityOutsideCurrentView(CEntity *, bool complex = false);
};
bool CRenderer::ms_bVrOmnidirectionalScan = false;
#include "plane-visibility-production.inc"
#undef RW_VULKAN
}
namespace mono {
struct CRenderer {
    static int PlaneVisibility(CEntity *);
    static bool IsEntityOutsideCurrentView(CEntity *, bool complex = false);
};
#include "plane-visibility-production.inc"
}
int main()
{
    int object = 0;
    for (bool loaded : {false, true})
        for (bool enabled : {false, true})
            for (bool inMonoView : {false, true}) {
                CEntity plane{loaded ? &object : nullptr, enabled, inMonoView};
                const int stock = loaded && enabled && inMonoView ? VIS_VISIBLE : VIS_INVISIBLE;
                assert(mono::CRenderer::PlaneVisibility(&plane) == stock);
                vr::CRenderer::ms_bVrOmnidirectionalScan = false;
                assert(vr::CRenderer::PlaneVisibility(&plane) == stock);
                vr::CRenderer::ms_bVrOmnidirectionalScan = true;
                assert(vr::CRenderer::PlaneVisibility(&plane) ==
                    (loaded && enabled ? VIS_VISIBLE : VIS_INVISIBLE));
            }
    std::puts("Plane visibility: shared VR list, mono/theater and hidden/unloaded cases passed.");
}
