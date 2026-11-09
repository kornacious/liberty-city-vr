#include "XrMath.h"
#include "VulkanPolicy.h"
#include "VulkanPlatform.h"
#include "MirrorPolicy.h"
#include "MatchedProfile.h"
#include "BenchmarkIdentity.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value, const char *message) { if(!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a-b) < 0.0001f; }
int main() {
    try {
        MatchedProfile::Values profile{{"schema",1},{"quest3",1},{"width",1872},{"height",2016},{"scale",100},{"hz",72},
            {"anisotropy",1},{"atomics",1},{"bc",0},{"astc",1},{"etc2",1},{"max_anisotropy",16},
            {"setting_MsaaSamples",4},{"setting_RenderScalePercent",100}};
        for(int eye=0;eye<2;++eye) {
            const auto prefix="eye"+std::to_string(eye)+"_";
            for(const char *part:{"px","py","pz","qx","qy","qz"}) profile[prefix+part]=0;
            profile[prefix+"px"]=eye?.032:-.032;profile[prefix+"qw"]=1;
            profile[prefix+"left"]=-.8;profile[prefix+"right"]=.8;profile[prefix+"up"]=.9;profile[prefix+"down"]=-.9;
        }
        auto parsed=[&](const std::string &text){std::istringstream input(text);return MatchedProfile::Parse(input);};
        const auto valid=MatchedProfile::Text(profile);
        require(parsed(valid)==profile,"Quest profile round trip");
        for(const auto &bad:{valid+"width=1000\n",valid+"personal_device=123\n",std::string("schema=nan\n")}) {
            bool failed=false;try { parsed(bad); } catch(...) { failed=true; } require(failed,"invalid/private profile rejected");
        }
        auto invalid=profile;invalid["eye0_qw"]=0;
        bool failed=false;try { parsed(MatchedProfile::Text(invalid)); } catch(...) { failed=true; }
        require(failed,"invalid profile stereo rejected");
        MatchedProfile::requested=profile;MatchedProfile::active=true;
        require(MatchedProfile::Read("VR","MsaaSamples",1)==4,"profile overrides settings in memory");
        require(!MatchedProfile::Write("VR","MsaaSamples","2") && MatchedProfile::dirty,"profile changes invalidate matching and never persist");
        MatchedProfile::active=false;MatchedProfile::dirty=false;MatchedProfile::captured.clear();
        require(BenchmarkIdentity::Hex(MatchedProfile::Hash("hello"))=="a430d84680aabd0b","stable workload fingerprint known vector");
        auto square=Desktop::FitPicture(2496,2688,1920,1080,Desktop::Framing::Square);
        require(square.width==1080 && square.height==1080 && square.x==420 && square.y==0,"square preview letterboxing");
        auto small=Desktop::FitPicture(640,480,1920,1080,Desktop::Framing::Cropped);
        require(small.width==640 && small.height==360 && small.x==640 && small.y==360,"preview never upscales");
        auto crop=Desktop::FitPicture(2496,2688,1920,1080,Desktop::Framing::Cropped);
        require(crop.width==1920 && crop.height==1080 && crop.dv<1 && crop.du==1,"crop covers larger target");
        require(Desktop::FitPicture(1,1,0,0,Desktop::Framing::Square).width==0,"minimized mirror");
        auto theater=Desktop::FitTheater(1872,2016,960,960,16.f/9,Desktop::Framing::Square);
        require(theater.width==960 && theater.height==540 && theater.y==210 && theater.du==1,"theater menus retain aspect and complete contents");
        require(Desktop::Deadzone(100,7849)==0 && Desktop::Deadzone(-32768,7849)==-1,"desktop gamepad deadzone and saturation");
        XrPosef pose{{0, std::sin(0.4f), 0, std::cos(0.4f)}, {1,2,-3}};
        auto inverse = XrMath::multiply(XrMath::viewFromPose(pose), XrMath::worldFromPose(pose));
        for(int i = 0; i < 16; ++i) require(near(inverse.m[i], i%5 == 0 ? 1.0f : 0.0f), "pose/view inverse");
        XrFovf fov{-0.6f,0.9f,0.8f,-0.7f};
        auto projection = XrMath::projectionFromFov(fov, 0.1f, 100.0f);
        auto clip = [&](float x, float y, float z, int component) {
            return (projection.m[component]*x + projection.m[4+component]*y + projection.m[8+component]*z + projection.m[12+component]) / -z;
        };
        require(near(clip(std::tan(fov.angleLeft),0,-1,0),-1), "left FOV edge");
        require(near(clip(std::tan(fov.angleRight),0,-1,0),1), "right FOV edge");
        require(near(clip(0,std::tan(fov.angleUp),-1,1),-1), "Vulkan Y flip");
        require(near(clip(0,std::tan(fov.angleDown),-1,1),1), "bottom FOV edge");
        require(near(clip(0,0,-0.1f,2),0) && near(clip(0,0,-100,2),1), "Vulkan depth range");
        projection = XrMath::projectionFromFov(fov,0.1f,0);
        require(near(projection.m[10],-1) && near(projection.m[14],-0.1f), "infinite far plane");
        require(XrVulkanPolicy::supportsVulkan11(XR_MAKE_VERSION(1,0,0),XR_MAKE_VERSION(1,3,0),VK_API_VERSION_1_1), "API negotiation");
        require(!XrVulkanPolicy::supportsVulkan11(XR_MAKE_VERSION(1,2,0),XR_MAKE_VERSION(1,3,0),VK_API_VERSION_1_1), "runtime minimum");
        require(!XrVulkanPolicy::supportsVulkan11(XR_MAKE_VERSION(1,0,0),XR_MAKE_VERSION(1,0,0),VK_API_VERSION_1_1), "runtime maximum");
        require(!XrVulkanPolicy::supportsVulkan11(XR_MAKE_VERSION(1,0,0),XR_MAKE_VERSION(1,3,0),VK_API_VERSION_1_0), "device version");
        VkQueueFamilyProperties queues[3]{}; queues[0].queueFlags=VK_QUEUE_GRAPHICS_BIT;
        queues[1].queueFlags=VK_QUEUE_TRANSFER_BIT; queues[1].queueCount=1;
        queues[2].queueFlags=VK_QUEUE_GRAPHICS_BIT; queues[2].queueCount=1;
        require(XrVulkanPolicy::graphicsQueue(queues,3)==2, "nonempty graphics queue");
        require(XrVulkanPolicy::graphicsQueue(queues,2)==UINT32_MAX, "missing graphics queue");
        VkPhysicalDeviceFeatures available{}; available.textureCompressionBC=VK_TRUE;
        auto features=XrVulkanPolicy::optionalFeatures(available);
        require(features.textureCompressionBC && !features.textureCompressionASTC_LDR && !features.samplerAnisotropy,
                "desktop must not require unsupported Quest features");
        auto start=VulkanPlatform::microseconds();
        require(VulkanPlatform::microseconds()>=start, "monotonic renderer timer");
        std::cout << "PCVR core checks passed.\n"; return 0;
    } catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
