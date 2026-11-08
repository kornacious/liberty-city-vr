#define _CRT_SECURE_NO_WARNINGS
#include <cstddef>
namespace QuestUpstate {
static const char *Resolve(const char *path, char *, size_t) { return path; }
}
#define LCVR_UPSTATE_ENGINE 1
#define main ClassicFixtureMain
#include "test-modelset-profiles.cpp"
#undef main

int main()
{
 using namespace ModelSets;
 auto root=std::filesystem::temp_directory_path()/("lcvr-upstate-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 touch(root/"modelsets/modern/models/gta3.img");
 touch(root/"modelsets/modern/models/gta3.dir");
 touch(root/"modelsets/modern/BUILD_INFO.txt","Source=ORIGINAL_XBOX_US_1_0\n");
 settings={{"ModelSet",1}};reset(root);
 require(IsSourceLocked(),"Upstate advertises source restriction");
 require(GetActive()==MODEL_SET_CLASSIC,"Upstate runs with Classic assets");
 require(GetRequested()==MODEL_SET_MODERN,"Xbox preference survives Upstate startup");
 require(settings["ModelSet"]==1&&writes==0,"startup preserves saved Xbox preference");
 require(!IsRestartRequired(),"locked source does not request a pointless restart");
 SetRequested(MODEL_SET_CLASSIC);CycleRequested(1);CycleRequested(-1);
 require(GetRequested()==MODEL_SET_MODERN&&settings["ModelSet"]==1&&writes==0,"locked controls preserve unmodded preference");
 reset(root);
 require(GetRequested()==MODEL_SET_MODERN&&GetActive()==MODEL_SET_CLASSIC,"relaunch preserves preference and restriction");
 settings={{"ModelSet",2}};reset(root);
 require(GetRequested()==MODEL_SET_MODERN&&GetActive()==MODEL_SET_CLASSIC,"legacy Xbox preference migrates without activating it");
 require(settings["ModelSet"]==1&&writes==1,"legacy preference saved canonically");
 std::filesystem::remove_all(root);
 printf("PASS %d Upstate source preference checks\n",checks);
}
