#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include "../../overlay/src/save/QuestSaveCompatibility.h"
using Bytes=std::vector<uint8_t>;
static int checks;
static void require(bool ok,const char *why) { ++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);} }
static void put(Bytes &v,size_t offset,uint32_t word) { for(unsigned i=0;i<4;++i) v[offset+i]=uint8_t(word>>(8*i)); }
static void appendWord(Bytes &v,uint32_t word) { size_t pos=v.size();v.resize(pos+4);put(v,pos,word); }
static void checksum(Bytes &v) { uint32_t sum=0;for(size_t i=0;i<v.size()-4;++i)sum+=v[i];put(v,v.size()-4,sum); }
static Bytes save(bool upstate) {
 Bytes bytes;uint32_t payload=0,target=upstate?806912:201728,maximum=upstate?220000:55000;
 for(int block=0;block<20;++block) {
  Bytes data(block==0?196:block==2?(upstate?7484:5488):8);
  if(block==0) { put(data,64,upstate?806916:201729);put(data,188,4);data[0]='M';data[2]='I';data[4]='S'; }
  else put(data,0,uint32_t(data.size()-4));
  appendWord(bytes,uint32_t(data.size()));bytes.insert(bytes.end(),data.begin(),data.end());payload+=uint32_t(data.size());
 }
 while(payload<target) {
  uint32_t size=std::min(maximum,target-payload);appendWord(bytes,size);bytes.resize(bytes.size()+size);payload+=size;
 }
 appendWord(bytes,0);checksum(bytes);return bytes;
}
static questsave::Profile inspect(const Bytes &bytes) {
 size_t pos=0;
 return questsave::Inspect([&](uint8_t *out,size_t requested){
  size_t count=std::min(requested,bytes.size()-pos);if(count)memcpy(out,bytes.data()+pos,count);pos+=count;return count;
 });
}
int main(int argc,char **argv) {
 using namespace questsave;
 char text[96];
 for(bool upstate:{false,true}) {
  auto bytes=save(upstate);const auto before=bytes;
  const auto expected=upstate?Profile::Upstate:Profile::Base;
  require(inspect(bytes)==expected,"valid format identified independently of active engine");
  require(bytes==before,"inspection preserves save bytes and mission name");
  require(!DescribeMismatch(expected,upstate,1,text,sizeof(text)),"matching configuration preserves ordinary mission label");
  require(DescribeMismatch(expected,!upstate,8,text,sizeof(text)),"other configuration receives explanation");
  require(std::string(text)==(upstate?"Slot 8 was saved with Upstate Liberty Mod":"Slot 8 was saved with base game."),"label includes exact slot and configuration");
  auto bad=bytes;bad[90]^=1;require(inspect(bad)==Profile::Unknown,"checksum failure remains corrupted");
  bad=bytes;put(bad,68,123456);checksum(bad);require(inspect(bad)==Profile::Unknown,"unknown configuration is not guessed");
  bad=bytes;put(bad,4+188,0xFFFFFFFF);checksum(bad);require(inspect(bad)==Profile::Unknown,"invalid script length rejected");
  bad=bytes;put(bad,0,0xFFFFFFFF);require(inspect(bad)==Profile::Unknown,"overflowing block length rejected");
  bad=bytes;put(bad,0,0);require(inspect(bad)==Profile::Unknown,"zero block rejected");
  bad=bytes;put(bad,196+4+8+4+4,777);checksum(bad);require(inspect(bad)==Profile::Unknown,"foreign garage layout rejected");
  bad=bytes;bad.push_back(0);require(inspect(bad)==Profile::Unknown,"trailing bytes cannot masquerade as foreign save");
  for(size_t length:{size_t(0),size_t(3),size_t(64),size_t(191),bytes.size()-1,bytes.size()-4}) {
   bad=bytes;bad.resize(length);require(inspect(bad)==Profile::Unknown,"truncated saves remain corrupted");
  }
  bad=bytes;bad[71]=2;checksum(bad);require(inspect(bad)==expected,"mission replay flag does not change configuration");
 }
 require(!DescribeMismatch(Profile::Unknown,false,1,text,sizeof(text)),"invalid save does not receive mod label");
 require(!DescribeMismatch(Profile::Base,true,1,nullptr,0),"empty destination is safe");
 require(DescribeMismatch(Profile::Upstate,false,1,text,sizeof(text))&&std::string(text)=="Slot 1 was saved with Upstate Liberty Mod","slot numbering starts at one");
 for(int i=1;i<argc;++i) {
  std::ifstream file(argv[i],std::ios::binary);require(bool(file),"open local save fixture");
  Bytes bytes((std::istreambuf_iterator<char>(file)),{});auto profile=inspect(bytes);
  require(profile!=Profile::Unknown,"existing local save passes format and checksum verification");
  printf("Local save profile: %s\n",profile==Profile::Upstate?"Upstate Liberty":"base game");
 }
 printf("PASS %d save configuration checks\n",checks);
}
