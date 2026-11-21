#include "../../librw/src/vulkan/StaticInstances.h"
#include <cassert>
#include <vector>
#include <random>
#include <limits>
struct Item {
    StaticInstances::Bounds bounds;
    float position[3]={};
    bool opaque=true;
    int key=0,id=0;
};
static Item Make(int id,int key,float left,float right,float depth,bool opaque=true){
    Item out;out.id=id;out.key=key;out.opaque=opaque;out.bounds.valid=true;
    for(int e=0;e<2;++e){
        out.bounds.left[e]=left;out.bounds.right[e]=right;
        out.bounds.bottom[e]=-.8f;out.bounds.top[e]=.8f;
        out.bounds.depth.nearDepth[e]=out.bounds.depth.farDepth[e]=depth;
    }
    return out;
}
static unsigned Plan(const Item *a,unsigned n,unsigned first,bool *used,unsigned *selected,StaticInstances::Stats &s){
    return StaticInstances::Plan(a,n,first,used,selected,[](const Item &x,const Item &y){return x.key==y.key;},s);
}
static std::vector<unsigned> Reorder(const Item *a,unsigned n){
    bool used[StaticInstances::Window]={};std::vector<unsigned> out;StaticInstances::Stats s;
    for(unsigned i=0;i<n;++i)if(!used[i]){
        unsigned ids[StaticInstances::MaxInstances];const unsigned count=Plan(a,n,i,used,ids,s);
        for(unsigned j=0;j<count;++j){assert(!used[ids[j]]);used[ids[j]]=true;out.push_back(ids[j]);}
    }
    return out;
}
static std::vector<float> Raster(const Item *a,const std::vector<unsigned> &order,int eye){
    std::vector<float> image(128,0),depth(128,1);
    for(unsigned id:order){const auto &d=a[id];
        for(int p=0;p<128;++p){const float x=-1+(p+.5f)/64;
            if(x<d.bounds.left[eye]||x>d.bounds.right[eye])continue;
            const float z=d.bounds.depth.nearDepth[eye];if(z>depth[p])continue;
            const float colour=float(d.id+1);
            if(d.opaque){image[p]=colour;depth[p]=z;}
            else image[p]=.5f*colour+.5f*image[p];
        }
    }
    return image;
}
int main(){
    float model[16]={};
    model[0]=model[5]=model[10]=model[15]=1;
    float planes[2][5][4]={};
    planes[0][0][0]=planes[1][0][0]=1;
    planes[1][0][3]=2;
    const StaticMeshVisibility::Bounds leftOnly={{-1,0,0},{-.5f,1,1},true};
    const StaticMeshVisibility::Bounds outside={{-4,0,0},{-3,1,1},true};
    assert(StaticMeshVisibility::Frusta(planes,model).Visible(leftOnly));
    assert(!StaticMeshVisibility::Frusta(planes,model).Visible(outside));
    model[12]=3;
    assert(StaticMeshVisibility::Frusta(planes,model).Visible(outside));
    model[12]=std::numeric_limits<float>::infinity();
    assert(StaticMeshVisibility::Frusta(planes,model).Visible(outside));
    Item a[StaticInstances::Window];bool used[StaticInstances::Window]={};unsigned selected[StaticInstances::MaxInstances];StaticInstances::Stats s;
    a[0]=Make(0,1,-.8f,.8f,.4f);a[1]=Make(1,2,-.8f,.8f,.5f);a[2]=Make(2,1,-.8f,.8f,.6f);
    assert(Plan(a,3,0,used,selected,s)==2); // separated opaque depth
    a[1].opaque=false;assert(Plan(a,3,0,used,selected,s)==1); // blending barrier
    for(int e=0;e<2;++e){a[1].bounds.left[e]=.9f;a[1].bounds.right[e]=1;}
    assert(Plan(a,3,0,used,selected,s)==2); // alpha is screen-disjoint
    a[1].bounds.left[1]=-.8f;assert(Plan(a,3,0,used,selected,s)==1); // must hold in BOTH eyes
    a[1]=Make(1,2,-.8f,.8f,.6f);assert(Plan(a,3,0,used,selected,s)==1); // depth tie
    a[1].bounds.left[0]=std::numeric_limits<float>::quiet_NaN();
    assert(Plan(a,3,0,used,selected,s)==1);
    a[2].position[0]=33;assert(Plan(a,3,0,used,selected,s)==1);
    a[2].position[0]=0;
    for(unsigned i=0;i<StaticInstances::Window;++i)a[i]=Make(int(i),1,-.8f,.8f,.4f);
    assert(Plan(a,StaticInstances::Window,0,used,selected,s)==StaticInstances::MaxInstances);
    auto order=Reorder(a,StaticInstances::Window);assert(order.size()==StaticInstances::Window);
    for(unsigned i=0;i<order.size();++i)assert(order[i]==i); // tie order preserved within instances
    // Compare the planned sequence against original depth/blend results in
    // both eyes, including random equal-depth and alpha cases.
    std::mt19937 rng(0x7148);
    for(unsigned trial=0;trial<3000;++trial){
        const unsigned count=3+rng()%61;std::vector<unsigned> original;
        for(unsigned i=0;i<count;++i){
            const float left=-1+float(rng()%100)/50;
            a[i]=Make(int(i),int(rng()%5),left,left+.05f+float(rng()%50)/50,.1f+float(rng()%8)/10,rng()%3!=0);
            // Matching keys stand in for identical pipeline/material/lighting;
            // transforms and bounds can differ independently.
            a[i].position[0]=float(rng()%60);
            a[i].bounds.left[1]+=.03f;a[i].bounds.right[1]+=.03f;
            original.push_back(i);
        }
        const auto planned=Reorder(a,count);
        assert(planned.size()==count);
        for(int eye=0;eye<2;++eye)assert(Raster(a,original,eye)==Raster(a,planned,eye));
    }
}
