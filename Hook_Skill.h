/**
 * @file Hook_Skill.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exposes hook functions to be installed by the patcher.
 */

#ifndef __SKYRIM_UNCAPPER_AE_HOOK_SKILL_H__
#define __SKYRIM_UNCAPPER_AE_HOOK_SKILL_H__

#include "GameFormComponents.h"
#include "ActorAttribute.h"

float CalculateChargePointsPerUse_Hook(float base_points, float enchant_level);
float GetEffectiveSkillLevel_Hook(void *av, UInt32 skill_id);

void ImprovePlayerSkillPoints_Hook(
    PlayerSkills *skill_data,
    UInt32 skill_id,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
);
void ImproveAttributeWhenLevelUp_Hook(void *player_avo, ActorAttribute choice);

#endif /* __SKYRIM_UNCAPPER_AE_HOOK_SKILL_H__ */
