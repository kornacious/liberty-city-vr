#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>

// Compare the original normalized linear-clamp stencil with integer fetches
// using a high-contrast, eye-specific virtual image. This checks native 1:1
// texel-center math; it does not model device filtering precision or replace
// an in-headset quality comparison. Directional FXAA sampling stays filtered.
static double texel(int x,int y,int width,int height,int eye){
    x=std::clamp(x,0,width-1);y=std::clamp(y,0,height-1);
    const uint32_t hash=uint32_t(x)*73856093u^uint32_t(y)*19349663u^uint32_t(eye)*83492791u;
    return double(hash&255)/255;
}
static double linear(double u,double v,int width,int height,int eye){
    u=std::clamp(u,.5/width,1-.5/width);v=std::clamp(v,.5/height,1-.5/height);
    const double px=u*width-.5,py=v*height-.5;
    const int x=int(std::floor(px)),y=int(std::floor(py));
    const double fx=px-x,fy=py-y;
    const double top=texel(x,y,width,height,eye)*(1-fx)+texel(x+1,y,width,height,eye)*fx;
    const double bottom=texel(x,y+1,width,height,eye)*(1-fx)+texel(x+1,y+1,width,height,eye)*fx;
    return top*(1-fy)+bottom*fy;
}
int main(){
    unsigned checks=0;double maximum=0;
    for(auto size:{std::array<int,2>{1,1},{1,19},{23,1},{17,31},{1512,1584},{1680,1760},{1890,1980}}){
        const int width=size[0],height=size[1];
        for(int eye=0;eye<2;++eye)for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            // Every border pixel, plus a dense deterministic interior sample.
            if(x>1&&y>1&&x<width-2&&y<height-2&&(x+31*y)%97)continue;
            for(auto offset:{std::array<int,2>{0,0},{-1,-1},{1,-1},{-1,1},{1,1}}){
                const double expected=linear((x+.5)/width+double(offset[0])/width,
                    (y+.5)/height+double(offset[1])/height,width,height,eye);
                const double actual=texel(x+offset[0],y+offset[1],width,height,eye);
                maximum=std::max(maximum,std::abs(expected-actual));
                assert(std::abs(expected-actual)<2e-12);++checks;
            }
        }
    }
    std::printf("PASS: native stencil centers, replicated borders, odd/degenerate sizes and separate eyes (%u checks, max ideal error %.9g).\n",checks,maximum);
}
