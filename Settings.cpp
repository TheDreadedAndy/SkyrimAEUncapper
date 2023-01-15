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

// Comment on each leveled setting description.
#define LEVELED_SETTING_NOTE\
    "# If a specific level is not specified, then the\n"\
    "# value for the closest lower level is used."

const char *const Settings::GeneralSettings::kSection = "General";
const char *const Settings::GeneralSettings::kVersionDesc =
    "# Do not manually change this field. Doing so can prevent this INI file\n"
    "# from updating with new versions.";
const char *const Settings::GeneralSettings::kEnableSkillCapsDesc =
    "# Enables the code which uncap skill levels.";
const char *const Settings::GeneralSettings::kEnableSkillFormulaCapsDesc =
    "# Enables the code which cap all skill formulas.";
const char *const Settings::GeneralSettings::kEnableEnchantingPatchDesc =
    "# Enables the code which patches to the enchantment charge cost "
    "calculation.";
const char *const Settings::GeneralSettings::kEnableSkillExpMultsDesc =
    "# Enables the code which applies the skill experience multipliers.";
const char *const Settings::GeneralSettings::kEnableLevelExpMultsDesc =
    "# Enables the code which applies the level experience multipliers.";
const char *const Settings::GeneralSettings::kEnablePerkPointsDesc =
    "# Enables the code which modifies perk point gain.";
const char *const Settings::GeneralSettings::kEnableAttributePointsDesc =
    "# Enables the code which modifies attribute point gain.";
const char *const Settings::GeneralSettings::kEnableLegendaryDesc =
    "# Enables the code which modifies the legendary skill system.";

const char *const Settings::EnchantSettings::kSection = "Enchanting";
const char *const Settings::EnchantSettings::kChargeLevelCapDesc =
    "# Sets the formula cap for the enchanting weapon charge calculation.\n"
    "# The formula breaks above level 199, so values above that will be ignored.\n"
    "# This value is also capped by the enchanting skill formula cap.";
const char *const Settings::EnchantSettings::kUseLinearChargeFormulaDesc =
    "# Forces the game to use a linear formula for charge level-based weapon\n"
    "# charge calculation. Useful if the charge cap is close to 199, as the\n"
    "# later level-ups in enchanting will give massive boosts to the number\n"
    "# of charge points available.";

const char *const Settings::LegendarySettings::kSection = "LegendarySkill";
const char *const Settings::LegendarySettings::kKeepSkillLevelDesc =
    "# This option determines whether the legendary feature will reset the\n"
    "# skill level. Setting this option to true will make the option\n"
    "# \"iSkillLevelAfterLegendary\" have no effect.";
const char *const Settings::LegendarySettings::kHideButtonDesc =
    "# This option determines whether to hide the legendary button in \"Skills\"\n"
    "# menu when you meet the requirements to legendary a skill.\n"
    "# If you set \"iSkillLevelEnableLegendary\" to below 100, the legendary\n"
    "# button will not show up, but you can make skills legendary normally\n"
    "# by pressing SPACE.";
const char *const Settings::LegendarySettings::kSkillLevelEnableDesc =
    "# This option determines the skill level required to make a skill legendary.";
const char *const Settings::LegendarySettings::kSkillLevelAfterDesc =
    "# This option determines the level of a skill after making it legendary.\n"
    "# Setting this option to 0 will reset the skill level to default level.";

const char *const Settings::kSkillCapsDesc =
    "# Set the skill level cap. This option determines the upper limit of\n"
    "# skill level you can reach.";
const char *const Settings::kSkillFormulaCapsDesc =
    "# Set the skill formula cap. This option determines the upper limit of\n"
    "# skill level used in the calculation of all kinds of magic effects.";
const char *const Settings::kSkillExpGainMultsDesc =
    "# Set the skill experience gained multiplier. The skill experience\n"
    "# you gained actually = The final calculated experience value right\n"
    "# before it is given to the character after any experience\n"
    "# modification * SkillExpGainMult * Corresponding\n"
    "# Sub-SkillExpGainMult listed below.";
const char *const Settings::kSkillExpGainMultsWithPCLevelDesc =
    "# All the subsections of SkillExpGainMults below allow to set an\n"
    "# additional multiplier depending on CHARACTER LEVEL, independantly\n"
    "# for each skill.";
const char *const Settings::kSkillExpGainMultsWithSkillsDesc =
    "# All the subsections of SkillExpGainMults below allow to set an\n"
    "# additional multiplier depending on BASE SKILL LEVEL, independantly\n"
    "# for each skill.";
const char *const Settings::kLevelSkillExpMultsDesc =
    "# Set the skill experience to PC experience multipliers. When you\n"
    "# level up a skill, the PC experience you gained actually = Current\n"
    "# base skill level * LevelSkillExpMults * Corresponding "
    "# Sub-LevelSkillExpMults listed below.";
const char *const Settings::kLevelSkillExpMultsWithPCLevelDesc =
    "# All the subsections of LevelSkillExpMults below allow to set an\n"
    "# additional multipliers depending on CHARACTER LEVEL, independantly\n"
    "# for each skill.";
const char *const Settings::kLevelSkillExpMultsWithSkillsDesc =
    "# All the subsections of LevelSkillExpMults below allow to set an\n"
    "# additional multipliers depending on BASE SKILL LEVEL, independantly\n"
    "# for each skill.";
const char *const Settings::kPerksAtLevelUpDesc =
    "# Set the number of perks gained at each level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kHealthAtLevelUpDesc =
    "# Set the number of health gained at each health level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kHealthAtMagickaLevelUpDesc =
    "# Set the number of health gained at each magicka level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kHealthAtStaminaLevelUpDesc =
    "# Set the number of health gained at each stamina level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kMagickaAtLevelUpDesc =
    "# Set the number of magicka gained at each magicka level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kMagickaAtHealthLevelUpDesc =
    "# Set the number of magicka gained at each health level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kMagickaAtStaminaLevelUpDesc =
    "# Set the number of magicka gained at each stamina level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kStaminaAtLevelUpDesc =
    "# Set the number of stamina gained at each stamina level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kStaminaAtHealthLevelUpDesc =
    "# Set the number of stamina gained at each health level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kStaminaAtMagickaLevelUpDesc =
    "# Set the number of stamina gained at each magicka level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kCarryWeightAtHealthLevelUpDesc =
    "# Set the number of carryweight gained at each health level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kCarryWeightAtMagickaLevelUpDesc =
    "# Set the number of carryweight gained at each magicka level up.\n"
    LEVELED_SETTING_NOTE;
const char *const Settings::kCarryWeightAtStaminaLevelUpDesc =
    "# Set the number of carryweight gained at each stamina level up.\n"
    LEVELED_SETTING_NOTE;

/**
 * @brief Reads in the general settings section
 */
void
Settings::GeneralSettings::ReadConfig(
    CSimpleIniA &ini
) {
    version.ReadConfig(ini, kSection);
    author.ReadConfig(ini, kSection);
    enableSkillCaps.ReadConfig(ini, kSection);
    enableSkillFormulaCaps.ReadConfig(ini, kSection);
    enableEnchantingPatch.ReadConfig(ini, kSection);
    enableSkillExpMults.ReadConfig(ini, kSection);
    enableLevelExpMults.ReadConfig(ini, kSection);
    enablePerkPoints.ReadConfig(ini, kSection);
    enableAttributePoints.ReadConfig(ini, kSection);
    enableLegendary.ReadConfig(ini, kSection);
}

/**
 * @brief Saves the general settings section.
 */
void
Settings::GeneralSettings::SaveConfig(
    CSimpleIniA &ini
) {
    version.SaveConfig(ini, kSection, kVersionDesc);
    author.SaveConfig(ini, kSection, NULL);
    enableSkillCaps.SaveConfig(ini, kSection, kEnableSkillCapsDesc);
    enableSkillFormulaCaps.SaveConfig(ini, kSection, kEnableSkillFormulaCapsDesc);
    enableEnchantingPatch.SaveConfig(ini, kSection, kEnableEnchantingPatchDesc);
    enableSkillExpMults.SaveConfig(ini, kSection, kEnableSkillExpMultsDesc);
    enableLevelExpMults.SaveConfig(ini, kSection, kEnableLevelExpMultsDesc);
    enablePerkPoints.SaveConfig(ini, kSection, kEnablePerkPointsDesc);
    enableAttributePoints.SaveConfig(ini, kSection, kEnableAttributePointsDesc);
    enableLegendary.SaveConfig(ini, kSection, kEnableLegendaryDesc);
}

/**
 * @brief Reads in the enchant settings section.
 */
void
Settings::EnchantSettings::ReadConfig(
    CSimpleIniA &ini
) {
    chargeLevelCap.ReadConfig(ini, kSection);
    useLinearChargeFormula.ReadConfig(ini, kSection);
}

/**
 * @brief Saves the enchant settings section.
 */
void
Settings::EnchantSettings::SaveConfig(
    CSimpleIniA &ini
) {
    chargeLevelCap.SaveConfig(ini, kSection, kChargeLevelCapDesc);
    useLinearChargeFormula.SaveConfig(ini, kSection, kUseLinearChargeFormulaDesc);
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
    enchant.SaveConfig(ini);
    skillExpGainMults.SaveConfig(ini, kSkillExpGainMultsDesc);
    skillExpGainMultsWithSkills.SaveConfig(ini, kSkillExpGainMultsWithSkillsDesc);
    skillExpGainMultsWithPCLevel.SaveConfig(ini, kSkillExpGainMultsWithPCLevelDesc);
    levelSkillExpMults.SaveConfig(ini, kLevelSkillExpMultsDesc);
    levelSkillExpMultsWithSkills.SaveConfig(ini, kLevelSkillExpMultsWithSkillsDesc);
    levelSkillExpMultsWithPCLevel.SaveConfig(ini, kLevelSkillExpMultsWithPCLevelDesc);
    perksAtLevelUp.SaveConfig(ini, kPerksAtLevelUpDesc);
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
    enchant.ReadConfig(ini);
    skillExpGainMults.ReadConfig(ini);
    skillExpGainMultsWithSkills.ReadConfig(ini);
    skillExpGainMultsWithPCLevel.ReadConfig(ini);
    levelSkillExpMults.ReadConfig(ini);
    levelSkillExpMultsWithSkills.ReadConfig(ini);
    levelSkillExpMultsWithPCLevel.ReadConfig(ini);
    perksAtLevelUp.ReadConfig(ini);
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
 * @brief Gets the skill cap for the given skill ID.
 */
float
Settings::GetSkillCap(
    ActorAttribute::t skill
) {
    return skillCaps.Get(SkillSlot::FromAttribute(skill)).Get();
}

/**
 * @brief Gets the skill formula cap for the given skill ID.
 */
float
Settings::GetSkillFormulaCap(
    ActorAttribute::t skill
) {
    return skillFormulaCaps.Get(SkillSlot::FromAttribute(skill)).Get();
}

/**
 * @brief Gets the current enchanting weapon charge cap.
 */
float
Settings::GetEnchantChargeCap() {
    return MIN(199.0, enchant.chargeLevelCap.Get());
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
    ActorAttribute::t skill,
    unsigned int skill_level,
    unsigned int player_level
) {
    SkillSlot::t slot = SkillSlot::FromAttribute(skill);
    float base_mult = skillExpGainMults.Get(slot).Get();
    float skill_mult = skillExpGainMultsWithSkills.Get(slot).GetNearest(skill_level);
    float pc_mult = skillExpGainMultsWithPCLevel.Get(slot).GetNearest(player_level);
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
    ActorAttribute::t skill,
    unsigned int skill_level,
    unsigned int player_level
) {
    SkillSlot::t slot = SkillSlot::FromAttribute(skill);
    float base_mult = levelSkillExpMults.Get(slot).Get();
    float skill_mult = levelSkillExpMultsWithSkills.Get(slot).GetNearest(skill_level);
    float pc_mult = levelSkillExpMultsWithPCLevel.Get(slot).GetNearest(player_level);
    return base_mult * skill_mult * pc_mult;
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
 * @brief Calculates the attribute increase from the given player level and
 *        selection.
 * @param player_level The level of the player.
 * @param choice The attribute the player selected to level.
 * @param level_up Returns the deltas for each attribute for this level up.
 */
void
Settings::GetAttributeLevelUp(
    unsigned int player_level,
    ActorAttribute::t choice,
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
