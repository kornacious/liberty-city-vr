#pragma once
#include <math.h>

// Offsets from the ordinary anchor keep locomotion/physics immediate while
// smoothing only animation-induced movement. Updated once, after physics.
class VrKnockdownView {
public:
	CVector offset = CVector(0.0f, 0.0f, 0.0f);
	float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
	bool active = false;
	bool returningHeading = false;
	float previousReturnYaw = 0.0f;

	bool OwnsBody() const {
		// Hands and IK only wait for head height/tilt, never the longer yaw
		// comfort tail. Turning remains available while that tail eases out.
		return active && (offset.MagnitudeSqr() >= 0.000025f ||
			fabsf(pitch) >= 0.001f || fabsf(roll) >= 0.001f);
	}

	void Reset() { *this = VrKnockdownView(); }
	static float Limit(float x, float lo, float hi) {
		return x < lo ? lo : (x > hi ? hi : x);
	}
	static float Wrap(float angle) {
		while(angle > 3.14159265f) angle -= 6.28318531f;
		while(angle < -3.14159265f) angle += 6.28318531f;
		return angle;
	}
	void Update(bool down, float seconds, const CVector &targetOffset,
	            float targetYaw, float targetPitch, float targetRoll,
	            bool gettingUp = false) {
		if(!down && !active) return;
		active = true;
		if(!down || gettingUp){
			// Follow voluntary body turns immediately. Only the remaining
			// imposed yaw offset eases out; a moving target must not keep the
			// camera, hand visibility and IK ownership waiting indefinitely.
			if(returningHeading)
				yaw = Wrap(yaw+Wrap(targetYaw-previousReturnYaw));
			previousReturnYaw = targetYaw;
			returningHeading = true;
		}else returningHeading = false;
		const float dt = Limit(seconds, 0.0f, 0.05f);
		const float blend = 1.0f-expf(-dt/(down ? 0.10f : 0.18f));
		offset += ((down ? targetOffset : CVector(0,0,0))-offset)*blend;
		const float turn = Wrap(targetYaw-yaw);
		// At most 45 degrees/sec of imposed yaw; the HMD remains independent.
		yaw = Wrap(yaw+Limit(turn*blend, -0.78539816f*dt, 0.78539816f*dt));
		pitch += Limit(((down ? targetPitch : 0.0f)-pitch)*blend,
		               -0.52359878f*dt, 0.52359878f*dt);
		roll += Limit(((down ? targetRoll : 0.0f)-roll)*blend,
		              -0.52359878f*dt, 0.52359878f*dt);
		if(!down && offset.MagnitudeSqr() < 0.000025f &&
		   fabsf(Wrap(yaw-targetYaw)) < 0.005f && fabsf(pitch) < 0.001f && fabsf(roll) < 0.001f)
			Reset();
	}
};
