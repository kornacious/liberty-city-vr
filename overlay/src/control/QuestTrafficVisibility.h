#pragma once

// Traffic population uses a full circle in VR, like the renderer's shared list.
// Keep the stock visible spawn ring, including its camera separation check.
namespace QuestTrafficVisibility {
inline float Scale(float generationMultiplier)
{
	return generationMultiplier > 1.0f ? generationMultiplier : 1.0f;
}

inline float SpawnDistance(float generationMultiplier)
{
	return 120.0f * Scale(generationMultiplier);
}

inline float RetentionDistance(float generationMultiplier)
{
	return 130.0f * Scale(generationMultiplier);
}

inline bool CanSpawn(float playerDistance, float cameraDistance, float generationMultiplier)
{
	const float scale = Scale(generationMultiplier);
	return playerDistance >= 110.0f * scale && playerDistance <= 130.0f * scale &&
		cameraDistance >= 90.0f * scale;
}
}
