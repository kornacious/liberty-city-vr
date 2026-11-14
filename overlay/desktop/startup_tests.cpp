#include "../src/core/PcvrStartup.h"
#include <chrono>
#include <iostream>

static void require(bool value) {if(!value) throw std::runtime_error("PCVR startup assertion failed.");}
template<class F> static void reject(F function) {try {function();}catch(const std::runtime_error&){return;} throw std::runtime_error("Expected invalid configuration rejection.");}
int main(int argc,char **argv) {
    pcvrstartup::Arguments utf8(argc,argv); argc=int(utf8.values.size()); argv=utf8.pointers.data();
    if(argc>1 && std::string(argv[1])=="--fake-upstate") {
        require(argc==9);
        require(std::string(argv[2])==R"(literal spaces & $() % quotes" \)");
        require(std::string(argv[3])=="--game-data" && std::string(argv[4])=="earlier");
        require(std::string(argv[5])=="--game-data" && std::string(argv[6])==R"(resolved data \)");
        require(std::string(argv[7])=="--user-data" && std::string(argv[8])==u8"resolved user é");
        return 23;
    }
    const auto directory=std::filesystem::temp_directory_path()/("pcvr-startup-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(directory);
        auto write=[&](const std::string &text) {std::ofstream(directory/"pcvr-paths.ini",std::ios::binary)<<text;};
        std::string data,user;
        pcvrstartup::Defaults(directory,data,user); require(data.empty() && user.empty());
        write("version=1\ndata=../data with spaces\nuser=../user\n");
        pcvrstartup::Defaults(directory,data,user);
        require(data==(directory/"../data with spaces").lexically_normal().u8string());
        require(user==(directory/"../user").lexically_normal().u8string());
        data="CLI data"; user.clear(); pcvrstartup::Defaults(directory,data,user,true,false); require(data=="CLI data" && !user.empty());
        user="CLI user"; data.clear(); pcvrstartup::Defaults(directory,data,user,false,true); require(user=="CLI user" && !data.empty());
        write("version=invalid\n");
        pcvrstartup::Defaults(directory,data,user,true,true);
        reject([&]{pcvrstartup::Defaults(directory,data,user);});
        write("version=1\ndata=one\ndata=two\nuser=user\n"); reject([&]{pcvrstartup::Defaults(directory,data,user);});
        write("version=1\ndata=\nuser=user\n"); reject([&]{pcvrstartup::Defaults(directory,data,user);});
        write("version=1\ndata="+std::string(33000,'a')); reject([&]{pcvrstartup::Defaults(directory,data,user);});
#ifdef _WIN32
        auto sibling=directory/"fake engine.exe";
        std::filesystem::copy_file(pcvrstartup::ExecutableDirectory()/"desktop-startup-tests.exe",sibling);
        std::string name="parent",flag="--fake-upstate",literal=R"(literal spaces & $() % quotes" \)",option="--game-data",earlier="earlier";
        char *arguments[]={name.data(),flag.data(),literal.data(),option.data(),earlier.data()};
        // Dispatch appends four arguments after the original four.
        require(pcvrstartup::DispatchSibling(sibling,5,arguments,R"(resolved data \)",u8"resolved user é")==23);
#endif
        std::filesystem::remove_all(directory);
        std::cout<<"PCVR executable-relative configuration and dispatch checks passed.\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n'; std::filesystem::remove_all(directory); return 1;}
}
