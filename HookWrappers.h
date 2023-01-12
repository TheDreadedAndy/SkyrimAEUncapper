/**
 * @file HookWrappers.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Exports the hook wrapper code to C/C++.
 * @bug No known bugs.
 */

#ifndef __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__
#define __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__

extern "C" void SkillCapPatch_Wrapper(void);
extern "C" float GetEffectiveSkillLevel_Original(void*, UInt32);
extern "C" void DisplayTrueSkillLevel_Hook(void);

extern "C" void ImproveLevelExpBySkillLevel_Wrapper(void);
extern "C" void ImprovePlayerSkillPoints_Original(
    void *skill_data,
    UInt32 skill_id,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
);
extern "C" void ModifyPerkPool_Wrapper(void);
extern "C" UInt64 ImproveAttributeWhenLevelUp_Original(void *unk0, UInt8 unk1);
extern "C" void LegendaryResetSkillLevel_Wrapper(void);

extern "C" void CheckConditionForLegendarySkill_Wrapper(void);
extern "C" void HideLegendaryButton_Wrapper(void);

#endif /* __SKYRIM_UNCAPPER_AE_HOOK_WRAPPERS_H__ */
