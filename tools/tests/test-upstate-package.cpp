#define _CRT_SECURE_NO_WARNINGS
#include "../../overlay/src/core/QuestUpstatePackage.h"
#include <chrono>
#include <cstdlib>
#include <iostream>

static int checks;
static void require(bool value,const char *message) { ++checks; if(!value) { std::cerr<<"FAIL "<<message<<"\n"; std::exit(1); } }
static std::string hash(const std::string &s) { questpackage::Sha256 h; h.update(s.data(),s.size()); return h.finish(); }
int main(int argc,char **argv) {
    using namespace questpackage;
    require(hash("")=="E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855","SHA256 empty");
    require(hash("abc")=="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD","SHA256 abc");
    require(hash(std::string(1000000,'a'))=="CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0","SHA256 multiple blocks");
    Sha256 streaming; for(int i=0;i<1000000;i++) streaming.update("a",1);
    require(streaming.finish()==hash(std::string(1000000,'a')),"streaming chunk boundaries");
    require(std::string(RelativeFile("/game","/game","DATA\\GTA3.DAT"))=="data/gta3.dat","case-insensitive root-relative map manifest");
    require(std::string(RelativeFile("/game","/game/data","main.scm"))=="data/main.scm","script read after SetDir(data)");
    require(std::string(RelativeFile("/game/","/game","./data/main.scm"))=="data/main.scm","dot-relative script path");
    require(std::string(RelativeFile("/game","/userfiles","/game/data/main.scm"))=="data/main.scm","absolute explicit package asset");
    require(!RelativeFile("/game","/userfiles","main.scm"),"save-directory reads stay outside the overlay");
    require(!RelativeFile("/game","/game","../data/main.scm"),"traversal rejected");
    require(!RelativeFile("/game","/game","vr_settings.ini"),"settings never redirected");
    require(!RelativeFile("/game","/game","/game-other/data/main.scm"),"root prefix boundary enforced");
    require(std::string(RelativeFile("/game","/game","DATA\\MAPS\\UL\\ul_generic\\ul_generic.IDE "))=="data/maps/UL/ul_generic/ul_generic.ide","DAT LoadLine terminal whitespace matches package entry");
    require(std::string(RelativeFile("/game","/game"," \tDATA\\MAIN.SCM\r\n"))=="data/main.scm","native file-open whitespace semantics preserved");
    require(!RelativeFile("/game","/game"," \r\n"),"empty whitespace request rejected");
    namespace fs=std::filesystem;
    fs::path root=fs::temp_directory_path()/("lcvr-upstate-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path package=root/"mods/upstate-liberty";
    fs::create_directories(package/"data");
    std::ofstream(package/"data/fixture.dat")<<"abc";
    std::ofstream(package/".lcvr-package.json")<<"{}";
    std::string digest=hash("abc");
    UpstateFile fixture[]={ {"data/fixture.dat",3,digest.c_str()} };
    require(!Enabled(root.string()),"missing activation marker selects Classic");
    std::ofstream(root/"mods/upstate-liberty.enabled",std::ios::binary)<<UpstateMarker<<"\n";
    require(Enabled(root.string()),"exact curated marker accepted");
    require(PackageFilesValid(root.string(),fixture,1),"complete synthetic payload accepted");
    require(PackageFilesValid(root.string()+"/",fixture,1),"trailing game-root separator accepted");
    std::ofstream(package/"data/fixture.dat")<<"abd";
    require(!PackageFilesValid(root.string(),fixture,1),"same-size content tampering rejected");
    std::ofstream(package/"data/fixture.dat")<<"abcd";
    require(!PackageFilesValid(root.string(),fixture,1),"size mismatch rejected");
    fs::remove(package/"data/fixture.dat");
    require(!PackageFilesValid(root.string(),fixture,1),"missing file rejected");
    std::ofstream(root/"mods/upstate-liberty.enabled",std::ios::binary)<<UpstateMarker<<"\nextra\n";
    require(!Enabled(root.string()),"appended marker data rejected");
    require(!CanActivate(root.string()),"invalid package cannot select expanded engine");
    fs::remove_all(root);
    if(argc==2) for(const auto &file:UpstateFiles) {
        fs::path path=fs::path(argv[1])/file.path;
        require(Regular(path)&&fs::file_size(path)==file.bytes&&FileHash(path.string().c_str())==file.sha256,"pinned local release payload");
    }
    std::cout<<"PASS "<<checks<<" native Upstate package/hash/startup checks\n";
}
