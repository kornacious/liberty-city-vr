#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
using wchar = char;
#define SCREEN_SCALE_X(x) (x)
#define SCREEN_SCALE_Y(x) (x)
#define SCREEN_SCALE_X_FIX(x) (x)
#define SCREEN_SCALE_Y_FIX(x) (x)
#define SCREEN_SCALE_X_PC(x) (x)
#define SCREEN_SCALE_Y_PC(x) (x)
#define SCREEN_STRETCH_X(x) (x)
#define SCREEN_SCALE_FROM_RIGHT(x) (640.0f-(x))
#define FONT_LOCALE(x) (x)
#define FIX_BUGS
constexpr int FONT_HEADING=1, COUNTER_DISPLAY_NUMBER=0;
constexpr float TIMER_RIGHT_OFFSET=34.0f, DEFAULT_SCREEN_WIDTH=640.0f;
struct CRGBA { CRGBA(int,int,int,int){} };
#define COUNTER_COLOR CRGBA(0,106,164,255)
template<class T> T Min(T a,T b){return std::min(a,b);}
void AsciiToUnicode(const char* a,char* b){std::snprintf(b,16,"%s",a);}
struct Entry { bool m_bCounterProcessed=true; int m_nType=1; char m_bCounterBuffer[16]="0"; char m_aCounterText[16]="damage:"; };
struct Display { Entry m_sEntries[1]; };
struct CUserDisplay { static inline Display OnscnTimer; };
struct Text { char* Get(char* s){return s;} } TheText;
struct CFont {
    static inline bool right,centre,justify,background,onlyText,prop;
    static inline int style,prints;
    static inline float sx,sy,wrap;
    static void SetJustifyOff(){justify=false;}
    static void SetCentreOff(){centre=false;}
    static void SetBackgroundOff(){background=false;}
    static void SetBackGroundOnlyTextOff(){onlyText=false;}
    static void SetBackGroundOnlyTextOn(){onlyText=true;}
    static void SetRightJustifyOn(){right=true;}
    static void SetRightJustifyWrap(float v){wrap=v;}
    static void SetFontStyle(int v){style=v;}
    static void SetPropOff(){prop=false;}
    static void SetPropOn(){prop=true;}
    static void SetScale(float x,float y){sx=x;sy=y;}
    static void SetWrapx(float){}
    static void SetColor(CRGBA){}
    static void PrintString(float x,float y,char* text){
        assert(right && !centre && !justify && !background);
        assert(style==FONT_HEADING && wrap==0.0f);
        assert(sx==0.8f && sy==1.35f);
        if(text==TheText.Get(CUserDisplay::OnscnTimer.m_sEntries[0].m_aCounterText)){
            // Right edge of label stays left of the bar's fixed left edge,
            // including its two-pixel shadow, at every fill level.
            assert(x<=640.0f-TIMER_RIGHT_OFFSET-59.0f);
            assert(y==132.0f || y==134.0f);
            assert(prop);
        }
        ++prints;
    }
};
struct CRect { float left,top,right,bottom; CRect(float l,float t,float r,float b):left(l),top(t),right(r),bottom(b){} };
struct CSprite2d {
    static inline int rects;
    static void DrawRect(CRect r,CRGBA){
        assert(r.left==560.0f && r.right>=r.left && r.right<=610.0f);
        assert(r.top==140.0f && r.bottom==151.0f);
        ++rects;
    }
};
void Draw(int FRAMECOUNTER,bool& CounterOnLastFrame,int& CounterFlashTimer){
    wchar sTimer[16];
#include "mission-counter-production.inc"
}
int main(){
    for(int type=0;type<2;++type) for(int value=0;value<=100;++value)
    for(int inherited=0;inherited<8;++inherited){
        auto& e=CUserDisplay::OnscnTimer.m_sEntries[0];
        e.m_nType=type;
        std::snprintf(e.m_bCounterBuffer,16,"%d",value);
        CFont::right=false; CFont::centre=(inherited&1)!=0;
        CFont::justify=(inherited&2)!=0; CFont::background=(inherited&4)!=0;
        CFont::style=99; CFont::wrap=500; CFont::prints=0; CSprite2d::rects=0;
        bool last=true; int flash=0;
        Draw(4,last,flash);
        assert(CFont::prints==(type==COUNTER_DISPLAY_NUMBER?4:2));
        assert(CSprite2d::rects==(type==COUNTER_DISPLAY_NUMBER?0:2));
        // Also exercise appearance flashing, which must not disturb layout.
        last=false; flash=0; CFont::prints=0; CSprite2d::rects=0;
        Draw(0,last,flash);
        assert(CFont::prints==0 && CSprite2d::rects==0);
        Draw(4,last,flash);
    }
    std::puts("Mission counter layout: numeric/bar, 0-100%, inherited font states and flashing passed");
}
