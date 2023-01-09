/**
 * @file Settings.h
 * @brief Defines the class which is used to load and interact with the settings
 *        specified by the player in the INI file.
 * @author Kassant.
 *
 * Rampaged through by Kasplat to change the interface/encapsulation to be
 * more clear.
 */

#ifndef __SKYRIM_UNCAPPER_AE_SETTINGS_H__
#define __SKYRIM_UNCAPPER_AE_SETTINGS_H__

#include <map>
#include <string>
#include <algorithm>
#include <vector>

#include "common/IErrors.h"
#include "SimpleIni.h"

#define CONFIG_VERSION 5

template<typename T>
class LeveledSetting {
  public:
    struct LevelItem {
        unsigned int level;
        T item;
    };

  private:
    std::vector<LevelItem> list;

  public:
    LeveledSetting() : list(0) {}

    /**
     * @brief Adds an item to the leveled setting list.
     *
     * If the given item is already in the setting list, it will not be added
     * again.
     *
     * @param level The level for the item.
     * @param item The item to be added.
     */
    void
    Add(
        unsigned int level,
        T item
    ) {
        // Store the items in sorted order, so we can binary search for the
        // nearest later.
        size_t lo = 0, hi = list.size();
        size_t mid = lo + ((hi - lo) >> 1);
        while (lo < hi) {
            if (level < list[mid].level) {
                hi = mid;
            } else if (level > list[mid].level) {
                lo = mid + 1;
            } else {
                return;
            }

            mid = lo + ((hi - lo) >> 1);
        }

        // Insert before the final hi element.
        list.insert(list.begin() + hi, { level, item });
    }

    /**
     * @brief Finds the value closest to the given level in the list.
     *
     * Note that only values whose level is less than or equal to the given
     * level will be considered.
     *
     * @param level The level to search for an item for.
     * @return The associated value.
     */
    T
    GetNearest(
        unsigned int level
    ) {
        ASSERT(list.size() > 0);

        size_t lo = 0, hi = list.size();
        size_t mid = lo + ((hi - lo) >> 1);
        while (lo < hi) {
            if ((list[mid].level <= level)
                    && ((mid + 1 == list.size()) || (level < list[mid + 1].level))) {
                return list[mid].item;
            } else if (level < list[mid].level) {
                hi = mid;
            } else {
                ASSERT((level > list[mid].level) || (level >= list[mid + 1].level));
                lo = mid + 1;
            }

            mid = lo + ((hi - lo) >> 1);
        }

        // If no direct match was found, return the closest lo item.
        return list[lo].item;
    }

    /**
     * @brief Accumulates the values across all previous levels, and determines
     *        what the increment from the last level was.
     *
     * This function is intended to be used for the calculation of partial
     * perk point awards.
     *
     * @param level The level to calculate the delta to.
     * @return The increment from the previous level.
     */
    unsigned int
    GetCumulativeDelta(
        unsigned int level
    ) {
        ASSERT(list.size() > 0);

        size_t plevel = 0;
        T pacc = 0, acc = 0;
        for (size_t i = 0; (i < list.size()) && (list[i].level <= level); i++) {
            // Update the accumulation.
            unsigned int this_level = (level < list[i].level) ? level : list[i].level;
            acc += (this_level - plevel) * list[i].item;

            // If this is the last iteration, get the previous accumulation.
            if (((i + 1) >= list.size()) || (list[i].level > level)) {
                pacc = acc - list[i].item;
            }
        }

        return static_cast<unsigned int>(acc) - static_cast<unsigned int>(pacc);
    }

    /**
     * @brief Shrinks the underlying vector to fit its contents.
     */
    void
    Finalize() {
        list.shrink_to_fit();
    }

    /**
     * @brief Gets the number of level sections in the list.
     */
    size_t
    Size() {
        return list.size();
    }

    /**
     * @brief Gets the LevelItem at the given index.
     */
    void
    GetItem(
        size_t i,
        LevelItem &ret
    ) {
        ASSERT(i < list.size());
        ret = list[i];
    }
};

class Settings {
  private:
    /// @brief Offset from raw skill IDs to the skill enumeration values.
    const unsigned int kSkillOffset = 6;

    /**
     * @brief Encodes the skills in the order of their IDs.
     *
     * Note that we must subtract 6 from a skill ID to convert it to this enum.
     * As such, the order of the values in this enumeration MUST NOT be changed.
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
        kSkillCount
    } player_skill_e;

    /// @brief Used to convert a skill enum to a skill name.
    const char *const kSkillNames[kSkillCount] = {
        "OneHanded",
        "TwoHanded",
        "Marksman",
        "Block",
        "Smithing",
        "HeavyArmor",
        "LightArmor",
        "Pickpocket",
        "LockPicking",
        "Sneak",
        "Alchemy",
        "SpeechCraft",
        "Alteration",
        "Conjuration",
        "Destruction",
        "Illusion",
        "Restoration",
        "Enchanting"
    };

    /// @brief Desciptions for sections in the INI file.
    ///@{
    const char *const kSkillCapsDesc =
        "# Set the skill level cap. This option determines the upper limit of "
        "skill level you can reach.";
    const char *const kSkillFormulaCapsDesc =
        "# Set the skill formula cap. This option determines the upper limit of"
        " skill level used in the calculation of all kinds of magic effects.";
    const char *const kSkillExpGainMultsDesc =
        "# Set the skill experience gained multiplier. The skill experience "
        "you gained actually = The final calculated experience value right "
        "before it is given to the character after any experience "
        "modification * SkillExpGainMult * Corresponding "
        "Sub-SkillExpGainMult listed below.";
    const char *const kSkillExpGainMultsWithPCLevelDesc =
        "# All the subsections of SkillExpGainMults below allow to set an "
        "additional multiplier depending on CHARACTER LEVEL, independantly "
        "for each skill.";
    const char *const kSkillExpGainMultsWithSkillsDesc =
        "# All the subsections of SkillExpGainMults below allow to set an "
        "additional multiplier depending on BASE SKILL LEVEL, independantly "
        "for each skill.";
    const char *const kLevelSkillExpMultsDesc =
        "# Set the skill experience to PC experience multipliers. When you "
        "level up a skill, the PC experience you gained actually = Current "
        "base skill level * LevelSkillExpMults * Corresponding "
        "Sub-LevelSkillExpMults listed below.";
    const char *const kLevelSkillExpMultsWithPCLevelDesc =
        "# All the subsections of LevelSkillExpMults below allow to set an "
        "additional multipliers depending on CHARACTER LEVEL, independantly "
        "for each skill.";
    const char *const kLevelSkillExpMultsWithSkillsDesc =
        "# All the subsections of LevelSkillExpMults below allow to set an "
        "additional multipliers depending on BASE SKILL LEVEL, independantly "
        "for each skill.";
    const char *const kPerksAtLevelUpDesc =
        "# Set the number of perks gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    const char *const kHealthAtLevelUpDesc =
        "# Set the number of health gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    const char *const kMagickaAtLevelUpDesc =
        "# Set the number of magicka gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    const char *const kStaminaAtLevelUpDesc =
        "# Set the number of stamina gained at each level up. If a specific "
        "level is not specified then the closest lower level setting is used.";
    const char *const kCarryWeightAtHealthLevelUpDesc =
        "# Set the number of carryweight gained at each health level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    const char *const kCarryWeightAtMagickaLevelUpDesc =
        "# Set the number of carryweight gained at each magicka level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    const char *const kCarryWeightAtStaminaLevelUpDesc =
        "# Set the number of carryweight gained at each stamina level up. "
        "If a specific level is not specified then the closest lower level "
        "setting is used.";
    const char *const kLegendaryKeepSkillLevelDesc =
        "# This option determines whether the legendary feature will reset the "
        "skill level. Set this option to true will make option "
        "\"iSkillLevelAfterLegendary\" have no effect.";
    const char *const kHideLegendaryButtonDesc =
        "# This option determines whether to hide the legendary button in "
        "StatsMenu when you meet the requirements of legendary skills. "
        "If you set \"iSkillLevelEnableLegendary\" to below 100, the legendary "
        "button will not show up, but you can make skills legendary normally "
        "by pressing SPACE.";
    const char *const kSkillLevelEnableLegendaryDesc =
        "# This option determines the skill level required to make a skill "
        "legendary.";
    const char *const kSkillLevelAfterLegendaryDesc =
        "# This option determines the level of a skill after making this skill "
        "legendary. Set this option to 0 will reset the skill level to default "
        "level.";
    ///@}

    void GetSkillStr(char *buf, size_t size, player_skill_e skill,
                     const char *prefix);
    player_skill_e GetSkillFromId(unsigned int skill_id);
    bool SaveConfig(CSimpleIniA &ini, const std::string &path);

    unsigned int settingsSkillCaps[kSkillCount];
    unsigned int settingsSkillFormulaCaps[kSkillCount];

    LeveledSetting<float>  settingsPerksAtLevelUp;

    float                  settingsSkillExpGainMults[kSkillCount];
    LeveledSetting<float>  settingsSkillExpGainMultsWithSkills[kSkillCount];
    LeveledSetting<float>  settingsSkillExpGainMultsWithPCLevel[kSkillCount];

    float                  settingsLevelSkillExpMults[kSkillCount];
    LeveledSetting<float>  settingsLevelSkillExpMultsWithSkills[kSkillCount];
    LeveledSetting<float>  settingsLevelSkillExpMultsWithPCLevel[kSkillCount];

    LeveledSetting<UInt32> settingsHealthAtLevelUp;
    LeveledSetting<UInt32> settingsMagickaAtLevelUp;
    LeveledSetting<UInt32> settingsStaminaAtLevelUp;
    LeveledSetting<UInt32> settingsCarryWeightAtHealthLevelUp;
    LeveledSetting<UInt32> settingsCarryWeightAtMagickaLevelUp;
    LeveledSetting<UInt32> settingsCarryWeightAtStaminaLevelUp;

  public:
    /// @brief Encodes the attribute selection during level-up.
    typedef enum {
        ATTR_HEALTH = 0x18,
        ATTR_MAGICKA,
        ATTR_STAMINA
    } player_attr_e;

    Settings();
    bool ReadConfig(const std::string& path);

    float GetSkillCap(unsigned int skill_id);
    float GetSkillFormulaCap(unsigned int skill_id);
    unsigned int GetPerkDelta(unsigned int player_level);
    float GetSkillExpGainMult(unsigned int skill_id, unsigned int skill_level,
                              unsigned int player_level);
    float GetLevelSkillExpMult(unsigned int skill_id, unsigned int skill_level,
                               unsigned int player_level);
    void GetAttributeLevelUp(unsigned int player_level, player_attr_e attr,
                             UInt32 &attr_up, float &carry_up);

    struct {
        UInt32 version;
        std::string author;
    } settingsGeneral;

    struct {
        bool bLegendaryKeepSkillLevel;
        bool bHideLegendaryButton;
        UInt32 iSkillLevelEnableLegendary;
        UInt32 iSkillLevelAfterLegendary;
    } settingsLegendarySkill;
};

extern Settings settings;

#endif /* __SKYRIM_UNCAPPER_AE_SETTINGS_H__ */
