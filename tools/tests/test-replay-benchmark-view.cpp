#include "../../overlay/android/app/src/main/cpp/replay_benchmark_view.h"
#include <cassert>
#include <cstdio>
#include <limits>
struct Vec {float x=0,y=0,z=0;};
struct Quat {float x=0,y=0,z=0,w=1;};
struct View {struct {Quat orientation;Vec position;}pose;float fov[4]={-1,1,1,-1};};
static bool Near(float a,float b){return std::fabs(a-b)<0.00001f;}
static Quat Times(Quat a,Quat b){return {
    a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
    a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
    a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static void Tracked(View v[2],float yaw,float pitch,float roll,float ipd=.064f){
    const Quat q=Times(Times({0,std::sin(yaw/2),0,std::cos(yaw/2)},
        {std::sin(pitch/2),0,0,std::cos(pitch/2)}),
        {0,0,std::sin(roll/2),std::cos(roll/2)});
    // First column of the tracking rotation matrix is the physical right axis.
    const Vec right={1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w)};
    for(int eye=0;eye<2;++eye){
        const float offset=(eye?1:-1)*ipd/2;
        v[eye].pose.position={10+yaw+offset*right.x,1.7f+pitch+offset*right.y,-5+roll+offset*right.z};
        v[eye].pose.orientation=q;
    }
    // A small relative eye cant must survive neutralization.
    v[1].pose.orientation=Times(q,{0,std::sin(.01f),0,std::cos(.01f)});
    v[1].fov[0]=-.9f;
}
int main(){
    ReplayBenchmark::ForwardView<View> lock;
    View tracked[2],out[2],reference[2];const float neutral[3]={1,2,3};
    const float yaw=.7f;
    Tracked(tracked,0,0,0);
    assert(lock.Apply(tracked,reference,1,neutral,yaw));
    assert(Near(reference[0].pose.position.x,1-.032f*std::cos(yaw)));
    assert(Near(reference[0].pose.position.z,3+.032f*std::sin(yaw)));
    assert(Near(reference[1].pose.position.y,2));
    assert(Near(reference[0].pose.orientation.x,0)&&Near(reference[0].pose.orientation.z,0));
    assert(Near(reference[0].pose.orientation.y,std::sin(yaw/2)));
    assert(Near(reference[1].pose.orientation.y,std::sin(yaw/2+.01f)));
    unsigned checks=0;
    for(int y=-12;y<=12;++y)for(int p=-2;p<=2;++p)for(int r=-2;r<=2;++r){
        Tracked(tracked,y*.25f,p*.4f,r*.5f);
        const View original[2]={tracked[0],tracked[1]};
        // Existing run ignores arbitrary subsequent head poses entirely.
        assert(lock.Apply(tracked,out,1,neutral,yaw));
        for(int e=0;e<2;++e){
            assert(out[e].pose.position.x==reference[e].pose.position.x);
            assert(out[e].pose.orientation.y==reference[e].pose.orientation.y);
        }
        // A fresh run must produce the same forward poses even when its first
        // physical head pose is turned, tilted and displaced.
        lock.Reset();assert(lock.Apply(tracked,out,2,neutral,yaw));
        for(int e=0;e<2;++e){
            const auto &a=out[e].pose;const auto &b=reference[e].pose;
            assert(Near(a.position.x,b.position.x)&&Near(a.position.y,b.position.y)&&Near(a.position.z,b.position.z));
            const auto &q=a.orientation;const auto &w=b.orientation;
            assert(Near(std::fabs(q.x*w.x+q.y*w.y+q.z*w.z+q.w*w.w),1));
            for(int i=0;i<4;++i)assert(out[e].fov[i]==original[e].fov[i]);
            assert(tracked[e].pose.position.x==original[e].pose.position.x);
            assert(tracked[e].pose.orientation.w==original[e].pose.orientation.w);
        }
        const Vec a=out[0].pose.position,b=out[1].pose.position;
        assert(Near(std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)),.064f));
        lock.Reset();Tracked(tracked,0,0,0);assert(lock.Apply(tracked,out,1,neutral,yaw));
        ++checks;
    }
    assert(!lock.Apply(tracked,out,0,neutral,yaw));
    Tracked(tracked,1,.2f,.3f,.07f);assert(lock.Apply(tracked,out,3,neutral,0));
    assert(Near(out[1].pose.position.x-out[0].pose.position.x,.07f));
    const float other[3]={3,4,5};assert(lock.Apply(tracked,out,4,other,0));
    assert(Near((out[0].pose.position.x+out[1].pose.position.x)/2,3));
    lock.Reset();tracked[1].pose.position=tracked[0].pose.position;
    assert(!lock.Apply(tracked,out,5,neutral,0));
    Tracked(tracked,0,0,0);tracked[0].pose.orientation.w=std::numeric_limits<float>::quiet_NaN();
    assert(!lock.Apply(tracked,out,5,neutral,0));
    Tracked(tracked,0,0,0);tracked[0].pose.position.x=std::numeric_limits<float>::infinity();
    assert(!lock.Apply(tracked,out,5,neutral,0));
    Tracked(tracked,0,0,0);assert(!lock.Apply(tracked,out,5,neutral,std::numeric_limits<float>::quiet_NaN()));
    std::printf("PASS: %u head pose combinations; forward alignment, stereo separation/cant, unchanged runtime poses/FOV, run reset and invalid-input fallback.\n",checks);
}
