#pragma once
class CControllerState;
namespace QuestRadio {
bool Update(bool blocked); // true for a short X release, preserving station tap
bool Interacting();
void SuppressDriving(CControllerState *state);
void Render();
void MenuRow(int row,char *text,int length);
void MenuAction(int row,int direction);
void CloseEditor();
const char *MenuStatus();
}
