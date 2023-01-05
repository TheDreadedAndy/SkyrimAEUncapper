#pragma once

#include "SimpleIni.h"
#include <map>
#include <string>
#include <algorithm>

#define CONFIG_VERSION 4

// FIXME: What the flying fuck is this doing? ~Kasplat
template<typename T>
struct SettingList : public std::map < UInt32, T>
{
    typename SettingList::mapped_type GetValue(const typename SettingList::key_type& index)
    {
        typename SettingList::mapped_type result = NULL;
        for (auto it = rbegin(); it != rend(); ++it)
        {
            if (it->first <= index)
            {
                result = it->second;
                break;
            }
        }
        return result;
    }

    float GetDecimal(const typename SettingList::key_type& index)
    {
        float decimal = 0.00f;
        for (auto iterator = rbegin(); iterator != rend(); ++iterator)
        {
            if (iterator->first < index)
            {
                if (iterator != --rend())
                {
                    decimal += (index - iterator->first) * (iterator->second);
                    auto it = iterator;
                    ++it;
                    for (; it != rend(); ++it)
                    {
                        auto nextIterator = it;
                        --nextIterator;
                        if (it != --rend())
                            decimal += (nextIterator->first - it->first) * (it->second);
                        else
                            decimal += (nextIterator->first - it->first - 1) * (it->second);
                    }
                }
                else
                    decimal += (index - iterator->first - 1) * (iterator->second);
                break;
            }
        }
        return decimal - static_cast<UInt32>(decimal);
    }
};

struct SettingsGeneral
{
    UInt32                version;
    std::string            author;
};

struct SettingsLegendarySkill
{
    bool                bLegendaryKeepSkillLevel;
    bool                bHideLegendaryButton;
    UInt32                iSkillLevelEnableLegendary;
    UInt32                iSkillLevelAfterLegendary;
};

class Settings {
  private:
    enum {
        kAdvanceableSkillOffset = 6,
        kNumAdvanceableSkills = 18
    };

    /**
     * @brief Encodes the skills in the order of their IDs.
     *
     * Note that we must subtract 6 from a skill ID to convert it to this enum.
     */
    typedef enum {
        SKILL_ONEHANDED,
        SKILL_TWOHANDED,
        SKILL_MARKSMAN,
        SKILL_BLOCK,
        SKILL_SMITHING,
        SKILL_HEAVYARMOR,
        SKILL_LIGHTARMOR,
        SKILL_PICKPOCKET,
        SKILL_LOCKPICKING,
        SKILL_SNEAK,
        SKILL_ALCHEMY,
        SKILL_SPEECHCRAFT,
        SKILL_ALTERATION,
        SKILL_CONJURATION,
        SKILL_DESTRUCTION,
        SKILL_ILLUSION,
        SKILL_RESTORATION,
        SKILL_ENCHANTING,
        SKILL__COUNT__
    } player_skill_e;

    /// @brief Used to convert a skill enum to a skill name.
    static const char *const kSkillNames[] = {
        .[SKILL_ONEHANDED]   = "OneHanded",
        .[SKILL_TWOHANDED]   = "TwoHanded",
        .[SKILL_MARKSMAN]    = "Marksman",
        .[SKILL_BLOCK]       = "Block",
        .[SKILL_SMITHING]    = "Smithing",
        .[SKILL_HEAVYARMOR]  = "HeavyArmor",
        .[SKILL_LIGHTARMOR]  = "LightArmor",
        .[SKILL_PICKPOCKET]  = "Pickpocket",
        .[SKILL_LOCKPICKING] = "LockPicking",
        .[SKILL_SNEAK]       = "Sneak",
        .[SKILL_ALCHEMY]     = "Alchemy",
        .[SKILL_SPEECHCRAFT] = "SpeechCraft",
        .[SKILL_ALTERATION]  = "Alteration",
        .[SKILL_CONJURATION] = "Conjuration",
        .[SKILL_DESTRUCTION] = "Destruction",
        .[SKILL_ILLUSION]    = "Illusion",
        .[SKILL_RESTORATION] = "Restoration",
        .[SKILL_ENCHANTING]  = "Enchanting"
    };

    /// @brief Desciptions for sections in the INI file.
    ///@{
    static const char *const kSkillCapsDesc =
        "# Set the skill level cap. This option determines the upper limit of "
        "skill level you can reach.";
    static const char *const kSkillFormulaCapsDesc =
        "# Set the skill formula cap. This option determines the upper limit of"
        " skill level used in the calculation of all kinds of magic effects.";
    static const char *const kSkillExpGainMultsDesc =
        "# Set the skill experience gained multiplier. The skill experience "
        "you gained actually = The final calculated experience value right "
        "before it is given to the character after any experience "
        "modification * SkillExpGainMult * Corresponding "
        "Sub-SkillExpGainMult listed below.";
    static const char *const kSkillExpGainMultsWithPCLevelDesc =
        "# All the subsections of SkillExpGainMults below allow to set an "
        "additional multiplier depending on CHARACTER LEVEL, independantly "
        "for each skill.";
    static const char *const kSkillExpGainMultsWithSkillsDesc =
        "# All the subsections of SkillExpGainMults below allow to set an "
        "additional multiplier depending on BASE SKILL LEVEL, independantly "
        "for each skill.";
    static const char *const kLevelSkillExpMultsDesc =
        "# Set the skill experience to PC experience multipliers. When you "
        "level up a skill, the PC experience you gained actually = Current "
        "base skill level * LevelSkillExpMults * Corresponding "
        "Sub-LevelSkillExpMults listed below.";
    static const char *const kLevelSkillExpMultsWithPCLevelDesc =
        "# All the subsections of LevelSkillExpMults below allow to set an "
        "additional multipliers depending on CHARACTER LEVEL, independantly "
        "for each skill.";
    static const char *const kLevelSkillExpMultsWithSkillDesc =
        "# All the subsections of LevelSkillExpMults below allow to set an "
        "additional multipliers depending on BASE SKILL LEVEL, independantly "
        "for each skill.";
    static const char *const kPerksAtLevelUpDesc =
        "# Set the number of perks gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    static const char *const kHealthAtLevelUpDesc =
        "# Set the number of health gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    static const char *const kMagickaAtLevelUpDesc =
        "# Set the number of magicka gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    static const char *const kStaminaAtLevelUpDesc =
        "# Set the number of stamina gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    static const char *const kCarryWeightAtHealthLevelUp =
        "# Set the number of carryweight gained at each health level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    static const char *const kCarryWeightAtMagickaLevelUp =
        "# Set the number of carryweight gained at each magicka level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    static const char *const kCarryWeightAtStaminaLevelUp =
        "# Set the number of carryweight gained at each stamina level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    static const char *const kLegendaryKeepSkillLevelDesc =
        "# This option determines whether the legendary feature will reset the "
        "skill level. Set this option to true will make option "
        "\"iSkillLevelAfterLegendary\" have no effect.";
    static const char *const kHideLegendaryButtonDesc =
        "# This option determines whether to hide the legendary button in "
        "StatsMenu when you meet the requirements of legendary skills. "
        "If you set \"iSkillLevelEnableLegendary\" to below 100, the lengedary "
        "button will not show up, but you can make skills lengedary normally "
        "by pressing SPACE.";
    static const char *const kSkillLevelEnableLegendaryDesc =
        "# This option determines the skill level required to make a skill "
        "legendary.";
    static const char *const kSkillLevelAfterLegendaryDesc =
        "# This option determines the level of a skill after making this skill "
        "legendary. Set this option to 0 will reset the skill level to default "
        "level.";
    ///@}

    void GetSkillStr(char *buf, size_t size, player_skill_e skill,
                     const char *prefix);

  public:
    Settings();
    bool ReadConfig(const std::string& path);
    bool SaveConfig(CSimpleIniA* ini, const std::string& path);

    SettingsGeneral                settingsGeneral;
    SettingsLegendarySkill        settingsLegendarySkill;
    SettingList<UInt32>            settingsSkillCaps;
    SettingList<UInt32>            settingsSkillFormulaCaps;
    SettingList<float>            settingsSkillExpGainMults;
    SettingList<float>            settingsLevelSkillExpMults;
    SettingList<float>            settingsPerksAtLevelUp;
    SettingList<UInt32>            settingsHealthAtLevelUp;
    SettingList<UInt32>            settingsMagickaAtLevelUp;
    SettingList<UInt32>            settingsStaminaAtLevelUp;
    SettingList<UInt32>            settingsCarryWeightAtHealthLevelUp;
    SettingList<UInt32>            settingsCarryWeightAtMagickaLevelUp;
    SettingList<UInt32>            settingsCarryWeightAtStaminaLevelUp;
    SettingList<float>            settingsSkillExpGainMultsWithSkills[kNumAdvanceableSkills];
    SettingList<float>            settingsSkillExpGainMultsWithPCLevel[kNumAdvanceableSkills];
    SettingList<float>            settingsLevelSkillExpMultsWithSkills[kNumAdvanceableSkills];
    SettingList<float>            settingsLevelSkillExpMultsWithPCLevel[kNumAdvanceableSkills];
};

extern Settings settings;
