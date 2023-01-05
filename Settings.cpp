/**
 * @file Settings.cpp
 * @Author Kassant
 * @brief Manages INI file settings.
 *
 * Modified by Kasplat to clean up initialization.
 */

#undef NDEBUG
#include <cassert>
#include <cstdio>

#include "Settings.h"
#include "Utilities.h"

/**
 * @brief Reads the given section of the given INI into the given MAP.
 * @param INI The INI to read from.
 * @param MAP The SettingList to load the data into.
 * @param SECTION The section of the INI to read.
 * @param DEFAULT The value to use if a key does not have one.
 * @param CONV The function to use to convert the value to the map type.
 */
#define ReadLevelListSection(INI, MAP, SECTION, DEFAULT, CONV)\
do {\
    CSimpleIniA::TNamesDepend _keys;\
    \
    if (ini.GetAllKeys(SECTION, _keys)) {\
        for (auto& _element : _keys) {\
            (MAP).insert({\
                atoi(_element.pItem),\
                CONV##(ini.GetValue(SECTION, _element.pItem, #DEFAULT))\
            });\
        }\
    }\
    \
    (MAP).insert({ 1, DEFAULT });\
} while (0)

#define ReadFloatLevelListSection(INI, MAP, SECTION, DEFAULT)\
    ReadLevelListSection(INI, MAP, SECTION, DEFAULT, atof)
#define ReadIntLevelListSection(INI, MAP, SECTION, DEFAULT)\
    ReadLevelListSection(INI, MAP, SECTION, DEFAULT, atoi)

/**
 * @brief Saves the given float-value list to the given section of the INI.
 * @param INI The ini file to save to.
 * @param MAP The SettingList to be saved.
 * @param SECTION The section to save the list to.
 * @param COMMENT The comment expression to be applied to each entry.
 */
#define SaveFloatLevelListSection(INI, MAP, SECTION, COMMENT)\
do {\
    for (const auto &_pair : MAP) {\
        char _key[16];\
        sprintf_s(_key, "%d", _pair.first);\
        IniSetValueFloat(INI, SECTION, _key, _pair.second, (_pair.first == 1) ? (COMMENT) : NULL);\
    }\
} while (0)

#define SaveIntLevelListSection(INI, MAP, SECTION, COMMENT)\
do {\
    for (const auto &_pair : MAP) {\
        char _key[16];\
        sprintf_s(_key, "%d", _pair.first);\
        (INI).SetValueLong(SECTION, _key, _pair.second, (_pair.first == 1) ? (COMMENT) : NULL);\
    }\
} while (0)

/// @brief Global settings manager, used throughout plugin.
Settings settings;

/**
 * @brief Sets a floating point value in an INI file.
 * @param ini The INI to set the value in.
 * @param section The section to set the value in.
 * @param key The key to set the value under.
 * @param val The floating point value.
 * @param comment An optional comment on the pair.
 */
static void
IniSetValueFloat(
    CSimpleIniA &ini,
    const char *section
    const char *key,
    float val,
    const char *comment
) {
    assert(section);
    assert(key);

    // Floats can be long bois.
    const size_t buf_size = 64;
    char buf[buf_size];

    int res = snprintf(buf, buf_size, "%.2f", val);
    assert((0 <= res) && (res < buf_size));

    ini.SetValue(section, key, buf, comment);
}

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
    assert(buf);
    assert(buf_size > 0);
    assert(skill < SKILL__COUNT__);
    assert(prefix);

    int res = snprintf(buf, buf_size, "%s%s", prefix, skillNames[skill]);
    assert((0 <= res) && (res < buf_size));
}

/**
 * @brief Creates a new settings manager.
 */
Settings::Settings() {}

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
    _MESSAGE("Config file path:%s", path.c_str());
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
        _MESSAGE("Config file does not exist. Trying to create.");
    } else if (settingsGeneral.version != CONFIG_VERSION) {
        _MESSAGE("Config file is outdated. Updating.");
        needSave = true;
    }

    // Big enough.
    const size_t skill_buf_size = 128;
    char skill_buf[skill_buf_size];

    // Iterate over settings for each skill.
    for (size_t i = 0; i < SKILL__COUNT__; i++) {
        /* Load default caps */
        GetSkillStr(skill_buf, skill_buf_size, i, "i");

        settingsSkillCaps.insert({
            i,
            ini.GetLongValue("SkillCaps", skill_buf, 100)
        });

        settingsSkillFormulaCaps.insert({
            i,
            ini.GetLongValue("SkillFormulaCaps", skill_buf, 100)
        });

        /* Load EXP multipliers */
        GetSkillStr(skill_buf, skill_buf_size, i, "f");

        settingsSkillExpGainMults.insert({
            i,
            atof(ini.GetValue("SkillExpGainMults", skill_buf, "1.00"))
        });

        settingsLevelSkillExpMults.insert({
            i,
            atof(ini.GetValue("LevelSkillExpMults", skill_buf, "1.00"))
        });

        GetSkillStr(skill_buf, skill_buf_size, i, "SkillExpGainMults\\CharacterLevel\\");
        ReadFloatLevelListSection(ini, settingsSkillExpGainMultsWithPCLevel[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, i, "SkillExpGainMults\\BaseSkillLevel\\");
        ReadFloatLevelListSection(ini, settingsSkillExpGainMultsWithSkills[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, i, "LevelSkillExpMults\\CharacterLevel\\");
        ReadFloatLevelListSection(ini, settingsLevelSkillExpMultsWithPCLevel[i], skill_buf, 1.00);

        GetSkillStr(skill_buf, skill_buf_size, i, "LevelSkillExpMults\\BaseSkillLevel\\");
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

    // Save the configuration, if necessary.
    if (needSave) {
        return SaveConfig(ini, path);
    } else {
        return true;
    }
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

    // Clear the INI file; we will rewrite it.
    ini.Reset();

    // Reset the general information.
    settingsGeneral.version = CONFIG_VERSION;
    ini.SetLongValue("General", "Version", CONFIG_VERSION, "# Configuration file version, DO NOT CHANGE");
    ini.SetValue("General", "Author", "Kassent", NULL);

    // Save the per-skill settings.
    for (int i = 0; i < SKILL__COUNT__; i++) {
        /* Skill cap settings */
        GetSkillStr(skill_buf, skill_buf_size, i, "i");

        ini.SetLongValue(
            "SkillCaps",
            skill_buf,
            settingsSkillCaps[i],
            (!i) ? kSkillCapsDesc : NULL
        );

        ini.SetLongValue(
            "SkillFormulaCaps",
            skill_buf,
            settingsSkillFormulaCaps[i],
            (!i) ? kSkillFormulaCapsDesc : NULL
        );

        /* Exp multiplier settings */
        GetSkillStr(skill_buf, skill_buf_size, i, "f");

        IniSetValueFloat(
            ini,
            "SkillExpGainMults",
            skill_buf,
            settingsSkillExpGainMults[i],
            (!i) ? kSkillExpGainMultsDesc : NULL
        );

        IniSetValueFloat(
            ini,
            "LevelSkillExpMults",
            skill_buf,
            settingsLevelSkillExpMults[i],
            (!i) ? kLevelSkillExpMultsDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, i, "SkillExpGainMults\\CharacterLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsSkillExpGainMultsWithPCLevel[i],
            skill_buf,
            (!i) ? kSkillExpGainMultsWithPCLevelDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, i, "SkillExpGainMults\\BaseSkillLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsSkillExpGainMultsWithSkills[i],
            skill_buf,
            (!i) ? kSkillExpGainMultsWithSkillsDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, i, "LevelSkillExpMults\\CharacterLevel\\");
        SaveFloatLevelListSection(
            ini,
            settingsLevelSkillExpMultsWithPCLevel[i],
            skill_buf,
            (!i) ? kLevelSkillExpMultsWithPCLevelDesc : NULL
        );

        GetSkillStr(skill_buf, skill_buf_size, i, "LevelSkillExpMults\\BaseSkillLevel\\");
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

    ini->SetBoolValue(
        "LegendarySkill",
        "bLegendaryKeepSkillLevel",
        settings.settingsLegendarySkill.bLegendaryKeepSkillLevel,
        kLegendaryKeepSkillLevelDesc
    );
    ini->SetBoolValue(
        "LegendarySkill",
        "bHideLegendaryButton",
        settings.settingsLegendarySkill.bHideLegendaryButton,
        kHideLegendaryButtonDesc
    );
    ini->SetLongValue(
        "LegendarySkill",
        "iSkillLevelEnableLegendary",
        settings.settingsLegendarySkill.iSkillLevelEnableLegendary,
        kSkillLevelEnableLegendaryDesc
    );
    ini->SetLongValue(
        "LegendarySkill",
        "iSkillLevelAfterLegendary",
        settings.settingsLegendarySkill.iSkillLevelAfterLegendary,
        kSkillLevelAfterLegendaryDesc
    );

    // Save the generated INI file.
    SI_Error er = ini->SaveFile(path.c_str());
    if (er < SI_OK) {
        _ERROR("Can't save config file ret:%d errno:%d", (int)er,  errno);
        return false;
    } else {
        _MESSAGE("Config file updated.");
        return true;
    }
}
