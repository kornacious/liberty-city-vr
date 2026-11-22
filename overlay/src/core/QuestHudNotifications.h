#pragma once

namespace androidgame {

enum class HudNotification {
	AreaName,
	VehicleName,
	MissionTitle,
	MissionPassed,
	Tips,
	RadioStation,
	Count
};

bool VrHudNotificationEnabled(HudNotification notification);

}
