#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstring>
#include <vector>
#include "../../overlay/src/core/QuestUpstatePackage.h"
// Isolate engine initialization; use production helpers at the overlay boundary.
namespace QuestUpstate {
static std::string root;
static const char *Resolve(const char *path, char *out, size_t size) {
 if(root.empty()) return path;
 const char *relative=questpackage::RelativeFile(root,root,path);
 if(!relative) return path;
 std::string resolved=root+"/mods/upstate-liberty/"+relative;
 if(resolved.size()+1>size) return path;
 memcpy(out,resolved.c_str(),resolved.size()+1);return out;
}
static bool IsAssetPath(const char *path) { return questpackage::IsPackageAssetPath(root,path); }
}
#define LCVR_UPSTATE_ENGINE 1
#define main ClassicFixtureMain
#include "test-modelset-profiles.cpp"
#undef main

int main()
{
 // Both engine variants retain the full source/audio/calibration contract.
 ClassicFixtureMain();
 using namespace ModelSets;
 auto root=std::filesystem::temp_directory_path()/("lcvr-upstate-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 QuestUpstate::root=root.generic_string();
 variant(root,0);variant(root,1);
 settings={{"ModelSet",1}};reset(root);
 require(GetActive()==MODEL_SET_MODERN&&GetRequested()==MODEL_SET_MODERN,"Upstate honors saved Xbox source");
 require(GetCalibrationSetForCategory(MODEL_CATEGORY_PEDS)==MODEL_SET_XBOX,"Xbox rig calibration stays active");
 require(writes==0&&!IsRestartRequired(),"startup preserves source without a restart loop");
 char out[1024];
 for(const char *name:{"DATA\\MAPS\\LANDNE\\LANDNE.IPL ","data/maps/landsw/landsw.ipl","data/gta3.dat","data/main.scm","data/maps/UL/ul.img","data/waterpro.dat"})
  require(QuestUpstate::IsAssetPath(ResolveAssetPath(name,out,sizeof(out))),"Upstate loose files outrank selected source");
 for(const char *name:{"data/maps/generic.ide","data/maps/generic.ide "," \tDATA\\MAPS\\GENERIC.IDE\r\n","models/coll/generic.col"})
  require(IsModernAssetPath(ResolveAssetPath(name,out,sizeof(out))),"remaining definitions and collisions use Xbox, including DAT whitespace");
 const char *base="data/maps/comse/missing.col";
 require(std::string(ResolveAssetPath(base,out,sizeof(out))).find("variants/xbox/")!=std::string::npos,"missing Xbox files remain scoped to Xbox base");
 const char *save="userfiles/upstate-liberty-1.1.2/GTA3sf1.b";
 require(ResolveAssetPath(save,out,sizeof(out))==save,"save paths never become asset overrides");
 SetRequested(MODEL_SET_CLASSIC);
 require(GetActive()==MODEL_SET_MODERN&&IsRestartRequired()&&settings["ModelSet"]==0,"selection saves without hot-swapping live objects");
 reset(root);
 require(GetActive()==MODEL_SET_CLASSIC&&!IsRestartRequired(),"Classic source takes effect on restart");
 require(QuestUpstate::IsAssetPath(ResolveAssetPath("data/main.scm",out,sizeof(out))),"Classic source retains Upstate script");
 CycleRequested(1);reset(root);
 require(GetActive()==MODEL_SET_MODERN&&!IsRestartRequired(),"Xbox source can be re-enabled with Upstate active");

 std::string mod=QuestUpstate::root+"/mods/upstate-liberty/data/maps/UL/ul.dir";
 std::string xbox=(root/"variants/xbox/models/gta3.dir").generic_string();
 const std::string baseDir="models/gta3.dir";
 require(GetArchivePriority(mod.c_str())==2&&GetArchivePriority(xbox.c_str())==1&&GetArchivePriority(baseDir.c_str())==0,"archive layers identify mod, Xbox and base");
 require(GetArchivePriority((QuestUpstate::root+"/mods/upstate-liberty-other/ul.dir").c_str())==0,"similar package prefix is not a mod archive");
 require(!QuestUpstate::IsAssetPath((QuestUpstate::root+"/mods/upstate-liberty/../other.img").c_str()),"traversal never gains mod priority");
 require(!QuestUpstate::IsAssetPath(nullptr),"null path has no mod priority");
 // The production visitor and selection predicate in both registration orders.
 // emplace models the native streaming table's first-entry contract.
 for(bool classicSource:{false,true}) {
  SetRequested(classicSource?MODEL_SET_CLASSIC:MODEL_SET_MODERN);reset(root);
  for(bool modRegisteredLast:{false,true}) {
  std::vector<std::string> images={baseDir,modRegisteredLast?xbox:mod,modRegisteredLast?mod:xbox};
  std::map<std::string,int> winners;
  const std::vector<std::string> baseEntries={"building.dff","building.txd","shared.txd","baseonly.txd","3d8ball.dff"};
  const std::vector<std::string> xboxEntries={"building.dff","building.txd","shared.txd","3d8ball.dff"};
  const std::vector<std::string> modEntries={"building.dff","shared.txd","ulnew.dff"};
  QuestVisitStreamingImages(int(images.size()),[&](int i){return GetArchivePriority(images[i].c_str());},[&](int i){
   int layer=GetArchivePriority(images[i].c_str());
   const auto &entries=layer==2?modEntries:layer==1?xboxEntries:baseEntries;
   for(const auto &name:entries) if(QuestOverlayEntrySelected(images[i].c_str(),name.c_str())) winners.emplace(name,layer);
  });
  require(winners.at("building.dff")==2&&winners.at("shared.txd")==2&&winners.at("ulnew.dff")==2,"mod models and shared textures win in any registration order");
  const int source=classicSource?0:1;
  require(winners.at("building.txd")==source&&winners.at("3d8ball.dff")==source,"textures and cutscene extras follow selected base source");
  require(winners.at("baseonly.txd")==0,"base-only textures remain available");
  }
 }
 std::vector<int> order;
 QuestVisitStreamingImages(3,[](int){return 0;},[&](int i){order.push_back(i);});
 require(order==std::vector<int>({2,1,0}),"same-layer priority retains native reverse order");
 order.clear();QuestVisitStreamingImages(0,[](int){return 0;},[&](int i){order.push_back(i);});
 require(order.empty(),"empty archive list is safe");
 SetRequested(MODEL_SET_MODERN);
 std::filesystem::remove(root/"variants/xbox/models/gta3.dir");reset(root);
 require(GetActive()==MODEL_SET_CLASSIC,"incomplete Xbox source falls back while retaining Upstate");
 require(QuestUpstate::IsAssetPath(ResolveAssetPath("data/main.scm",out,sizeof(out))),"mod remains active after Xbox fallback");
 std::filesystem::remove_all(root);
 printf("PASS %d production Upstate/Xbox routing and priority checks\n",checks);
}
