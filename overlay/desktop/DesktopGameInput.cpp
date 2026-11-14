#include "common.h" // Engine typedefs must precede ControllerConfig/Frontend.
#include "ControllerConfig.h"
#include "DesktopMirror.h"
#include "Frontend.h"
#include "Pad.h"

#include "android.h"
#include "crossplatform.h"
#include "main.h"
#include "skeleton.h"
#include <algorithm>
#include <cmath>

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
    keys.VK_KEYS[' '] = in.keys[Key_Space] * 255;
    for (unsigned i=33;i<127;++i) if(in.keys[i]) keys.VK_KEYS[i]=255;
    keys.ESC = in.keys[Key_Escape] * 255;
    keys.ENTER = in.keys[Key_Enter] * 255;
    keys.TAB = in.keys[Key_Tab] * 255;
    keys.BACKSP = in.keys[Key_Back] * 255;
    keys.UP = in.keys[Key_Up] * 255;
    keys.DOWN = in.keys[Key_Down] * 255;
    keys.LEFT = in.keys[Key_Left] * 255;
    keys.RIGHT = in.keys[Key_Right] * 255;
    keys.INS = in.keys[Key_Insert] * 255;
    keys.DEL = in.keys[Key_Delete] * 255;
    keys.HOME = in.keys[Key_Home] * 255;
    keys.END = in.keys[Key_End] * 255;
    keys.PGUP = in.keys[Key_PageUp] * 255;
    keys.PGDN = in.keys[Key_PageDown] * 255;
    keys.LSHIFT = keys.RSHIFT = keys.SHIFT = in.keys[Key_Shift] * 255;
    keys.LCTRL = keys.RCTRL = in.keys[Key_Control] * 255;
    keys.LALT = keys.RALT = in.keys[Key_Alt] * 255;
    keys.NUM0 = in.keys[Key_Num0] * 255;
    keys.NUM1 = in.keys[Key_Num1] * 255;
    keys.NUM2 = in.keys[Key_Num2] * 255;
    keys.NUM3 = in.keys[Key_Num3] * 255;
    keys.NUM4 = in.keys[Key_Num4] * 255;
    keys.NUM5 = in.keys[Key_Num5] * 255;
    keys.NUM6 = in.keys[Key_Num6] * 255;
    keys.NUM7 = in.keys[Key_Num7] * 255;
    keys.NUM8 = in.keys[Key_Num8] * 255;
    keys.NUM9 = in.keys[Key_Num9] * 255;
    keys.DIV = in.keys[Key_Divide] * 255;
    keys.MUL = in.keys[Key_Multiply] * 255;
    keys.SUB = in.keys[Key_Subtract] * 255;
    keys.ADD = in.keys[Key_Add] * 255;
    keys.DECIMAL = in.keys[Key_Decimal] * 255;
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
    if (in.keys[Key_Up])
      pad.leftStickY = 1;
    if (in.keys[Key_Down])
      pad.leftStickY = -1;
    if (in.keys[Key_Left])
      pad.leftStickX = -1;
    if (in.keys[Key_Right])
      pad.leftStickX = 1;
    pad.a |= in.keys[Key_Enter];
    pad.b |= in.keys[Key_Escape] || in.keys[Key_Back];
  }
  if (in.pad && Settings().gamepad) {
    pad.leftStickX = in.lx;
    pad.leftStickY = in.ly;
    if (in.padButtons & PadUp)
      pad.leftStickY = 1;
    if (in.padButtons & PadDown)
      pad.leftStickY = -1;
    if (in.padButtons & PadLeft)
      pad.leftStickX = -1;
    if (in.padButtons & PadRight)
      pad.leftStickX = 1;
    pad.a |= (in.padButtons & PadA) != 0;
    pad.b |= (in.padButtons & PadB) != 0;
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
  button(pad.Cross, PadA);
  button(pad.Circle, PadB);
  button(pad.Square, PadX);
  button(pad.Triangle, PadY);
  button(pad.DPadUp, PadUp);
  button(pad.DPadDown, PadDown);
  button(pad.DPadLeft, PadLeft);
  button(pad.DPadRight, PadRight);
  button(pad.Start, PadStart);
  button(pad.Select, PadBack);
  button(pad.LeftShock, PadLeftThumb);
  button(pad.RightShock, PadRightThumb);
  button(pad.LeftShoulder1, PadLeftShoulder);
  button(pad.RightShoulder1, PadRightShoulder);
  pad.LeftShoulder2 = std::max(pad.LeftShoulder2, int16(in.lt * 255));
  pad.RightShoulder2 = std::max(pad.RightShoulder2, int16(in.rt * 255));
}
} // namespace Desktop
