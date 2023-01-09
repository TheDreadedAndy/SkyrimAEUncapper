/**
 * @file HookWrappers.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exports the hook wrapper code to C/C++.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__
#define __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__

#include <cstdint>

extern "C" void SkillEffectiveCapPatch_Wrapper(void);
extern "C" void SkillCapPatch_Wrapper(void);
extern "C" void ModifyPerkPool_Wrapper(void);
extern "C" void ImproveLevelExpBySkillLevel_Wrapper(void);
extern "C" void ImprovePlayerSkillPoints_Original(void *skillData, UInt32 skillID, float exp, UInt64 unk1, UInt32 unk2, UInt8 unk3, bool unk4);
extern "C" UInt64 ImproveAttributeWhenLevelUp_Original(void *unk0, UInt8 unk1);

#endif /* __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__ */
