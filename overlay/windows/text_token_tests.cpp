#include "QuestTextTokens.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

using Text=std::u16string;
static bool lookup(const char *action,char16_t *out,std::size_t capacity)
{
    Text keys;
    if(!std::strcmp(action,"CAMERA_CHANGE_VIEW_ALL_SITUATIONS")) keys=u"VIEW";
    else if(!std::strcmp(action,"VEHICLE_FIREWEAPON")) keys=u"~X~";
    else return false;
    assert(keys.size()<capacity);
    std::copy(keys.begin(),keys.end(),out);
    out[keys.size()]=0;
    return true;
}
static Text expand(const Text &input,bool japanese=false)
{
    struct Buffer { char16_t text[256]; uint64_t canary; } buffer{};
    buffer.canary=0x123456789abcdef0ULL;
    std::copy_n(input.begin(),std::min(input.size(),std::size_t(255)),buffer.text);
    questtext::ExpandControls<256>(buffer.text,lookup,japanese);
    assert(buffer.canary==0x123456789abcdef0ULL);
    assert(buffer.text[255]==0);
    return buffer.text;
}
int main()
{
    assert(expand(u"~w~View: ~k~~CYCLE_CAMERA_MODE~.")==u"~w~View: VIEW.");
    assert(expand(u"~k~~CAMERA_CHANGE_VIEW_ALL_SITUATIONS~")==u"VIEW");
    assert(expand(u"~k~~CAR_GUN~ ~k~~VEHICLE_FIREWEAPON~")==u"~X~ ~X~");
    assert(expand(u"~k~~UNKNOWN_ACTION~ tail")==u"UNKNOWN ACTION tail");
    assert(expand(u"~k~~UNKNOWN_ACTION")==u"UNKNOWN ACTION");
    assert(expand(u"end~")==u"end");
    assert(expand(u"~h~end~bad")==u"~h~endbad");
    assert(expand(u"plain text")==u"plain text");
    Text japanese=u"~k~~CYCLE_CAMERA_MODE~";
    for(auto &c:japanese)c|=0x8000;
    assert(expand(japanese,true)==u"VIEW");
    for(std::size_t size=0;size<256;++size) {
        // Every possible truncation of a late control token must terminate and
        // produce paired formatting delimiters, without touching the canary.
        Text source(size,u'a'); source+=u"~h~~k~~CYCLE_CAMERA_MODE~~w~";
        auto result=expand(source);
        assert(std::count(result.begin(),result.end(),u'~')%2==0);
    }
    char16_t unfinished[]={u'b',u'a',u'd',0,u'~',0};
    assert(questtext::TokenEnd(unfinished)==unfinished+3);
    char16_t closed[]={u'x',u'~',0};
    assert(questtext::TokenEnd(closed)==closed+1);
    char16_t jp[]={u'x',char16_t(0x807e),0};
    assert(questtext::TokenEnd(jp,true)==jp+1);
    questtext::ExpandControls<256>(static_cast<char16_t*>(nullptr),lookup);
    std::puts("Text token alias, unknown action, truncation, Japanese and bounded scan checks passed.");
}
