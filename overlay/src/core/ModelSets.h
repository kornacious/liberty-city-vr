
#pragma once

#include <stddef.h>

namespace ModelSets
{
enum eModelSet
{
	MODEL_SET_CLASSIC = 0,
	MODEL_SET_MODERN, // Persisted value 1: complete Original Xbox variant.
	MODEL_SET_XBOX, // Legacy saved value and Xbox calibration identity; never selectable.
	MODEL_SET_COUNT
};

// Select a complete PC or Original Xbox variant. Categories share one selection.
// Changes require a restart so loaded RenderWare objects are not replaced.
enum eModelCategory
{
	MODEL_CATEGORY_WORLD = 0,
	MODEL_CATEGORY_VEGETATION,
	MODEL_CATEGORY_VEHICLES,
	MODEL_CATEGORY_PEDS,
	MODEL_CATEGORY_WEAPONS,
	MODEL_CATEGORY_COUNT
};

// The active set is captured before RenderWare and the game assets start
// loading.  Menu changes only update the requested set for the next process.
void InitializeStartup(const char *gameRoot);
const char *ContentRoot();
eModelSet GetActive();
eModelSet GetRequested();
void SetRequested(eModelSet modelSet);
void CycleRequested(int direction);
bool IsModernActive();
bool IsXboxActive();
bool IsOriginalXboxAudioActive();
bool UseXboxParticles();
// Resolves audio only within the complete Xbox variant captured at startup.
bool GetOriginalXboxAudioPath(const char *originalPath, char *resolvedPath, size_t size);
bool IsRestartRequired();
#ifdef __ANDROID__
bool IsAstcTextureAvailable();
bool IsAstcTextureRequested();
bool IsAstcTextureActive();
bool IsTextureRestartRequired();
void ToggleAstcTextures();
#endif
bool IsAvailable(eModelSet modelSet);
const char *GetName(eModelSet modelSet);
// Describes where files are actually read from. "Classic" is a promise made
// by the installation layout, while the base directory can contain any files.
const char *GetSourceName(eModelSet modelSet);

eModelSet GetActiveForCategory(eModelCategory category);
// Retail Xbox uses the existing Xbox geometry calibration; verify fit in VR.
eModelSet GetCalibrationSetForCategory(eModelCategory category);
eModelSet GetRequestedForCategory(eModelCategory category);
void SetRequestedForCategory(eModelCategory category, eModelSet modelSet);
void CycleRequestedCategory(eModelCategory category, int direction);
bool IsCategoryModernActive(eModelCategory category);
bool IsCategoryModernRequested(eModelCategory category);
bool IsCategoryRestartRequired(eModelCategory category);
bool IsCategoryAvailable(eModelCategory category);
const char *GetCategoryName(eModelCategory category);

// Vegetation is intentionally manifest-driven.  Model-name heuristics would
// eventually classify a building or prop incorrectly.  The model-set builder
// emits this manifest from the authored GTA III vegetation dictionaries.
bool HasVegetationManifest();
bool IsVegetationModel(const char *modelName);

// Compatibility helpers used by render/gameplay patches. A complete variant's
// GTA3 archive is image zero; GetModernAssetPath never adds it a second time.
bool GetModernAssetPath(const char *relativePath, char *resolvedPath,
	size_t resolvedPathSize);
bool IsModernAssetPath(const char *path);
bool GetXboxAssetPath(const char *relativePath, char *resolvedPath,
	size_t resolvedPathSize);
bool IsXboxAssetPath(const char *path);
// Streaming directories are first-wins: mod (2), selected source (1), base (0).
int GetArchivePriority(const char *path);

// Resolve retail reads into the selected complete variant; missing content never
// borrows from the other variant. Personal state and mod storage remain shared.
const char *ResolveAssetPath(const char *originalPath, char *resolvedPath,
	size_t resolvedPathSize);
}

