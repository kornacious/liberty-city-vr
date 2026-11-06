#pragma once

namespace QuestHeadCalibration
{
enum Item { FORWARD, HEIGHT, SIDE, BODY_YAW, LOCK, BONE_VIEW,
	SAVE, RESTORE, FACTORY, RECENTER, BACK, COUNT };
enum { VALUE_COUNT = SAVE };
inline const char *Key(int item)
{
	static const char *keys[]={"XboxEyeForwardCm","XboxEyeHeightCm","XboxEyeSideCm",
		"XboxBodyYawDegrees","XboxKnockdownPositionLock","XboxKnockdownBoneView"};
	return keys[item];
}
inline int Factory(int item)
{
	// The animation head joint is below/behind the eye midpoint. These are
	// starting eye offsets; the overlay permits fitting the retail mesh.
	static const int values[]={6,8,0,-6,1,1};
	return values[item];
}
inline int Limit(int item,int value)
{
	const int lo=item>=LOCK ? 0 : (item==BODY_YAW ? -30 : -20);
	const int hi=item>=LOCK ? 1 : (item==BODY_YAW ? 30 : 20);
	return value<lo ? lo : (value>hi ? hi : value);
}
struct Settings
{
	int value[VALUE_COUNT],saved[VALUE_COUNT];
	Settings(){for(int i=0;i<VALUE_COUNT;++i) value[i]=saved[i]=Factory(i);}
	void Adjust(int item,int direction){
		if(item<0 || item>=VALUE_COUNT) return;
		value[item]=item>=LOCK ? !value[item] : Limit(item,value[item]+direction);
	}
	void Save(){for(int i=0;i<VALUE_COUNT;++i) saved[i]=value[i];}
	void Restore(){for(int i=0;i<VALUE_COUNT;++i) value[i]=saved[i];}
	void Reset(){for(int i=0;i<VALUE_COUNT;++i) value[i]=Factory(i);}
	CVector Offset(const CVector &right,const CVector &up,const CVector &forward) const {
		return (forward*float(value[FORWARD])+up*float(value[HEIGHT])+right*float(value[SIDE]))*0.01f;
	}
};
}
