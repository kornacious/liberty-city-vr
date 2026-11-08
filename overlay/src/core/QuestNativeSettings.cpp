#include "common.h"
#if defined(RW_VULKAN) && defined(__ANDROID__)
#include "QuestNativeSettings.h"
#include "Frontend.h"
#include "Font.h"
#include "Sprite2d.h"
#include "Text.h"
#include <stdio.h>

namespace androidgame {
static float X(float value) { return value*SCREEN_WIDTH/640.0f; }
static float Y(float value) { return value*SCREEN_HEIGHT/448.0f; }

static void NativeText(const std::string &text, float x, float y, float width,
                       float scale, const CRGBA &colour, bool right = false)
{
    wchar value[512];
    AsciiToUnicode(text.substr(0, 250).c_str(), value);
    CFont::SetCentreOff();
    CFont::SetJustifyOff();
    CFont::SetRightJustifyOff();
    CFont::SetBackgroundOff();
    CFont::SetPropOn();
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetScale(X(scale), Y(scale*1.4f));
    const float measured = CFont::GetStringWidth(value, true);
    if(measured > X(width)) CFont::SetScale(X(scale)*X(width)/measured, Y(scale*1.4f));
    if(right) CFont::SetRightJustifyOn();
    CFont::SetColor(colour);
    CFont::PrintString(X(x), Y(y), value);
}

void VrDrawNativeSettings()
{
    QuestNativeSettings::Page page;
    if(!VrCollectNativeSettings(page)) return;
    CSprite2d::InitPerFrame();
    CFont::InitPerFrame();
    CFont::SetAlphaFade(255.0f);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
    FrontEndMenuManager.m_aMenuSprites[MENUSPRITE_PLAYERSET].Draw(
        CRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), CRGBA(255,255,255,255));
    CFont::SetDropShadowPosition(1);
    CFont::SetDropColor(CRGBA(0,0,0,220));
    CFont::SetFontStyle(FONT_HEADING);
    // The original player/settings texture carries its yellow title strip
    // near the bottom. Keep black headings there and the rows above it.
    NativeText(page.heading, 600, 355, 560, 0.95f, CRGBA(0,0,0,255), true);
    CFont::SetFontStyle(FONT_BANK);
    NativeText(page.subtitle, 40, 32, 560, 0.32f, CRGBA(210,190,145,255));
    const int first = page.First();
    for(int i=first;i<(int)page.rows.size() && i<first+QuestNativeSettings::Page::RowsPerPage;++i) {
        const auto &row = page.rows[i];
        const float y = 64.0f+(i-first)*26.0f;
        if(row.selected) CSprite2d::DrawRect(CRect(X(30),Y(y-4),X(610),Y(y+23)),CRGBA(100,200,50,50));
        const CRGBA colour = !row.available ? CRGBA(160,146,114,255) :
            row.selected ? CRGBA(255,217,106,255) : CRGBA(235,170,50,255);
        NativeText(row.label, 42, y, row.value.empty()?550.0f:310.0f, 0.55f, colour);
        if(!row.value.empty()) NativeText(row.value, 598, y, 230, 0.48f, colour, true);
    }
    // About uses shared explanatory text rather than editable rows.
    if(page.rows.empty()) for(size_t i=0;i<page.notes.size() && i<15;++i)
        NativeText(page.notes[i],40,64.0f+(float)i*18.0f,560,0.33f,CRGBA(235,170,50,255));
    if(page.rows.size()>QuestNativeSettings::Page::RowsPerPage) {
        char number[64];
        snprintf(number,sizeof(number),"%d / %d",page.Selected()+1,(int)page.rows.size());
        NativeText(number,598,333,100,0.34f,CRGBA(210,190,145,255),true);
    }
    NativeText("STICK SELECT / ADJUST   L2 / R2 ADJUST   A SELECT   B BACK",40,420,560,0.32f,CRGBA(210,190,145,255));
    CFont::DrawFonts();
    CFont::SetRightJustifyOff();
}
}
#endif
