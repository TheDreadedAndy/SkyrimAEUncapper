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
enum class ActorAttribute {
    Health = 0x18,
    Magicka,
    Stamina,
    CarryWeight = 0x20
};

/**
 * @brief Contains the delta value for each attribute at a given level up.
 */
struct ActorAttributeLevelUp {
    float health;
    float magicka;
    float stamina;
    float carry_weight;
};

#endif /* __SKYRIM_UNCAPPER_AE_ACTOR_ATTRIBUTE_H__ */
