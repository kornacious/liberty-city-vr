#pragma once
// Shared procedural instruments: metres, bottom-centre origin, +Y up,
// +Z toward the driver. Runtime and desktop editor use the same mesh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Speedometer {
inline float kmh(float engineSpeed) {
    return std::isfinite(engineSpeed) ? std::max(0.f, engineSpeed * 180.f) : 0.f;
}
inline float displaySpeed(float speedKmh, bool metric) {
    return std::isfinite(speedKmh) ? std::max(0.f, speedKmh) / (metric ? 1.f : 1.609344f) : 0.f;
}
// Handling is an estimate, not a limiter. Leave 10% headroom and never
// rescale in response to actual speed (e.g. downhill or a physics impulse).
inline float dialMaximum(float handlingKmh, bool metric) {
    float estimate = displaySpeed(handlingKmh > 0 && std::isfinite(handlingKmh) ? handlingKmh : 180.f, metric) * 1.1f;
    const float mph[] = {100, 140, 180, 220};
    const float kph[] = {160, 220, 280, 360};
    const float *ranges = metric ? kph : mph;
    for (int i = 0; i < 4; ++i) if (estimate <= ranges[i]) return ranges[i];
    return std::ceil(estimate / 40.f) * 40.f;
}
inline float needleAngle(float speed, float maximum) {
    return (225.f - 270.f * std::max(0.f, std::min(1.f, speed / std::max(1.f, maximum)))) * .01745329252f;
}
struct Color { unsigned char r, g, b; };
struct Vertex { float x, y, z; Color color; };
struct Mesh {
    std::vector<Vertex> vertices;
    void triangle(float ax, float ay, float bx, float by, float cx, float cy, Color c, float z) {
        vertices.push_back({ax, ay, z, c}); vertices.push_back({bx, by, z, c}); vertices.push_back({cx, cy, z, c});
    }
    void rect(float x, float y, float w, float h, Color c, float z) {
        triangle(x,y,x+w,y,x+w,y+h,c,z); triangle(x,y,x+w,y+h,x,y+h,c,z);
    }
    void line(float ax, float ay, float bx, float by, float width, Color c, float z) {
        float dx=bx-ax, dy=by-ay, length=std::sqrt(dx*dx+dy*dy);
        if(length < 1e-7f) return;
        float x=-dy/length*width*.5f, y=dx/length*width*.5f;
        triangle(ax+x,ay+y,bx+x,by+y,bx-x,by-y,c,z);
        triangle(ax+x,ay+y,bx-x,by-y,ax-x,ay-y,c,z);
    }
    void disc(float x, float y, float radius, Color c, float z) {
        for(int i=0;i<64;++i) {
            float a=i*6.2831853f/64, b=(i+1)*6.2831853f/64;
            triangle(x,y,x+std::cos(a)*radius,y+std::sin(a)*radius,
                     x+std::cos(b)*radius,y+std::sin(b)*radius,c,z);
        }
    }
    // Small 5x7 font, deliberately limited to the instrument's numeric and unit labels.
    static const unsigned char *glyph(char c) {
        static const unsigned char rows[][7] = {
            {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
            {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
            {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
            {14,17,17,15,1,1,14},{17,27,21,21,17,17,17}, // M
            {30,17,17,30,16,16,16},{17,17,17,31,17,17,17}, // P H
            {17,18,20,24,20,18,17},{1,2,2,4,8,8,16},{0,0,0,0,0,0,0}}; // K /
        int i = c>='0' && c<='9' ? c-'0' : c=='M'?10:c=='P'?11:c=='H'?12:c=='K'?13:c=='/'?14:15;
        return rows[i];
    }
    void text(const char *s, float cx, float bottom, float pixel, Color c, float z) {
        float x=cx-(std::strlen(s)*6-1)*pixel*.5f;
        for(;*s;++s,x+=6*pixel) for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(glyph(*s)[row] & (1<<(4-col))) rect(x+col*pixel,bottom+(6-row)*pixel,pixel*.86f,pixel*.86f,c,z);
    }
};
inline void build(Mesh &m, bool analog, bool metric, float speedKmh, float maximum) {
    m.vertices.clear();
    const Color rim={58,67,78}, dark={10,13,18}, white={240,238,220}, amber={255,174,62}, blue={104,185,232};
    char value[24];
    // Bound formatting only for malformed/absurd physics, never to the dial maximum.
    std::snprintf(value,sizeof(value),"%.0f",std::min(99999.f,displaySpeed(speedKmh,metric)));
    if(!analog) {
        m.rect(-.09f,0,.18f,.065f,rim,0);
        m.rect(-.088f,.002f,.176f,.061f,dark,.0005f);
        m.line(-.075f,.008f,.075f,.008f,.0013f,blue,.001f);
        m.text(value,-.020f,.020f,.0045f,white,.0015f);
        m.text(metric?"KM/H":"MPH",.056f,.025f,.0013f,white,.0015f);
    } else {
        const float cy=.08f;
        m.disc(0,cy,.08f,rim,0);
        m.disc(0,cy,.076f,dark,.0005f);
        const int majorCount=(int)std::round(maximum/(maximum<=220.f?20.f:40.f));
        const int tickCount=majorCount*4;
        for(int i=0;i<=tickCount;++i) {
            float a=needleAngle((float)i,(float)tickCount), inner=i%4==0?.060f:.065f;
            m.line(std::cos(a)*inner,cy+std::sin(a)*inner,std::cos(a)*.070f,cy+std::sin(a)*.070f,
                   i%4==0?.0013f:.0007f,white,.001f);
            if(i%4==0) {
                char label[16]; std::snprintf(label,sizeof(label),"%.0f",maximum*i/tickCount);
                m.text(label,std::cos(a)*.049f,cy+std::sin(a)*.049f-.003f,.00095f,white,.0015f);
            }
        }
        m.text(metric?"KM/H":"MPH",0,.052f,.0011f,white,.0015f);
        m.rect(-.034f,.019f,.068f,.019f,{20,20,20},.001f);
        m.text(value,0,.022f,.0018f,amber,.0015f);
        float a=needleAngle(displaySpeed(speedKmh,metric),maximum);
        m.line(-std::cos(a)*.008f,cy-std::sin(a)*.008f,std::cos(a)*.057f,cy+std::sin(a)*.057f,.0025f,amber,.002f);
        m.disc(0,cy,.006f,rim,.0025f);
    }
}
} // namespace Speedometer
