#include "AssetVariants.h"
#include <iostream>
#include <stdexcept>
#include <chrono>

namespace fs=std::filesystem;
static void check(bool value) {if(!value)throw std::runtime_error("Asset variant contract failed");}
static void create(const fs::path &root,int id) {
    auto base=root/"variants"/(id==0?"pc":"xbox");fs::create_directories(base);
    std::ofstream marker(base/"VARIANT_INFO.txt");marker<<(id==0?"LCVR_VARIANT_PC_1\n":"LCVR_VARIANT_XBOX_1\n");
    for(const char *p:{"data/main.scm","data/default.dat","data/gta3.dat","data/handling.cfg","data/weapon.dat",
        "models/gta3.img","models/gta3.dir","models/frontend.txd","models/menu.txd","models/fonts.txd",
        "anim/ped.ifp","anim/cuts.img","anim/cuts.dir","text/american.gxt","audio/sfx.raw","audio/sfx.sdt"}) {
        fs::create_directories((base/p).parent_path());std::ofstream(base/p)<<'x';marker<<"1\t"<<p<<'\n';
    }
}
int main() {
    auto root=fs::temp_directory_path()/("lcvr-variants-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        check(assetvariants::Select(root.string())==-1);
        create(root,1);check(assetvariants::Select(root.string())==1);
        {
            auto path=root/"variants/xbox/VARIANT_INFO.txt";
            std::ifstream read(path,std::ios::binary);std::string text((std::istreambuf_iterator<char>(read)),{});read.close();
            // Exercise CRLF even when the host text reader already normalizes it.
            std::string crlf;for(char c:text){if(c=='\n')crlf+='\r';crlf+=c;}
            std::ofstream write(path,std::ios::binary);write<<crlf;write.close();check(assetvariants::Complete(root.string(),1));
        }
        check(assetvariants::Resolve(root.string(),"audio/test.wav").find("xbox")!=std::string::npos);
        check(assetvariants::Resolve(root.string(),"vr_settings.ini")== (root/"vr_settings.ini").generic_string());
        create(root,0);check(assetvariants::Select(root.string())==0);
        std::ofstream(root/"vr_settings.ini")<<"[VR]\nModelSet=1\n";check(assetvariants::Select(root.string())==1);
        std::ofstream(root/"vr_settings.ini")<<"\xEF\xBB\xBF[vr]\n modelset = 1 ; saved choice\n";check(assetvariants::Select(root.string())==1);
        std::ofstream(root/"vr_settings.ini")<<"[VR]\nModelSet=999\n";check(assetvariants::Select(root.string())==0);
        fs::remove(root/"variants/xbox/models/frontend.txd");check(!assetvariants::Complete(root.string(),1));check(assetvariants::Select(root.string())==0);
        std::ofstream(root/"variants/pc/data/main.scm")<<"bad size";check(assetvariants::Select(root.string())==-1);
        fs::remove_all(root);std::cout<<"Complete variant selection, missing/corrupt inventory and shared state passed\n";return 0;
    } catch(const std::exception &e) {fs::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}
}
