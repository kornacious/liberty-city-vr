#pragma once
#include <cmath>

// Synthetic render poses only. Retain the runtime poses for compositor
// submission and the head-tracked overlay. Independent of OpenXR for host tests.
namespace ReplayBenchmark {
template<class View> class ForwardView {
    unsigned long long run = 0;
    View fixed[2]{};
    static void Product(const float a[4], const float b[4], float out[4]) {
        out[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];
        out[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
        out[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];
        out[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];
    }
    static void Rotate(const float q[4], const float v[3], float out[3]) {
        const float p[4]={v[0],v[1],v[2],0}, inverse[4]={-q[0],-q[1],-q[2],q[3]};
        float a[4],b[4];Product(q,p,a);Product(a,inverse,b);
        for(int i=0;i<3;++i)out[i]=b[i];
    }
public:
    void Reset(){run=0;}
    bool Apply(const View tracked[2], View output[2], unsigned long long token,
               const float neutral[3], float yaw) {
        if(!token){Reset();return false;}
        if(run!=token){
            Reset();
            if(!std::isfinite(yaw))return false;
            for(int i=0;i<3;++i)if(!std::isfinite(neutral[i]))return false;
            float q[2][4],center[3],ipdSquared=0;
            for(int eye=0;eye<2;++eye){
                const auto &p=tracked[eye].pose;
                q[eye][0]=p.orientation.x;q[eye][1]=p.orientation.y;
                q[eye][2]=p.orientation.z;q[eye][3]=p.orientation.w;
                float length=0;
                for(float v:q[eye]){if(!std::isfinite(v))return false;length+=v*v;}
                if(length<0.5f || length>1.5f)return false;
                for(float &v:q[eye])v/=std::sqrt(length);
                if(!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||
                   !std::isfinite(p.position.z))return false;
            }
            const float pos[2][3]={
                {tracked[0].pose.position.x,tracked[0].pose.position.y,tracked[0].pose.position.z},
                {tracked[1].pose.position.x,tracked[1].pose.position.y,tracked[1].pose.position.z}};
            for(int i=0;i<3;++i){center[i]=(pos[0][i]+pos[1][i])*0.5f;
                const float d=pos[1][i]-pos[0][i];ipdSquared+=d*d;}
            if(ipdSquared<0.02f*0.02f || ipdSquared>0.12f*0.12f)return false;
            const float inverse[4]={-q[0][0],-q[0][1],-q[0][2],q[0][3]};
            const float forward[4]={0,std::sin(yaw*0.5f),0,std::cos(yaw*0.5f)};
            for(int eye=0;eye<2;++eye){
                fixed[eye]=tracked[eye]; // Retain per-eye FOV and stereo cant.
                float delta[3],local[3],offset[3],relative[4],orientation[4];
                for(int i=0;i<3;++i)delta[i]=pos[eye][i]-center[i];
                Rotate(inverse,delta,local);Rotate(forward,local,offset);
                Product(inverse,q[eye],relative);Product(forward,relative,orientation);
                auto &p=fixed[eye].pose;
                p.position.x=neutral[0]+offset[0];p.position.y=neutral[1]+offset[1];p.position.z=neutral[2]+offset[2];
                p.orientation.x=orientation[0];p.orientation.y=orientation[1];
                p.orientation.z=orientation[2];p.orientation.w=orientation[3];
            }
            run=token;
        }
        output[0]=fixed[0];output[1]=fixed[1];return true;
    }
};
}
