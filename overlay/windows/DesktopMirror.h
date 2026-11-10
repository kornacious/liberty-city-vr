#pragma once
#include "MirrorPolicy.h"
#include <string>
#include <vulkan/vulkan.h>
namespace Desktop {
struct Options {
  int eye = 0;
  Framing framing = Framing::Square;
  WindowMode mode = WindowMode::Windowed;
  bool panel = false, keyboard = true, gamepad = true, capture = false,
       statistics = false;
};
struct InputState {
  bool focused = false, keys[256]{}, buttons[5]{}, wantMouse = false,
       wantKeyboard = false;
  float mouseX = 0, mouseY = 0, deltaX = 0, deltaY = 0, wheel = 0;
  bool pad = false;
  unsigned short padButtons = 0;
  float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
bool Open(const std::string &userRoot);
void Close();
void Pump();
bool QuitRequested();
Options &Settings();
const InputState &Input();
void SetMode(WindowMode mode);
void ResetWindow();
void BeginUi();
// Game supplies telemetry without linking re3 into the synthetic diagnostic.
void SetRenderingDetailsProvider(const char *(*provider)(bool visible));
void SetOcclusionMapProvider(const unsigned char *(*provider)(int eye,int *size));
bool RenderingDetailsVisible();
void EndUi();
void *WindowHandle();
void ClientSize(unsigned &width, unsigned &height);
void RequestResize();
bool ConsumeResize();
bool InitializeGpu(VkInstance instance, VkPhysicalDevice physical,
                   VkDevice device, VkQueue queue, unsigned family);
void ShutdownGpu();
void Present(VkImage image, VkImageView view, unsigned width, unsigned height,
             float theaterAspect = 0);
void SetGameOverlay(const unsigned char *rgba, int width, int height,
                    bool menu, bool detailedStats = false);
Picture CurrentPicture();
void RequestSnapshot(const std::string &path);
unsigned PresentedFrames();
// Game-only adapters; the diagnostic links the window/GPU layer without re3.
void UpdateGameInput();
void InitializeGameInput();
void MergeVrInput(void *padInput);
void MergeGamepad(void *controllerState);
} // namespace Desktop
