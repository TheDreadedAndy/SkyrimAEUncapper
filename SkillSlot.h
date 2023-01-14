/**
 * @file SkillSlot.h
 * @author Andrew Spaulding (Kasplat)
 * @brief A class-enum for converting attribute IDs to skill slots.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__
#define __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__

/**
 * @brief Encodes the various skill slots of the player.
 */
class SkillSlot {
  private:
    /// @brief Offset from raw skill IDs to the skill enumeration values.
    const unsigned int kOffset = 6;

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

  public:
    /**
     * @brief Encodes the skills in the order of their IDs.
     *
     * Note that we must subtract 6 from a skill ID to convert it to this enum.
     * As such, the order of the values in this enumeration MUST NOT be changed.
     */
    enum t {
        OneHanded,
        TwoHanded,
        Marksman,
        Block,
        Smithing,
        HeavyArmor,
        LightArmor,
        Pickpocket,
        LockPicking,
        Sneak,
        Alchemy,
        SpeechCraft,
        Alteration,
        Conjuration,
        Destruction,
        Illusion,
        Restoration,
        Enchanting,
        kCount
    };

    static bool IsSkill(unsigned int id);
    static t FromId(unsigned int id);
    static const char *Str(t slot);
}

#endif /* __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__ */
