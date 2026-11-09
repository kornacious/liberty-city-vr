#pragma once
#include "MatchedProfile.h"
#include <array>
#include <filesystem>
#include <vector>

namespace BenchmarkIdentity {
inline uint64_t Add(uint64_t hash,const char *bytes,size_t count) {
    for(size_t i=0;i<count;++i) hash=(hash^static_cast<unsigned char>(bytes[i]))*1099511628211ULL;
    return hash;
}
inline uint64_t File(uint64_t hash,const std::filesystem::path &path) {
    std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("Benchmark fingerprint cannot read a file.");
    std::array<char,65536> buffer{};
    while(in) { in.read(buffer.data(),buffer.size()); hash=Add(hash,buffer.data(),size_t(in.gcount())); }
    if(!in.eof()) throw std::runtime_error("Benchmark fingerprint read failed.");
    return hash;
}
inline std::string Hex(uint64_t value) { std::ostringstream out; out<<std::hex<<value; return out.str(); }
inline std::string Assets(const std::string &root) {
    namespace fs=std::filesystem;
    std::vector<std::pair<std::string,fs::path>> files;
    for(const char *directory : {"data","models","anim","txd","modelsets"}) {
        const auto base=fs::u8path(root)/directory; if(!fs::exists(base)) continue;
        if(fs::is_symlink(fs::symlink_status(base))) throw std::runtime_error("Linked benchmark asset directory.");
        for(const auto &entry:fs::recursive_directory_iterator(base)) {
            if(fs::is_symlink(entry.symlink_status())) throw std::runtime_error("Linked benchmark asset file.");
            if(!entry.is_regular_file()) continue;
            auto relative=entry.path().lexically_relative(fs::u8path(root)).generic_u8string();
            for(char &c:relative) if(c>='A' && c<='Z') c=char(c-'A'+'a');
            files.emplace_back(relative,entry.path());
        }
    }
    std::sort(files.begin(),files.end());
    if(files.empty()) throw std::runtime_error("No benchmark assets found.");
    uint64_t hash=14695981039346656037ULL; std::string previous;
    for(const auto &entry:files) {
        if(entry.first==previous) throw std::runtime_error("Ambiguous benchmark asset case.");
        previous=entry.first;
        const auto header=entry.first+"="+std::to_string(fs::file_size(entry.second))+"\n";
        hash=Add(hash,header.data(),header.size()); hash=File(hash,entry.second);
    }
    return Hex(hash);
}
} // namespace BenchmarkIdentity
