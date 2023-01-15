/**
 * @file Settings.cpp
 * @author Kassent
 * @author Andrew Spaulding (Kasplat)
 * @brief Manages INI file settings.
 *
 * Modified by Kasplat to clean up initialization.
 */

#include <cstdio>
#include <cstdlib>

#include "Settings.h"
#include "Compare.h"
#include "Utilities.h"

/// @brief Global settings manager, used throughout this plugin.
Settings settings;

/**
 * @brief Reads in the general settings section
 */
void
Settings::GeneralSettings::ReadConfig(
    CSimpleIniA &ini
) {
    version.ReadConfig(ini, kSection);
    author.ReadConfig(ini, kSection);
}

/**
 * @brief Saves the general settings section.
 */
void
Settings::GeneralSettings::SaveConfig(
    CSimpleIniA &ini
) {
    version.SaveConfig(ini, kSection, NULL);
    author.SaveConfig(ini, kSection, NULL);
}

/**
 * @brief Reads in the legendary skill settings section.
 */
void
Settings::LegendarySettings::ReadConfig(
    CSimpleIniA &ini
) {
    keepSkillLevel.ReadConfig(ini, kSection);
    hideButton.ReadConfig(ini, kSection);
    skillLevelEnable.ReadConfig(ini, kSection);
    skillLevelAfter.ReadConfig(ini, kSection);
}

/**
 * @brief Saves the legendary skill settings section.
 */
void
Settings::LegendarySettings::SaveConfig(
    CSimpleIniA &ini
) {
    keepSkillLevel.SaveConfig(ini, kSection, kKeepSkillLevelDesc);
    hideButton.SaveConfig(ini, kSection, kHideButtonDesc);
    skillLevelEnable.SaveConfig(ini, kSection, kSkillLevelEnableDesc);
    skillLevelAfter.SaveConfig(ini, kSection, kSkillLevelAfterDesc);
}

/**
 * @brief Saves the given INI file to the given path.
 * @param ini The INI file to be saved.
 * @param path The path to save the file to.
 */
bool
Settings::SaveConfig(
    CSimpleIniA &ini,
    const std::string &path
) {
    _MESSAGE("Saving config file...");

    // Clear the INI file; we will rewrite it.
    ini.Reset();

    // Reset the general information.
    general.version.Set(CONFIG_VERSION);
    general.SaveConfig(ini);
    skillCaps.SaveConfig(ini, kSkillCapsDesc);
    skillFormulaCaps.SaveConfig(ini, kSkillFormulaCapsDesc);
    perksAtLevelUp.SaveConfig(ini, kPerksAtLevelUpDesc);
    skillExpGainMults.SaveConfig(ini, kSkillExpGainMultsDesc);
    skillExpGainMultsWithSkills.SaveConfig(ini, kSkillExpGainMultsWithSkillsDesc);
    skillExpGainMultsWithPCLevel.SaveConfig(ini, kSkillExpGainMultsWithPCLevelDesc);
    levelSkillExpMults.SaveConfig(ini, kLevelSkillExpMultsDesc);
    levelSkillExpMultsWithSkills.SaveConfig(ini, kLevelSkillExpMultsWithSkillsDesc);
    levelSkillExpMultsWithPCLevel.SaveConfig(ini, kLevelSkillExpMultsWithPCLevelDesc);
    healthAtLevelUp.SaveConfig(ini, kHealthAtLevelUpDesc);
    healthAtMagickaLevelUp.SaveConfig(ini, kHealthAtMagickaLevelUpDesc);
    healthAtStaminaLevelUp.SaveConfig(ini, kHealthAtStaminaLevelUpDesc);
    magickaAtLevelUp.SaveConfig(ini, kMagickaAtLevelUpDesc);
    magickaAtHealthLevelUp.SaveConfig(ini, kMagickaAtHealthLevelUpDesc);
    magickaAtStaminaLevelUp.SaveConfig(ini, kMagickaAtStaminaLevelUpDesc);
    staminaAtLevelUp.SaveConfig(ini, kStaminaAtLevelUpDesc);
    staminaAtHealthLevelUp.SaveConfig(ini, kStaminaAtHealthLevelUpDesc);
    staminaAtMagickaLevelUp.SaveConfig(ini, kStaminaAtMagickaLevelUpDesc);
    carryWeightAtHealthLevelUp.SaveConfig(ini, kCarryWeightAtHealthLevelUpDesc);
    carryWeightAtMagickaLevelUp.SaveConfig(ini, kCarryWeightAtMagickaLevelUpDesc);
    carryWeightAtStaminaLevelUp.SaveConfig(ini, kCarryWeightAtStaminaLevelUpDesc);
    legendary.SaveConfig(ini);

    // Save the generated INI file.
    SI_Error er = ini.SaveFile(path.c_str());
    if (er < SI_OK) {
        _ERROR("Can't save config file ret:%d errno:%d", (int)er,  errno);
        return false;
    } else {
        _MESSAGE("Config file saved.");
        return true;
    }
}

/**
 * @brief Loads in the INI configuration from the given path.
 *
 * If the given file does not exist, the INI will be created with the default
 * settings at the specified location.
 *
 * @param path The path to read the INI file from.
 */
bool
Settings::ReadConfig(
    const std::string &path
) {
    // Attempt to load the INI file.
    _MESSAGE("Loading config file %s...", path.c_str());
    CSimpleIniA ini;
    SI_Error er = ini.LoadFile(path.c_str());
    bool need_save = false;
    if (er < SI_OK) {
        if ((er != SI_FILE) || (errno != ENOENT)) {
            _ERROR("Can't load config file ret:%d errno:%d", (int)er,  errno);
            return false;
        }
        need_save = true; // No such file or directory.
    }

    // Load general info.
    general.ReadConfig(ini);
    _MESSAGE("INI version: %d", general.version.Get());

    // Check if we need to write out the configuration.
    if (need_save) {
        _MESSAGE("Config file does not exist. It will be created.");
    } else if (general.version.Get() < CONFIG_VERSION) {
        _MESSAGE("Config file is outdated. It will be updated.");
        need_save = true;
    }

    skillCaps.ReadConfig(ini);
    skillFormulaCaps.ReadConfig(ini);
    perksAtLevelUp.ReadConfig(ini);
    skillExpGainMults.ReadConfig(ini);
    skillExpGainMultsWithSkills.ReadConfig(ini);
    skillExpGainMultsWithPCLevel.ReadConfig(ini);
    levelSkillExpMults.ReadConfig(ini);
    levelSkillExpMultsWithSkills.ReadConfig(ini);
    levelSkillExpMultsWithPCLevel.ReadConfig(ini);
    healthAtLevelUp.ReadConfig(ini);
    healthAtMagickaLevelUp.ReadConfig(ini);
    healthAtStaminaLevelUp.ReadConfig(ini);
    magickaAtLevelUp.ReadConfig(ini);
    magickaAtHealthLevelUp.ReadConfig(ini);
    magickaAtStaminaLevelUp.ReadConfig(ini);
    staminaAtLevelUp.ReadConfig(ini);
    staminaAtHealthLevelUp.ReadConfig(ini);
    staminaAtMagickaLevelUp.ReadConfig(ini);
    carryWeightAtHealthLevelUp.ReadConfig(ini);
    carryWeightAtMagickaLevelUp.ReadConfig(ini);
    carryWeightAtStaminaLevelUp.ReadConfig(ini);
    legendary.ReadConfig(ini);

    _MESSAGE("Done!");

    // Save the configuration, if necessary.
    if (need_save) {
        return SaveConfig(ini, path);
    } else {
        return true;
    }
}

/**
 * @brief Checks if the given skill ID is one that this mod interacts with.
 */
bool
Settings::IsManagedSkill(
    unsigned int skill_id
) {
    return SkillSlot::IsSkill(skill_id);
}

/**
 * @brief Gets the skill cap for the given skill ID.
 */
float
Settings::GetSkillCap(
    unsigned int skill_id
) {
    return skillCaps.Get(SkillSlot::FromId(skill_id)).Get();
}

/**
 * @brief Gets the skill formula cap for the given skill ID.
 */
float
Settings::GetSkillFormulaCap(
    unsigned int skill_id
) {
    return skillFormulaCaps.Get(SkillSlot::FromId(skill_id)).Get();
}

/**
 * @brief Gets the number of perk points the player should be given for reaching
 *        the given level.
 */
unsigned int
Settings::GetPerkDelta(
    unsigned int player_level
) {
    return perksAtLevelUp.GetCumulativeDelta(player_level);
}

/**
 * @brief Calculates the skill exp gain multiplier.
 * @param skill_id The ID of the skill which is gaining exp.
 * @param skill_level The level of the skill.
 * @param player_level The level of the player.
 * @return The multiplier.
 */
float
Settings::GetSkillExpGainMult(
    unsigned int skill_id,
    unsigned int skill_level,
    unsigned int player_level
) {
    SkillSlot::t skill = SkillSlot::FromId(skill_id);
    float base_mult = skillExpGainMults.Get(skill).Get();
    float skill_mult = skillExpGainMultsWithSkills.Get(skill).GetNearest(skill_level);
    float pc_mult = skillExpGainMultsWithPCLevel.Get(skill).GetNearest(player_level);
    return base_mult * skill_mult * pc_mult;
}

/**
 * @brief Calculates the level-up exp multiplier.
 * @param skill_id The ID of the skill which is gaining exp.
 * @param skill_level The level of the skill.
 * @param player_level The level of the player.
 * @return The multiplier.
 */
float
Settings::GetLevelSkillExpMult(
    unsigned int skill_id,
    unsigned int skill_level,
    unsigned int player_level
) {
    SkillSlot::t skill = SkillSlot::FromId(skill_id);
    float base_mult = levelSkillExpMults.Get(skill).Get();
    float skill_mult = levelSkillExpMultsWithSkills.Get(skill).GetNearest(skill_level);
    float pc_mult = levelSkillExpMultsWithPCLevel.Get(skill).GetNearest(player_level);
    return base_mult * skill_mult * pc_mult;
}

/**
 * @brief Calculates the attribute increase from the given player level and
 *        selection.
 * @param player_level The level of the player.
 * @param choice The attribute the player selected to level.
 * @param level_up Returns the deltas for each attribute for this level up.
 */
void
Settings::GetAttributeLevelUp(
    unsigned int player_level,
    ActorAttribute choice,
    ActorAttributeLevelUp &level_up
) {
    unsigned int health = 0, magicka = 0, stamina = 0, carry_weight = 0;
    switch (choice) {
        case ActorAttribute::Health:
            health = healthAtLevelUp.GetNearest(player_level);
            magicka = magickaAtHealthLevelUp.GetNearest(player_level);
            stamina = staminaAtHealthLevelUp.GetNearest(player_level);
            carry_weight = carryWeightAtHealthLevelUp.GetNearest(player_level);
            break;
        case ActorAttribute::Magicka:
            health = healthAtMagickaLevelUp.GetNearest(player_level);
            magicka = magickaAtLevelUp.GetNearest(player_level);
            stamina = staminaAtMagickaLevelUp.GetNearest(player_level);
            carry_weight = carryWeightAtMagickaLevelUp.GetNearest(player_level);
            break;
        case ActorAttribute::Stamina:
            health = healthAtStaminaLevelUp.GetNearest(player_level);
            magicka = magickaAtStaminaLevelUp.GetNearest(player_level);
            stamina = staminaAtLevelUp.GetNearest(player_level);
            carry_weight = carryWeightAtStaminaLevelUp.GetNearest(player_level);
            break;
        default:
            HALT("Cannot get attribute level up with an invalid choice.");
    }

    level_up = {
        static_cast<float>(health),
        static_cast<float>(magicka),
        static_cast<float>(stamina),
        static_cast<float>(carry_weight)
    };
}

/**
 * @brief Determines if a skill at the given level should have its legendary
 *        button displayed.
 * @param skill_level The level of the skill.
 */
bool
Settings::IsLegendaryButtonVisible(
    unsigned int skill_level
) {
    return (skill_level >= legendary.skillLevelEnable.Get())
        && (!legendary.hideButton.Get());
}

/**
 * @brief Checks if the given skill level is high enough to legendary.
 */
bool
Settings::IsLegendaryAvailable(
    unsigned int skill_level
) {
    return skill_level >= legendary.skillLevelEnable.Get();
}

/**
 * @brief Returns the level a skill should have after being legendaried.
 */
float
Settings::GetPostLegendarySkillLevel(
    float default_reset,
    float base_level
) {
    // Check if legendarying should reset the level at all.
    if (legendary.keepSkillLevel.Get()) {
        return base_level;
    }

    // 0 in the conf file means we should use the default value.
    float reset_level = static_cast<float>(legendary.skillLevelAfter.Get());
    if (reset_level == 0) {
        reset_level = default_reset;
    }

    // Don't allow legendarying to raise the skill level.
    return MIN(base_level, reset_level);
}
