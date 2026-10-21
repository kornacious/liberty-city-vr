#pragma once

namespace VrWeaponCalibrationMenu {
enum Row {
	HAND, STEP, AIM_OFFSET, AIM_ROTATION, WEAPON_OFFSET, WEAPON_ROTATION,
	SUPPORT_OFFSET, SUPPORT_ROTATION, STYLE, LASER, SAVE_DEFAULT,
	RESTORE_DEFAULT, COPY_HAND, FACTORY_DEFAULT, BACK, ROW_COUNT
};
enum { GROUP_COUNT = 6, GROUP_BACK = 5 };
inline int Count(int group) { return group < 0 ? ROW_COUNT : GROUP_BACK+1; }
inline int ValueItem(int group, int row)
{
	return group >= 0 && group < GROUP_COUNT && row >= 2 && row <= 4 ?
		group*3+row-2 : -1;
}
inline int StepSize(bool turbo) { return turbo ? 20 : 1; } // half cm/degrees
inline bool IsRotation(int item)
{
	return (item >= 3 && item <= 5) || (item >= 9 && item <= 11) ||
		(item >= 15 && item <= 17);
}
inline int Adjusted(int value, int item, int delta, int minimum, int maximum)
{
	int next = value+delta;
	if(IsRotation(item)){
		// Both endpoints remain selectable; crossing either continues on the
		// other side of the circle, preserving the full turbo/repeat step.
		while(next > maximum) next -= 720;
		while(next < minimum) next += 720;
		return next;
	}
	return next < minimum ? minimum : next > maximum ? maximum : next;
}
}
