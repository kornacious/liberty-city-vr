#include <cstdio>
#include <cstdlib>
#include <cstdint>
#define nil nullptr
enum { PED_TORSO, PED_MID, PED_HEAD, PED_NODE_MAX };
enum { BONE_torso = 8, BONE_head = 9 };
struct RwMatrix {};
struct RwFrame { RwFrame *parent; };
struct RpHAnimHierarchy { int numNodes; int ids[16]; RwMatrix *matrices; };
struct RpClump { bool skinned; RpHAnimHierarchy *hier; RwFrame *root; };
struct AnimBlendFrameData { RwFrame *frame; };
struct CPed {
    RpClump *m_rwObject;
    AnimBlendFrameData *m_pFrames[PED_NODE_MAX];
    bool vehicle;
    RpClump *GetClump() { return m_rwObject; }
    bool InVehicle() { return vehicle; }
};
static unsigned hierarchyReads, frameReads, checks;
static bool IsClumpSkinned(RpClump *clump) { return clump && clump->skinned; }
static RpHAnimHierarchy *GetAnimHierarchyFromSkinClump(RpClump *clump) { ++hierarchyReads; return clump->hier; }
static RwMatrix *RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy *h) { return h->matrices; }
static int RpHAnimIDGetIndex(RpHAnimHierarchy *h, int id) {
    for(int i=0;i<h->numNodes;i++) if(h->ids[i]==id) return i;
    return -1;
}
static RwFrame *RwFrameGetParent(RwFrame *frame) { ++frameReads; return frame->parent; }
static RwFrame *RpClumpGetFrame(RpClump *clump) { return clump->root; }
static void require(bool value, const char *message) {
    ++checks; if(!value) { std::fprintf(stderr,"FAIL %s\n",message);std::exit(1); }
}
#include "../../overlay/src/peds/QuestPlayerRig.h"
int main() {
    RwFrame root{}, waist{&root}, mid{&waist};
    AnimBlendFrameData rigid{&mid};
    RpClump clump{false,nullptr,&root};
    CPed player{&clump,{nullptr,nullptr,nullptr},false};
    require(!QuestPlayerRig::HasCameraPose(nullptr),"absent player");
    // Every old Classic head/torso/vehicle combination retains its result.
    for(unsigned mask=0;mask<8;mask++) {
        player.m_pFrames[PED_HEAD]=(mask&1)?&rigid:nullptr;
        player.m_pFrames[PED_TORSO]=(mask&2)?&rigid:nullptr;
        player.vehicle=(mask&4)!=0;
        require(QuestPlayerRig::HasCameraPose(&player)==((mask&1)&&((mask&2)||(mask&4))),"Classic policy preserved");
    }
    require(hierarchyReads==0,"Classic never reads skin data");
    RwMatrix matrices[16]; RpHAnimHierarchy hierarchy{16,{},matrices};
    for(int i=0;i<16;i++) hierarchy.ids[i]=i;
    clump.skinned=true; clump.hier=&hierarchy;
    player.m_pFrames[PED_HEAD]=nullptr; player.m_pFrames[PED_TORSO]=nullptr;
    for(int vehicle=0;vehicle<2;vehicle++) {
        player.vehicle=vehicle!=0;
        require(QuestPlayerRig::HasCameraPose(&player),"retail 16-bone Xbox rig anchors on foot and in vehicles without rigid slots");
    }
    // Skin frame data aliases an HAnim keyframe, not an RwFrame. Poison it
    // to catch the transition fallback dereferencing the wrong representation.
    AnimBlendFrameData skinAlias{reinterpret_cast<RwFrame*>(uintptr_t(1))};
    player.m_pFrames[PED_MID]=&skinAlias;
    require(QuestPlayerRig::RigidTransitionWaist(&player)==nullptr,"skinned transition never uses rigid waist");
    require(frameReads==0,"skinned alias never dereferenced");
    hierarchy.ids[BONE_head]=99; require(!QuestPlayerRig::HasCameraPose(&player),"missing skin head rejected");
    hierarchy.ids[BONE_head]=BONE_head; hierarchy.ids[BONE_torso]=99;
    require(!QuestPlayerRig::HasCameraPose(&player),"missing skin torso rejected");
    hierarchy.ids[BONE_torso]=BONE_torso; hierarchy.matrices=nullptr;
    require(!QuestPlayerRig::HasCameraPose(&player),"missing pose matrices rejected");
    clump.hier=nullptr; require(!QuestPlayerRig::HasCameraPose(&player),"missing hierarchy rejected");
    clump.skinned=false; player.m_pFrames[PED_MID]=&rigid;
    require(QuestPlayerRig::RigidTransitionWaist(&player)==&waist,"Classic authored transition waist preserved");
    mid.parent=&root; require(QuestPlayerRig::RigidTransitionWaist(&player)==nullptr,"clump root is not a waist offset");
    player.m_rwObject=nullptr; require(!QuestPlayerRig::HasCameraPose(&player),"unloaded model rejected");
    require(QuestPlayerRig::RigidTransitionWaist(&player)==nullptr,"unloaded transition rejected");
    std::printf("PASS %u production player-rig checks\n",checks);
}
