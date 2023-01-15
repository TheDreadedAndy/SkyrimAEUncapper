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
#include "ActorAttribute.h"

/**
 * @brief Determines the real skill cap of the given skill.
 */
extern "C" float
GetSkillCap_Hook(
    ActorAttribute::t skill
) {
    ASSERT(settings.IsSkillCapEnabled());
    return settings.GetSkillCap(skill);
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
    ASSERT(settings.IsEnchantPatchEnabled());

    float cost_exponent = *GetFloatGameSetting("fEnchantingCostExponent");
    float cost_base = *GetFloatGameSetting("fEnchantingSkillCostBase");
    float cost_scale = *GetFloatGameSetting("fEnchantingSkillCostScale");
    float cost_mult = *GetFloatGameSetting("fEnchantingSkillCostMult");
    float cap = settings.GetEnchantChargeCap();

    enchanting_level = MIN(enchanting_level, cap);
    float base = cost_mult * pow(base_points, cost_exponent);

    if (settings.IsEnchantChargeLinear()) {
        // Linearly scale between the normal min/max of charge points.
        // FIXME: Need to know the maximum number of charges to make the
        //        number of increased usages actually linear.
        float slope = (-1 * base * pow(cap * cost_base, cost_scale)) / cap;
        return slope * enchanting_level + base;
    } else {
        return base * (1.0f - pow(enchanting_level * cost_base, cost_scale));
    }
}

/**
 * @brief Caps the formulas for the given skill_id to the value specified in
 *        the INI file.
 */
float
PlayerAVOGetCurrent_Hook(
    void *av,
    ActorAttribute::t attr
) {
    ASSERT(settings.IsSkillFormulaCapEnabled());
    // FIXME: Need to find where this is called in the text color code and
    //        replace it so the skills menu is actually correct.

    float val = PlayerAVOGetCurrent_Original(av, attr);

    if (ActorAttribute::IsSkill(attr)) {
        float cap = settings.GetSkillFormulaCap(attr);
        val = MAX(0, MIN(val, cap));
    }

    return val;
}

/**
 * @brief Applies a multiplier to the exp gain for the given skill.
 */
void
ImprovePlayerSkillPoints_Hook(
    PlayerSkills *skill_data,
    ActorAttribute::t attr,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
) {
    ASSERT(settings.IsSkillExpEnabled());

    if (ActorAttribute::IsSkill(attr)) {
        exp *= settings.GetSkillExpGainMult(
            attr,
            PlayerAVOGetBase(attr),
            GetPlayerLevel()
        );
    }

    ImprovePlayerSkillPoints_Original(skill_data, attr, exp, unk1, unk2, unk3, unk4);
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
    ASSERT(settings.IsPerkPointsEnabled());
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
    ActorAttribute::t attr
) {
    ASSERT(settings.IsLevelExpEnabled());
    if (ActorAttribute::IsSkill(attr)) {
        exp *= settings.GetLevelSkillExpMult(
            attr,
            PlayerAVOGetBase(attr),
            GetPlayerLevel()
        );
    }

    return exp;
}

/**
 * @brief Adjusts the attribute gain at each level up based on the settings in
 *        the INI file.
 *
 * This function overwrites a call to player_avo->ModBase. Since we're
 * overwriting a call, we don't need to reg save and, thus, don't need a
 * wrapper. We also overwrite the carry weight level up.
 */
void
ImproveAttributeWhenLevelUp_Hook(
    void *player_avo,
    ActorAttribute::t choice
) {
    (void)player_avo;
    ASSERT(settings.IsAttributePointsEnabled());
    
    ActorAttributeLevelUp level_up;
    settings.GetAttributeLevelUp(
        GetPlayerLevel(),
        choice,
        level_up
    );

    PlayerAVOModBase(ActorAttribute::Health, level_up.health);
    PlayerAVOModBase(ActorAttribute::Magicka, level_up.magicka);
    PlayerAVOModBase(ActorAttribute::Stamina, level_up.stamina);
    PlayerAVOModCurrent(
        0, // Same as OG call.
        ActorAttribute::CarryWeight,
        level_up.carry_weight
    );
}

/**
 * @brief Determines what level a skill should take on after being legendaried.
 */
extern "C" void
LegendaryResetSkillLevel_Hook(
    float base_level
) {
    ASSERT(settings.IsLegendaryEnabled());
    float *reset_val = GetFloatGameSetting("fLegendarySkillResetValue");
    *reset_val = settings.GetPostLegendarySkillLevel(*reset_val, base_level);
}

/**
 * @brief Overwrites the check which determines when a skill can be legendaried.
 */
extern "C" bool
CheckConditionForLegendarySkill_Hook(
    void *player_actor,
    ActorAttribute::t skill
) {
    ASSERT(settings.IsLegendaryEnabled());
    float skill_level = PlayerAVOGetBase(skill);
    return settings.IsLegendaryAvailable(skill);
}

/**
 * @brief Determines if the legendary button should be displayed for the given
 *        skill based on the users settings.
 */
extern "C" bool
HideLegendaryButton_Hook(
    void *player_actor,
    ActorAttribute::t skill
) {
    ASSERT(settings.IsLegendaryEnabled());
    float skill_level = PlayerAVOGetBase(skill);
    return settings.IsLegendaryButtonVisible(skill_level);
}
