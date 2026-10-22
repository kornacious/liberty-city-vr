#pragma once
class CControllerState;
namespace QuestRadio {
enum { MenuRows=16, BackRow=15, FirstPlacementRow=3, LastPlacementRow=9 };
bool Update(bool blocked); // true for a short X release, preserving station tap
bool Interacting();
bool TouchEnabled();
void SuppressDriving(CControllerState *state);
void Render();
void MenuRow(int row,char *text,int length);
void MenuAction(int row,int direction);
void CloseEditor();
const char *MenuStatus();
}
