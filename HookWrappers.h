/**
 * @file HookWrappers.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exports the hook wrapper code to C/C++.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__
#define __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__

#include <cstdint>

extern "C" uintptr_t SkillEffectiveCapPatch_Wrapper;
extern "C" uintptr_t SkillCapPatch_Wrapper;

#endif /* __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__ */
