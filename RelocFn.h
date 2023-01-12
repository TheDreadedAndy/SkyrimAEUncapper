/**
 * @file RelocFn.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exposes game functions which are called by this plugin.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_RELOC_FN_H__
#define __SKYRIM_UNCAPPER_AE_RELOC_FN_H__

#include "GameSettings.h"
#include "GameAPI.h"

PlayerCharacter *GetPlayer(void);
SettingCollectionMap *GetGameSettings(void);
UInt16 GetLevel(void *actor);
float GetBaseActorValue(void *actor, UInt32 skill_id);

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_FN_H__ */
