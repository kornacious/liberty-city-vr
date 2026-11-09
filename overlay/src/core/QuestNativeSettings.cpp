#include "common.h"
#if defined(RW_VULKAN) && (defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS))
#include "QuestNativeSettings.h"
#include "Frontend.h"
#include "Font.h"
#include "Sprite2d.h"
#include "Text.h"
#include <stdio.h>

namespace androidgame {
static float X(float value) { return value*SCREEN_WIDTH/640.0f; }
static float Y(float value) { return value*SCREEN_HEIGHT/448.0f; }

static void NativeText(const std::string &text, float x, float y,
                       float scaleX, float scaleY, const CRGBA &colour,
                       bool right = false, bool centered = false)
{
    wchar value[512];
    AsciiToUnicode(text.substr(0, 250).c_str(), value);
    CFont::SetCentreOff();
    CFont::SetJustifyOff();
    CFont::SetRightJustifyOff();
    CFont::SetBackgroundOff();
    CFont::SetPropOn();
    CFont::SetWrapx(SCREEN_WIDTH);
    // Use the original frontend scales uniformly, never shrink individual rows.
    CFont::SetScale(X(scaleX), Y(scaleY));
    if(centered) x -= CFont::GetStringWidth(value, true)*320.0f/SCREEN_WIDTH;
    if(right) CFont::SetRightJustifyOn();
    CFont::SetColor(colour);
    CFont::PrintString(X(x), Y(y), value);
}

void VrDrawNativeSettings()
{
    QuestNativeSettings::Page page;
    if(!VrCollectNativeSettings(page)) return;
    // The shared frontend page is opaque. Legacy widgets read this alpha
    // through FadeIn(), whose old menu animation is bypassed by this presenter.
    FrontEndMenuManager.m_nMenuFadeAlpha = 255;
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
    NativeText(page.heading, 600, 355, MENUHEADER_WIDTH, MENUHEADER_HEIGHT, CRGBA(0,0,0,255), true);
    CFont::SetFontStyle(FONT_BANK);
    NativeText(page.subtitle, 110, 32, SMALLESTTEXT_X_SCALE, SMALLESTTEXT_Y_SCALE, CRGBA(210,190,145,255));
    CFont::SetFontStyle(FONT_LOCALE(FONT_HEADING));
    const int first = page.First();
    for(int i=first;i<(int)page.rows.size() && i<first+QuestNativeSettings::Page::RowsPerPage;++i) {
        const auto &row = page.rows[i];
        const float y = 64.0f+(i-first)*(page.categories ? 26.0f : 20.0f)+
            (page.radioStation >= 0 && first == 0 && i >= 3 ? 65.0f : 0.0f);
        if(row.selected) CSprite2d::DrawRect(CRect(X(10),Y(y-(page.categories ? 4 : 2)),X(630),Y(y+(page.categories ? 23 : 18))),CRGBA(100,200,50,50));
        const CRGBA colour = !row.available ? CRGBA(160,146,114,255) :
            row.selected ? CRGBA(255,217,106,255) : CRGBA(235,170,50,255);
        if(page.categories) {
            NativeText(row.label, 320, y, BIGTEXT_X_SCALE, BIGTEXT_Y_SCALE, colour, false, true);
        } else {
            NativeText(row.label, 110, y, MEDIUMTEXT_X_SCALE, MEDIUMTEXT_Y_SCALE, colour);
            if(row.sliderProgress >= 0.0f) {
                const float progress = row.sliderProgress > 1.0f ? 1.0f : row.sliderProgress;
                FrontEndMenuManager.DisplaySlider(X(330),Y(y),Y(2),Y(16),X(190),progress);
            } else if(!row.value.empty()) {
                NativeText(row.value, 530, y, MEDIUMTEXT_X_SCALE, MEDIUMTEXT_Y_SCALE, colour, true);
            }
        }
    }
    if(page.radioStation >= 0 && first == 0) {
        // Preserve the original station-to-sprite mapping: texture order does
        // not match radio tuning order. Untuned stations use the original red.
        static const int sprites[] = { FE_RADIO1, FE_RADIO2, FE_RADIO5, FE_RADIO7,
            FE_RADIO8, FE_RADIO3, FE_RADIO4, FE_RADIO6, FE_RADIO9 };
        const int count = page.customRadioAvailable ? 10 : 9;
        const float size = page.customRadioAvailable ? 36.0f : 40.0f;
        const float start = (640.0f-(count*size+(count-1)*5.0f))*0.5f;
        for(int station=0;station<count;++station) {
            const float x = start+station*(size+5.0f);
            const CRGBA tint = station == page.radioStation ? CRGBA(255,255,255,255) : CRGBA(225,0,0,170);
            auto &sprite = station == 9 ? FrontEndMenuManager.m_aMenuSprites[MENUSPRITE_MP3LOGO] :
                FrontEndMenuManager.m_aFrontEndSprites[sprites[station]];
            sprite.Draw(CRect(X(x),Y(130),X(x+size),Y(130+size)),tint);
        }
    }
    CFont::SetFontStyle(FONT_BANK);
    // Non-editable explanatory text uses the original helper font.
    if(page.rows.empty()) for(size_t i=0;i<page.notes.size() && i<15;++i)
        NativeText(page.notes[i],40,64.0f+(float)i*18.0f,SMALLESTTEXT_X_SCALE,SMALLESTTEXT_Y_SCALE,CRGBA(235,170,50,255));
    if(page.showStatusNotes && !page.rows.empty()) for(size_t i=0;i<page.notes.size() && i<3;++i)
        NativeText(page.notes[i],40,280.0f+(float)i*18.0f,SMALLESTTEXT_X_SCALE,SMALLESTTEXT_Y_SCALE,CRGBA(235,170,50,255));
    if(!page.categories && page.rows.size()>QuestNativeSettings::Page::RowsPerPage) {
        char number[64];
        snprintf(number,sizeof(number),"%d / %d",page.Selected()+1,(int)page.rows.size());
        NativeText(number,598,333,SMALLESTTEXT_X_SCALE,SMALLESTTEXT_Y_SCALE,CRGBA(210,190,145,255),true);
    }
    NativeText("STICK SELECT / ADJUST   L2 / R2 ADJUST   A SELECT   B BACK",(page.categories ? 40.0f : 110.0f),420,SMALLESTTEXT_X_SCALE,SMALLESTTEXT_Y_SCALE,CRGBA(210,190,145,255));
    CFont::DrawFonts();
    CFont::SetRightJustifyOff();
}
}
#endif
