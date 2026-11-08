#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

static float SCREEN_WIDTH, SCREEN_HEIGHT;
struct CRect {
    float left, top, right, bottom;
    CRect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
};
struct CRGBA {
    int r, g, b, a;
    CRGBA(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {}
};
static std::vector<CRect> rectangles;
namespace CSprite2d {
static void DrawRect(CRect rect, CRGBA colour)
{
    assert(colour.r == 0 && colour.g == 0 && colour.b == 0 && colour.a == 255);
    assert(rect.left <= rect.right && rect.top <= rect.bottom);
    rectangles.push_back(rect);
}
}
using std::min;
using std::max;
#define Min min
#define Max max

static void draw(bool vrTrackedScope, float rLeft, float rRight, float rTop, float rBottom)
{
    rectangles.clear();
#include "scope-overlay-mask-production.inc"
}

int main()
{
    for (float width : {1280.0f, 2160.0f})
    for (float height : {720.0f, 2160.0f})
    for (float cx : {0.0f, width * 0.5f, width})
    for (float cy : {0.0f, height * 0.5f, height})
    for (bool vr : {false, true}) {
        SCREEN_WIDTH = width;
        SCREEN_HEIGHT = height;
        const float radius = height * 210.0f / 480.0f;
        draw(vr, cx-radius, cx+radius, cy-radius, cy+radius);
        // Sample well beyond every HUD edge, including both asymmetric eye
        // views and a reduced/offset HUD safe area. The aperture stays clear.
        for (int y = -20; y <= 30; ++y)
        for (int x = -20; x <= 30; ++x) {
            const float px = width*x/10.0f, py = height*y/10.0f;
            const bool inAperture = px > cx-radius && px < cx+radius &&
                                    py > cy-radius && py < cy+radius;
            const bool inDisplay = px >= 0 && px <= width && py >= 0 && py <= height;
            bool covered = false;
            for (const auto &r : rectangles)
                covered |= px >= r.left && px <= r.right && py >= r.top && py <= r.bottom;
            if (inAperture) assert(!covered);
            else if (vr || inDisplay) assert(covered);
        }
    }
    std::puts("Scope overlay mask: aperture and outer coverage passed (72 layouts).");
}
