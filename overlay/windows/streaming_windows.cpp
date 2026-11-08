#include "common.h"
#include "CdStream.h"
#include "ModelSets.h"
extern "C" bool CdStreamAddModelSetImages(const char *basePath) {
    char resolved[1024];
    if(!CdStreamAddImage(ModelSets::ResolveAssetPath(basePath,resolved,sizeof(resolved)))) return false;
    char overlay[1024];
    if(ModelSets::IsModernActive() && ModelSets::GetModernAssetPath("models/gta3.img",overlay,sizeof(overlay)) && !CdStreamAddImage(overlay)) return false;
    if(ModelSets::IsXboxActive() && ModelSets::GetXboxAssetPath("models/gta3.img",overlay,sizeof(overlay)) && !CdStreamAddImage(overlay)) return false;
    return true;
}
