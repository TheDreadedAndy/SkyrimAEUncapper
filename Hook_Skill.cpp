/**
 * @file Hook_SKill.cpp
 * @author Kassent
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
        GetBaseActorValue(GetPlayerActorValueOwner(), skill_id)
    );
}

/**
 * @brief Gets a floating point value from the game setting collection.
 */
static float
GetFloatGameSetting(
    const char* var
) {
    Setting *val = GetGameSetting(var);
    ASSERT(val);
    return val->data.f32;
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
 * @brief Adjusts the number of perks the player receives at a level-up.
 * @param points The number of perk points the player has.
 * @param count The original number of points the perk pool was being
 *        adjusted by.
 * @return The new number of perk points the player has.
 */
extern "C" UInt8
ModifyPerkPool_Hook(
    UInt8 points,
    SInt8 count
) {
    int delta = MIN(0xFF, settings.GetPerkDelta(GetPlayerLevel()));
    int res = points + ((count > 0) ? delta : count);
    return static_cast<UInt8>(MAX(0, MIN(0xFF, res)));
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
    void *unk0,
    UInt8 unk1
) {
    Setting *iAVDhmsLevelUp = GetGameSetting("iAVDhmsLevelUp");
    Setting *fLevelUpCarryWeightMod = GetGameSetting("fLevelUpCarryWeightMod");

    ASSERT(iAVDhmsLevelUp);
    ASSERT(fLevelUpCarryWeightMod);

    // Not sure what this is offsetting into, but its consistent
    // between all SE versions.
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
 * @brief Determines what level a skill should take on after being legendaried.
 */
extern "C" void
LegendaryResetSkillLevel_Hook(
    float base_level
) {
    Setting *reset_val = GetGameSetting("fLegendarySkillResetValue");
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
    void *player_actor,
    UInt32 skill_id
) {
    float skill_level = GetBaseActorValue(player_actor, skill_id);
    return settings.IsLegendaryAvailable(skill_id);
}

/**
 * @brief Determines if the legendary button should be displayed for the given
 *        skill based on the users settings.
 */
extern "C" bool
HideLegendaryButton_Hook(
    void *player_actor,
    UInt32 skill_id
) {
    float skill_level = GetBaseActorValue(player_actor, skill_id);
    return settings.IsLegendaryButtonVisible(skill_level);
}
