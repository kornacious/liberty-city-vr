// Local-only Xbox GTA III asset conversion. No network or image modification.
#include <rw.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <unordered_set>
#include "vendor/lzokay/lzokay.hpp"
namespace fs = std::filesystem;
using Bytes = std::vector<rw::uint8>;
static uint32_t word(const Bytes &b, size_t p) {
    if(p > b.size() || b.size()-p < 4) throw std::runtime_error("Truncated asset header");
    return b[p] | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
static Bytes unpack(Bytes b) {
    if(word(b,0) != 0x67A3A1CE) return b;
    size_t end = size_t(word(b,8))+12;
    if(end>b.size()) throw std::runtime_error("Truncated Xbox compressed asset");
    Bytes out;
    for(size_t p=12;p<end;) {
        if(end-p<12) throw std::runtime_error("Truncated Xbox LZO block header");
        auto kind=word(b,p), count=word(b,p+4), repeat=word(b,p+8);
        if(kind!=4 || count!=repeat || count==0 || count>end-p-12)
            throw std::runtime_error("Invalid Xbox LZO block");
        Bytes block(128*1024); size_t n=0;
        if(lzokay::decompress(b.data()+p+12,count,block.data(),block.size(),n)!=lzokay::EResult::Success || !n)
            throw std::runtime_error("Invalid Xbox LZO data");
        if(out.size()+n>64*1024*1024) throw std::runtime_error("Xbox asset exceeds size limit");
        out.insert(out.end(),block.begin(),block.begin()+n); p+=12+count;
    }
    return out;
}
static void decoderTest() {
    Bytes original(260000);
    for(size_t i=0;i<original.size();i++) original[i]=(rw::uint8)((i*17+i/13)%251);
    Bytes wrapped(12,0);
    auto append=[](Bytes &b,uint32_t v) {for(int j=0;j<4;j++) b.push_back((rw::uint8)(v>>(8*j)));};
    for(size_t p=0;p<original.size();p+=128*1024) {
        size_t n=std::min(size_t(128*1024),original.size()-p), compressedSize=0;
        Bytes compressed(lzokay::compress_worst_size(n));
        if(lzokay::compress(original.data()+p,n,compressed.data(),compressed.size(),compressedSize)!=lzokay::EResult::Success)
            throw std::runtime_error("Synthetic compression failed");
        append(wrapped,4);append(wrapped,(uint32_t)compressedSize);append(wrapped,(uint32_t)compressedSize);
        wrapped.insert(wrapped.end(),compressed.begin(),compressed.begin()+compressedSize);
    }
    uint32_t magic=0x67A3A1CE,size=(uint32_t)wrapped.size()-12;
    for(int j=0;j<4;j++) {wrapped[j]=(rw::uint8)(magic>>(8*j));wrapped[8+j]=(rw::uint8)(size>>(8*j));}
    if(unpack(wrapped)!=original) throw std::runtime_error("Multi-block Xbox decompression changed bytes");
    for(int test=0;test<3;test++) {
        auto bad=wrapped;
        if(test==0) bad.resize(20);
        if(test==1) bad[12]=3;
        if(test==2) bad[20]^=1;
        bool rejected=false;try{unpack(bad);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected) throw std::runtime_error("Invalid Xbox compression accepted");
    }
    std::cout<<"PASS multi-block decompression, truncation, block type and size validation\n";
}
static int nodeOffset;
static void *nodeCtor(void *o,rw::int32 off,rw::int32) { std::memset((char*)o+off,0,64); return o; }
static void *nodeCopy(void *d,void *s,rw::int32 off,rw::int32) { std::memcpy((char*)d+off,(char*)s+off,64);return d; }
static rw::Stream *nodeRead(rw::Stream *s,rw::int32 n,void *o,rw::int32 off,rw::int32) {
    if(n<0 || n>=64) throw std::runtime_error("Invalid model node name");
    s->read8((char*)o+off,n);return s;
}
static rw::Stream *nodeWrite(rw::Stream *s,rw::int32 n,void *o,rw::int32 off,rw::int32) {s->write8((char*)o+off,n);return s;}
static rw::int32 nodeSize(void *o,rw::int32 off,rw::int32) {return (rw::int32)std::strlen((char*)o+off);}
static bool correctGameplayHeadUVs(const fs::path &input, rw::Geometry *geometry, const char *node) {
    // These US 1.0 gameplay heads use the vertically reversed second V tile.
    // The disc's WRAP sampler repeats it without reversing it. Keep this retail
    // asset correction separate from Xbox vertex decoding: bodies, hands and
    // cutscene heads (including mariah/curlyh) have different UV layouts.
    const auto model=input.filename().string();
    if((model!="maria.dff" && model!="curly.dff" && model!="cat.dff") ||
       std::strcmp(node,"Shead01")!=0) return false;
    for(int set=0;set<geometry->numTexCoordSets;set++)
        for(int i=0;i<geometry->numVertices;i++)
            geometry->texCoords[set][i].v=2.0f-geometry->texCoords[set][i].v;
    return true;
}
static void init();
static void headUVTest() {
    init();
    auto geometry=rw::Geometry::create(3,0,rw::Geometry::TEXTURED);
    const rw::TexCoords authored[]={{0.25f,1.75f},{0.5f,1.25f},{0.75f,0.99f}};
    for(const char *model:{"maria.dff","curly.dff","cat.dff"}) {
        std::memcpy(geometry->texCoords[0],authored,sizeof(authored));
        if(!correctGameplayHeadUVs(model,geometry,"Shead01")) throw std::runtime_error("Gameplay head correction missing");
        const float expected[]={0.25f,0.75f,1.01f};
        for(int i=0;i<3;i++)
            if(geometry->texCoords[0][i].u!=authored[i].u || std::abs(geometry->texCoords[0][i].v-expected[i])>0.00001f)
                throw std::runtime_error("Gameplay head UV orientation changed incorrectly");
    }
    for(const char *model:{"maria.dff","mariah.dff","curlyh.dff","asuka.dff","player.dff"})
        for(const char *node:{"Swaist","SLhand01","SRhand01","Shead01"}) {
            if(std::strcmp(model,"maria.dff")==0 && std::strcmp(node,"Shead01")==0) continue;
            std::memcpy(geometry->texCoords[0],authored,sizeof(authored));
            if(correctGameplayHeadUVs(model,geometry,node) || std::memcmp(geometry->texCoords[0],authored,sizeof(authored))!=0)
                throw std::runtime_error("Unrelated geometry UVs changed");
        }
    geometry->destroy();
    std::cout<<"PASS gameplay head orientation and body/hand/cutscene exclusions\n";
}
static void init() {
    if(!rw::Engine::init()) throw std::runtime_error("Cannot initialize librw");
    rw::registerMeshPlugin();rw::registerNativeDataPlugin();
    rw::registerAtomicRightsPlugin();rw::registerMaterialRightsPlugin();
    rw::xbox::registerVertexFormatPlugin();rw::registerSkinPlugin();
    rw::registerHAnimPlugin();rw::registerMatFXPlugin();rw::registerUVAnimPlugin();rw::registerUserDataPlugin();
    nodeOffset=rw::Frame::registerPlugin(64,0x0253F2FE,nodeCtor,nullptr,nodeCopy);
    rw::Frame::registerPluginStream(0x0253F2FE,nodeRead,nodeWrite,nodeSize);
    if(!rw::Engine::open(nullptr)||!rw::Engine::start()) throw std::runtime_error("Cannot start librw");
    rw::platform=rw::PLATFORM_D3D8;
    rw::version=0x34003; rw::build=0xFFFF;
    rw::Texture::setCreateDummies(true);
    rw::Texture::setLoadTextures(false);
}
static void verify(const fs::path &path) {
    rw::StreamFile stream;
    if(!stream.open(path.string().c_str(),"rb")) throw std::runtime_error("Cannot reopen converted asset");
    if(path.extension()==".txd") {
        if(!rw::findChunk(&stream,rw::ID_TEXDICTIONARY,nullptr,nullptr)) throw std::runtime_error("Invalid converted dictionary");
        auto dict=rw::TexDictionary::streamRead(&stream);
        if(!dict) throw std::runtime_error("Converted dictionary cannot be read");
        FORLIST(lnk,dict->textures) {
            auto tex=rw::Texture::fromDict(lnk);
            if(!tex->raster || tex->raster->platform!=rw::PLATFORM_D3D8 || tex->raster->width<=0 || tex->raster->height<=0)
                throw std::runtime_error("Invalid converted texture");
        }
        dict->destroy();
    } else {
        unsigned count=0;
        while(rw::findChunk(&stream,rw::ID_CLUMP,nullptr,nullptr)) {
            auto clump=rw::Clump::streamRead(&stream);
            if(!clump) throw std::runtime_error("Converted model cannot be read");
            FORLIST(lnk,clump->atomics) {
                auto g=rw::Atomic::fromClump(lnk)->geometry;
                if((g->flags&rw::Geometry::NATIVE) || g->numVertices<=0 || g->numTriangles<0)
                    throw std::runtime_error("Invalid converted geometry");
                for(int i=0;i<g->numVertices;i++) {
                    auto p=g->morphTargets[0].vertices[i];
                    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) throw std::runtime_error("Non-finite converted vertex");
                }
            }
            clump->destroy();count++;
        }
        if(!count) throw std::runtime_error("Empty converted model");
    }
    stream.close();
}
static void convert(const fs::path &input,const fs::path &output) {
    std::ifstream file(input,std::ios::binary);
    Bytes bytes((std::istreambuf_iterator<char>(file)),{});
    bytes=unpack(std::move(bytes));
    rw::StreamMemory stream;stream.open(bytes.data(),(rw::uint32)bytes.size());
    rw::StreamFile target;
    if(!target.open(output.string().c_str(),"wb")) throw std::runtime_error("Cannot create converted asset");
    if(input.extension()==".txd") {
        if(!rw::findChunk(&stream,rw::ID_TEXDICTIONARY,nullptr,nullptr)) throw std::runtime_error("Missing texture dictionary");
        auto dict=rw::TexDictionary::streamRead(&stream);
        if(!dict) throw std::runtime_error("Cannot read Xbox texture dictionary");
        FORLIST(lnk,dict->textures) {
            auto tex=rw::Texture::fromDict(lnk);
            if(!tex->raster) throw std::runtime_error("Missing texture raster");
            // Preserves DXT mip data through librw's Xbox -> D3D fast path.
            tex->raster=rw::Raster::convertTexToCurrentPlatform(tex->raster);
            if(!tex->raster) throw std::runtime_error("Cannot convert Xbox texture");
            if(input.filename()=="player.txd" && std::strcmp(tex->name,"playa5")==0) {
                auto image=tex->raster->toImage();
                if(!image) throw std::runtime_error("Cannot export default player skin");
                auto skin=output.parent_path()/"player.bmp";
                rw::writeBMP(image,skin.string().c_str());image->destroy();
            }
        }
        dict->streamWrite(&target);dict->destroy();
    } else {
        unsigned count=0;
        while(rw::findChunk(&stream,rw::ID_CLUMP,nullptr,nullptr)) {
            // Uninstancing must select the source platform's default/skin/MatFX
            // pipelines. Unadorned Xbox atomics otherwise pick the D3D8 driver
            // and interpret Xbox native buffers with the wrong layout.
            rw::platform=rw::PLATFORM_XBOX;
            auto clump=rw::Clump::streamRead(&stream);
            if(!clump) throw std::runtime_error("Cannot read Xbox model");
            std::unordered_set<rw::Geometry*> corrected;
            FORLIST(lnk,clump->atomics) {
                auto atomic=rw::Atomic::fromClump(lnk);
                if(atomic->geometry->flags&rw::Geometry::NATIVE) atomic->uninstance();
                if(atomic->geometry->flags&rw::Geometry::NATIVE) throw std::runtime_error("Model remains Xbox-native");
                if(!corrected.count(atomic->geometry) &&
                   correctGameplayHeadUVs(input,atomic->geometry,(char*)atomic->getFrame()+nodeOffset))
                    corrected.insert(atomic->geometry);
            }
            rw::platform=rw::PLATFORM_D3D8;
            if(!clump->streamWrite(&target)) throw std::runtime_error("Cannot write converted model");
            clump->destroy();count++;
        }
        if(!count) throw std::runtime_error("Missing model clump");
    }
    target.close();
    verify(output);
}
int main(int argc,char **argv) {
    try {
        if(argc==2 && std::strcmp(argv[1],"--self-test")==0) {decoderTest();return 0;}
        if(argc==2 && std::strcmp(argv[1],"--head-uv-test")==0) {headUVTest();return 0;}
        if(argc==3 && std::strcmp(argv[1],"--frontend")==0) {
            // Port-owned fallback artwork, independent of retail PC menu art.
            // Settings layout, fonts and backgrounds remain in the presenter.
            init();auto dict=rw::TexDictionary::create();
            const char *names[]={"fe2_mainpanel_ul","fe2_mainpanel_ur","fe2_mainpanel_dl","fe2_mainpanel_dr","fe2_mainpanel_dr2",
                "fe2_tabactive","fe_iconbrief","fe_iconstats","fe_iconcontrols","fe_iconsave","fe_iconaudio","fe_icondisplay",
                "fe_iconlanguage","fe_controller","fe_controllersh","fe_arrows1","fe_arrows2","fe_arrows3","fe_arrows4",
                "fe_radio1","fe_radio2","fe_radio3","fe_radio4","fe_radio5","fe_radio6","fe_radio7","fe_radio8","fe_radio9"};
            const char *digits[]={"010110010010111","110001010100111","110001010001110","101101111001001","111100110001110",
                "011100110101010","111001010010010","010101010101010","010101011001110"};
            for(const char *name:names) {
                auto image=rw::Image::create(64,64,32);image->allocate();
                const bool radio=std::strncmp(name,"fe_radio",8)==0;
                const bool arrow=std::strncmp(name,"fe_arrows",9)==0;
                for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
                    auto pixel=image->pixels+y*image->stride+x*4;
                    bool mark=false;
                    if(radio&&x>=20&&x<44&&y>=12&&y<52)mark=digits[name[8]-'1'][(y-12)/8*3+(x-20)/8]=='1';
                    else if(arrow)mark=x>=16&&x<48&&std::abs(y-32)<(x-16)/2;
                    else if(!radio)mark=x<2||x>61||y<2||y>61;
                    pixel[0]=mark?230:16;pixel[1]=mark?169:24;pixel[2]=mark?64:32;pixel[3]=radio||mark?255:200;
                }
                auto raster=rw::Raster::createFromImage(image,rw::PLATFORM_D3D8);image->destroy();
                if(!raster)throw std::runtime_error("Cannot create frontend raster");
                auto texture=rw::Texture::create(raster);std::strcpy(texture->name,name);texture->setFilter(rw::Texture::LINEAR);dict->add(texture);
            }
            rw::StreamFile output;if(!output.open(argv[2],"wb"))throw std::runtime_error("Cannot write frontend dictionary");
            dict->streamWrite(&output);output.close();dict->destroy();verify(argv[2]);return 0;
        }
        if(argc==4 && std::strcmp(argv[1],"--neo")==0) {
            init(); auto dict=rw::TexDictionary::create();unsigned count=0;
            for(auto &entry:fs::directory_iterator(argv[2])) {
                if(entry.path().extension()!=".png") continue;
                auto img=rw::readPNG(entry.path().string().c_str());
                if(!img) throw std::runtime_error("Invalid Neo image");
                auto raster=rw::Raster::createFromImage(img,rw::PLATFORM_D3D8);img->destroy();
                if(!raster) throw std::runtime_error("Cannot create Neo raster");
                auto tex=rw::Texture::create(raster);auto name=entry.path().stem().string();
                if(name.size()>=32) throw std::runtime_error("Neo texture name too long");
                std::strcpy(tex->name,name.c_str());tex->setFilter(rw::Texture::LINEAR);dict->add(tex);count++;
            }
            if(count!=15) throw std::runtime_error("Incomplete Neo image collection");
            rw::StreamFile output;if(!output.open(argv[3],"wb")) throw std::runtime_error("Cannot create Neo dictionary");
            dict->streamWrite(&output);output.close();dict->destroy();verify(argv[3]);return 0;
        }
        if(argc!=3) throw std::runtime_error("Usage: xbox-convert INPUT_DIRECTORY OUTPUT_DIRECTORY");
        init();unsigned count=0;
        for(auto &entry:fs::recursive_directory_iterator(argv[1])) {
            if(!entry.is_regular_file()) continue;
            auto ext=entry.path().extension();if(ext!=".dff" && ext!=".txd") continue;
            auto output=fs::path(argv[2])/fs::relative(entry.path(),argv[1]);fs::create_directories(output.parent_path());
            std::cout<<"Converting "<<fs::relative(entry.path(),argv[1]).generic_string()<<std::endl;
            convert(entry.path(),output);count++;
        }
        if(!count) throw std::runtime_error("No assets to convert");
        std::cout<<"Converted "<<count<<" assets\n";
        rw::Engine::stop();rw::Engine::close();rw::Engine::term();return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
