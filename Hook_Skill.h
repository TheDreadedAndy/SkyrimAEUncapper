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
float PlayerAVOGetCurrent_Hook(void *av, ActorAttribute::t skill);

void ImprovePlayerSkillPoints_Hook(
    PlayerSkills *skill_data,
    ActorAttribute::t skill,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
);
void ImproveAttributeWhenLevelUp_Hook(void *player_avo, ActorAttribute::t choice);

#endif /* __SKYRIM_UNCAPPER_AE_HOOK_SKILL_H__ */
