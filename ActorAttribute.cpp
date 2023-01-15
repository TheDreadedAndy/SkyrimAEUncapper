/**
 * @file ActorAttribute.cpp
 * @author Andrew Spaulding (Kasplat)
 * @brief Implementation of helpers for the ActorAttribute class.
 * @bug No known bugs.
 */

#include "ActorAttribute.h"

/**
 * @brief Checks if the given actor attribute is a skill.
 */
bool
ActorAttribute::IsSkill(
    t attr
) {
    return (OneHanded <= attr) && (attr <= Enchanting);
}
