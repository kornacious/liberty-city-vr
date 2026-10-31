#pragma once

// The public cheat source indices belong to core/Pad.cpp. Categories only
// reorder their presentation; activation, toggles and vehicle selection all
// use the original source index.
namespace VrCheatMenu {
enum Category {
	GENERAL, WANTED, WEAPONS, WEATHER, VEHICLE_SELECTOR,
	VEHICLE_EFFECTS, PEDESTRIANS, CATEGORY_COUNT, HIDDEN = 255
};

static const unsigned char kSourceCategories[] = {
	VEHICLE_SELECTOR, // SPAWN NEXT CAR
	VEHICLE_SELECTOR, // SPAWN NEXT BOAT
	GENERAL,          // GOD MODE
	GENERAL,          // RUN WITHOUT LIMITS
	WANTED,           // NEVER WANTED
	VEHICLE_EFFECTS,  // CAR INVINCIBILITY
	WEAPONS,          // WEAPONS
	GENERAL,          // RESTORE HEALTH
	GENERAL,          // RESTORE ARMOUR
	GENERAL,          // ADD 250000 MONEY
	WANTED,           // RAISE WANTED
	WANTED,           // CLEAR WANTED
	VEHICLE_SELECTOR, // SPAWN RHINO
	VEHICLE_EFFECTS,  // BLOW UP ALL CARS
	HIDDEN,           // CHANGE PLAYER PED
	PEDESTRIANS,      // PEDESTRIAN MAYHEM
	PEDESTRIANS,      // EVERYONE ATTACKS PLAYER
	PEDESTRIANS,      // ARMED PEDESTRIANS
	WEATHER,          // SUNNY WEATHER
	WEATHER,          // CLOUDY WEATHER
	WEATHER,          // RAINY WEATHER
	WEATHER,          // FOGGY WEATHER
	WEATHER,          // FAST WEATHER
	WEATHER,          // FASTER GAME
	WEATHER,          // SLOWER GAME
	VEHICLE_EFFECTS,  // WHEELS ONLY
	VEHICLE_EFFECTS,  // FLYING CARS
	VEHICLE_EFFECTS,  // STRONG GRIP
	GENERAL,          // NASTY LIMBS
	WEATHER,          // MORNING (06:00)
	WEATHER,          // NOON (12:00)
	WEATHER,          // EVENING (18:00)
	WEATHER,          // MIDNIGHT (00:00)
	WEATHER,          // FAST FORWARD ONE HOUR
	WEATHER,          // REWIND ONE HOUR
	WEATHER           // STOP MISSION TIMER
};

inline const char *CategoryName(int category)
{
	switch(category){
	case GENERAL: return "GENERAL / PLAYER";
	case WANTED: return "WANTED / POLICE";
	case WEAPONS: return "WEAPONS";
	case WEATHER: return "WEATHER / TIME";
	case VEHICLE_SELECTOR: return "VEHICLE SPAWN";
	case VEHICLE_EFFECTS: return "VEHICLE EFFECTS";
	case PEDESTRIANS: return "PEDESTRIANS";
	default: return "CHEATS";
	}
}

inline int CategoryForSource(int source)
{
	if(source < 0) return -1;
	return source < (int)sizeof(kSourceCategories) ?
		kSourceCategories[source] : GENERAL;
}

inline int Count(int category, int sourceCount)
{
	if(category < 0 || category >= CATEGORY_COUNT) return 0;
	int count = 0;
	for(int source = 0; source < sourceCount; ++source)
		if(CategoryForSource(source) == category) ++count;
	return count;
}

inline int SourceIndex(int category, int item, int sourceCount)
{
	if(category < 0 || category >= CATEGORY_COUNT || item < 0) return -1;
	for(int source = 0; source < sourceCount; ++source)
		if(CategoryForSource(source) == category && item-- == 0)
			return source;
	return -1;
}
}
