/**
 * @file Hook_SKill.cpp
 * @author Kassant
 * @brief TODO
 */

#include "Hook_Skill.h"

#include "GameReferences.h"
#include "GameSettings.h"

#include "HookWrappers.h"
#include "Settings.h"
#include "RelocFn.h"
#include "Compare.h"

using LevelData = PlayerSkills::StatData::LevelData;

/**
 * @brief Gets the base level of a skill on the player character.
 *
 * The player pointer offset here is a magic constant. It seems to be accessing
 * the actorState field in the object (which is private).
 *
 * @param skill_id The ID of the skill to get the base level of.
 */
static unsigned int
GetPlayerBaseSkillLevel(
    unsigned int skill_id
) {
    return static_cast<unsigned int>(
        GetBaseActorValue(reinterpret_cast<char*>(GetPlayer()) + 0xB8, skill_id)
    );
}

/**
 * @brief Gets the current level of the player.
 */
static unsigned int
GetPlayerLevel() {
    return GetLevel(GetPlayer());
}

/**
 * @brief Gets a floating point value from the game setting collection.
 */
static float
GetFloatGameSetting(
    const char* var
) {
    Setting* val = (GetGameSettings())->Get(var);
    ASSERT(val);
    return val->data.f32;
}

#if 0
float CalculateSkillExpForLevel(UInt32 skillID, float skillLevel)
{
    float result = 0.0f;
    float fSkillUseCurve = GetFloatGameSetting("fSkillUseCurve");

    if (skillLevel < settings.GetSkillCap(skillID)) {
        result = pow(skillLevel, fSkillUseCurve);
        float a = 1.0f, b = 0.0f, c = 1.0f, d = 0.0f;
        if (GetSkillCoefficients(skillID, &a, &b, &c, &d))
            result = result * c + d;
    }

    return result;
}
#endif

float 
CalculateChargePointsPerUse_Hook(
    float base_points,
    float enchanting_level
) {
    float cost_exponent = GetFloatGameSetting("fEnchantingCostExponent");
    float cost_base = GetFloatGameSetting("fEnchantingSkillCostBase");
    float cost_scale = GetFloatGameSetting("fEnchantingSkillCostScale");
    float cost_mult = GetFloatGameSetting("fEnchantingSkillCostMult");

    enchanting_level = MIN(enchanting_level, 199.0f);
    return cost_mult 
         * pow(base_points, cost_exponent) 
         * (1.0f - pow(enchanting_level * cost_base, cost_scale));
}

#if 0
void ImproveSkillByTraining_Hook(void* pPlayer, UInt32 skillID, UInt32 count)
{
    PlayerSkills* skillData = *reinterpret_cast<PlayerSkills**>(reinterpret_cast<uintptr_t>(pPlayer)+0x9B0);
    if (count < 1)
        count = 1;
    if ((skillID >= 6) && (skillID <= 23))
    {
        LevelData* levelData = &(skillData->data->levelData[skillID - 6]);
        float skillProgression = 0.0f;
        if (levelData->pointsMax > 0.0f)
        {
            skillProgression = levelData->points / levelData->pointsMax;
#ifdef _DEBUG
            _MESSAGE("player:%p, skill:%d, points:%.2f, maxPoints:%.2f, level:%.2f", pPlayer, skillID, levelData->points, levelData->pointsMax, levelData->level);
#endif
            if (skillProgression >= 1.0f)
                skillProgression = 0.99f;
        }
        else
            skillProgression = 0.0f;
        for (UInt32 i = 0; i < count; ++i)
        {
            float skillLevel = GetPlayerBaseSkillLevel(skillID);
            float expRequired = CalculateSkillExpForLevel(skillID, skillLevel);
#ifdef _DEBUG
            _MESSAGE("maxPoints:%.2f, expRequired:%.2f", levelData->pointsMax, expRequired);
#endif
            if (levelData->pointsMax != expRequired)
                levelData->pointsMax = expRequired;
            if (levelData->points <= 0.0f)
                levelData->points = (levelData->pointsMax > 0.0f) ? 0.1f : 0.0f;
            if (levelData->points >= levelData->pointsMax)
                levelData->points = (levelData->pointsMax > 0.0f) ? (levelData->pointsMax - 0.1f) : 0.0f;
            float expNeeded = levelData->pointsMax - levelData->points;
            ImprovePlayerSkillPoints_Original(skillData, skillID, expNeeded, 0, 0, 0, (i < count - 1));
        }
        levelData->points += levelData->pointsMax * skillProgression;
    }
}
#endif

void
ImprovePlayerSkillPoints_Hook(
    PlayerSkills* skillData,
    UInt32 skillID,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
) {
    if (settings.IsManagedSkill(skillID)) {
        exp *= settings.GetSkillExpGainMult(
            skillID,
            GetPlayerBaseSkillLevel(skillID),
            GetPlayerLevel()
        );
    }

    ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

extern "C" float
ImproveLevelExpBySkillLevel_Hook(
    float exp,
    UInt32 skill_id
) {
    if (settings.IsManagedSkill(skill_id)) {
        exp *= settings.GetLevelSkillExpMult(
            skill_id,
            GetPlayerBaseSkillLevel(skill_id),
            GetPlayerLevel()
        );
    }

    return exp;
}

UInt64
ImproveAttributeWhenLevelUp_Hook(
    void* unk0,
    UInt8 unk1
) {
    Setting *iAVDhmsLevelUp = (GetGameSettings())->Get("iAVDhmsLevelUp");
    Setting *fLevelUpCarryWeightMod = (GetGameSettings())->Get("fLevelUpCarryWeightMod");

    ASSERT(iAVDhmsLevelUp);
    ASSERT(fLevelUpCarryWeightMod);

    Settings::player_attr_e choice =
        *reinterpret_cast<Settings::player_attr_e*>(static_cast<char*>(unk0) + 0x18);

    settings.GetAttributeLevelUp(
        GetPlayerLevel(),
        choice,
        iAVDhmsLevelUp->data.u32,
        fLevelUpCarryWeightMod->data.f32
    );

    return ImproveAttributeWhenLevelUp_Original(unk0, unk1);
}

extern "C" void
ModifyPerkPool_Hook(
    SInt8 count
) {
    UInt8* points = &((GetPlayer())->numPerkPoints);
    if (count > 0) { // Add perk points
        UInt32 sum = settings.GetPerkDelta(GetPlayerLevel()) + *points;
        *points = (sum > 0xFF) ? 0xFF : static_cast<UInt8>(sum);
    } else { // Remove perk points
        SInt32 sum = *points + count;
        *points = (sum < 0) ? 0 : static_cast<UInt8>(sum);
    }
}

/**
 * @brief Determines the real skill cap of the given skill.
 */
extern "C" float
GetSkillCap_Hook(
    UInt32 skill_id
) {
    return settings.GetSkillCap(skill_id);
}

/**
 * @brief Caps the formulas for the given skill_id to the value specified in
 *        the INI file.
 */
float
GetEffectiveSkillLevel_Hook(
    void *av,
    UInt32 skill_id
) {
    float val = GetEffectiveSkillLevel_Original(av, skill_id);

    if (settings.IsManagedSkill(skill_id)) {
        float cap = settings.GetSkillFormulaCap(skill_id);
        val = MAX(0, MIN(val, cap));
    }

    return val;
}

/**
 * @brief Determines what level a skill should take on after being legendaried.
 */
void 
LegendaryResetSkillLevel_Hook(
    float base_level,
    UInt32 skill_id
) {
    ASSERT(settings.IsManagedSkill(skill_id));

    Setting* reset_val = (GetGameSettings())->Get("fLegendarySkillResetValue");
    ASSERT(reset_val);
    reset_val->data.f32 = settings.GetPostLegendarySkillLevel(
        reset_val->data.f32,
        base_level
    );
}

/**
 * @brief Overwrites the check which determines when a skill can be legendaried.
 */
extern "C" bool
CheckConditionForLegendarySkill_Hook(
    void* av_owner, 
    UInt32 skill_id
) {
    (void)av_owner; // Always the player.
    float skill_level = GetPlayerBaseSkillLevel(skill_id);
    return settings.IsLegendaryAvailable(skill_id);
}

/**
 * @brief Determines if the legendary button should be displayed for the given
 *        skill based on the users settings.
 */
extern "C" bool
HideLegendaryButton_Hook(
    UInt32 skill_id
) {
    float skill_level = GetPlayerBaseSkillLevel(skill_id);
    return settings.IsLegendaryButtonVisible(skill_level);
}
