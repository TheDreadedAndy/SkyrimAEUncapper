/**
 * @file Settings.cpp
 * @author Kassent
 * @author Andrew Spaulding (Kasplat)
 * @brief Manages INI file settings.
 *
 * Modified by Kasplat to clean up initialization.
 */

#include <cstdio>

#include "Settings.h"
#include "Compare.h"
#include "Utilities.h"

/**
 * @brief Reads the given section of the given INI into the given MAP.
 * @param INI The INI to read from.
 * @param LIST The LeveledSetting to load the data into.
 * @param SECTION The section of the INI to read.
 * @param DEFAULT The value to use if a key does not have one.
 * @param CONV The function to use to convert the value to the map type.
 */
#define ReadLevelListSection(INI, LIST, SECTION, DEFAULT, CONV)\
do {\
    CSimpleIniA::TNamesDepend _keys;\
    \
    if (ini.GetAllKeys(SECTION, _keys)) {\
        for (auto& _element : _keys) {\
            (LIST).Add(\
                atoi(_element.pItem),\
                CONV##(ini.GetValue(SECTION, _element.pItem, #DEFAULT))\
            );\
        }\
    }\
    \
    (LIST).Add(0, DEFAULT);\
} while (0)

#define ReadFloatLevelListSection(INI, LIST, SECTION, DEFAULT)\
    ReadLevelListSection(INI, LIST, SECTION, DEFAULT, atof)
#define ReadIntLevelListSection(INI, LIST, SECTION, DEFAULT)\
    ReadLevelListSection(INI, LIST, SECTION, DEFAULT, atoi)

/**
 * @brief Saves the given float-value list to the given section of the INI.
 * @param INI The ini file to save to.
 * @param LIST The LeveledSetting to be saved.
 * @param SECTION The section to save the list to.
 * @param COMMENT The comment expression to be applied to each entry.
 */
#define SaveFloatLevelListSection(INI, LIST, SECTION, COMMENT)\
do {\
    LeveledSetting<float>::LevelItem level;\
    for (size_t i = 0; i < (LIST).Size(); i++) {\
        (LIST).GetItem(i, level);\
        char _key[32];\
        ASSERT(sprintf_s(_key, "%d", level.level) > 0);\
        ASSERT((INI).SetDoubleValue(SECTION, _key, level.item, (!i) ? (COMMENT) : NULL) >= 0);\
    }\
} while (0)

#define SaveIntLevelListSection(INI, LIST, SECTION, COMMENT)\
do {\
    LeveledSetting<UInt32>::LevelItem level;\
    for (size_t i = 0; i < (LIST).Size(); i++) {\
        (LIST).GetItem(i, level);\
        char _key[32];\
        ASSERT(sprintf_s(_key, "%d", level.level) > 0);\
        ASSERT((INI).SetLongValue(SECTION, _key, level.item, (!i) ? (COMMENT) : NULL) >= 0);\
    }\
} while (0)

/// @brief Global settings manager, used throughout this plugin.
Settings settings;

/**
 * @brief Fills the given string buffer with prefix + skill_name
 */
void
Settings::GetSkillStr(
    char *buf,
    size_t buf_size,
    player_skill_e skill,
    const char *prefix
) {
    ASSERT(buf);
    ASSERT(buf_size > 0);
    ASSERT(skill < kSkillCount);
    ASSERT(prefix);

    int res = snprintf(buf, buf_size, "%s%s", prefix, kSkillNames[skill]);
    ASSERT((0 <= res) && (res < buf_size));
}

/**
 * @brief Converts the given skill ID to a player skill enumeration.
 *
 * The provided ID must be valid.
 *
 * @param skill_id The ID to be converted.
 */
Settings::player_skill_e
Settings::GetSkillFromId(
    unsigned int skill_id
) {
    ASSERT(IsManagedSkill(skill_id));
    return static_cast<player_skill_e>(skill_id - kSkillOffset);
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
    const size_t skill_buf_size = 128;
    char skill_buf[skill_buf_size];
    
    _MESSAGE("Saving config file...");

    // Clear the INI file; we will rewrite it.
    ini.Reset();

    // Reset the general information.
    settingsGeneral.version = CONFIG_VERSION;
    ASSERT(ini.SetLongValue(
        "General",
        "Version", 
        CONFIG_VERSION, 
        "# Configuration file version, DO NOT CHANGE"
    ) >= 0);
    ASSERT(ini.SetValue("General", "Author", "Kassent", NULL) >= 0);

    // Save the per-skill settings.
    for (int i = 0; i < kSkillCount; i++) {
        /* Skill cap settings */
        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "i");

        ASSERT(ini.SetLongValue(
            "SkillCaps",
            skill_buf,
            settingsSkillCaps[i],
            (!i) ? kSkillCapsDesc : NULL
        ) >= 0);

        ASSERT(ini.SetLongValue(
            "SkillFormulaCaps",
            skill_buf,
            settingsSkillFormulaCaps[i],
            (!i) ? kSkillFormulaCapsDesc : NULL
        ) >= 0);

        /* Exp multiplier settings */
        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "f");

        ASSERT(ini.SetDoubleValue(
            "SkillExpGainMults",
            skill_buf,
            settingsSkillExpGainMults[i],
            (!i) ? kSkillExpGainMultsDesc : NULL
        ) >= 0);

        ASSERT(ini.SetDoubleValue(
            "LevelSkillExpMults",
            skill_buf,
            settingsLevelSkillExpMults[i],
            (!i) ? kLevelSkillExpMultsDesc : NULL
        ) >= 0);

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "SkillExpGainMults\\CharacterLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsSkillExpGainMultsWithPCLevel[i],
            skill_buf,
            (!i) ? kSkillExpGainMultsWithPCLevelDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "SkillExpGainMults\\BaseSkillLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsSkillExpGainMultsWithSkills[i],
            skill_buf,
            (!i) ? kSkillExpGainMultsWithSkillsDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "LevelSkillExpMults\\CharacterLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsLevelSkillExpMultsWithPCLevel[i],
            skill_buf,
            (!i) ? kLevelSkillExpMultsWithPCLevelDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "LevelSkillExpMults\\BaseSkillLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsLevelSkillExpMultsWithSkills[i],
            skill_buf,
            (!i) ? kLevelSkillExpMultsWithSkillsDesc : NULL
        );
    }

    /* Level-up settings */
    SaveFloatLevelListSection(ini, settingsPerksAtLevelUp, "PerksAtLevelUp", kPerksAtLevelUpDesc);
    SaveIntLevelListSection(ini, settingsHealthAtLevelUp, "HealthAtLevelUp", kHealthAtLevelUpDesc);
    SaveIntLevelListSection(ini, settingsMagickaAtLevelUp, "MagickaAtLevelUp", kMagickaAtLevelUpDesc);
    SaveIntLevelListSection(ini, settingsStaminaAtLevelUp, "StaminaAtLevelUp", kStaminaAtLevelUpDesc);
    SaveIntLevelListSection(ini, settingsCarryWeightAtHealthLevelUp, "CarryWeightAtHealthLevelUp", kCarryWeightAtHealthLevelUpDesc);
    SaveIntLevelListSection(ini, settingsCarryWeightAtMagickaLevelUp, "CarryWeightAtMagickaLevelUp", kCarryWeightAtMagickaLevelUpDesc);
    SaveIntLevelListSection(ini, settingsCarryWeightAtStaminaLevelUp, "CarryWeightAtStaminaLevelUp", kCarryWeightAtStaminaLevelUpDesc);

    ASSERT(ini.SetBoolValue(
        "LegendarySkill",
        "bLegendaryKeepSkillLevel",
        settings.settingsLegendarySkill.bLegendaryKeepSkillLevel,
        kLegendaryKeepSkillLevelDesc
    ) >= 0);
    ASSERT(ini.SetBoolValue(
        "LegendarySkill",
        "bHideLegendaryButton",
        settings.settingsLegendarySkill.bHideLegendaryButton,
        kHideLegendaryButtonDesc
    ) >= 0);
    ASSERT(ini.SetLongValue(
        "LegendarySkill",
        "iSkillLevelEnableLegendary",
        settings.settingsLegendarySkill.iSkillLevelEnableLegendary,
        kSkillLevelEnableLegendaryDesc
    ) >= 0);
    ASSERT(ini.SetLongValue(
        "LegendarySkill",
        "iSkillLevelAfterLegendary",
        settings.settingsLegendarySkill.iSkillLevelAfterLegendary,
        kSkillLevelAfterLegendaryDesc
    ) >= 0);

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
    bool needSave = false;
    if (er < SI_OK) {
        if ((er != SI_FILE) || (errno != ENOENT)) {
            _ERROR("Can't load config file ret:%d errno:%d", (int)er,  errno);
            return false;
        }
        needSave = true; // No such file or directory.
    }

    // Load general info.
    settingsGeneral.version = ini.GetLongValue("General", "Version", 0);
    settingsGeneral.author = ini.GetValue("General", "Author", "Kassent");
    _MESSAGE("INI version: %d", settingsGeneral.version);

    // Check if we need to write out the configuration.
    if (needSave) {
        _MESSAGE("Config file does not exist. It will be created.");
    } else if (settingsGeneral.version < CONFIG_VERSION) {
        _MESSAGE("Config file is outdated. It will be updated.");
        needSave = true;
    }

    // Big enough.
    const size_t skill_buf_size = 128;
    char skill_buf[skill_buf_size];

    // Iterate over settings for each skill.
    for (int i = 0; i < kSkillCount; i++) {
        /* Load default caps */
        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "i");
        settingsSkillCaps[i] = ini.GetLongValue("SkillCaps", skill_buf, 100);
        settingsSkillFormulaCaps[i] = ini.GetLongValue("SkillFormulaCaps", skill_buf, 100);

        /* Load EXP multipliers */
        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "f");
        settingsSkillExpGainMults[i] = atof(ini.GetValue("SkillExpGainMults", skill_buf, "1.00"));
        settingsLevelSkillExpMults[i] = atof(ini.GetValue("LevelSkillExpMults", skill_buf, "1.00"));

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "SkillExpGainMults\\CharacterLevel\\");
        ReadFloatLevelListSection(ini, settingsSkillExpGainMultsWithPCLevel[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "SkillExpGainMults\\BaseSkillLevel\\");
        ReadFloatLevelListSection(ini, settingsSkillExpGainMultsWithSkills[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "LevelSkillExpMults\\CharacterLevel\\");
        ReadFloatLevelListSection(ini, settingsLevelSkillExpMultsWithPCLevel[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, static_cast<player_skill_e>(i), "LevelSkillExpMults\\BaseSkillLevel\\");
        ReadFloatLevelListSection(ini, settingsLevelSkillExpMultsWithSkills[i], skill_buf, 1.00);
    }

    /* Level-up settings */
    ReadFloatLevelListSection(ini, settingsPerksAtLevelUp, "PerksAtLevelUp", 1.00);
    ReadIntLevelListSection(ini, settingsHealthAtLevelUp, "HealthAtLevelUp", 10);
    ReadIntLevelListSection(ini, settingsMagickaAtLevelUp, "MagickaAtLevelUp", 10);
    ReadIntLevelListSection(ini, settingsStaminaAtLevelUp, "StaminaAtLevelUp", 10);
    ReadIntLevelListSection(ini, settingsCarryWeightAtHealthLevelUp, "CarryWeightAtHealthLevelUp", 0);
    ReadIntLevelListSection(ini, settingsCarryWeightAtMagickaLevelUp, "CarryWeightAtMagickaLevelUp", 0);
    ReadIntLevelListSection(ini, settingsCarryWeightAtStaminaLevelUp, "CarryWeightAtStaminaLevelUp", 5);

    /* Legendary Skill Settings */
    settings.settingsLegendarySkill.bLegendaryKeepSkillLevel =
        ini.GetBoolValue("LegendarySkill", "bLegendaryKeepSkillLevel", false);
    settings.settingsLegendarySkill.bHideLegendaryButton =
        ini.GetBoolValue("LegendarySkill", "bHideLegendaryButton", true);
    settings.settingsLegendarySkill.iSkillLevelEnableLegendary =
        ini.GetLongValue("LegendarySkill", "iSkillLevelEnableLegendary", 100);
    settings.settingsLegendarySkill.iSkillLevelAfterLegendary =
        ini.GetLongValue("LegendarySkill", "iSkillLevelAfterLegendary", 0);

    _MESSAGE("Done!");

    // Save the configuration, if necessary.
    if (needSave) {
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
    return (kSkillOffset <= skill_id) && (skill_id < (kSkillOffset + kSkillCount));
}

/**
 * @brief Gets the skill cap for the given skill ID.
 */
float
Settings::GetSkillCap(
    unsigned int skill_id
) {
    return settingsSkillCaps[GetSkillFromId(skill_id)];
}

/**
 * @brief Gets the skill formula cap for the given skill ID.
 */
float
Settings::GetSkillFormulaCap(
    unsigned int skill_id
) {
    return settingsSkillFormulaCaps[GetSkillFromId(skill_id)];
}

/**
 * @brief Gets the number of perk points the player should be given for reaching
 *        the given level.
 */
unsigned int
Settings::GetPerkDelta(
    unsigned int player_level
) {
    return settingsPerksAtLevelUp.GetCumulativeDelta(player_level);
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
    player_skill_e skill = GetSkillFromId(skill_id);
    float base_mult = settingsSkillExpGainMults[skill];
    float skill_mult = settingsSkillExpGainMultsWithSkills[skill].GetNearest(skill_level);
    float pc_mult = settingsSkillExpGainMultsWithPCLevel[skill].GetNearest(player_level);
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
    player_skill_e skill = GetSkillFromId(skill_id);
    float base_mult = settingsLevelSkillExpMults[skill];
    float skill_mult = settingsLevelSkillExpMultsWithSkills[skill].GetNearest(skill_level);
    float pc_mult = settingsLevelSkillExpMultsWithPCLevel[skill].GetNearest(player_level);
    return base_mult * skill_mult * pc_mult;
}

/**
 * @brief Calculates the attribute increase from the given player level and
 *        selection.
 * @param player_level The level of the player.
 * @param attr The attribute the player selected to level.
 * @param attr_up The returned increase to that attribute.
 * @param carry_up The returned increase to carry weight.
 */
void
Settings::GetAttributeLevelUp(
    unsigned int player_level,
    player_attr_e attr,
    UInt32 &attr_up,
    float &carry_up
) {
    ASSERT((attr == ATTR_HEALTH) || (attr == ATTR_MAGICKA) || (attr == ATTR_STAMINA));

    UInt32 a_up = 0, c_up = 0;
    switch (attr) {
        case ATTR_HEALTH:
            a_up = settingsHealthAtLevelUp.GetNearest(player_level);
            c_up = settingsCarryWeightAtHealthLevelUp.GetNearest(player_level);
            break;
        case ATTR_MAGICKA:
            a_up = settingsMagickaAtLevelUp.GetNearest(player_level);
            c_up = settingsCarryWeightAtMagickaLevelUp.GetNearest(player_level);
            break;
        case ATTR_STAMINA:
            a_up = settingsStaminaAtLevelUp.GetNearest(player_level);
            c_up = settingsCarryWeightAtStaminaLevelUp.GetNearest(player_level);
            break;
    }

    attr_up = a_up;
    carry_up = static_cast<float>(c_up);
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
    return (skill_level >= settingsLegendarySkill.iSkillLevelEnableLegendary)
        && (!settingsLegendarySkill.bHideLegendaryButton);
}

/**
 * @brief Checks if the given skill level is high enough to legendary.
 */
bool
Settings::IsLegendaryAvailable(
    unsigned int skill_level
) {
    return skill_level >= settingsLegendarySkill.iSkillLevelEnableLegendary;
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
    if (settingsLegendarySkill.bLegendaryKeepSkillLevel) {
        return base_level;
    }

    // 0 in the conf file means we should use the default value.
    float reset_level = static_cast<float>(settingsLegendarySkill.iSkillLevelAfterLegendary);
    if (reset_level == 0) {
        reset_level = default_reset;
    }

    // Don't allow legendarying to raise the skill level.
    return MIN(base_level, reset_level);
}
