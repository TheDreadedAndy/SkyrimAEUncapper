/**
 * @file SkillSlot.cpp
 * @author Andrew Spaulding (Kasplat)
 * @brief Implementation of member functions for the PlayerSkillSlot class.
 * @bug No known bugs.
 */

#include "SkillSlot.h"

const char *const SkillSlot::kSkillNames[kCount] = {
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

/**
 * @brief Converts the given skill ID to a player skill enumeration.
 *
 * The provided ID must be valid.
 *
 * @param id The ID to be converted.
 */
SkillSlot::t
SkillSlot::FromAttribute(
    ActorAttribute::t attr
) {
    ASSERT(ActorAttribute::IsSkill(attr));
    return static_cast<t>(static_cast<int>(attr) - kOffset);
}

/**
 * @brief Converts the given skill type to a string.
 *
 * The returned string must not be freed.
 *
 * @param slot The skill to convert.
 */
const char *
SkillSlot::Str(
    t slot
) {
    ASSERT(slot < kCount);
    return kSkillNames[slot];
}
