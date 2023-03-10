/**
 * @file SkillSlot.h
 * @author Andrew Spaulding (Kasplat)
 * @brief A class-enum for converting attribute IDs to skill slots.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__
#define __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__

#include "ActorAttribute.h"

/**
 * @brief Encodes the various skill slots of the player.
 */
class SkillSlot {
  public:
    /**
     * @brief Encodes the skills in the order of their IDs.
     *
     * Note that we must subtract 6 from an ActorAttribute to convert it to
     * this enum. As such, the order of the values in this enumeration 
     * MUST NOT be changed.
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

  private:
    /// @brief Used to convert a skill enum to a skill name.
    static const char *const kSkillNames[kCount];

    /// @brief Offset from raw skill IDs to the skill enumeration values.
    static const unsigned int kOffset = 6;

  public:
    static t FromAttribute(ActorAttribute::t attr);
    static const char *Str(t slot);
};

#endif /* __SKYRIM_UNCAPPER_AE_SKILL_SLOT_H__ */
