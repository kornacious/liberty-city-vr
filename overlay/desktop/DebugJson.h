#pragma once
// Small bounded JSON value reader/writer for local viewer documents. No game
// types or platform dependencies: automation files can be validated in tests.
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace DebugJson {
struct Value {
    enum Type { Null, Number, Boolean, String, Array, Object } type=Null;
    double number=0; bool boolean=false; std::string string;
    std::vector<Value> array; std::map<std::string,Value> object;
    Value()=default;
    Value(double n):type(Number),number(n) {}
    Value(int n):Value(double(n)) {}
    Value(bool b):type(Boolean),boolean(b) {}
    Value(const std::string &s):type(String),string(s) {}
    Value(const char *s):Value(std::string(s)) {}
    static Value List() {Value v;v.type=Array;return v;}
    static Value Map() {Value v;v.type=Object;return v;}
    const Value &At(const std::string &key) const {
        if(type!=Object || !object.count(key))throw std::runtime_error("Missing JSON field: "+key);
        return object.at(key);
    }
    double Num() const {if(type!=Number || !std::isfinite(number))throw std::runtime_error("Expected finite JSON number");return number;}
    int Int(int low,int high) const {double n=Num();if(n!=std::floor(n)||n<low||n>high)throw std::runtime_error("JSON integer out of range");return int(n);}
    bool Bool() const {if(type!=Boolean)throw std::runtime_error("Expected JSON boolean");return boolean;}
    const std::string &Str() const {if(type!=String)throw std::runtime_error("Expected JSON string");return string;}
};
class Reader {
    const std::string &s; size_t p=0;
    [[noreturn]] void Fail() const {throw std::runtime_error("Invalid JSON at byte "+std::to_string(p));}
    void Space(){while(p<s.size() && (s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
    bool Eat(char c){Space();if(p<s.size()&&s[p]==c){++p;return true;}return false;}
    unsigned Hex(){unsigned n=0;for(int i=0;i<4;++i){if(p==s.size())Fail();char c=s[p++];n<<=4;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else Fail();}return n;}
    void Utf8(std::string &r,unsigned c){if(c<128)r+=char(c);else if(c<2048){r+=char(192|(c>>6));r+=char(128|(c&63));}else if(c<65536){r+=char(224|(c>>12));r+=char(128|((c>>6)&63));r+=char(128|(c&63));}else{r+=char(240|(c>>18));r+=char(128|((c>>12)&63));r+=char(128|((c>>6)&63));r+=char(128|(c&63));}}
    std::string Text(){
        if(!Eat('"'))Fail();std::string r;
        while(p<s.size()) {unsigned char c=s[p++];if(c=='"')return r;if(c<32)Fail();
            if(c!='\\'){r+=char(c);continue;}if(p==s.size())Fail();c=s[p++];
            switch(c){case '"':case '\\':case '/':r+=char(c);break;case 'b':r+='\b';break;case 'f':r+='\f';break;case 'n':r+='\n';break;case 'r':r+='\r';break;case 't':r+='\t';break;
            case 'u':{unsigned u=Hex();if(u>=0xd800&&u<=0xdbff){if(p+2>s.size()||s[p++]!='\\'||s[p++]!='u')Fail();unsigned lo=Hex();if(lo<0xdc00||lo>0xdfff)Fail();u=65536+((u-0xd800)<<10)+(lo-0xdc00);}else if(u>=0xdc00&&u<=0xdfff)Fail();Utf8(r,u);break;}default:Fail();}
        }Fail();
    }
    Value Read(unsigned depth){
        if(depth>32)Fail();Space();if(p==s.size())Fail();
        if(s[p]=='"')return Value(Text());
        if(Eat('{')){Value v=Value::Map();if(Eat('}'))return v;do{auto k=Text();if(!Eat(':')||v.object.count(k))Fail();v.object.emplace(k,Read(depth+1));}while(Eat(','));if(!Eat('}'))Fail();return v;}
        if(Eat('[')){Value v=Value::List();if(Eat(']'))return v;do{v.array.push_back(Read(depth+1));}while(Eat(','));if(!Eat(']'))Fail();return v;}
        for(const auto &lit: {std::string("true"),std::string("false"),std::string("null")})if(s.compare(p,lit.size(),lit)==0){p+=lit.size();return lit=="null"?Value():Value(lit=="true");}
        size_t start=p;if(s[p]=='-')++p;if(p==s.size())Fail();
        if(s[p]=='0')++p;else{if(s[p]<'1'||s[p]>'9')Fail();while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
        if(p<s.size()&&s[p]=='.'){++p;size_t a=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==a)Fail();}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t a=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==a)Fail();}
        double n=std::stod(s.substr(start,p-start));if(!std::isfinite(n))Fail();return Value(n);
    }
public:
    explicit Reader(const std::string &text):s(text){if(s.size()>8*1024*1024)throw std::runtime_error("JSON exceeds 8 MiB");}
    Value Parse(){Value v=Read(0);Space();if(p!=s.size())Fail();return v;}
};
inline std::string Quote(const std::string &s){std::ostringstream o;o<<'"';for(unsigned char c:s){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default:if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else o<<char(c);}}return o.str()+'"';}
inline std::string Dump(const Value &v){
    switch(v.type){case Value::Null:return "null";case Value::Boolean:return v.boolean?"true":"false";case Value::String:return Quote(v.string);
    case Value::Number:{if(!std::isfinite(v.number))throw std::runtime_error("Nonfinite JSON output");std::ostringstream o;o<<std::setprecision(17)<<v.number;return o.str();}
    case Value::Array:{std::string r="[";for(const auto &x:v.array){if(r.size()>1)r+=',';r+=Dump(x);}return r+']';}
    case Value::Object:{std::string r="{";for(const auto &x:v.object){if(r.size()>1)r+=',';r+=Quote(x.first)+':'+Dump(x.second);}return r+'}';}}
    throw std::runtime_error("Invalid JSON value");
}
inline Value Load(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open JSON document");std::string s;char block[4096];while(in){in.read(block,sizeof(block));s.append(block,size_t(in.gcount()));if(s.size()>8*1024*1024)throw std::runtime_error("JSON exceeds 8 MiB");}return Reader(s).Parse();}
inline void Save(const std::filesystem::path &path,const Value &v){auto temp=path;temp+=".partial";{std::ofstream o(temp,std::ios::binary|std::ios::trunc);o<<Dump(v)<<'\n';o.close();if(!o)throw std::runtime_error("Cannot write JSON document");}std::error_code ec;std::filesystem::rename(temp,path,ec);if(ec){ // Windows replacement is explicit; callers only save viewer-owned files.
    if(!std::filesystem::exists(path))throw std::runtime_error("Cannot publish JSON document");
    std::filesystem::remove(path);std::filesystem::rename(temp,path);
}}
}
