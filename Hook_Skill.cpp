/**
 * @file Hook_SKill.cpp
 * @author Kassant
 * @author Vadfromnu
 * @author Andrew Spaulding (Kasplat)
 * @brief C++ implementation of game patches.
 * 
 * Note that several of these functions first go through wrappers in
 * HookWrappers.S. See the documentation in RelocPatch.cpp and HookWrappers.S
 * for more information on how these patches are modifying the game.
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

/**
 * @brief Calculates the amount of exp needed to raise a skill to its next 
 *        level.
 * 
 * https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
 * 
 * TODO: It'd be better to hook into the actual game function. Reimplementing
 *       means that any mod which alters this equation will be ignored.
 * 
 * @param skill_id The skill this calculation is being performed on.
 * @return The amount of exp needed.
 */
static float 
CalculateSkillExpForLevel(
    UInt32 skill_id
) {
    float result = 0.0f;
    float skill_level = GetPlayerBaseSkillLevel(skill_id);
    float skill_curve = GetFloatGameSetting("fSkillUseCurve");

    if (skill_level < settings.GetSkillCap(skill_id)) {
        result = pow(skill_level, skill_curve);
        float a = 1.0f, b = 0.0f, c = 1.0f, d = 0.0f;
        if (GetSkillCoefficients(skill_id, a, b, c, d)) {
            result = result * c + d;
        }
    }

    return result;
}

/**
 * @brief Improves a player skill through training.
 * 
 * Reimplements the games original ImproveSkillByTraining function
 * to uncap it and ensure it calls the correct ImprovePlayerSkillPoints()
 * function.
 * 
 * @param skill_data A pointer to the players skill data.
 * @param skill_id The skill to be improved.
 * @param count The number of levels to improve the skill.
 */
void
ImproveSkillByTraining_Hook(
    PlayerSkills *skill_data, 
    UInt32 skill_id,
    UInt32 count
) {
    ASSERT(settings.IsManagedSkill(skill_id));
    LevelData *level_data = &(skill_data->data->levelData[skill_id - 6]); // FIXME

    // pointsMax == 0 => level cap has been reached.
    float skill_progress = 0;
    if (level_data->pointsMax > 0) {
        skill_progress = level_data->points / level_data->pointsMax;
    }

    for (UInt32 i = 0; i < count; ++i) {
        // We need to overwrite pointsMax to uncap training.
        level_data->pointsMax = CalculateSkillExpForLevel(skill_id);
        level_data->points = 0;

        // We call the original here; we don't want our multipliers.
        // This invocation is the same as in the original implementation.
        // I have no idea what unk4 is doing.
        ImprovePlayerSkillPoints_Original(
            skill_data,
            skill_id, 
            level_data->pointsMax, 
            0, 
            0, 
            0, 
            (i < count - 1)
        );
    }

    level_data->pointsMax = CalculateSkillExpForLevel(skill_id);
    level_data->points += level_data->pointsMax * skill_progress;
}

/**
 * @brief Applies a multiplier to the exp gain for the given skill.
 */
void
ImprovePlayerSkillPoints_Hook(
    PlayerSkills* skill_data,
    UInt32 skill_id,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
) {
    if (settings.IsManagedSkill(skill_id)) {
        exp *= settings.GetSkillExpGainMult(
            skill_id,
            GetPlayerBaseSkillLevel(skill_id),
            GetPlayerLevel()
        );
    }

    ImprovePlayerSkillPoints_Original(skill_data, skill_id, exp, unk1, unk2, unk3, unk4);
}

/**
 * @brief Multiplies the EXP gain of a level-up by our configured multiplier.
 * @param exp The original exp gain.
 * @param skill_id The skill which this exp is being gained from.
 * @return The new exp multiplier.
 */
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

/**
 * @brief Adjusts the carry weight/attribute gain at each level up based on
 *        the settings in the INI file.
 */
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

/**
 * @brief Adjusts the number of perks the player receives at a level-up.
 * @param count The original number of points the perk pool was being
 *        adjusted by.
 */
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
 * @brief Determines what level a skill should take on after being legendaried.
 */
extern "C" void
LegendaryResetSkillLevel_Hook(
    float base_level
) {
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

/**
 * @brief Reimplements the enchantment charge point equation.
 *
 * The original equation would fall apart for levels above 199, se this
 * implementation caps the level in the calculation to 199.
 *
 * @param base_points The base point value for the enchantment.
 * @param enchanting_level The enchanting skill of the player.
 * @return The charge points per use on the item.
 */
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