#include "DesktopMirror.h"
#include "DesktopAudio.h"
#include "MatchedProfile.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cctype>
namespace Desktop {
namespace {
SDL_Window *window=nullptr;
SDL_Gamepad *gamepad=nullptr;
Options options;
InputState input;
bool quit=false,resize=true,uiActive=false,renderingDetails=false;
ImGuiContext *context=nullptr;
void (*toolUi)()=nullptr;
std::string toolLayout;
const char *(*renderingDetailsProvider)(bool)=nullptr;
const unsigned char *(*occlusionMapProvider)(int,int*)=nullptr;
std::string config;
void Cursor() {
  if(window) SDL_SetWindowRelativeMouseMode(window,input.focused && options.capture && (toolUi || !options.panel));
}
void Save() {
  if(config.empty())return;
  std::ofstream out(std::filesystem::u8path(config+".partial"),std::ios::trunc);
  out<<"1 "<<options.eye<<' '<<int(options.framing)<<' '<<int(options.mode)<<' '
     <<options.keyboard<<' '<<options.gamepad<<'\n';
  out.close();if(out)SDL_RenamePath((config+".partial").c_str(),config.c_str());
}
void Shortcut(unsigned key) {
  if(toolUi){
    if(key==Key_F1){options.panel=!options.panel;return;}
    if(key==Key_F2){options.statistics=!options.statistics;return;}
    if(key==Key_F8)return;
  }
  switch(key) {
  case Key_F1:options.panel=!options.panel;if(options.panel)options.capture=false;break;
  case Key_F2:options.eye^=1;break;
  case Key_F3:options.framing=options.framing==Framing::Square?Framing::Cropped:Framing::Square;break;
  case Key_F4:SetMode(WindowMode::Windowed);break;
  case Key_F5:SetMode(WindowMode::Borderless);break;
  case Key_F6:SetMode(WindowMode::Fullscreen);break;
  case Key_F7:ResetWindow();break;
  case Key_F8:options.capture=!options.capture;if(options.capture)options.panel=false;break;
  case Key_F9:options.keyboard=!options.keyboard;break;
  case Key_F10:options.gamepad=!options.gamepad;break;
  case Key_F11:options.statistics=!options.statistics;break;
  case Key_F12:renderingDetails=!renderingDetails;break;
  }
  Cursor();Save();
}
unsigned KeyIndex(SDL_Keycode key) {
  if(key>=32 && key<127)return unsigned(std::toupper(int(key)));
  if(key>=SDLK_F1 && key<=SDLK_F12)return Key_F1+unsigned(key-SDLK_F1);
  if(key>=SDLK_KP_1 && key<=SDLK_KP_9)return Key_Num1+unsigned(key-SDLK_KP_1);
  switch(key) {
  case SDLK_TAB:return Key_Tab;case SDLK_RETURN:return Key_Enter;
  case SDLK_ESCAPE:return Key_Escape;case SDLK_BACKSPACE:return Key_Back;
  case SDLK_LEFT:return Key_Left;case SDLK_RIGHT:return Key_Right;
  case SDLK_UP:return Key_Up;case SDLK_DOWN:return Key_Down;
  case SDLK_HOME:return Key_Home;case SDLK_END:return Key_End;
  case SDLK_DELETE:return Key_Delete;case SDLK_INSERT:return Key_Insert;
  case SDLK_PAGEUP:return Key_PageUp;case SDLK_PAGEDOWN:return Key_PageDown;
  case SDLK_LSHIFT:case SDLK_RSHIFT:return Key_Shift;
  case SDLK_LCTRL:case SDLK_RCTRL:return Key_Control;
  case SDLK_LALT:case SDLK_RALT:return Key_Alt;
  case SDLK_KP_0:return Key_Num0;case SDLK_KP_DIVIDE:return Key_Divide;
  case SDLK_KP_MULTIPLY:return Key_Multiply;case SDLK_KP_MINUS:return Key_Subtract;
  case SDLK_KP_PLUS:return Key_Add;case SDLK_KP_PERIOD:return Key_Decimal;
  case SDLK_KP_ENTER:return Key_Enter;default:return 0;
  }
}
void OpenGamepad() {
  if(gamepad && SDL_GamepadConnected(gamepad))return;
  if(gamepad)SDL_CloseGamepad(gamepad);
  gamepad=nullptr;int count=0;auto ids=SDL_GetGamepads(&count);
  if(count)gamepad=SDL_OpenGamepad(ids[0]);SDL_free(ids);
}
}
Options &Settings(){return options;}
const InputState &Input(){return input;}
void SetToolUi(void (*draw)()){
  if(toolUi && !draw)SetToolCameraCapture(false);
  toolUi=draw;
  if(context && draw){toolLayout=(std::filesystem::u8path(config).parent_path()/"viewer-layout.ini").u8string();ImGui::GetIO().IniFilename=toolLayout.c_str();}
}
void SetToolCameraCapture(bool captured){
  if(options.capture!=captured){
    options.capture=captured;input.deltaX=input.deltaY=0;
    if(captured && toolUi && context){ImGui::ClearActiveID();ImGui::SetWindowFocus(nullptr);}
    Cursor();
  }
}
void *WindowHandle(){return window;}
const char *const *VulkanExtensions(unsigned &count){return SDL_Vulkan_GetInstanceExtensions(&count);}
bool QuitRequested(){return quit;}
void RequestResize(){resize=true;}
bool ConsumeResize(){bool value=resize;resize=false;return value;}
bool RenderingDetailsVisible(){return renderingDetails;}
void SetRenderingDetailsProvider(const char *(*provider)(bool)){renderingDetailsProvider=provider;}
void SetOcclusionMapProvider(const unsigned char *(*provider)(int,int*)){occlusionMapProvider=provider;}
void ClientSize(unsigned &w,unsigned &h){int x=0,y=0;if(window && !(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED))SDL_GetWindowSizeInPixels(window,&x,&y);w=unsigned(x);h=unsigned(y);}
void SetMode(WindowMode mode){if(!window)return;SDL_SetWindowFullscreen(window,mode==WindowMode::Fullscreen);SDL_SetWindowBordered(window,mode==WindowMode::Windowed);options.mode=mode;resize=true;Cursor();Save();}
void ResetWindow(){if(!window)return;SetMode(WindowMode::Windowed);SDL_SetWindowSize(window,960,960);SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);}
void InjectFocus(bool focused){input={};input.focused=focused;if(context){auto &io=ImGui::GetIO();for(auto &v:io.KeysDown)v=false;for(auto &v:io.MouseDown)v=false;}Cursor();}
void InjectKey(unsigned key,bool down){if(key>=512)return;if(key>=Key_F1 && key<=Key_F12){if(down)Shortcut(key);return;}input.keys[key]=down;if(context)ImGui::GetIO().KeysDown[key]=down;}
bool Open(const std::string &root) {
  if(window)return true;
  if(!SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))return false;
  quit=false;resize=true;input={};options={};
  config=root.empty()?"":(std::filesystem::u8path(root)/"desktop-preview.cfg").u8string();
  if(!config.empty()){int version,eye,frame,mode,kb,pad;std::ifstream in(std::filesystem::u8path(config));
    if(in>>version>>eye>>frame>>mode>>kb>>pad && version==1 && eye>=0 && eye<2 && frame>=0 && frame<2 && mode>=0 && mode<3){options.eye=eye;options.framing=Framing(frame);options.mode=WindowMode(mode);options.keyboard=kb!=0;options.gamepad=pad!=0;}}
  window=SDL_CreateWindow("Liberty City VR - desktop preview (F1 controls)",960,960,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if(!window){SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD);return false;}
  context=ImGui::CreateContext();ImGui::SetCurrentContext(context);auto &io=ImGui::GetIO();io.IniFilename=nullptr;
  const int mapping[][2]={{ImGuiKey_Tab,Key_Tab},{ImGuiKey_LeftArrow,Key_Left},{ImGuiKey_RightArrow,Key_Right},{ImGuiKey_UpArrow,Key_Up},{ImGuiKey_DownArrow,Key_Down},{ImGuiKey_Enter,Key_Enter},{ImGuiKey_Escape,Key_Escape},{ImGuiKey_Backspace,Key_Back},{ImGuiKey_Home,Key_Home},{ImGuiKey_End,Key_End},{ImGuiKey_Delete,Key_Delete},{ImGuiKey_A,'A'},{ImGuiKey_C,'C'},{ImGuiKey_V,'V'},{ImGuiKey_X,'X'},{ImGuiKey_Y,'Y'},{ImGuiKey_Z,'Z'}};
  for(auto &m:mapping)io.KeyMap[m[0]]=m[1];
  SDL_StartTextInput(window);SetMode(options.mode);OpenGamepad();input.focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
  return true;
}
void Close(){ShutdownGpu();Save();if(gamepad){SDL_CloseGamepad(gamepad);gamepad=nullptr;}if(context){EndUi();ImGui::Shutdown();ImGui::DestroyContext(context);context=nullptr;}if(window){SDL_DestroyWindow(window);window=nullptr;SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD);}}
void Pump() {
  input.deltaX=input.deltaY=input.wheel=0;
  SDL_Event e;while(SDL_PollEvent(&e)) {
    switch(e.type) {
    case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:quit=true;break;
    case SDL_EVENT_WINDOW_RESIZED:case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:resize=true;break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:InjectFocus(true);break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:InjectFocus(false);break;
    case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:
      if(e.key.down && e.key.key==SDLK_F4 && (e.key.mod&SDL_KMOD_ALT)){quit=true;break;}
      if(!e.key.repeat)InjectKey(KeyIndex(e.key.key),e.key.down);break;
    case SDL_EVENT_TEXT_INPUT:
      if(options.panel){const unsigned char *p=reinterpret_cast<const unsigned char*>(e.text.text);while(*p){unsigned c=*p++;if(c>=0xc0){int n=c<0xe0?1:c<0xf0?2:3;c&=(1u<<(6-n))-1;while(n-- && *p)c=(c<<6)|(*p++&63);}if(c<=0xffff)ImGui::GetIO().AddInputCharacter(ImWchar(c));}}break;
    case SDL_EVENT_MOUSE_MOTION:{int w=1,h=1;SDL_GetWindowSize(window,&w,&h);unsigned pw,ph;ClientSize(pw,ph);input.mouseX=e.motion.x*pw/std::max(1,w);input.mouseY=e.motion.y*ph/std::max(1,h);input.deltaX+=e.motion.xrel;input.deltaY+=e.motion.yrel;break;}
    case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:{int b=e.button.button==SDL_BUTTON_LEFT?0:e.button.button==SDL_BUTTON_RIGHT?1:e.button.button==SDL_BUTTON_MIDDLE?2:e.button.button==SDL_BUTTON_X1?3:4;input.buttons[b]=e.button.down;break;}
    case SDL_EVENT_MOUSE_WHEEL:input.wheel+=e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-e.wheel.y:e.wheel.y;break;
    case SDL_EVENT_GAMEPAD_ADDED:case SDL_EVENT_GAMEPAD_REMOVED:OpenGamepad();break;
    }
  }
  input.pad=false;input.padButtons=0;input.lx=input.ly=input.rx=input.ry=input.lt=input.rt=0;
  if(gamepad && input.focused && options.gamepad){input.pad=true;
    const unsigned short masks[]={PadA,PadB,PadX,PadY,PadBack,0,PadStart,PadLeftThumb,PadRightThumb,PadLeftShoulder,PadRightShoulder,PadUp,PadDown,PadLeft,PadRight};
    for(int i=0;i<15;++i)if(SDL_GetGamepadButton(gamepad,SDL_GamepadButton(i)))input.padButtons|=masks[i];
    input.lx=Deadzone(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFTX),7849);input.ly=-Deadzone(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFTY),7849);
    input.rx=Deadzone(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHTX),8689);input.ry=-Deadzone(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHTY),8689);
    input.lt=std::max(0,int(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)))/32767.f;input.rt=std::max(0,int(SDL_GetGamepadAxis(gamepad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)))/32767.f;
  }
  DesktopAudio::Pump();Cursor();
}
// Shared preview UI follows; it uses the existing ImGui version and GPU renderer.
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
  if(toolUi){for(unsigned i=0;i<512;++i)io.KeysDown[i]=input.focused&&input.keys[i];}
  if(toolUi && options.capture){io.MousePos=ImVec2(-10000,-10000);for(auto &down:io.MouseDown)down=false;for(auto &down:io.KeysDown)down=false;}
  io.KeyCtrl = input.keys[Key_Control];
  io.KeyShift = input.keys[Key_Shift];
  io.KeyAlt = input.keys[Key_Alt];
  ImGui::NewFrame();
  uiActive = true;
  if(toolUi){toolUi();return;}
  if (options.panel) {
    ImGui::SetNextWindowSize(ImVec2(390, 460), ImGuiCond_FirstUseEver);
    ImGui::Begin("Desktop preview (F1)", &options.panel);
    ImGui::Text("Graphics: %s", MatchedProfile::active ? "Quest 3 compatibility" : "Native PCVR");
    if(MatchedProfile::active) ImGui::TextWrapped("Restart via the manager to change profile. Timings measure this PC.");
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
    ImGui::Checkbox("Rendering diagnostics / F12", &renderingDetails);
    if (ImGui::Button("Reset preview options")) {
      options = Options{};
      ResetWindow();
      Cursor();
      Save();
    }
    auto devices=DesktopAudio::Devices();
    std::string items="System default";items.push_back(0);int selected=0;
    for(size_t i=0;i<devices.size();++i){if(devices[i]==DesktopAudio::Preference())selected=int(i+1);items+=devices[i];items.push_back(0);}items.push_back(0);
    if(ImGui::Combo("Audio output",&selected,items.c_str()))DesktopAudio::Select(selected?devices[size_t(selected-1)]:"");
    ImGui::TextWrapped("%s",DesktopAudio::Status());
    ImGui::TextUnformatted("Always downscale to fit; never upscale.");
    ImGui::TextUnformatted("Input requires preview focus. Alt+F4: quit.");
    ImGui::Text("Gamepad: %s", input.pad ? "connected" : "not active");
    ImGui::End();
  }
  if (renderingDetails && options.capture) {
    options.capture = false;
    Cursor();
  }
  const char *details = renderingDetailsProvider ? renderingDetailsProvider(renderingDetails) : nullptr;
  if (renderingDetails) {
    ImGui::SetNextWindowSize(ImVec2(560, 820), ImGuiCond_FirstUseEver);
    ImGui::Begin("Rendering diagnostics (F12)", &renderingDetails);
    ImGui::TextWrapped("%s. Timings measure this PC. CPU counters and completed GPU samples cover different frames.",
                       MatchedProfile::active ? "Quest compatibility workload" : "Native PCVR workload");
    ImGui::TextUnformatted(details ? details : "Game rendering telemetry unavailable in the synthetic diagnostic.");
    for (int eye=0;eye<2;++eye) {
      if (eye) ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::TextUnformatted(eye ? "Right occlusion map" : "Left occlusion map");
      int size=0;
      const auto *map=occlusionMapProvider ? occlusionMapProvider(eye,&size) : nullptr;
      if (map && size>0 && size<=64) {
        const auto origin=ImGui::GetCursorScreenPos();
        auto *draw=ImGui::GetWindowDrawList();
        const float scale=144.f/size;
        for (int y=0;y<size;++y) for (int x=0;x<size;) {
          // Coalesce equal display shades; the exact proof grid is unchanged.
          const unsigned shade=map[y*size+x] ? 1+map[y*size+x]/18 : 0;
          int end=x+1;
          while(end<size && (map[y*size+end] ? 1+map[y*size+end]/18 : 0)==shade) ++end;
          const unsigned depth=shade*17;
          draw->AddRectFilled(ImVec2(origin.x+x*scale,origin.y+y*scale),
                              ImVec2(origin.x+end*scale,origin.y+(y+1)*scale),
                              IM_COL32(depth/4,depth,shade?128+depth/2:0,255));
          x=end;
        }
        ImGui::Dummy(ImVec2(144,144));
      } else ImGui::TextUnformatted("Map unavailable");
      ImGui::EndGroup();
    }
    ImGui::TextUnformatted("Black: no proven depth. Brighter: nearer (nonlinear).");
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
