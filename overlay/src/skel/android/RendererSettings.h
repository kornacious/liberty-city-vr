#pragma once
#include <cstddef>
// Both the VR menu and desktop tools address the same live engine preferences.
namespace RendererSettings {
struct Control {
    const char *name;int minimum,maximum;bool restart;
    int (*get)();void (*set)(int);
};
const Control *Controls(size_t &count);
void ApplySideEffects();
struct Frame {unsigned colourMode,red,green,blue,alpha;};
Frame ApplyFrame(bool theaterFrame);
}
