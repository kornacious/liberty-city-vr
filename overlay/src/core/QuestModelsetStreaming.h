#ifndef QUEST_MODELSET_STREAMING_H
#define QUEST_MODELSET_STREAMING_H

// The native streaming table and extra-object directory keep the first entry.
// Keep mod overrides above the selected source even if image registration order
// changes. Within a layer retain the engine's reverse image order.
template<typename Priority, typename Visit>
static void QuestVisitStreamingImages(int count, Priority priority, Visit visit)
{
    for(int layer = 2; layer >= 0; --layer)
        for(int image = count-1; image >= 0; --image)
            if(priority(image) == layer) visit(image);
}

// Included by Streaming.cpp after ModelInfo/TxdStore/ModelIndices. GTA III
// weapon models are simple objects, so their IDs matter more than model type.
static ModelSets::eModelCategory
QuestModelCategory(CBaseModelInfo *info, int id)
{
    using namespace ModelSets;
    if(info->GetModelType() == MITYPE_VEHICLE) return MODEL_CATEGORY_VEHICLES;
    if(info->GetModelType() == MITYPE_PED) return MODEL_CATEGORY_PEDS;
    if(id >= MI_GRENADE && id <= MI_BOMB) return MODEL_CATEGORY_WEAPONS;
    if(IsVegetationModel(info->GetModelName())) return MODEL_CATEGORY_VEGETATION;
    return MODEL_CATEGORY_WORLD;
}

static bool
QuestOverlayEntrySelected(const char *directory, const char *entry)
{
    using namespace ModelSets;
    const eModelSet set = IsXboxAssetPath(directory) ? MODEL_SET_XBOX :
        IsModernAssetPath(directory) ? MODEL_SET_MODERN : MODEL_SET_CLASSIC;
    if(set == MODEL_SET_CLASSIC) return true; // Always keep retail fallbacks.
    char name[24];
    strncpy(name, entry, sizeof(name)-1);
    name[sizeof(name)-1] = '\0';
    char *dot = strrchr(name, '.');
    if(!dot) return false;
    *dot++ = '\0';
    if(!strcmp(dot, "dff") || !strcmp(dot, "DFF")){
        int id = -1;
        CBaseModelInfo *info = CModelInfo::GetModelInfo(name, &id);
        if(info) return GetActiveForCategory(QuestModelCategory(info, id)) == set;
        // Cutscene actors are extra objects, absent from the ordinary IDEs.
        const eModelCategory category = name[0] == '3' && name[1] == 'd' ?
            MODEL_CATEGORY_PEDS : MODEL_CATEGORY_WORLD;
        return GetActiveForCategory(category) == set;
    }
    if(strcmp(dot, "txd") && strcmp(dot, "TXD")) return false;
    const int slot = CTxdStore::FindTxdSlot(name);
    bool referenced = false;
    if(slot >= 0){
        for(int id = 0; id < MODELINFOSIZE; id++){
            CBaseModelInfo *info = CModelInfo::GetModelInfo(id);
            if(!info || info->GetTxdSlot() != slot) continue;
            referenced = true;
            // A shared dictionary falls back to retail when its owners use
            // different categories. Do not recolor a Classic vehicle or ped.
            if(GetActiveForCategory(QuestModelCategory(info, id)) != set) return false;
        }
    }
    if(referenced) return true;
    const eModelCategory category = name[0] == '3' && name[1] == 'd' ?
        MODEL_CATEGORY_PEDS : MODEL_CATEGORY_WORLD;
    return GetActiveForCategory(category) == set;
}
#endif
