#pragma once

// The public cheat source indices belong to core/Pad.cpp. Categories only
// reorder their presentation; activation, toggles and vehicle selection all
// use the original source index.
namespace VrCheatMenu {
enum Category {
	GENERAL, WANTED, WEAPONS, APPEARANCE, WEATHER, VEHICLE_SELECTOR,
	SPECIAL_VEHICLES, VEHICLE_EFFECTS, PEDESTRIANS, CATEGORY_COUNT
};

static const unsigned char kSourceCategories[] = {
	VEHICLE_SELECTOR, // SPAWN NEXT CAR
	VEHICLE_SELECTOR, // SPAWN NEXT BOAT
	GENERAL,          // GOD MODE
	GENERAL,          // RUN WITHOUT LIMITS
	WANTED,           // NEVER WANTED
	WEAPONS,          // WEAPONS
	GENERAL,          // RESTORE HEALTH
	GENERAL,          // RESTORE ARMOUR
	GENERAL,          // ADD 250000 MONEY
	WANTED,           // RAISE WANTED
	WANTED,           // CLEAR WANTED
	SPECIAL_VEHICLES, // SPAWN RHINO
	VEHICLE_EFFECTS,  // BLOW UP ALL CARS
	APPEARANCE,       // CHANGE PLAYER PED
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
	GENERAL           // NASTY LIMBS
};

inline const char *CategoryName(int category)
{
	switch(category){
	case GENERAL: return "GENERAL / PLAYER";
	case WANTED: return "WANTED / POLICE";
	case WEAPONS: return "WEAPONS";
	case APPEARANCE: return "PLAYER APPEARANCE";
	case WEATHER: return "WEATHER / TIME";
	case VEHICLE_SELECTOR: return "VEHICLE SELECTOR";
	case SPECIAL_VEHICLES: return "SPECIAL VEHICLES";
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
