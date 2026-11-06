#define _CRT_SECURE_NO_WARNINGS
#include "ragdoll-test-vector.h"
#include "VrRagdollPhysics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#define nil nullptr
// Engine surface fixture: matrices use RW's row-vector multiplication order.
struct RwMatrix {
	CVector right{1,0,0},up{0,1,0},at{0,0,1},pos;
};
namespace rw {
struct Matrix {
	static CVector Vector(const RwMatrix &m,CVector v) { return m.right*v.x+m.up*v.y+m.at*v.z; }
	static void mult(RwMatrix *out,const RwMatrix *a,const RwMatrix *b) {
		*out={Vector(*b,a->right),Vector(*b,a->up),Vector(*b,a->at),Vector(*b,a->pos)+b->pos};
	}
	static void invert(RwMatrix *out,const RwMatrix *m) {
		*out={{m->right.x,m->up.x,m->at.x},{m->right.y,m->up.y,m->at.y},{m->right.z,m->up.z,m->at.z},{}};
		out->pos=-Vector(*out,m->pos);
	}
};
struct HAnimHierarchy { enum { PUSH=1,POP=2 }; };
}
enum { BONE_waist,BONE_upperlegr,BONE_lowerlegr,BONE_footr,BONE_upperlegl,BONE_lowerlegl,BONE_footl,
	BONE_mid,BONE_torso,BONE_head,BONE_upperarmr,BONE_lowerarmr,BONE_Rhand,BONE_upperarml,BONE_lowerarml,BONE_Lhand };
static const char *names[]={"Swaist","Supperlegr","Slowerlegr","Sfootr","Supperlegl","Slowerlegl","Sfootl",
	"Smid","Storso","Shead","Supperarmr","Slowerarmr","SRhand","Supperarml","Slowerarml","SLhand"};
static const char *ConvertBoneTag2BoneName(int tag) { return names[tag]; }
struct RwFrame { RwMatrix local,world; RwFrame *parent=nullptr; const char *name=""; std::vector<RwFrame*> children; };
static RwMatrix *RwFrameGetLTM(RwFrame *f) { return &f->world; }
static RwMatrix *RwFrameGetMatrix(RwFrame *f) { return &f->local; }
static RwFrame *RwFrameGetParent(RwFrame *f) { return f->parent; }
static const char *GetFrameNodeName(RwFrame *f) { return f->name; }
static void RwFrameUpdateObjects(RwFrame *f) {
	if(f->parent) rw::Matrix::mult(&f->world,&f->local,&f->parent->world); else f->world=f->local;
	for(auto *child:f->children) RwFrameUpdateObjects(child);
}
struct Node { int id,flags; };
struct RpHAnimHierarchy { int numNodes; Node *nodeInfo; RwMatrix *matrices; };
struct AnimBlendFrameData { RwFrame *frame; };
struct CAnimBlendClumpData { int numFrames; AnimBlendFrameData *frames; };
struct RpClump { RwFrame root; RpHAnimHierarchy *skin=nullptr; CAnimBlendClumpData *data=nullptr; };
struct CPed { RpClump clump; RpClump *GetClump(){return &clump;} };
static bool IsClumpSkinned(RpClump *c) { return c->skin!=nullptr; }
static RpHAnimHierarchy *GetAnimHierarchyFromSkinClump(RpClump *c) { return c->skin; }
static RwMatrix *RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy *h) { return h->matrices; }
static bool RpAnimBlendClumpIsInitialized(RpClump *c) { return c->data!=nullptr; }
#define RPANIMBLENDCLUMPDATA(c) (&(c)->data)
static RwFrame *RpClumpGetFrame(RpClump *c) { return &c->root; }
#include "VrRagdollRig.h"
namespace R=VrRagdollRig;
namespace P=VrRagdollPhysics;
static unsigned checks;
static void Check(bool b,const char *why) { ++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);} }
static bool Near(CVector a,CVector b) { return (a-b).Magnitude()<0.00001f; }
static const CVector positions[]={
	{0,0,1},{.12f,0,.95f},{.12f,0,.5f},{.12f,0,.08f},{-.12f,0,.95f},{-.12f,0,.5f},{-.12f,0,.08f},
	{0,0,1.2f},{0,0,1.45f},{0,0,1.72f},{.22f,0,1.45f},{.48f,0,1.45f},{.70f,0,1.45f},
	{-.22f,0,1.45f},{-.48f,0,1.45f},{-.70f,0,1.45f}
};
static void Validate(const R::Pose &pose,bool missingMid=false) {
	int nodes[P::SIM_BONES];RwMatrix joints[P::SIM_BONES];
	Check(R::Capture(pose,nodes,joints),"GTA III pose admitted");
	for(int i=0;i<P::SIM_BONES;i++) for(int j=i+1;j<P::SIM_BONES;j++)
		Check(nodes[i]<0 || nodes[j]<0 || nodes[i]!=nodes[j],"every real model matrix has exactly one owner");
	Check(nodes[P::RD_NECK]<0&&nodes[P::RD_LCLAV]<0&&nodes[P::RD_RCLAV]<0,"virtual joints never overwrite skin/frame matrices");
	Check((nodes[P::RD_SPINE]<0)==missingMid,"optional Xbox mid bone");
	P::State body{};for(int i=0;i<P::SIM_BONES;i++) body.pos[i]=joints[i].pos;
	P::Initialize(&body,joints[0].right,joints[0].at);
	for(int i=0;i<P::SKELETON_STICKS;i++) Check(body.stickLen[i]>.001f,"no collapsed limb/spine constraints");
}
static void LocalModels() {
	const char *root=std::getenv("GTA3_TEST_GAME_DATA");
	if(!root || !*root) { std::puts("SKIP: optional local GTA III NPC model diagnostics (no game-data path supplied).");return; }
	for(const char *model:{"male01.dff","cop.dff","b_man1.dff"}) {
		std::ifstream directory(std::string(root)+"/models/gta3.dir",std::ios::binary);
		Check(bool(directory),"local GTA III archive directory readable");
		char entry[32];uint32_t sector=0,sectors=0;
		while(directory.read(entry,32)) {
			char name[25]={};std::memcpy(name,entry+8,24);
			if(R::SameName(name,model)){std::memcpy(&sector,entry,4);std::memcpy(&sectors,entry+4,4);break;}
		}
		Check(sectors>0 && sectors<32768,"local NPC model found");
		std::ifstream archive(std::string(root)+"/models/gta3.img",std::ios::binary);
		archive.seekg(std::streamoff(sector)*2048);std::vector<char> bytes(size_t(sectors)*2048);
		Check(bool(archive.read(bytes.data(),std::streamsize(bytes.size()))),"local NPC bytes read without exporting assets");
		auto u32=[&](size_t offset){Check(offset+4<=bytes.size(),"DFF read in range");uint32_t v;std::memcpy(&v,bytes.data()+offset,4);return v;};
		Check(u32(0)==16,"NPC DFF starts with clump");
		size_t framesAt=0,framesEnd=0;
		for(size_t at=12;at+12<=bytes.size() && at<12+u32(4);at+=12+u32(at+4))
			if(u32(at)==14){framesAt=at+12;framesEnd=framesAt+u32(at+4);break;}
		Check(framesAt>0 && framesEnd<=bytes.size(),"NPC frame list located");
		Check(u32(framesAt)==1,"NPC frame-list structure");
		const unsigned count=u32(framesAt+12);Check(count>1 && count<=40,"bounded native NPC frame count");
		RwFrame frames[40];std::string frameNames[40];int parents[40];
		for(unsigned i=0;i<count;i++) {
			size_t at=framesAt+16+size_t(i)*56;Check(at+56<=framesEnd,"NPC matrix in frame list");
			std::memcpy(&frames[i].local.right,bytes.data()+at,12);
			std::memcpy(&frames[i].local.up,bytes.data()+at+12,12);
			std::memcpy(&frames[i].local.at,bytes.data()+at+24,12);
			std::memcpy(&frames[i].local.pos,bytes.data()+at+36,12);
			parents[i]=int32_t(u32(at+48));
		}
		size_t extension=framesAt+12+u32(framesAt+4);
		for(unsigned i=0;i<count;i++) {
			Check(u32(extension)==3,"one extension per NPC frame");size_t end=extension+12+u32(extension+4);
			for(size_t at=extension+12;at+12<=end;at+=12+u32(at+4)) if(u32(at)==0x253f2fe) {
				size_t size=u32(at+4);Check(at+12+size<=end,"NPC frame name in range");
				frameNames[i]=std::string(bytes.data()+at+12,size);frameNames[i].resize(std::strlen(frameNames[i].c_str()));
			}
			extension=end;frames[i].name=frameNames[i].c_str();
		}
		CPed ped;ped.clump.root.local=frames[0].local;AnimBlendFrameData anim[40];
		for(unsigned i=1;i<count;i++) {
			Check(parents[i]>=0 && parents[i]<int(i),"NPC hierarchy parent precedes child");
			frames[i].parent=parents[i]==0 ? &ped.clump.root : &frames[parents[i]];
			frames[i].parent->children.push_back(&frames[i]);anim[i-1].frame=&frames[i];
		}
		CAnimBlendClumpData data{int(count)-1,anim};ped.clump.data=&data;RwFrameUpdateObjects(&ped.clump.root);
		R::Binding binding;Check(R::Read(&ped,binding),"actual PC NPC frame tree supported");Validate(binding.pose);
		std::printf("PASS: actual local GTA III NPC hierarchy %s.\n",model);
	}
}
int main() {
	CPed ped;RwFrame frames[16];AnimBlendFrameData anim[16];CAnimBlendClumpData data{16,anim};ped.clump.data=&data;
	ped.clump.root.local={{0,1,0},{-1,0,0},{0,0,1},{31,-12,4}};
	for(int i=0;i<16;i++) {
		frames[i].name=names[i];frames[i].parent=i ? &frames[0] : &ped.clump.root;
		frames[i].local.pos=positions[i]-(i ? positions[0] : CVector());
		frames[i].parent->children.push_back(&frames[i]);anim[i].frame=&frames[i];
	}
	frames[BONE_lowerarml].name="slowerarml"; // original PC assets mix case
	RwFrameUpdateObjects(&ped.clump.root);
	R::Binding rigid;Check(R::Read(&ped,rigid),"read original unskinned PC frame hierarchy");Validate(rigid.pose);
	RwMatrix target[16];for(int i=0;i<16;i++){target[i]=rigid.pose.world[i];target[i].pos+=CVector(2,-1,-.4f);}
	const RwMatrix rootBefore=ped.clump.root.local;
	R::Write(&ped,rigid,target);
	Check(Near(rootBefore.pos,ped.clump.root.local.pos),"entity root remains untouched");
	for(int i=0;i<16;i++) Check(Near(frames[i].world.pos,target[i].pos),"rigid world/local conversion uses solved parents under rotated root");
	Node nodes[16];RwMatrix matrices[16];for(int i=0;i<16;i++){nodes[i]={i,0};matrices[i]=rigid.pose.world[i];}
	RpHAnimHierarchy skin{16,nodes,matrices};ped.clump.skin=&skin;
	R::Binding skinned;Check(R::Read(&ped,skinned),"read Xbox/Mobile skin hierarchy");Validate(skinned.pose);
	R::Write(&ped,skinned,target);for(int i=0;i<16;i++) Check(Near(matrices[i].pos,target[i].pos),"skin writes preserve world coordinates");
	nodes[BONE_mid].id=99;R::Binding xbox;Check(R::Read(&ped,xbox),"read skin without optional Smid");Validate(xbox.pose,true);
	nodes[BONE_head].id=99;R::Binding unsupported;Check(R::Read(&ped,unsupported),"read unsupported rig metadata");
	int mapping[P::SIM_BONES];RwMatrix joints[P::SIM_BONES];Check(!R::Capture(unsupported.pose,mapping,joints),"missing required bone rejects safely");
	R::Pose invalid=rigid.pose;invalid.world[BONE_head].pos=invalid.world[BONE_torso].pos;Check(!R::Capture(invalid,mapping,joints),"collapsed head cannot enter solver");
	invalid=rigid.pose;invalid.world[BONE_head].pos.x=std::numeric_limits<float>::quiet_NaN();Check(!R::Capture(invalid,mapping,joints),"nonfinite pose cannot enter solver");
	skin.numNodes=41;Check(!R::Read(&ped,unsupported),"oversized skin bounded");ped.clump.skin=nullptr;data.numFrames=41;Check(!R::Read(&ped,unsupported),"oversized rigid hierarchy bounded");
	LocalModels();
	std::printf("PASS: GTA III rigid/skinned ragdoll adapter, %u checks.\n",checks);
}
