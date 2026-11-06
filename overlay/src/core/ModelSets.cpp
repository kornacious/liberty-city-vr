
#include "common.h"

#include "ModelSets.h"
#include "crossplatform.h"

#ifdef __ANDROID__
#include "platform_android.h"
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "LibertyVR", __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "LibertyVR", __VA_ARGS__)
#else
#define ALOG(...)
#define ALOGE(...)
#endif

#ifdef _WIN32
#include <windows.h>
#define MODELSET_STRICMP _stricmp
#define MODELSET_STRNICMP _strnicmp
#else
#include <sys/stat.h>
#include <strings.h>
#define MODELSET_STRICMP strcasecmp
#define MODELSET_STRNICMP strncasecmp
#endif

namespace ModelSets
{
namespace
{
eModelSet gActiveModelSet = MODEL_SET_CLASSIC;
eModelSet gRequestedModelSet = MODEL_SET_CLASSIC;
bool gInitialized;
bool gModernArchivePairAvailable;
bool gXboxAvailable;
bool gOriginalXboxProfiles[MODEL_SET_COUNT];
enum { MAX_VEGETATION_MODELS = 512, MAX_MANIFEST_MODEL_NAME = 24 };
char gVegetationModels[MODEL_SET_COUNT][MAX_VEGETATION_MODELS][MAX_MANIFEST_MODEL_NAME] = {};
int gNumVegetationModels[MODEL_SET_COUNT];
bool gVegetationManifestAvailable[MODEL_SET_COUNT];
char gGameRoot[1024] = {};
char gSettingsPath[1024] = {};

const char *const gCategoryNames[MODEL_CATEGORY_COUNT] = {
	"WORLD / BUILDINGS",
	"VEGETATION / PALMS",
	"VEHICLES",
	"PEDESTRIANS",
	"WEAPONS"
};

bool FileExists(const char *path)
{
	if(!path || path[0] == '\0')
		return false;
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
	struct stat st;
	if(stat(path, &st) == 0 && S_ISREG(st.st_mode))
		return true;
	FILE *file = fcaseopen(path, "rb");
	if(!file)
		return false;
	fclose(file);
	return true;
#endif
}

void NormalizeRoot(const char *root)
{
	gGameRoot[0] = '\0';
	if(root && root[0] != '\0')
		strncpy(gGameRoot, root, sizeof(gGameRoot)-1);
	gGameRoot[sizeof(gGameRoot)-1] = '\0';
	const size_t length = strlen(gGameRoot);
	if(length > 0 && gGameRoot[length-1] != '\\' &&
	   gGameRoot[length-1] != '/'){
		if(length+1 < sizeof(gGameRoot)){
			#ifdef _WIN32
			gGameRoot[length] = '\\';
			#else
			gGameRoot[length] = '/';
			#endif
			gGameRoot[length+1] = '\0';
		}
	}
	#ifndef _WIN32
	for(char *cursor = gGameRoot; *cursor; cursor++)
		if(*cursor == '\\')
			*cursor = '/';
	#endif
}

void FindSettingsPath()
{
#ifdef _WIN32
	const DWORD length = GetModuleFileNameA(nil, gSettingsPath,
		(DWORD)ARRAY_SIZE(gSettingsPath));
	if(length > 0 && length < ARRAY_SIZE(gSettingsPath)){
		char *separator = strrchr(gSettingsPath, '\\');
		if(!separator)
			separator = strrchr(gSettingsPath, '/');
		if(separator){
			strcpy(separator+1, "vr_settings.ini");
			return;
		}
	}
#endif
	// Android's Win32-profile shim resolves relative paths against the active
	// game-data directory. Keep the same portable filename used by the Quest VR
	// menu instead of baking an app-specific storage path into this subsystem.
#ifdef _WIN32
	snprintf(gSettingsPath, sizeof(gSettingsPath), "%svr_settings.ini", gGameRoot);
#else
	strcpy(gSettingsPath, ".\\vr_settings.ini");
#endif
}

bool IsSafeModelsPath(const char *path)
{
	if(!path || path[0] == '\0' || path[1] == ':')
		return false;
	if(path[0] == '\\' || path[0] == '/')
		return false;
	if(strstr(path, "..") != nil)
		return false;
	return MODELSET_STRICMP(path, "data\\default.ide") == 0 ||
		MODELSET_STRICMP(path, "data/default.ide") == 0 ||
		MODELSET_STRNICMP(path, "models\\", 7) == 0 ||
		MODELSET_STRNICMP(path, "models/", 7) == 0 ||
		MODELSET_STRNICMP(path, "txd\\", 4) == 0 ||
		MODELSET_STRNICMP(path, "txd/", 4) == 0 ||
		MODELSET_STRNICMP(path, "data\\maps\\", 10) == 0 ||
		MODELSET_STRNICMP(path, "data/maps/", 10) == 0;
}

void BuildOverlayPath(eModelSet modelSet, const char *relativePath, char *destination,
	size_t destinationSize)
{
	char normalized[512];
	strncpy(normalized, relativePath, sizeof(normalized)-1);
	normalized[sizeof(normalized)-1] = '\0';
	for(char *cursor = normalized; *cursor; cursor++)
		#ifdef _WIN32
		if(*cursor == '/') *cursor = '\\';
		#else
		if(*cursor == '\\') *cursor = '/';
		#endif
	const char *setName = modelSet == MODEL_SET_XBOX ? "xbox" : "modern";
#ifdef _WIN32
	snprintf(destination, destinationSize, "%smodelsets\\%s\\%s", gGameRoot, setName, normalized);
#else
	snprintf(destination, destinationSize, "%smodelsets/%s/%s", gGameRoot, setName, normalized);
	char *real = casepath(destination, false);
	if(real){
		strncpy(destination, real, destinationSize-1);
		destination[destinationSize-1] = '\0';
		free(real);
	} else if(!FileExists(destination)){
		char rel[512];
		snprintf(rel, sizeof(rel), "modelsets/%s/%s", setName, normalized);
		char *realRel = casepath(rel, false);
		if(realRel){
			strncpy(destination, realRel, destinationSize-1);
			destination[destinationSize-1] = '\0';
			free(realRel);
		} else if(FileExists(rel))
			strncpy(destination, rel, destinationSize-1);
	}
#endif
}

bool IsGta3ArchivePath(const char *relative)
{
	return MODELSET_STRICMP(relative, "models\\gta3.img") == 0 ||
		MODELSET_STRICMP(relative, "models/gta3.img") == 0 ||
		MODELSET_STRICMP(relative, "models\\gta3.dir") == 0 ||
		MODELSET_STRICMP(relative, "models/gta3.dir") == 0;
}

void LoadPackIdentity(eModelSet set)
{
	gOriginalXboxProfiles[set] = false;
	char path[1024], line[256];
	BuildOverlayPath(set, "BUILD_INFO.txt", path, sizeof(path));
	FILE *file = fopen(path, "rt");
	if(!file) return;
	while(fgets(line, sizeof(line), file)){
		line[strcspn(line, "\r\n")] = '\0';
		if(strcmp(line, "Source=ORIGINAL_XBOX_US_1_0") == 0)
			gOriginalXboxProfiles[set] = true;
	}
	fclose(file);
}

void LoadVegetationManifest(eModelSet set)
{
	gNumVegetationModels[set] = 0;
	gVegetationManifestAvailable[set] = false;
	char path[1024];
	BuildOverlayPath(set, "vegetation_models.txt", path, sizeof(path));
	FILE *file = fopen(path, "rt");
	if(!file){
		debug("Model set: no vegetation manifest at %s; vegetation cannot be separated from world assets\n", path);
		return;
	}

	char line[256];
	while(gNumVegetationModels[set] < MAX_VEGETATION_MODELS &&
	      fgets(line, sizeof(line), file)){
		char *begin = line;
		while(*begin == ' ' || *begin == '\t')
			begin++;
		char *end = begin + strlen(begin);
		while(end > begin && (end[-1] == '\r' || end[-1] == '\n' ||
		      end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';
		if(begin[0] == '\0' || begin[0] == '#' || begin[0] == ';')
			continue;
		char *extension = strrchr(begin, '.');
		if(extension && MODELSET_STRICMP(extension, ".dff") == 0)
			*extension = '\0';
		strncpy(gVegetationModels[set][gNumVegetationModels[set]], begin,
			MAX_MANIFEST_MODEL_NAME-1);
		gVegetationModels[set][gNumVegetationModels[set]][MAX_MANIFEST_MODEL_NAME-1] = '\0';
		gNumVegetationModels[set]++;
	}
	fclose(file);
	gVegetationManifestAvailable[set] = gNumVegetationModels[set] > 0;
	debug("Model set: loaded %d vegetation model names\n",
		gNumVegetationModels[set]);
}
}

void InitializeStartup(const char *gameRoot)
{
	if(gInitialized)
		return;
	NormalizeRoot(gameRoot);
	FindSettingsPath();
	char path[1024];
	BuildOverlayPath(MODEL_SET_MODERN, "models/gta3.img", path, sizeof(path));
	gModernArchivePairAvailable = FileExists(path);
	BuildOverlayPath(MODEL_SET_MODERN, "models/gta3.dir", path, sizeof(path));
	gModernArchivePairAvailable = gModernArchivePairAvailable && FileExists(path);
	// One complete retail Xbox profile. Never register the duplicate legacy Xbox image
	// or combine its geometry with independent category texture choices.
	gXboxAvailable = false;
	LoadPackIdentity(MODEL_SET_MODERN);
	gModernArchivePairAvailable = gModernArchivePairAvailable && gOriginalXboxProfiles[MODEL_SET_MODERN];
	LoadVegetationManifest(MODEL_SET_MODERN);
	int requested = (int)(int32)GetPrivateProfileIntA("VR", "ModelSet",
		MODEL_SET_MODERN, gSettingsPath);
	const bool legacyXbox = requested == MODEL_SET_XBOX;
	if(legacyXbox) requested = MODEL_SET_MODERN;
	if(requested < MODEL_SET_CLASSIC || requested > MODEL_SET_MODERN ||
	   !IsAvailable((eModelSet)requested)) requested = MODEL_SET_CLASSIC;
	gRequestedModelSet = gActiveModelSet = (eModelSet)requested;
	if(legacyXbox)
		WritePrivateProfileStringA("VR", "ModelSet", "1", gSettingsPath);
	gInitialized = true;
	ALOG("ModelSets::InitializeStartup: root='%s' modern=%d xbox=%d active=%s (vehicles=%s world=%s)",
		gGameRoot, (int)gModernArchivePairAvailable, (int)gXboxAvailable,
		GetName(gActiveModelSet),
		GetName(GetActiveForCategory(MODEL_CATEGORY_VEHICLES)),
		GetName(GetActiveForCategory(MODEL_CATEGORY_WORLD)));
	debug("Model sets: preset=%s modern=%d xbox=%d vehicles=%s world=%s\n",
		GetName(gActiveModelSet), gModernArchivePairAvailable, gXboxAvailable,
		GetName(GetActiveForCategory(MODEL_CATEGORY_VEHICLES)),
		GetName(GetActiveForCategory(MODEL_CATEGORY_WORLD)));
}

eModelSet GetActive() { return gActiveModelSet; }
eModelSet GetRequested() { return gRequestedModelSet; }

void SetRequested(eModelSet modelSet)
{
	if(!IsAvailable(modelSet))
		modelSet = MODEL_SET_CLASSIC;
	gRequestedModelSet = modelSet;
	char value[16];
	sprintf(value, "%d", (int)modelSet);
	WritePrivateProfileStringA("VR", "ModelSet", value, gSettingsPath);
}

void CycleRequested(int direction)
{
	if(direction == 0)
		return;
	int requested = gRequestedModelSet;
	for(int i = 0; i < MODEL_SET_COUNT; i++){
		requested = (requested+MODEL_SET_COUNT+(direction > 0 ? 1 : -1)) % MODEL_SET_COUNT;
		if(IsAvailable((eModelSet)requested)){
			SetRequested((eModelSet)requested);
			return;
		}
	}
}

bool IsModernActive()
{
	return gActiveModelSet == MODEL_SET_MODERN && gModernArchivePairAvailable;
}

bool IsXboxActive() { return false; }

bool IsRestartRequired() { return gRequestedModelSet != gActiveModelSet; }

bool IsAvailable(eModelSet modelSet)
{
	return modelSet == MODEL_SET_CLASSIC ||
		(modelSet == MODEL_SET_MODERN && gModernArchivePairAvailable);
}

const char *GetName(eModelSet modelSet)
{
	return modelSet == MODEL_SET_CLASSIC ? "CLASSIC" : "ORIGINAL XBOX";
}

const char *GetSourceName(eModelSet modelSet)
{
	return modelSet == MODEL_SET_CLASSIC ? "CLASSIC" : "ORIGINAL XBOX";
}

eModelSet GetActiveForCategory(eModelCategory category)
{
	return category >= 0 && category < MODEL_CATEGORY_COUNT ? gActiveModelSet : MODEL_SET_CLASSIC;
}

eModelSet GetCalibrationSetForCategory(eModelCategory category)
{
	const eModelSet set = GetActiveForCategory(category);
	return gOriginalXboxProfiles[set] ? MODEL_SET_XBOX : set;
}

eModelSet GetRequestedForCategory(eModelCategory category)
{
	return category >= 0 && category < MODEL_CATEGORY_COUNT ? gRequestedModelSet : MODEL_SET_CLASSIC;
}

void SetRequestedForCategory(eModelCategory category, eModelSet modelSet)
{
	if(category >= 0 && category < MODEL_CATEGORY_COUNT)
		SetRequested(modelSet == MODEL_SET_XBOX ? MODEL_SET_MODERN : modelSet);
}

void CycleRequestedCategory(eModelCategory category, int direction)
{
	if(category >= 0 && category < MODEL_CATEGORY_COUNT) CycleRequested(direction);
}

bool IsCategoryModernActive(eModelCategory category)
{
	return GetActiveForCategory(category) == MODEL_SET_MODERN;
}
bool IsCategoryModernRequested(eModelCategory category)
{
	return GetRequestedForCategory(category) == MODEL_SET_MODERN;
}
bool IsCategoryRestartRequired(eModelCategory category)
{
	return category >= 0 && category < MODEL_CATEGORY_COUNT && IsRestartRequired();
}
bool IsCategoryAvailable(eModelCategory category)
{
	return category >= 0 && category < MODEL_CATEGORY_COUNT && gModernArchivePairAvailable;
}

const char *GetCategoryName(eModelCategory category)
{
	if(category < 0 || category >= MODEL_CATEGORY_COUNT)
		return "UNKNOWN";
	return gCategoryNames[category];
}

bool HasVegetationManifest()
{
	return gVegetationManifestAvailable[MODEL_SET_MODERN] || gVegetationManifestAvailable[MODEL_SET_XBOX];
}

bool IsVegetationModel(const char *modelName)
{
	if(!modelName) return false;
	for(int set = MODEL_SET_MODERN; set < MODEL_SET_COUNT; set++)
		for(int i = 0; i < gNumVegetationModels[set]; i++)
			if(MODELSET_STRICMP(modelName, gVegetationModels[set][i]) == 0)
				return true;
	return false;
}

bool GetModernAssetPath(const char *relativePath, char *resolvedPath,
	size_t resolvedPathSize)
{
	if(!relativePath || !resolvedPath || resolvedPathSize == 0 ||
	   !gModernArchivePairAvailable)
		return false;
	BuildOverlayPath(MODEL_SET_MODERN, relativePath, resolvedPath, resolvedPathSize);
	return FileExists(resolvedPath);
}

bool GetXboxAssetPath(const char *, char *, size_t) { return false; }

static bool IsOverlayPath(eModelSet modelSet, const char *path)
{
	if(!path || !gInitialized)
		return false;
	char overlayRoot[1024];
	BuildOverlayPath(modelSet, "", overlayRoot, sizeof(overlayRoot));
	return MODELSET_STRNICMP(path, overlayRoot, strlen(overlayRoot)) == 0;
}
bool IsModernAssetPath(const char *path) { return IsOverlayPath(MODEL_SET_MODERN, path); }
bool IsXboxAssetPath(const char *) { return false; }

const char *ResolveAssetPath(const char *originalPath, char *resolvedPath,
	size_t resolvedPathSize)
{
	if(!originalPath || !resolvedPath || resolvedPathSize == 0 ||
	   !gInitialized || (!IsModernActive() && !IsXboxActive()))
		return originalPath;
	const char *relative = originalPath;
	while(relative[0] == '.' &&
	      (relative[1] == '\\' || relative[1] == '/'))
		relative += 2;
	if(!IsSafeModelsPath(relative))
		return originalPath;
	// GTA3 is special: keep the base archive as image zero and let the
	// streaming directory register selected entries from a second Modern image.
	// Loose TXDs still use the ordinary Modern fallback behavior.
	if(IsGta3ArchivePath(relative))
		return originalPath;
	// Preserve complete retail vehicle collisions (train/boats included).
	// World collision and map definitions travel with their matching geometry.
	if(MODELSET_STRICMP(relative, "models\\coll\\vehicles.col") == 0 ||
	   MODELSET_STRICMP(relative, "models/coll/vehicles.col") == 0)
		return originalPath;
	eModelCategory cat = MODEL_CATEGORY_WORLD;
	if(MODELSET_STRICMP(relative, "models\\generic\\wheels.dff") == 0 ||
	   MODELSET_STRICMP(relative, "models/generic/wheels.dff") == 0 ||
	   MODELSET_STRICMP(relative, "models\\generic\\wheels.txd") == 0 ||
	   MODELSET_STRICMP(relative, "models/generic/wheels.txd") == 0)
		cat = MODEL_CATEGORY_VEHICLES;
	// generic.txd is shared by roads/props and trees; it follows World.

	eModelSet active = GetActiveForCategory(cat);
	if(active == MODEL_SET_XBOX && gXboxAvailable){
		BuildOverlayPath(MODEL_SET_XBOX, relative, resolvedPath, resolvedPathSize);
		if(FileExists(resolvedPath))
			return resolvedPath;
	}
	if(active == MODEL_SET_MODERN && gModernArchivePairAvailable){
		BuildOverlayPath(MODEL_SET_MODERN, relative, resolvedPath, resolvedPathSize);
		if(FileExists(resolvedPath))
			return resolvedPath;
	}
	return originalPath;
}
}
