// The shared game requests session capabilities through this interface. A
// desktop observation session has no XR tracking, haptics or runtime metrics.
// This translation unit deliberately has no OpenXR loader calls or linkage.
#include "xr_vulkan_session.h"
#include "DebugVulkan.h"
namespace RenderDebug {xrvk::GraphicsContext sessionGraphics{};long long sessionTime=0;}
namespace xrvk {
bool create(android_app*){return false;}void destroy(){}bool pollEvents(){return true;}
void triggerHaptic(int,float,float,float){}void setPreferredDisplayRefreshRate(float){}int getSupportedDisplayRefreshRates(float*,int){return 0;}
void setPerformanceMode(int){}int getPerformanceMode(){return PERFORMANCE_MODE_AUTO;}void setGpuPerformanceMode(int){}int getGpuPerformanceMode(){return PERFORMANCE_MODE_AUTO;}int getActiveGpuPerformanceMode(){return -1;}bool isPerformanceModeSupported(){return false;}bool isPerformanceBoostBlocked(){return false;}int getActivePerformanceMode(){return -1;}
bool shouldRender(){return true;}bool hasTrackedGameplayHeadPose(){return false;}void renderFrame(){}
bool getContext(GraphicsContext *out){if(!out)return false;*out=RenderDebug::sessionGraphics;return out->device!=VK_NULL_HANDLE;}
bool writeQuestProfile(const char*){return false;}
bool getRenderScaleStatus(RenderScaleStatus *out){if(!out)return false;*out={};out->requestedPercent=out->selectedPresetPercent=100;out->effectivePercent=100;out->actualWidth=out->recommendedWidth=RenderDebug::sessionGraphics.width;out->actualHeight=out->recommendedHeight=RenderDebug::sessionGraphics.height;return true;}
const char *getRenderScaleFallbackReasonName(int){return "unavailable (desktop viewer)";}void confirmRenderScaleRendererReady(){}
void getInput(ControllerInput *out){if(out)*out={};}void setFrameRenderer(FrameRenderer){}void setTheaterMode(bool){}bool isReplayForwardViewActive(){return false;}long long getPredictedDisplayTimeNs(){return RenderDebug::sessionTime;}
void setDebugOverlay(const unsigned char*,int,int,const unsigned char*){}void setPerformanceMetricsEnabled(bool){}bool getPerformanceMetrics(PerformanceMetrics *out){if(out)*out={};return false;}
}
