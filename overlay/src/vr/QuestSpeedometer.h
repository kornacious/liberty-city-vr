#pragma once
namespace QuestSpeedometer {
enum { FirstPlacementRow=3, LastPlacementRow=9, BackRow=11, MenuRows=12 };
void Render();
void MenuRow(int row, char *text, int length);
void MenuAction(int row, int direction);
const char *Status();
}
