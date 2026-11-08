#define NOMINMAX
#include "DesktopMirror.h"
#include "imgui.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <windowsx.h>
#include <xinput.h>
namespace Desktop {
namespace {
HWND window = nullptr;
Options options;
InputState input;
bool quit = false, resize = true, uiActive = false;
ImGuiContext *context = nullptr;
RECT saved{100, 100, 1060, 1060};
std::string config;
using ReadPad = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
ReadPad readPad = nullptr;
HMODULE xinput = nullptr;
void Cursor() {
  if (window && input.focused && options.capture && !options.panel) {
    RECT rect;
    GetClientRect(window, &rect);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT *>(&rect), 2);
    ClipCursor(&rect);
  } else
    ClipCursor(nullptr);
}
void Save() {
  if (config.empty())
    return;
  std::ofstream file(config + ".partial", std::ios::trunc);
  file << "1 " << options.eye << ' ' << int(options.framing) << ' '
       << int(options.mode) << ' ' << options.keyboard << ' ' << options.gamepad
       << '\n';
  file.close();
  if (file)
    MoveFileExA((config + ".partial").c_str(), config.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
void MouseButton(unsigned button, bool down) {
  input.buttons[button] = down;
  if (down)
    SetCapture(window);
  else {
    bool held = false;
    for (bool value : input.buttons)
      held |= value;
    if (!held && GetCapture() == window)
      ReleaseCapture();
  }
}
void Shortcut(unsigned key) {
  switch (key) {
  case VK_F1:
    options.panel = !options.panel;
    if (options.panel)
      options.capture = false;
    break;
  case VK_F2:
    options.eye ^= 1;
    break;
  case VK_F3:
    options.framing =
        options.framing == Framing::Square ? Framing::Cropped : Framing::Square;
    break;
  case VK_F4:
    SetMode(WindowMode::Windowed);
    break;
  case VK_F5:
    SetMode(WindowMode::Borderless);
    break;
  case VK_F6:
    SetMode(WindowMode::Fullscreen);
    break;
  case VK_F7:
    ResetWindow();
    break;
  case VK_F8:
    options.capture = !options.capture;
    if (options.capture)
      options.panel = false;
    break;
  case VK_F9:
    options.keyboard = !options.keyboard;
    break;
  case VK_F10:
    options.gamepad = !options.gamepad;
    break;
  case VK_F11:
    options.statistics = !options.statistics;
    break;
  case VK_F12:
    options = Options{};
    ResetWindow();
    break;
  }
  Cursor();
  Save();
}
LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
  auto &io = ImGui::GetIO();
  switch (message) {
  case WM_CLOSE:
    quit = true;
    return 0;
  case WM_SIZE:
    resize = true;
    Cursor();
    return 0;
  case WM_ACTIVATE:
    input.focused = LOWORD(w) != WA_INACTIVE;
    if (!input.focused) {
      auto keep = InputState{};
      input = keep;
      for (auto &key : io.KeysDown)
        key = false;
      for (auto &button : io.MouseDown)
        button = false;
      if (GetCapture() == hwnd)
        ReleaseCapture();
    }
    Cursor();
    return 0;
  case WM_SETCURSOR:
    if (LOWORD(l) == HTCLIENT && options.capture && input.focused &&
        !options.panel) {
      SetCursor(nullptr);
      return TRUE;
    }
    break;
  case WM_SYSKEYDOWN:
    if (w == VK_F4 && (l & (1 << 29))) {
      quit = true;
      return 0;
    }
    [[fallthrough]];
  case WM_KEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYUP:
    if (w < 256) {
      const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      if (w >= VK_F1 && w <= VK_F12) {
        if (down && !(l & (1 << 30)))
          Shortcut(unsigned(w));
        return 0;
      }
      input.keys[w] = down;
      io.KeysDown[w] = down;
    }
    return 0;
  case WM_CHAR:
    if (w > 0 && w < 65536 && options.panel)
      io.AddInputCharacter(ImWchar(w));
    return 0;
  case WM_MOUSEMOVE:
    input.mouseX = float(GET_X_LPARAM(l));
    input.mouseY = float(GET_Y_LPARAM(l));
    return 0;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    MouseButton(0, message == WM_LBUTTONDOWN);
    return 0;
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    MouseButton(1, message == WM_RBUTTONDOWN);
    return 0;
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    MouseButton(2, message == WM_MBUTTONDOWN);
    return 0;
  case WM_XBUTTONDOWN:
  case WM_XBUTTONUP:
    MouseButton(GET_XBUTTON_WPARAM(w) == XBUTTON1 ? 3 : 4,
                message == WM_XBUTTONDOWN);
    return TRUE;
  case WM_CAPTURECHANGED:
    for (auto &button : input.buttons)
      button = false;
    return 0;
  case WM_MOUSEWHEEL:
    input.wheel += float(GET_WHEEL_DELTA_WPARAM(w)) / WHEEL_DELTA;
    return 0;
  case WM_INPUT: {
    RAWINPUT raw{};
    UINT size = sizeof(raw);
    if (GetRawInputData(HRAWINPUT(l), RID_INPUT, &raw, &size,
                        sizeof(RAWINPUTHEADER)) != UINT(-1) &&
        raw.header.dwType == RIM_TYPEMOUSE &&
        !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
      input.deltaX += float(raw.data.mouse.lLastX);
      input.deltaY += float(raw.data.mouse.lLastY);
    }
    break;
  }
  }
  return DefWindowProcW(hwnd, message, w, l);
}
} // namespace
Options &Settings() { return options; }
const InputState &Input() { return input; }
void *WindowHandle() { return window; }
bool QuitRequested() { return quit; }
void RequestResize() { resize = true; }
bool ConsumeResize() {
  const bool value = resize;
  resize = false;
  return value;
}
void ClientSize(unsigned &width, unsigned &height) {
  RECT r{};
  if (window)
    GetClientRect(window, &r);
  width = unsigned(r.right);
  height = unsigned(r.bottom);
}
void SetMode(WindowMode mode) {
  if (!window)
    return;
  if (options.mode != WindowMode::Fullscreen)
    GetWindowRect(window, &saved);
  options.mode = mode;
  const DWORD style =
      mode == WindowMode::Windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP;
  SetWindowLongPtrW(window, GWL_STYLE, style | WS_VISIBLE);
  RECT rect = saved;
  if (mode == WindowMode::Fullscreen) {
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                    &monitor);
    rect = monitor.rcMonitor;
  }
  SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left,
               rect.bottom - rect.top, SWP_NOZORDER | SWP_FRAMECHANGED);
  resize = true;
  Cursor();
  Save();
}
void ResetWindow() {
  MONITORINFO monitor{sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY),
                  &monitor);
  const int side = int(std::min<LONG>(
      960, std::min(monitor.rcWork.right - monitor.rcWork.left,
                    monitor.rcWork.bottom - monitor.rcWork.top)));
  SetMode(WindowMode::Windowed);
  SetWindowPos(window, nullptr,
               monitor.rcWork.left +
                   (monitor.rcWork.right - monitor.rcWork.left - side) / 2,
               monitor.rcWork.top +
                   (monitor.rcWork.bottom - monitor.rcWork.top - side) / 2,
               side, side, SWP_NOZORDER);
  GetWindowRect(window, &saved);
}
bool Open(const std::string &root) {
  if (window)
    return true;
  quit = false;
  resize = true;
  input = {};
  context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  ImGui::GetIO().IniFilename = nullptr;
  config =
      root.empty()
          ? ""
          : (std::filesystem::u8path(root) / "desktop-preview.cfg").u8string();
  if (!config.empty()) {
    int version, eye, framing, mode, kb, pad;
    std::ifstream file(config);
    if (file >> version >> eye >> framing >> mode >> kb >> pad &&
        version == 1 && eye >= 0 && eye <= 1 && framing >= 0 && framing <= 1 &&
        mode >= 0 && mode <= 2) {
      options.eye = eye;
      options.framing = Framing(framing);
      options.mode = WindowMode(mode);
      options.keyboard = kb != 0;
      options.gamepad = pad != 0;
    }
  }
  SetProcessDPIAware();
  WNDCLASSW cls{};
  cls.lpfnWndProc = Procedure;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"LibertyVrDesktopPreview";
  cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  cls.hbrBackground = HBRUSH(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&cls);
  window = CreateWindowExW(0, cls.lpszClassName,
                           L"Liberty City VR — desktop preview (F1 controls)",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 960, 960,
                           nullptr, nullptr, cls.hInstance, nullptr);
  if (!window) {
    ImGui::Shutdown();
    ImGui::DestroyContext(context);
    context = nullptr;
    return false;
  }
  auto mode = options.mode;
  options.mode = WindowMode::Windowed;
  ResetWindow();
  SetMode(mode);
  RAWINPUTDEVICE raw{1, 2, 0, window};
  RegisterRawInputDevices(&raw, 1, sizeof(raw));
  xinput =
      LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!xinput)
    xinput = LoadLibraryExW(L"xinput9_1_0.dll", nullptr,
                            LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (xinput)
    readPad =
        reinterpret_cast<ReadPad>(GetProcAddress(xinput, "XInputGetState"));
  auto &io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_Tab] = VK_TAB;
  io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
  io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
  io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
  io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
  io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
  io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
  io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
  io.KeyMap[ImGuiKey_Home] = VK_HOME;
  io.KeyMap[ImGuiKey_End] = VK_END;
  io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
  io.KeyMap[ImGuiKey_A] = 'A';
  io.KeyMap[ImGuiKey_C] = 'C';
  io.KeyMap[ImGuiKey_V] = 'V';
  io.KeyMap[ImGuiKey_X] = 'X';
  io.KeyMap[ImGuiKey_Y] = 'Y';
  io.KeyMap[ImGuiKey_Z] = 'Z';
  return true;
}
void Close() {
  ShutdownGpu();
  Save();
  if (window) {
    DestroyWindow(window);
    window = nullptr;
  }
  ClipCursor(nullptr);
  if (xinput) {
    FreeLibrary(xinput);
    xinput = nullptr;
  }
  readPad = nullptr;
  if (context) {
    EndUi();
    ImGui::Shutdown();
    ImGui::DestroyContext(context);
    context = nullptr;
  }
}
void Pump() {
  input.deltaX = input.deltaY = input.wheel = 0;
  MSG message;
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  input.pad = false;
  if (input.focused && options.gamepad && readPad)
    for (DWORD i = 0; i < 4; ++i) {
      XINPUT_STATE pad{};
      if (readPad(i, &pad) != ERROR_SUCCESS)
        continue;
      input.pad = true;
      input.padButtons = pad.Gamepad.wButtons;
      input.lx =
          Deadzone(pad.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      input.ly =
          Deadzone(pad.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      input.rx =
          Deadzone(pad.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
      input.ry =
          Deadzone(pad.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
      input.lt = pad.Gamepad.bLeftTrigger / 255.f;
      input.rt = pad.Gamepad.bRightTrigger / 255.f;
      break;
    }
  Cursor();
}
void BeginUi() {
  auto &io = ImGui::GetIO();
  unsigned w, h;
  ClientSize(w, h);
  io.DisplaySize = ImVec2(float(w), float(h));
  static auto previous = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  io.DeltaTime = std::clamp(
      std::chrono::duration<float>(now - previous).count(), .001f, .1f);
  previous = now;
  io.MousePos = ImVec2(input.mouseX, input.mouseY);
  io.MouseWheel = input.wheel;
  for (int i = 0; i < 5; ++i)
    io.MouseDown[i] = input.focused && input.buttons[i];
  io.KeyCtrl = input.keys[VK_CONTROL];
  io.KeyShift = input.keys[VK_SHIFT];
  io.KeyAlt = input.keys[VK_MENU];
  ImGui::NewFrame();
  uiActive = true;
  if (options.panel) {
    ImGui::SetNextWindowSize(ImVec2(390, 410), ImGuiCond_FirstUseEver);
    ImGui::Begin("Desktop preview (F1)", &options.panel);
    int eye = options.eye;
    if (ImGui::Combo("Eye / F2", &eye, "Left\0Right\0")) {
      options.eye = eye;
      Save();
    }
    int crop = int(options.framing);
    if (ImGui::Combo("Framing / F3", &crop, "Square\0Cropped to window\0")) {
      options.framing = Framing(crop);
      Save();
    }
    int mode = int(options.mode);
    if (ImGui::Combo(
            "Window mode", &mode,
            "Windowed / F4\0Borderless window / F5\0Fullscreen desktop / F6\0"))
      SetMode(WindowMode(mode));
    if (ImGui::Button("Reset window / F7"))
      ResetWindow();
    if (ImGui::Checkbox("Capture mouse / F8", &options.capture)) {
      if (options.capture)
        options.panel = false;
      Cursor();
    }
    if (ImGui::Checkbox("Keyboard + mouse / F9", &options.keyboard))
      Save();
    if (ImGui::Checkbox("Gamepad / F10", &options.gamepad))
      Save();
    ImGui::Checkbox("Preview statistics / F11", &options.statistics);
    ImGui::TextUnformatted("F12: reset preview options");
    ImGui::TextUnformatted("Always downscale to fit; never upscale.");
    ImGui::TextUnformatted("Input requires preview focus. Alt+F4: quit.");
    ImGui::Text("Gamepad: %s", input.pad ? "connected" : "not active");
    ImGui::End();
  }
  if (options.statistics) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::Begin("Preview statistics", nullptr,
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%s eye / %.1f FPS", options.eye ? "Right" : "Left",
                io.Framerate);
    ImGui::End();
  }
  input.wantMouse = io.WantCaptureMouse;
  input.wantKeyboard = io.WantCaptureKeyboard;
}
void EndUi() {
  if (uiActive) {
    ImGui::Render();
    uiActive = false;
  }
}
} // namespace Desktop
