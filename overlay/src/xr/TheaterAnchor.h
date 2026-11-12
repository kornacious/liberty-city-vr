#pragma once
#include <openxr/openxr.h>
#include <cmath>

namespace XrMath {
// Startup runtimes may supply orientation before a tracked head position.
// Keep the provisional screen in VIEW space until a room anchor is reliable.
struct TheaterAnchor {
    bool valid = false;
    XrSpace space = XR_NULL_HANDLE;
    XrPosef pose{};

    void update(XrSpace base, const XrView *views, XrViewStateFlags flags, bool focused) {
        if(space != base) valid = false;
        const XrViewStateFlags tracked = XR_VIEW_STATE_POSITION_VALID_BIT |
            XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_TRACKED_BIT |
            XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
        if(valid || !focused || (flags & tracked) != tracked) return;

        const auto &q = views[0].pose.orientation;
        const float fx = -(2.0f*(q.x*q.z + q.y*q.w));
        const float fz = -(1.0f - 2.0f*(q.x*q.x + q.y*q.y));
        const float yaw = std::atan2(-fx, -fz);
        pose = {};
        pose.orientation.y = std::sin(yaw*0.5f);
        pose.orientation.w = std::cos(yaw*0.5f);
        pose.position.x = (views[0].pose.position.x + views[1].pose.position.x)*0.5f - std::sin(yaw)*2.0f;
        pose.position.y = (views[0].pose.position.y + views[1].pose.position.y)*0.5f;
        pose.position.z = (views[0].pose.position.z + views[1].pose.position.z)*0.5f - std::cos(yaw)*2.0f;
        space = base;
        valid = true;
    }

    XrSpace layerSpace(XrSpace viewSpace) const { return valid ? space : viewSpace; }
    XrPosef layerPose() const {
        return valid ? pose : XrPosef{{0,0,0,1}, {0,0,-2}};
    }
};
}
