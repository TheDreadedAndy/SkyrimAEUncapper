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
float GetBaseActorValue(void *actor, UInt32 skill_id);
UInt16 GetLevel(void *actor);
bool GetSkillCoefficients(UInt32 skill_id, float &a, float &b, float &c, float &d);

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_FN_H__ */
