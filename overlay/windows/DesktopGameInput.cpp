#include "common.h" // Engine typedefs must precede ControllerConfig/Frontend.
#include "ControllerConfig.h"
#include "DesktopMirror.h"
#include "Frontend.h"
#include "Pad.h"
#include "RuntimeCompat.h"
#include "android.h"
#include "crossplatform.h"
#include "main.h"
#include "skeleton.h"
#include <algorithm>
#include <cmath>
#include <xinput.h>
namespace Desktop {
void InitializeGameInput() {
  CMouseControllerState mouse;
  mouse.LMB = mouse.RMB = mouse.MMB = mouse.MXB1 = mouse.MXB2 = mouse.WHEELUP =
      mouse.WHEELDN = true;
  ControlsManager.InitDefaultControlConfigMouse(mouse);
}
void UpdateGameInput() {
  const auto &in = Input();
  const auto &settings = Settings();
  auto &keys = CPad::TempKeyState;
  keys.Clear();
  const bool keyboard = in.focused && settings.keyboard && !in.wantKeyboard;
  if (keyboard) {
    for (int i = '0'; i <= '9'; ++i)
      keys.VK_KEYS[i] = in.keys[i] ? 255 : 0;
    for (int i = 'A'; i <= 'Z'; ++i)
      keys.VK_KEYS[i] = in.keys[i] ? 255 : 0;
    keys.VK_KEYS[' '] = in.keys[VK_SPACE] * 255;
    for (unsigned i = VK_OEM_1; i <= VK_OEM_102; ++i)
      if (in.keys[i]) {
        auto character = MapVirtualKeyW(i, MAPVK_VK_TO_CHAR) & 0xffff;
        if (character < 256)
          keys.VK_KEYS[character] = 255;
      }
    keys.ESC = in.keys[VK_ESCAPE] * 255;
    keys.ENTER = in.keys[VK_RETURN] * 255;
    keys.TAB = in.keys[VK_TAB] * 255;
    keys.BACKSP = in.keys[VK_BACK] * 255;
    keys.UP = in.keys[VK_UP] * 255;
    keys.DOWN = in.keys[VK_DOWN] * 255;
    keys.LEFT = in.keys[VK_LEFT] * 255;
    keys.RIGHT = in.keys[VK_RIGHT] * 255;
    keys.INS = in.keys[VK_INSERT] * 255;
    keys.DEL = in.keys[VK_DELETE] * 255;
    keys.HOME = in.keys[VK_HOME] * 255;
    keys.END = in.keys[VK_END] * 255;
    keys.PGUP = in.keys[VK_PRIOR] * 255;
    keys.PGDN = in.keys[VK_NEXT] * 255;
    keys.LSHIFT = keys.RSHIFT = keys.SHIFT = in.keys[VK_SHIFT] * 255;
    keys.LCTRL = keys.RCTRL = in.keys[VK_CONTROL] * 255;
    keys.LALT = keys.RALT = in.keys[VK_MENU] * 255;
    keys.NUM0 = in.keys[VK_NUMPAD0] * 255;
    keys.NUM1 = in.keys[VK_NUMPAD1] * 255;
    keys.NUM2 = in.keys[VK_NUMPAD2] * 255;
    keys.NUM3 = in.keys[VK_NUMPAD3] * 255;
    keys.NUM4 = in.keys[VK_NUMPAD4] * 255;
    keys.NUM5 = in.keys[VK_NUMPAD5] * 255;
    keys.NUM6 = in.keys[VK_NUMPAD6] * 255;
    keys.NUM7 = in.keys[VK_NUMPAD7] * 255;
    keys.NUM8 = in.keys[VK_NUMPAD8] * 255;
    keys.NUM9 = in.keys[VK_NUMPAD9] * 255;
    keys.DIV = in.keys[VK_DIVIDE] * 255;
    keys.MUL = in.keys[VK_MULTIPLY] * 255;
    keys.SUB = in.keys[VK_SUBTRACT] * 255;
    keys.ADD = in.keys[VK_ADD] * 255;
    keys.DECIMAL = in.keys[VK_DECIMAL] * 255;
  }
  CPad::OldMouseControllerState = CPad::NewMouseControllerState;
  CPad::NewMouseControllerState.Clear();
  static bool mouseGate = false;
  bool held = false;
  for (bool button : in.buttons)
    held |= button;
  if (!held)
    mouseGate = false;
  else if (in.wantMouse)
    mouseGate = true;
  if (in.focused && settings.keyboard && !in.wantMouse && !mouseGate) {
    auto &mouse = CPad::NewMouseControllerState;
    const auto picture = CurrentPicture();
    const bool inside =
        picture.width > 0 && picture.height > 0 && in.mouseX >= picture.x &&
        in.mouseX < picture.x + picture.width && in.mouseY >= picture.y &&
        in.mouseY < picture.y + picture.height;
    if (inside || settings.capture) {
      mouse.LMB = in.buttons[0];
      mouse.RMB = in.buttons[1];
      mouse.MMB = in.buttons[2];
      mouse.MXB1 = in.buttons[3];
      mouse.MXB2 = in.buttons[4];
      mouse.WHEELUP = in.wheel > 0;
      mouse.WHEELDN = in.wheel < 0;
      mouse.x = in.deltaX;
      mouse.y = in.deltaY;
      if (inside && !settings.capture) {
        FrontEndMenuManager.m_nMouseTempPosX = int(
            (picture.u + (in.mouseX - picture.x) / picture.width * picture.du) *
            RsGlobal.width);
        FrontEndMenuManager.m_nMouseTempPosY =
            int((picture.v +
                 (in.mouseY - picture.y) / picture.height * picture.dv) *
                RsGlobal.height);
      }
    }
  }
}
void MergeVrInput(void *value) {
  auto &pad = *static_cast<androidgame::PadInput *>(value);
  const auto &in = Input();
  if (!in.focused || Settings().panel || !androidgame::VrMenuConsumesInput())
    return;
  if (Settings().keyboard && !in.wantKeyboard) {
    if (in.keys[VK_UP])
      pad.leftStickY = 1;
    if (in.keys[VK_DOWN])
      pad.leftStickY = -1;
    if (in.keys[VK_LEFT])
      pad.leftStickX = -1;
    if (in.keys[VK_RIGHT])
      pad.leftStickX = 1;
    pad.a |= in.keys[VK_RETURN];
    pad.b |= in.keys[VK_ESCAPE] || in.keys[VK_BACK];
  }
  if (in.pad && Settings().gamepad) {
    pad.leftStickX = in.lx;
    pad.leftStickY = in.ly;
    if (in.padButtons & XINPUT_GAMEPAD_DPAD_UP)
      pad.leftStickY = 1;
    if (in.padButtons & XINPUT_GAMEPAD_DPAD_DOWN)
      pad.leftStickY = -1;
    if (in.padButtons & XINPUT_GAMEPAD_DPAD_LEFT)
      pad.leftStickX = -1;
    if (in.padButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
      pad.leftStickX = 1;
    pad.a |= (in.padButtons & XINPUT_GAMEPAD_A) != 0;
    pad.b |= (in.padButtons & XINPUT_GAMEPAD_B) != 0;
  }
}
void MergeGamepad(void *value) {
  const auto &in = Input();
  if (!in.focused || !in.pad || !Settings().gamepad || Settings().panel)
    return;
  auto &pad = *static_cast<CControllerState *>(value);
  auto axis = [](int16 &target, float source) {
    int16 value = int16(source * 128);
    if (std::abs(int(value)) > std::abs(int(target)))
      target = value;
  };
  axis(pad.LeftStickX, in.lx);
  axis(pad.LeftStickY, -in.ly);
  axis(pad.RightStickX, in.rx);
  axis(pad.RightStickY, -in.ry);
  auto button = [&](int16 &target, unsigned mask) {
    if (in.padButtons & mask)
      target = 255;
  };
  button(pad.Cross, XINPUT_GAMEPAD_A);
  button(pad.Circle, XINPUT_GAMEPAD_B);
  button(pad.Square, XINPUT_GAMEPAD_X);
  button(pad.Triangle, XINPUT_GAMEPAD_Y);
  button(pad.DPadUp, XINPUT_GAMEPAD_DPAD_UP);
  button(pad.DPadDown, XINPUT_GAMEPAD_DPAD_DOWN);
  button(pad.DPadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
  button(pad.DPadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
  button(pad.Start, XINPUT_GAMEPAD_START);
  button(pad.Select, XINPUT_GAMEPAD_BACK);
  button(pad.LeftShock, XINPUT_GAMEPAD_LEFT_THUMB);
  button(pad.RightShock, XINPUT_GAMEPAD_RIGHT_THUMB);
  button(pad.LeftShoulder1, XINPUT_GAMEPAD_LEFT_SHOULDER);
  button(pad.RightShoulder1, XINPUT_GAMEPAD_RIGHT_SHOULDER);
  pad.LeftShoulder2 = std::max(pad.LeftShoulder2, int16(in.lt * 255));
  pad.RightShoulder2 = std::max(pad.RightShoulder2, int16(in.rt * 255));
}
} // namespace Desktop
