#include "QuestMusicTimeline.h"
#include "RadioBoardLayout.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main() {
    QuestMusic::Timeline t;
    assert(!t.Add(0)); assert(t.Add(10000)); assert(t.Add(20000)); assert(t.Add(30000));
    uint32_t ms=10000; assert(t.Locate(ms)==1 && ms==0);
    ms=59999; assert(t.Locate(ms)==2 && ms==29999);
    ms=60000; assert(t.Locate(ms)==0 && ms==0);
    assert(t.Step(1,-1,3001)==1); assert(t.Step(1,-1,3000)==0);
    assert(t.Step(0,-1,0)==2); assert(t.Step(2,1,0)==0);
    QuestMusic::Timeline cap; assert(cap.Add(QuestMusic::MaxDuration)); assert(!cap.Add(1));
    QuestMusic::Timeline count; for(size_t i=0;i<1000;++i) assert(count.Add(1)); assert(!count.Add(1));
    using namespace RadioBoard;
    Placement pm,pg,bm,bg; bg.v[0]=1; bm.v[0]=2; pg.v[0]=3; pm.v[0]=4;
    assert(Resolve(pm,pg,bm,bg).v[0]==1);
    bm.owned=true; assert(Resolve(pm,pg,bm,bg).v[0]==2);
    pg.owned=true; assert(Resolve(pm,pg,bm,bg).v[0]==3);
    pm.owned=true; assert(Resolve(pm,pg,bm,bg).v[0]==4);
    pm.owned=false; assert(Resolve(pm,pg,bm,bg).v[0]==3);
    assert(ClampValue(0,3000)==2000 && ClampValue(3,180)==179 && ClampValue(6,0)==40);
    for(float width:{.16f,.4f,1.f}) {
        float x,y;
        assert(Intersect(0,0,1,0,0,-1,width,x,y) && x==256 && y==128);
        assert(!Intersect(0,0,-1,0,0,1,width,x,y));
        assert(!Intersect(0,0,1,1,0,0,width,x,y));
        assert(!Intersect(0,0,3,0,0,-1,width,x,y));
        for(int i=0;i<8;++i) {
            const auto &b=Buttons[i]; float px=(b.left+b.right)/2,py=(b.top+b.bottom)/2;
            assert(Intersect((px-256)*width/512,(128-py)*width/512,1,0,0,-1,width,x,y));
            assert(Hit(x,y)==i);
        }
        assert(!Intersect(std::numeric_limits<float>::infinity(),0,1,0,0,-1,width,x,y));
    }
    assert(Hit(256,128)==-1); assert(Hit(-1,90)==-1);
    puts("Radio: timeline boundaries, caps, previous/restart, placement precedence and scaled pointer hits passed.");
}
