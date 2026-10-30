#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include "postfx-colour-gain.inc"

static float reference(float source,float colour,float alpha)
{
    float previous=source;
    for(int i=0;i<5;++i)
        previous=std::clamp(source*(1-alpha)+previous*colour*alpha,0.0f,1.0f);
    return previous;
}
int main()
{
    float maximum=0;
    unsigned checks=0;
    auto check=[&](float source,float colour,float alpha){
        float gain=ColourFilterGain(colour,alpha);
        float actual=source*gain,expected=reference(source,colour,alpha);
        maximum=std::max(maximum,std::abs(actual-expected));
        assert(std::isfinite(gain) && gain>=0 && gain<=1.000001f);
        assert(std::abs(actual-expected)<2e-6f);
        assert(std::abs(std::lround(actual*255)-std::lround(expected*255))<=1);
        ++checks;
    };
    for(int c=0;c<=255;++c)for(int a=0;a<=255;++a)
        for(float s:{0.f,1.f/255,0.1f,0.499f,0.5f,0.501f,254.f/255,1.f})
            check(s,c/255.f,a/255.f);
    std::mt19937 rng(20261031);
    std::uniform_real_distribution<float> unit(0,1);
    for(int i=0;i<100000;++i)check(unit(rng),unit(rng),unit(rng));
    for(float s:{0.f,.5f,1.f})for(float c:{0.f,.5f,1.f}){
        assert(ColourFilterGain(c,0)==1);
        check(s,c,1);
    }
    std::printf("PASS: extracted shader gain agrees with original clamped recurrence (%u cases, max error %.9g).\n",checks,maximum);
}
