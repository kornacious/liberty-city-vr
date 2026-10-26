#pragma once
#include "VrKnockdownView.h"

// Reuse knockdown's animation-only position easing without imposing rotation.
// The standing anchor continues to carry locomotion and physical HMD tracking.
class VrCrouchView {
	VrKnockdownView motion;
	unsigned int frame = 0;
	bool frameValid = false;
public:
	void Reset() { motion.Reset(); frameValid = false; }
	CVector Update(bool enabled, bool crouched, bool posed, bool postPhysics,
	               unsigned int currentFrame, float seconds, const CVector &target) {
		if(!enabled){
			Reset();
			return CVector(0,0,0);
		}
		if(postPhysics && (!frameValid || frame != currentFrame)){
			frame = currentFrame;
			frameValid = true;
			// Reach the crouched neck sooner, before the visible body crosses
			// the eye. Keep the existing standing recovery speed.
			const bool following = crouched && posed;
			motion.Update(following, following ? seconds*2.0f : seconds, target, 0, 0, 0);
		}
		return motion.offset;
	}
};
