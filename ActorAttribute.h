/**
 * @file ActorAttribute.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Enumerations and structures to aid in managing attribute level ups.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_ACTOR_ATTRIBUTE_H__
#define __SKYRIM_UNCAPPER_AE_ACTOR_ATTRIBUTE_H__

/**
 * @brief Encodes the attribute IDs used by AVOModCurrent and AVOModBase.
 *
 * These IDs match the games internal IDs, and are simply passed directly to
 * the game.
 */
class ActorAttribute {
  public:
    enum t {
        /* 0x00 - 0x05 unknown */
        OneHanded = 0x06,
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
        Health,
        Magicka,
        Stamina,
        /* 0x1b - 0x1f unknown */
        CarryWeight = 0x20
    };

    static bool IsSkill(t attr);
};

/**
 * @brief Contains the delta value for each body attribute at a given level up.
 */
struct ActorAttributeLevelUp {
    float health;
    float magicka;
    float stamina;
    float carry_weight;
};

#endif /* __SKYRIM_UNCAPPER_AE_ACTOR_ATTRIBUTE_H__ */
