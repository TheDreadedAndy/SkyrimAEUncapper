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

#include "ActorAttribute.h"

float *GetFloatGameSetting(const char *var);
UInt16 GetPlayerLevel(void);
void *GetPlayerActorValueOwner(void);
float PlayerAVOGetBase(ActorAttribute::t attr);
float PlayerAVOGetCurrent_Original(void *av, ActorAttribute::t attr);
void PlayerAVOModBase(ActorAttribute::t attr, float val);
void PlayerAVOModCurrent(UInt32 unk1, ActorAttribute::t attr, float val);

#endif /* __SKYRIM_UNCAPPER_AE_RELOC_FN_H__ */
