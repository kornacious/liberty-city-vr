#include "vulkan/UniformAlphaPolicy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
namespace rw { namespace vulkan {
using uint32=unsigned;
#define nil nullptr
enum {SHADER_WORLD,SHADER_SKIN,SHADER_IM3D,ALPHAALWAYS=10,ALPHAGREATEREQUAL=11,PLATFORM_VULKAN=12};
struct InstanceData {bool vertexAlpha=false;};
struct Material {struct {unsigned char alpha=255;}color;};
struct Raster {enum {TEXTURE=4,CAMERATEXTURE=5};int platform=PLATFORM_VULKAN,type=TEXTURE;Raster *parent=nullptr;};
struct Geometry {enum {MODULATE=1};unsigned flags=MODULATE;};
struct Texture {enum {WRAP=1,MIRROR,CLAMP,BORDER};};
static bool vehicleAlphaPass=false,dynamicObjectPass=false;
static struct {unsigned alphaTestFunction=ALPHAGREATEREQUAL,alphaTestRef=2;
    bool vertexAlphaEnabled=false;unsigned zWriteEnabled=1;
    unsigned textureAddressU=Texture::WRAP,textureAddressV=Texture::WRAP;}gstate;
#include "uniform-alpha-production.inc"
using VkPipeline=unsigned;
using VkPrimitiveTopology=unsigned;
static unsigned lookupAlpha=0,lookupDepth=0;
static bool failPipeline=false;
static VkPipeline getPipeline(unsigned,VkPrimitiveTopology){
    lookupAlpha=gstate.alphaTestFunction;lookupDepth=gstate.zWriteEnabled;
    return failPipeline?0:1;
}
static VkPipeline InspectPipeline(bool vehicleSplit,bool opaqueWorldPipeline,
                                 unsigned savedZWrite,bool &alpha,bool &depth){
    const unsigned shader=SHADER_WORLD;
    struct Header {unsigned primType=3;}data;
    const auto *header=&data;
#include "opaque-world-pipeline.inc"
    alpha=profileAlphaTest;depth=profileDepthWrite;return pipeline;
}
} }
// Mirrors the fragment shader's exact complementary discard comparisons,
// including the non-complementary negative-zero reference endpoint.
static bool Keeps(unsigned pass,float alpha,float reference){
    const float encoded=pass==0?reference:-reference;
    return encoded>=0?!(alpha<encoded):!(alpha>=-encoded);
}
int main(){
    using namespace rw::vulkan;
    using namespace UniformAlphaPolicy;
    InstanceData inst;Material material;Geometry geometry;Raster raster;
    unsigned optimized=0,checks=0;
    for(unsigned a=0;a<256;++a)for(unsigned r=0;r<256;++r){
        material.color.alpha=static_cast<unsigned char>(a);
        const float alpha=a/255.0f,reference=r/255.0f;
        const unsigned mask=selectAtomicAlphaPasses(&inst,&material,&raster,&geometry,
            SHADER_WORLD,nullptr,true,false,false,reference);
        assert(mask==Both||mask==Solid||mask==Faint);
        optimized+=mask!=Both;
        for(float error:{-Margin*.5f,0.0f,Margin*.5f}){
            const float fragmentAlpha=std::fmax(0.0f,std::fmin(1.0f,alpha+error));
            // Every fragment retained by either original pass survives with
            // the same pass (and therefore depth/blend/motion behavior).
            for(unsigned pass=0;pass<2;++pass){
                const bool before=Keeps(pass,fragmentAlpha,reference);
                const bool after=(mask&(1u<<pass))&&Keeps(pass,fragmentAlpha,reference);
                assert(before==after);++checks;
            }
        }
    }
    assert(optimized>60000);
    material.color.alpha=64;
    auto select=[&](bool split=true,bool vehicleSplit=false,bool textureAlpha=false,
                   unsigned shader=SHADER_WORLD,const unsigned *bones=nullptr,
                   const Raster *texture=nullptr,const Material *mat=nullptr){
        return selectAtomicAlphaPasses(&inst,mat?mat:&material,texture,&geometry,
            shader,bones,split,vehicleSplit,textureAlpha,0.5f);
    };
    assert(select()==Faint);
    material.color.alpha=240;assert(select()==Solid);
    assert(select(false)==Both&&select(true,true)==Both&&select(true,false,true)==Both);
    inst.vertexAlpha=true;assert(select()==Both);inst.vertexAlpha=false;
    vehicleAlphaPass=true;assert(select()==Both);vehicleAlphaPass=false;
    dynamicObjectPass=true;assert(select()==Both);dynamicObjectPass=false;
    assert(select(true,false,false,SHADER_SKIN)==Both&&select(true,false,false,SHADER_IM3D)==Both);
    unsigned bones=0;assert(select(true,false,false,SHADER_WORLD,&bones)==Both);
    gstate.alphaTestFunction=ALPHAALWAYS;assert(select()==Both);gstate.alphaTestFunction=ALPHAGREATEREQUAL;
    gstate.textureAddressU=Texture::BORDER;assert(select()==Both);gstate.textureAddressU=Texture::WRAP;
    gstate.textureAddressV=Texture::BORDER;assert(select()==Both);gstate.textureAddressV=Texture::WRAP;
    gstate.textureAddressU=0;assert(select()==Both);gstate.textureAddressU=Texture::MIRROR;
    gstate.textureAddressV=Texture::CLAMP;assert(select()==Solid);gstate.textureAddressV=Texture::WRAP;
    gstate.textureAddressU=Texture::WRAP;
    assert(select(true,false,false,SHADER_WORLD,nullptr,&raster)==Solid);
    raster.type=Raster::CAMERATEXTURE;assert(select(true,false,false,SHADER_WORLD,nullptr,&raster)==Both);raster.type=Raster::TEXTURE;
    raster.platform=0;assert(select(true,false,false,SHADER_WORLD,nullptr,&raster)==Both);
    raster.platform=PLATFORM_VULKAN;raster.parent=&raster;
    assert(select(true,false,false,SHADER_WORLD,nullptr,&raster)==Solid); // librw root rasters self-parent
    Raster parent;raster.parent=&parent;
    assert(select(true,false,false,SHADER_WORLD,nullptr,&raster)==Both);raster.parent=nullptr;
    geometry.flags=0;material.color.alpha=0;assert(select()==Solid); // shader ignores material alpha
    geometry.flags=Geometry::MODULATE;assert(select()==Faint);
    assert(selectAtomicAlphaPasses(&inst,nullptr,nullptr,&geometry,SHADER_WORLD,nullptr,true,false,false,.5f)==Solid);
    for(float ref:{0.0f,-0.0f,1.0f,-1.0f,Margin,1-Margin,
                   std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        assert(Select(.25f,ref)==Both);
    for(float alpha:{-1.0f,2.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        assert(Select(alpha,.5f)==Both);
    for(float alpha:{.5f,.5f-Margin,.5f+Margin})assert(Select(alpha,.5f)==Both);
    assert(Select(std::nextafter(.5f+Margin,1.0f),.5f)==Solid);
    assert(Select(std::nextafter(.5f-Margin,0.0f),.5f)==Faint);
    // New opaque-world path: preserve coverage for all authored references,
    // including endpoint fallbacks and conservative interpolation perturbations.
    geometry.flags=Geometry::MODULATE;material.color.alpha=255;
    unsigned opaqueChecks=0,opaqueReferences=0;
    for(unsigned reference=0;reference<256;++reference){
        gstate.alphaTestRef=reference;
        const bool opaque=useOpaqueWorldPipeline(&inst,&material,&raster,&geometry,
            SHADER_WORLD,nullptr,false,false,false);
        opaqueReferences+=opaque;
        for(float error:{-Margin*.5f,0.0f,Margin*.5f}){
            const float fragmentAlpha=std::fmax(0.0f,std::fmin(1.0f,1.0f+error));
            const bool before=Keeps(0,fragmentAlpha,reference/255.0f);
            assert(before==(opaque?true:before));++opaqueChecks;
        }
    }
    assert(opaqueReferences==254);
    gstate.alphaTestRef=2;
    auto opaque=[&](bool split=false,bool blend=false,bool textureAlpha=false,
                    unsigned shader=SHADER_WORLD,const unsigned *boneData=nullptr){
        return useOpaqueWorldPipeline(&inst,&material,&raster,&geometry,shader,boneData,split,blend,textureAlpha);
    };
    assert(opaque());
    assert(!opaque(true)&&!opaque(false,true)&&!opaque(false,false,true));
    gstate.vertexAlphaEnabled=true;assert(!opaque());gstate.vertexAlphaEnabled=false;
    inst.vertexAlpha=true;assert(!opaque());inst.vertexAlpha=false;
    vehicleAlphaPass=true;assert(!opaque());vehicleAlphaPass=false;
    dynamicObjectPass=true;assert(!opaque());dynamicObjectPass=false;
    assert(!opaque(false,false,false,SHADER_SKIN)&&!opaque(false,false,false,SHADER_IM3D));
    assert(!opaque(false,false,false,SHADER_WORLD,&bones));
    raster.parent=&parent;assert(!opaque());raster.parent=&raster;assert(opaque());raster.parent=nullptr;
    raster.type=Raster::CAMERATEXTURE;assert(!opaque());raster.type=Raster::TEXTURE;
    raster.platform=0;assert(!opaque());raster.platform=PLATFORM_VULKAN;
    gstate.textureAddressU=Texture::BORDER;assert(!opaque());gstate.textureAddressU=Texture::WRAP;
    gstate.textureAddressV=0;assert(!opaque());gstate.textureAddressV=Texture::CLAMP;
    assert(opaque());gstate.textureAddressV=Texture::WRAP;
    gstate.alphaTestFunction=ALPHAALWAYS;assert(!opaque());gstate.alphaTestFunction=ALPHAGREATEREQUAL;
    material.color.alpha=64;assert(!opaque(false,true));material.color.alpha=255;
    bool alpha=false,depth=false;
    assert(InspectPipeline(false,true,1,alpha,depth)==1);
    assert(lookupAlpha==ALPHAALWAYS&&!alpha&&depth&&lookupDepth==1);
    assert(gstate.alphaTestFunction==ALPHAGREATEREQUAL&&gstate.zWriteEnabled==1);
    assert(InspectPipeline(false,false,1,alpha,depth)==1&&lookupAlpha==ALPHAGREATEREQUAL&&alpha);
    failPipeline=true;assert(InspectPipeline(false,true,1,alpha,depth)==0);
    assert(gstate.alphaTestFunction==ALPHAGREATEREQUAL&&gstate.zWriteEnabled==1);failPipeline=false;
    gstate.alphaTestFunction=ALPHAALWAYS;gstate.zWriteEnabled=0;
    assert(InspectPipeline(true,true,1,alpha,depth)==1);
    assert(lookupAlpha==ALPHAGREATEREQUAL&&alpha&&!depth&&gstate.alphaTestFunction==ALPHAALWAYS&&gstate.zWriteEnabled==1);
    std::printf("PASS: opaque-world selector, %u differential coverage checks, %u eligible references; fallback policies, actual pipeline key inputs/profile flags and state restoration on success/failure.\n",opaqueChecks,opaqueReferences);
    std::printf("PASS: production selector, %u differential shader coverage checks, %u optimized alpha/reference pairs; endpoint/threshold guards, invalid inputs, MODULATE, texture, vehicle/dynamic/skin and alpha-test fallbacks.\n",checks,optimized);
}
