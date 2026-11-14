#pragma once
namespace Desktop {
// Printable keys use uppercase ASCII; special keys are independent of host APIs.
enum Key : unsigned {
  Key_Space = 32, Key_Tab = 256, Key_Left, Key_Right, Key_Up, Key_Down,
  Key_Enter, Key_Escape, Key_Back, Key_Home, Key_End, Key_Delete, Key_Insert,
  Key_PageUp, Key_PageDown, Key_Shift, Key_Control, Key_Alt,
  Key_Num0, Key_Num1, Key_Num2, Key_Num3, Key_Num4, Key_Num5, Key_Num6,
  Key_Num7, Key_Num8, Key_Num9, Key_Divide, Key_Multiply, Key_Subtract,
  Key_Add, Key_Decimal, Key_F1, Key_F2, Key_F3, Key_F4, Key_F5, Key_F6,
  Key_F7, Key_F8, Key_F9, Key_F10, Key_F11, Key_F12
};
enum PadButton : unsigned short {
  PadUp=1, PadDown=2, PadLeft=4, PadRight=8, PadStart=16, PadBack=32,
  PadLeftThumb=64, PadRightThumb=128, PadLeftShoulder=256,
  PadRightShoulder=512, PadA=4096, PadB=8192, PadX=16384, PadY=32768
};
}
