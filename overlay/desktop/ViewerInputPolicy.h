#pragma once
namespace RenderDebug {
// Start a flight gesture in the viewport, then retain it until release. UI
// visibility does not change ownership of an already active gesture.
class HoldCameraCapture {
    bool active=false, previousDown=false;
public:
    bool Update(bool focused,bool enabled,bool rightDown,bool uiOwnsMouse,bool blocked){
        const bool pressed=rightDown&&!previousDown;
        previousDown=rightDown;
        if(!focused||!enabled||!rightDown||blocked)active=false;
        else if(pressed&&!uiOwnsMouse)active=true;
        return active;
    }
};
}
