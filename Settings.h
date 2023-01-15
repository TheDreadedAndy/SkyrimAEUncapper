/**
 * @file Settings.h
 * @brief Defines the class which is used to load and interact with the settings
 *        specified by the player in the INI file.
 * @author Kassent.
 * @author Andrew Spaulding (Kasplat)
 *
 * Rampaged through by Kasplat to change the interface/encapsulation to be
 * more clear.
*/

#ifndef __SKYRIM_UNCAPPER_AE_SETTINGS_H__
#define __SKYRIM_UNCAPPER_AE_SETTINGS_H__

#include <string>
#include <vector>

#include "Compare.h"
#include "SkillSlot.h"
#include "Ini.h"
#include "ActorAttribute.h"

#define CONFIG_VERSION 6

// Comment on each leveled setting description.
#define LEVELED_SETTING_NOTE\
    " If a specific level is not specified, then "\
    "the value for the closest lower level is used."

template<typename T>
class LeveledSetting {
  private:
    struct LevelItem {
        unsigned int level;
        T item;
    };

    /// @brief The buffer size used to concat the section and subsection.
    static const size_t kBufSize = 256;

    std::vector<LevelItem> list;
    const char *section;
    T defaultVal;

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
            ASSERT(mid < list.size());
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
        ASSERT(hi <= list.size());
        list.insert(list.begin() + hi, { level, item });
    }

    /**
     * @brief Reads in the configuration from the given INI file.
     * @param ini The INI to read the config from.
     * @param sec The section to read in the INI.
     * @param val The default value if none is found.
     */
    void
    InternalReadConfig(
        CSimpleIniA &ini,
        const char *sec,
        T val
    ) {
        CSimpleIniA::TNamesDepend keys;

        list.clear();
        if (ini.GetAllKeys(sec, keys)) {
            for (auto& element : keys) {\
                Add(
                    atoi(element.pItem),
                    ReadIniValue(ini, sec, element.pItem, val)
                );
            }
        }

        Add(0, val);
        list.shrink_to_fit();
    }

    /**
     * @brief Saves the content of the list to the given INI file.
     * @param ini The INI file to write to.
     * @param sec The section to write to.
     * @param comment The comment to write to the first element.
     */
    void
    InternalSaveConfig(
        CSimpleIniA &ini,
        const char *sec,
        const char *comment
    ) {
        char key[16];
        for (size_t i = 0; i < list.size(); i++) {
            ASSERT(sprintf_s(key, "%d", list[i].level) > 0);\
            SaveIniValue(
                ini,
                sec,
                key,
                list[i].item,
                (!i) ? (comment) : NULL
            );
        }
    }

  public:
    /// @brief Default constructor. Must give args to ReadConfig()/SaveConfig().
    LeveledSetting(
    ) : list(0),
        section(nullptr),
        defaultVal(0)
    {}

    /**
     * @brief Constructs a leveled setting with the given section and default.
     *
     * If this initializer is used, then it is illegal to call ReadConfig()
     * and SaveConfig() with more than the ini argument.
     *
     * @param section The section to read/write the setting to.
     * @param default The default value for the setting.
     */
    LeveledSetting(
        const char *section,
        T default_val
    ) : list(0),
        section(section),
        defaultVal(default_val)
    {}

    /**
     * @brief Reads in the configuration from the given INI file.
     *
     * It is illegal to call this function if the list was initialized with
     * the non-default constructor.
     *
     * @param ini The ini file to read the configuration from.
     * @param sec The section to read the configuration from.
     * @param subsec The subsection to read the config from.
     * @param val A default value if no config is found.
     */
    void
    ReadConfig(
        CSimpleIniA &ini,
        const char *sec,
        const char *subsec,
        T val
    ) {
        ASSERT(section == nullptr);

        char buf[kBufSize];
        auto res = sprintf_s(buf, "%s%s", sec, subsec);
        ASSERT((0 < res) && (res < kBufSize));
        InternalReadConfig(ini, buf, val);
    }

    /**
     * @brief Reads in the configuration from the given INI file.
     *
     * It is illegal to call this function if the list was initialized with
     * the default constuctor.
     *
     * @param ini The ini file to read the configuration from.
     */
    void
    ReadConfig(
        CSimpleIniA &ini
    ) {
        ASSERT(section);
        InternalReadConfig(ini, section, defaultVal);
    }

    /**
     * @brief Saves the content of the list to the given INI file.
     *
     * It is illegal to call this function if the list was initialized with
     * the non-default constructor.
     *
     * @param ini The INI file to save the content to.
     * @param sec The section to save the configuration to.
     * @param subsec The subsection to save the configuration to.
     * @param comment The comment to add to the top of the section.
     */
    void
    SaveConfig(
        CSimpleIniA &ini,
        const char *sec,
        const char *subsec,
        const char *comment
    ) {
        ASSERT(section == nullptr);

        char buf[kBufSize];
        auto res = sprintf_s(buf, "%s%s", sec, subsec);
        ASSERT((0 < res) && (res < kBufSize));
        InternalSaveConfig(ini, buf, comment);
    }

    /**
     * @brief Saves the content of the list to the given INI file.
     *
     * It is illegal to call this function if the list was initialized with
     * the default constructor.
     *
     * @param ini The INI file to save the content to.
     * @param comment The comment to add to the top of the section.
     */
    void
    SaveConfig(
        CSimpleIniA &ini,
        const char *comment
    ) {
        ASSERT(section);
        InternalSaveConfig(ini, section, comment);
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
            ASSERT(mid < list.size());
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
     * @param level The new level of the player.
     * @return The increment from the previous level.
     */
    unsigned int
    GetCumulativeDelta(
        unsigned int level
    ) {
        ASSERT(list.size() > 0);

        T pacc = 0, acc = 0;
        for (size_t i = 0; (i < list.size()) && (list[i].level <= level); i++) {
            // Update the accumulation. Note the exclusive upper bound on level.
            unsigned int bound = ((i + 1) < list.size()) ? list[i + 1].level : level + 1;
            unsigned int this_level = MIN(level + 1, bound);
            acc += (this_level - list[i].level) * list[i].item;

            // If this is the last iteration, get the previous accumulation.
            if (((i + 1) >= list.size()) || (list[i + 1].level > level)) {
                pacc = acc - list[i].item;
            }
        }

        return static_cast<unsigned int>(acc) - static_cast<unsigned int>(pacc);
    }
};

template <typename T>
class SkillSetting {
  private:
    /// @brief Size of the prefixed skill field buffer.
    static const size_t kBufSize = 32;

    T val;

  public:
    SkillSetting() {}

    /**
     * @brief Gets the value contained by this skill setting.
     */
    inline T
    Get() {
        return val;
    }

    /**
     * @brief Reads in the config from the given ini file.
     * @param ini The INI to read from.
     * @param section The section to read from.
     * @param field The field to read from.
     * @param default_val The value to assume if the data is not available.
     */
    void
    ReadConfig(
        CSimpleIniA &ini,
        const char *section,
        const char *field,
        T default_val
    ) {
        char buf[kBufSize];
        auto res = sprintf_s(buf, "%c%s", GetPrefix<T>(), field);
        ASSERT((0 < res) && (res < kBufSize));
        val = ReadIniValue(ini, section, buf, default_val);
    }

    /**
     * @brief Saves the current value to the given INI file.
     * @param ini The INI to save to.
     * @param section The section to save to.
     * @param field The field to write to.
     * @param comment The comment to add to the field.
     */
    void
    SaveConfig(
        CSimpleIniA &ini,
        const char *section,
        const char *field,
        const char *comment
    ) {
        char buf[kBufSize];
        auto res = sprintf_s(buf, "%c%s", GetPrefix<T>(), field);
        ASSERT((0 < res) && (res < kBufSize));
        SaveIniValue(ini, section, buf, val, comment);
    }
};

template <template<typename> class T, typename U>
class SkillSettingManager {
  private:
    const char *section;
    T<U> data[SkillSlot::kCount];
    U defaultVal;

  public:
    SkillSettingManager(
        const char *section,
        U default_val
    ) : section(section),
        defaultVal(default_val)
    {}

    /**
     * @brief Gets the setting for the given skill.
     * @param The skill to get the setting for.
     */
    T<U>&
    Get(
        SkillSlot::t skill
    ) {
        return data[skill];
    }

    /**
     * @brief Reads in the given configuration file.
     * @param ini The ini file to read.
     */
    void
    ReadConfig(
        CSimpleIniA &ini
    ) {
        for (int i = 0; i < SkillSlot::kCount; i++) {
            SkillSlot::t slot = static_cast<SkillSlot::t>(i);
            data[i].ReadConfig(ini, section, SkillSlot::Str(slot), defaultVal);
        }
    }

    /**
     * @brief Writes out the configuration to the given INI file.
     * @param ini The ini file to write the configuration to.
     * @param comment The comment to write to the first element.
     */
    void
    SaveConfig(
        CSimpleIniA &ini,
        const char *comment
    ) {
        for (int i = 0; i < SkillSlot::kCount; i++) {
            SkillSlot::t slot = static_cast<SkillSlot::t>(i);
            data[i].SaveConfig(ini, section, SkillSlot::Str(slot), (!i) ? comment : NULL);
        }
    }
};

class Settings {
  private:
    class GeneralSettings {
      private:
        const char *const kSection = "General";

      public:
        SectionField<unsigned int> version;
        SectionField<std::string> author;

        GeneralSettings(
        ) : version("Version", 0),
            author("Author", "Kassent")
        {}

        void ReadConfig(CSimpleIniA &ini);
        void SaveConfig(CSimpleIniA &ini);
    };

    class LegendarySettings {
      private:
        /// @brief Field comments
        ///@{
        const char *const kKeepSkillLevelDesc =
            "# This option determines whether the legendary feature will reset the "
            "skill level. Set this option to true will make option "
            "\"iSkillLevelAfterLegendary\" have no effect.";
        const char *const kHideButtonDesc =
            "# This option determines whether to hide the legendary button in "
            "StatsMenu when you meet the requirements of legendary skills. "
            "If you set \"iSkillLevelEnableLegendary\" to below 100, the legendary "
            "button will not show up, but you can make skills legendary normally "
            "by pressing SPACE.";
        const char *const kSkillLevelEnableDesc =
            "# This option determines the skill level required to make a skill "
            "legendary.";
        const char *const kSkillLevelAfterDesc =
            "# This option determines the level of a skill after making this skill "
            "legendary. Set this option to 0 will reset the skill level to default "
            "level.";
        ///@}

        const char *const kSection = "LegendarySkill";

      public:
        SectionField<bool> keepSkillLevel;
        SectionField<bool> hideButton;
        SectionField<unsigned int> skillLevelEnable;
        SectionField<unsigned int> skillLevelAfter;

        LegendarySettings(
        ) : keepSkillLevel("bLegendaryKeepSkillLevel", false),
            hideButton("bHideLegendaryButton", true),
            skillLevelEnable("iSkillLevelEnableLegendary", 100),
            skillLevelAfter("iSkillLevelAfterLegendary", 0)
        {}

        void ReadConfig(CSimpleIniA &ini);
        void SaveConfig(CSimpleIniA &ini);
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
        "# Set the number of perks gained at each level up."
        LEVELED_SETTING_NOTE;
    const char *const kHealthAtLevelUpDesc =
        "# Set the number of health gained at each health level up."
        LEVELED_SETTING_NOTE;
    const char *const kHealthAtMagickaLevelUpDesc =
        "# Set the number of health gained at each magicka level up."
        LEVELED_SETTING_NOTE;
    const char *const kHealthAtStaminaLevelUpDesc =
        "# Set the number of health gained at each stamina level up."
        LEVELED_SETTING_NOTE;
    const char *const kMagickaAtLevelUpDesc =
        "# Set the number of magicka gained at each magicka level up."
        LEVELED_SETTING_NOTE;
    const char *const kMagickaAtHealthLevelUpDesc =
        "# Set the number of magicka gained at each health level up."
        LEVELED_SETTING_NOTE;
    const char *const kMagickaAtStaminaLevelUpDesc =
        "# Set the number of magicka gained at each stamina level up."
        LEVELED_SETTING_NOTE;
    const char *const kStaminaAtLevelUpDesc =
        "# Set the number of stamina gained at each stamina level up."
        LEVELED_SETTING_NOTE;
    const char *const kStaminaAtHealthLevelUpDesc =
        "# Set the number of stamina gained at each health level up."
        LEVELED_SETTING_NOTE;
    const char *const kStaminaAtMagickaLevelUpDesc =
        "# Set the number of stamina gained at each magicka level up."
        LEVELED_SETTING_NOTE;
    const char *const kCarryWeightAtHealthLevelUpDesc =
        "# Set the number of carryweight gained at each health level up."
        LEVELED_SETTING_NOTE;
    const char *const kCarryWeightAtMagickaLevelUpDesc =
        "# Set the number of carryweight gained at each magicka level up."
        LEVELED_SETTING_NOTE;
    const char *const kCarryWeightAtStaminaLevelUpDesc =
        "# Set the number of carryweight gained at each stamina level up."
        LEVELED_SETTING_NOTE;
    ///@}

    bool SaveConfig(CSimpleIniA &ini, const std::string &path);

    GeneralSettings general;

    SkillSettingManager<SkillSetting, unsigned int> skillCaps;
    SkillSettingManager<SkillSetting, unsigned int> skillFormulaCaps;

    LeveledSetting<float> perksAtLevelUp;

    SkillSettingManager<SkillSetting, float> skillExpGainMults;
    SkillSettingManager<LeveledSetting, float> skillExpGainMultsWithSkills;
    SkillSettingManager<LeveledSetting, float> skillExpGainMultsWithPCLevel;

    SkillSettingManager<SkillSetting, float> levelSkillExpMults;
    SkillSettingManager<LeveledSetting, float> levelSkillExpMultsWithSkills;
    SkillSettingManager<LeveledSetting, float> levelSkillExpMultsWithPCLevel;

    LeveledSetting<unsigned int> healthAtLevelUp;
    LeveledSetting<unsigned int> healthAtMagickaLevelUp;
    LeveledSetting<unsigned int> healthAtStaminaLevelUp;
    LeveledSetting<unsigned int> magickaAtLevelUp;
    LeveledSetting<unsigned int> magickaAtHealthLevelUp;
    LeveledSetting<unsigned int> magickaAtStaminaLevelUp;
    LeveledSetting<unsigned int> staminaAtLevelUp;
    LeveledSetting<unsigned int> staminaAtHealthLevelUp;
    LeveledSetting<unsigned int> staminaAtMagickaLevelUp;
    LeveledSetting<unsigned int> carryWeightAtHealthLevelUp;
    LeveledSetting<unsigned int> carryWeightAtMagickaLevelUp;
    LeveledSetting<unsigned int> carryWeightAtStaminaLevelUp;

    LegendarySettings legendary;

  public:
    Settings(
    ) : general(),
        skillCaps("SkillCaps", 100),
        skillFormulaCaps("SkillFormulaCaps", 100),
        perksAtLevelUp("PerksAtLevelUp", 1.00),
        skillExpGainMults("SkillExpGainMults", 1.00),
        skillExpGainMultsWithPCLevel("SkillExpGainMults\\CharacterLevel\\", 1.00),
        skillExpGainMultsWithSkills("SkillExpGainMults\\BaseSkillLevel\\", 1.00),
        levelSkillExpMults("LevelSkillExpMults", 1.00),
        levelSkillExpMultsWithPCLevel("LevelSkillExpMults\\CharacterLevel\\", 1.00),
        levelSkillExpMultsWithSkills("LevelSkillExpMults\\BaseSkillLevel\\", 1.00),
        healthAtLevelUp("HealthAtLevelUp", 10),
        healthAtMagickaLevelUp("HealthAtMagickaLevelUp", 0),
        healthAtStaminaLevelUp("HealthAtStaminaLevelUp", 0),
        magickaAtLevelUp("MagickaAtLevelUp", 10),
        magickaAtHealthLevelUp("MagickaAtHealthLevelUp", 0),
        magickaAtStaminaLevelUp("MagickaAtStaminaLevelUp", 0),
        staminaAtLevelUp("StaminaAtLevelUp", 10),
        staminaAtHealthLevelUp("StaminaAtHealthLevelUp", 0),
        staminaAtMagickaLevelUp("StaminaAtMagickaLevelUp", 0),
        carryWeightAtHealthLevelUp("CarryWeightAtHealthLevelUp", 0),
        carryWeightAtMagickaLevelUp("CarryWeightAtMagickaLevelUp", 0),
        carryWeightAtStaminaLevelUp("CarryWeightAtStaminaLevelUp", 5),
        legendary()
    {}

    bool ReadConfig(const std::string& path);

    bool IsManagedSkill(unsigned int skill_id);
    float GetSkillCap(unsigned int skill_id);
    float GetSkillFormulaCap(unsigned int skill_id);
    unsigned int GetPerkDelta(unsigned int player_level);
    float GetSkillExpGainMult(unsigned int skill_id, unsigned int skill_level,
                              unsigned int player_level);
    float GetLevelSkillExpMult(unsigned int skill_id, unsigned int skill_level,
                               unsigned int player_level);
    void GetAttributeLevelUp(unsigned int player_level, ActorAttribute attr,
                             ActorAttributeLevelUp &level_up);
    bool IsLegendaryButtonVisible(unsigned int skill_level);
    bool IsLegendaryAvailable(unsigned int skill_level);
    float GetPostLegendarySkillLevel(float default_reset, float base_level);
};

extern Settings settings;

#endif /* __SKYRIM_UNCAPPER_AE_SETTINGS_H__ */

