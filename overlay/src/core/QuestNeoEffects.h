#pragma once
#include "ModelSets.h"
#include "Clock.h"
#include "Weather.h"
#include "ZoneCull.h"
#include "vulkan/rwvk.h"
#include <cmath>

namespace QuestNeoEffects {
inline void Update(bool enabled, bool droplets)
{
    static bool attempted;
    static bool valid;
    static float tables[24][4][22];
    static rw::TexDictionary *textures;
    if(!attempted && ModelSets::IsModernActive()){
        attempted = true;
        char path[1024];
        if(ModelSets::GetModernAssetPath("neo/tuning.bin", path, sizeof(path))){
            FILE *file = fopen(path, "rb"); char magic[4];
            if(file){
                valid = fread(magic,1,4,file)==4 && memcmp(magic,"NEO1",4)==0 &&
                    fread(tables,1,sizeof(tables),file)==sizeof(tables) && fgetc(file)==EOF;
                fclose(file);
                for(auto &hour:tables) for(auto &weather:hour) for(float value:weather)
                    if(!std::isfinite(value)) valid=false;
            }
        }
        if(valid && ModelSets::GetModernAssetPath("neo/neo.txd",path,sizeof(path))){
            rw::StreamFile stream;
            if(stream.open(path,"rb") && rw::findChunk(&stream,rw::ID_TEXDICTIONARY,nil,nil))
                textures=rw::TexDictionary::streamRead(&stream);
        }
        valid = valid && textures != nil;
    }
    float values[22] = {};
    if(valid){
        const int h0=CClock::GetHours()%24,h1=(h0+1)%24;
        const int w0=CWeather::OldWeatherType>=0 && CWeather::OldWeatherType<4?CWeather::OldWeatherType:0;
        const int w1=CWeather::NewWeatherType>=0 && CWeather::NewWeatherType<4?CWeather::NewWeatherType:0;
        const float t=(CClock::GetMinutes()+CClock::GetSeconds()/60.0f)/60.0f;
        const float w=CWeather::InterpolationValue;
        for(int i=0;i<22;i++) values[i]=
            (tables[h0][w0][i]*(1-t)+tables[h1][w0][i]*t)*(1-w)+
            (tables[h0][w1][i]*(1-t)+tables[h1][w1][i]*t)*w;
    }
    rw::vulkan::setNeoEffects(textures,values,enabled && valid && ModelSets::IsModernActive());
    rw::vulkan::setNeoDroplets(enabled && droplets && !CCullZones::CamNoRain()?CWeather::Rain:0.0f);
}
}
