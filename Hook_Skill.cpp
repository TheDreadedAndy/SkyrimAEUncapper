#include "common/IPrefix.h"
#include "GameFormComponents.h"
#include "GameReferences.h"
#include "GameSettings.h"
#include "Relocation.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"

#include "Hook_Skill.h"
#include "HookWrappers.h"
#include "Settings.h"
#include "Signatures.h"
#include "RelocPatch.h"

using LevelData = PlayerSkills::StatData::LevelData;

/**
 * @brief Function definitions for our hooks into the game.
 */
///@{
typedef void(*_ImproveSkillLevel)(void* pPlayer, UInt32 skillID, UInt32 count);
typedef void(*_ImprovePlayerSkillPoints)(void*, UInt32, float, UInt64, UInt32, UInt8, bool);
typedef UInt64(*_ImproveAttributeWhenLevelUp)(void*, UInt8);
typedef bool(*_GetSkillCoefficients)(UInt32, float*, float*, float*, float*);
typedef UInt16(*_GetLevel)(void* pPlayer);
typedef float(*_ImproveLevelExpBySkillLevel)(float skillLevel);
typedef float(*_GetCurrentActorValue)(void*, UInt32);
typedef float(*_GetBaseActorValue)(void*, UInt32);
typedef float(*_CalculateChargePointsPerUse)(float basePoints, float enchantingLevel);
typedef float(*_GetEffectiveSkillLevel)(ActorValueOwner *, UInt32 skillID);
///@}

/**
 * @brief Local hooks for the functions we modify.
 */
///@{
static RelocPatch<uintptr_t*> kHook_ModifyPerkPool(&kHook_ModifyPerkPoolSig);
static RelocPatch<uintptr_t*> kHook_SkillCapPatch(&kHook_SkillCapPatchSig);
static RelocPatch<_ImproveSkillLevel> kHook_ImproveSkillLevel(&kHook_ImproveSkillLevelSig);
static RelocPatch<_ImprovePlayerSkillPoints> kHook_ImprovePlayerSkillPoints(&kHook_ImprovePlayerSkillPointsSig);
static RelocPatch<void*> kHook_ImproveLevelExpBySkillLevel(&kHook_ImproveLevelExpBySkillLevelSig);
static RelocPatch<_ImproveAttributeWhenLevelUp> kHook_ImproveAttributeWhenLevelUp(&kHook_ImproveAttributeWhenLevelUpSig);
static RelocPatch<_GetEffectiveSkillLevel> kHook_GetEffectiveSkillLevel(&kHook_GetEffectiveSkillLevelSig);
static RelocPatch<_GetCurrentActorValue> kHook_GetCurrentActorValue(&kHook_GetCurrentActorValueSig);
///@}

/**
 * @brief Local hooks for the game functions that we call.
 */
///@{
static RelocPatch<_GetLevel> GetLevel(&GetLevelSig);
static RelocPatch<_GetBaseActorValue> GetBaseActorValue(&GetBaseActorValueSig);
///@}

/**
 * @brief Used by the OG game functions we replace to return to their
 *        unmodified implementations.
 *
 * Boing!
 */
///@{
extern "C" {
    uintptr_t ImprovePlayerSkillPoints_ReturnTrampoline;
    uintptr_t ImproveAttributeWhenLevelUp_ReturnTrampoline;
    uintptr_t GetCurrentActorValue_ReturnTrampoline;
    uintptr_t GetEffectiveSkillLevel_ReturnTrampoline;
}
///@}

#if 0
static RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ent = 0x08C95FD;//get it when legendary skill by trace setbaseav.
static RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ret = 0x08C95FD + 0x6;

static RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ent = 0x08BF655;
static RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ret = 0x08BF655 + 0x13;

static RelocAddr <uintptr_t *> kHook_HideLegendaryButton_Ent = 0x08C1456;
static RelocAddr <uintptr_t *> kHook_HideLegendaryButton_Ret = 0x08C1456 + 0x1D;

static RelocAddr <_GetSkillCoefficients> GetSkillCoefficients(0x03E37F0);

static RelocAddr <_ImproveLevelExpBySkillLevel> ImproveLevelExpBySkillLevel_Original = 0x06E5D90;

static RelocAddr <_GetCurrentActorValue> GetCurrentActorValue = 0x061F6C0;
static _GetCurrentActorValue GetCurrentActorValue_Original = nullptr;

static RelocAddr <_CalculateChargePointsPerUse> CalculateChargePointsPerUse_Original(0x03C0F10);
#endif

/**
 * @brief Gets the base level of a skill on the player character.
 *
 * The player pointer offset here is a magic constant. It seems to be accessing
 * the actorState field in the object (which is private).
 *
 * @param skill_id The ID of the skill to get the base level of.
 */
static unsigned int
GetPlayerBaseSkillLevel(
    unsigned int skill_id
) {
    return static_cast<unsigned int>(
        GetBaseActorValue(reinterpret_cast<char*>(*g_thePlayer) + 0xB8, skill_id)
    );
}

/**
 * @brief Gets the current level of the player.
 */
static unsigned int
GetPlayerLevel() {
    return GetLevel(*g_thePlayer);
}

#if 0
float CalculateSkillExpForLevel(UInt32 skillID, float skillLevel)
{
    float result = 0.0f;
    float fSkillUseCurve = 1.95f;//0x01D88258;
    if ((*g_gameSettingCollection) != nullptr)
    {
        auto pSetting = (*g_gameSettingCollection)->Get("fSkillUseCurve");
        fSkillUseCurve = (pSetting != nullptr) ? pSetting->data.f32 : 1.95f;
    }
    if (skillLevel < settings.GetSkillCap(skillID))
    {
        result = pow(skillLevel, fSkillUseCurve);
        float a = 1.0f, b = 0.0f, c = 1.0f, d = 0.0f;
        if (GetSkillCoefficients(skillID, &a, &b, &c, &d))
            result = result * c + d;
    }
    return result;
}
#endif

#if 0
float CalculateChargePointsPerUse_Hook(float basePoints, float enchantingLevel)
{
    float fEnchantingCostExponent = 1.10f;// 0x01D8A058;             //1.10
    float fEnchantingSkillCostBase = 0.005f; // 0x01D8A010;         //1/200 = 0.005
    float fEnchantingSkillCostScale = 0.5f;// 0x01D8A040;         //0.5 sqrt
    //RelocPtr<float> unk0 = 0x014E8F78;             //1.00f
    float fEnchantingSkillCostMult = 3.00f;// 0x01D8A028;             //3.00

    if ((*g_gameSettingCollection) != nullptr)
    {
        Setting * pSetting = (*g_gameSettingCollection)->Get("fEnchantingCostExponent");
        fEnchantingCostExponent = (pSetting != nullptr) ? pSetting->data.f32 : 1.10f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostBase");
        fEnchantingSkillCostBase = (pSetting != nullptr) ? pSetting->data.f32 : 0.005f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostScale");
        fEnchantingSkillCostScale = (pSetting != nullptr) ? pSetting->data.f32 : 0.5f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostMult");
        fEnchantingSkillCostMult = (pSetting != nullptr) ? pSetting->data.f32 : 3.00f;
    }

    enchantingLevel = (enchantingLevel > 199.0f) ? 199.0f : enchantingLevel;
    float result = fEnchantingSkillCostMult * pow(basePoints, fEnchantingCostExponent) * (1.00f - pow((enchantingLevel * fEnchantingSkillCostBase), fEnchantingSkillCostScale));

    return result;
    //Charges Per Use = 3 * (base enchantment cost * magnitude / maximum magnitude)^1.1 * (1 - sqrt(skill/200))
}
#endif

#if 0
void ImproveSkillLevel_Hook(void* pPlayer, UInt32 skillID, UInt32 count)
{
    PlayerSkills* skillData = *reinterpret_cast<PlayerSkills**>(reinterpret_cast<uintptr_t>(pPlayer)+0x9B0);
    if (count < 1)
        count = 1;
    if ((skillID >= 6) && (skillID <= 23))
    {
        LevelData* levelData = &(skillData->data->levelData[skillID - 6]);
        float skillProgression = 0.0f;
        if (levelData->pointsMax > 0.0f)
        {
            skillProgression = levelData->points / levelData->pointsMax;
#ifdef _DEBUG
            _MESSAGE("player:%p, skill:%d, points:%.2f, maxPoints:%.2f, level:%.2f", pPlayer, skillID, levelData->points, levelData->pointsMax, levelData->level);
#endif
            if (skillProgression >= 1.0f)
                skillProgression = 0.99f;
        }
        else
            skillProgression = 0.0f;
        for (UInt32 i = 0; i < count; ++i)
        {
            float skillLevel = GetBaseActorValue((char*)(*g_thePlayer) + 0xB0, skillID);
            float expRequired = CalculateSkillExpForLevel(skillID, skillLevel);
#ifdef _DEBUG
            _MESSAGE("maxPoints:%.2f, expRequired:%.2f", levelData->pointsMax, expRequired);
#endif
            if (levelData->pointsMax != expRequired)
                levelData->pointsMax = expRequired;
            if (levelData->points <= 0.0f)
                levelData->points = (levelData->pointsMax > 0.0f) ? 0.1f : 0.0f;
            if (levelData->points >= levelData->pointsMax)
                levelData->points = (levelData->pointsMax > 0.0f) ? (levelData->pointsMax - 0.1f) : 0.0f;
            float expNeeded = levelData->pointsMax - levelData->points;
            ImprovePlayerSkillPoints_Original(skillData, skillID, expNeeded, 0, 0, 0, (i < count - 1));
        }
        levelData->points += levelData->pointsMax * skillProgression;
    }
}
#endif

static void ImprovePlayerSkillPoints_Hook(PlayerSkills* skillData, UInt32 skillID, float exp, UInt64 unk1, UInt32 unk2, UInt8 unk3, bool unk4)
{
    if (settings.IsManagedSkill(skillID)) {
        exp *= settings.GetSkillExpGainMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
    }

    ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

extern "C" float ImproveLevelExpBySkillLevel_Hook(float exp, UInt32 skillID)
{
    if (settings.IsManagedSkill(skillID)) {
        exp *= settings.GetLevelSkillExpMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
    }

    return exp;
}

static UInt64 ImproveAttributeWhenLevelUp_Hook(void* unk0, UInt8 unk1)
{

    Setting * iAVDhmsLevelUp = (*g_gameSettingCollection)->Get("iAVDhmsLevelUp");
    Setting * fLevelUpCarryWeightMod = (*g_gameSettingCollection)->Get("fLevelUpCarryWeightMod");

    ASSERT(iAVDhmsLevelUp);
    ASSERT(fLevelUpCarryWeightMod);

    Settings::player_attr_e choice =
        *reinterpret_cast<Settings::player_attr_e*>(static_cast<char*>(unk0) + 0x18);

    settings.GetAttributeLevelUp(
        GetPlayerLevel(),
        choice,
        iAVDhmsLevelUp->data.u32,
        fLevelUpCarryWeightMod->data.f32
    );

    return ImproveAttributeWhenLevelUp_Original(unk0, unk1);
}

extern "C" void ModifyPerkPool_Hook(SInt8 count)
{
    UInt8* points = &((*g_thePlayer)->numPerkPoints);
    if (count > 0) { // Add perk points
        UInt32 sum = settings.GetPerkDelta(GetPlayerLevel()) + *points;
        *points = (sum > 0xFF) ? 0xFF : static_cast<UInt8>(sum);
    } else { // Remove perk points
        SInt32 sum = *points + count;
        *points = (sum < 0) ? 0 : static_cast<UInt8>(sum);
    }
}

extern "C" float GetSkillCap_Hook(UInt32 skillID)
{
    return settings.GetSkillCap(skillID);
}

/**
 * @brief Overwrites the GetCurrentActorValue function, clamping the output
 *        to the configured effective cap.
 * @param av Unknown. The actor this is operating on?
 * @param skill_id The raw ID of the skill.
 */
static float
GetCurrentActorValue_Hook(
    void *av,
    uint32_t skill_id
) {
    float val = GetCurrentActorValue_Original(av, skill_id);

    if (settings.IsManagedSkill(skill_id)) {
        float cap = settings.GetSkillFormulaCap(skill_id);
        val = (val <= 0) ? 0 : ((val >= cap) ? cap : val);
    }

    return val;
}

static float
GetEffectiveSkillLevel_Hook(
    void *av,
    uint32_t skill_id
) {
    float val = GetEffectiveSkillLevel_Original(av, skill_id);
    
    if (settings.IsManagedSkill(skill_id)) {
        float cap = settings.GetSkillFormulaCap(skill_id);
        val = (val <= 0) ? 0 : ((val >= cap) ? cap : val);
    }

    return val;
}

#if 0
void LegendaryResetSkillLevel_Hook(float baseLevel, UInt32 skillID)
{
    if ((*g_gameSettingCollection) != nullptr)
    {
        Setting * fLegendarySkillResetValue = (*g_gameSettingCollection)->Get("fLegendarySkillResetValue");
        if (fLegendarySkillResetValue != nullptr)
        {
            static float originalSetting = fLegendarySkillResetValue->data.f32;
            if ((skillID >= 6) && (skillID <= 23))
            {
                if (settings.settingsLegendarySkill.bLegendaryKeepSkillLevel)
                    fLegendarySkillResetValue->data.f32 = baseLevel;
                else
                {
                    UInt32 legendaryLevel = settings.settingsLegendarySkill.iSkillLevelAfterLegendary;
                    if ((legendaryLevel && legendaryLevel > baseLevel) || (!legendaryLevel && originalSetting > baseLevel))
                        fLegendarySkillResetValue->data.f32 = baseLevel;
                    else
                        fLegendarySkillResetValue->data.f32 = (!legendaryLevel) ? originalSetting : legendaryLevel;
                }
            }
            else
                fLegendarySkillResetValue->data.f32 = originalSetting;
        }
    }
}

bool CheckConditionForLegendarySkill_Hook(void* pActorValueOwner, UInt32 skillID)
{
    float skillLevel = GetBaseActorValue(*(char**)(g_thePlayer.GetPtr()) + 0xB0, skillID);
    return (skillLevel >= settings.settingsLegendarySkill.iSkillLevelEnableLegendary) ? true : false;
}

bool HideLegendaryButton_Hook(UInt32 skillID)
{
    float skillLevel = GetBaseActorValue(*(char**)(g_thePlayer.GetPtr()) + 0xB0, skillID);
    if (skillLevel >= settings.settingsLegendarySkill.iSkillLevelEnableLegendary && !settings.settingsLegendarySkill.bHideLegendaryButton)
        return true;
    return false;
}
#endif

void Hook_Skill_Commit()
{
    _MESSAGE("Do hooks");

    // No hooks. These are just game functions we use.
    GetLevel.Resolve();
    GetBaseActorValue.Resolve();

    // These overlap, so we resolve them before changing anything.
    kHook_ImprovePlayerSkillPoints.Resolve();
    kHook_ImproveLevelExpBySkillLevel.Resolve();

    // Set up the return trampolines for our reimplemented game function calls.
    ImprovePlayerSkillPoints_ReturnTrampoline = kHook_ImprovePlayerSkillPoints.GetRetAddr();
    ImproveAttributeWhenLevelUp_ReturnTrampoline = kHook_ImproveAttributeWhenLevelUp.GetRetAddr();
    GetCurrentActorValue_ReturnTrampoline = kHook_GetCurrentActorValue.GetRetAddr();
    GetEffectiveSkillLevel_ReturnTrampoline = kHook_GetEffectiveSkillLevel.GetRetAddr();

    // The hooks!
    kHook_GetEffectiveSkillLevel.Apply(reinterpret_cast<uintptr_t>(GetEffectiveSkillLevel_Hook));
    kHook_SkillCapPatch.Apply(reinterpret_cast<uintptr_t>(SkillCapPatch_Wrapper));
    kHook_ModifyPerkPool.Apply(reinterpret_cast<uintptr_t>(ModifyPerkPool_Wrapper));
    kHook_ImproveSkillLevel.Apply(reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Original));
    kHook_ImproveLevelExpBySkillLevel.Apply(reinterpret_cast<uintptr_t>(ImproveLevelExpBySkillLevel_Wrapper));
    kHook_ImprovePlayerSkillPoints.Apply(reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Hook));
    kHook_ImproveAttributeWhenLevelUp.Apply(reinterpret_cast<uintptr_t>(ImproveAttributeWhenLevelUp_Hook));
    kHook_GetCurrentActorValue.Apply(reinterpret_cast<uintptr_t>(GetCurrentActorValue_Hook));

    // FIXME: This should probably have its own signature.
    SafeWrite8(kHook_ImproveAttributeWhenLevelUp.GetUIntPtr() + 0x9B, 0);
/*
       1408c4793 ff 50 28        CALL       qword ptr [RAX + 0x28]
       1408c4796 83 7f 18 1a     CMP        dword ptr [RDI + 0x18],0x1a
       1408c479a 75 22           JNZ        LAB_1408c47be
22 replaced with 0. To disable jump over increasing carry weight if not stamina(0x1a) selected?
*/

#if 0 // not updated code

    g_branchTrampoline.Write6Branch(CalculateChargePointsPerUse_Original.GetUIntPtr(), (uintptr_t)CalculateChargePointsPerUse_Hook);

    {
        struct GetCurrentActorValue_Code : Xbyak::CodeGenerator
        {
            GetCurrentActorValue_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;

                push(rbp);
                push(rsi);
                push(rdi);
                push(r14);

                jmp(ptr[rip + retnLabel]);

            L(retnLabel);
                dq(GetCurrentActorValue.GetUIntPtr() + 0x6);
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        GetCurrentActorValue_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        GetCurrentActorValue_Original = (_GetCurrentActorValue)codeBuf;

        g_branchTrampoline.Write6Branch(GetCurrentActorValue.GetUIntPtr(), (uintptr_t)GetCurrentActorValue_Hook);

    }

    {
        struct ExecuteLegendarySkill_Code : Xbyak::CodeGenerator
        {
            ExecuteLegendarySkill_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;

                mov(edx, ptr[rsi + 0x1C]);
                call((void*)&LegendaryResetSkillLevel_Hook);

                jmp(ptr[rip + retnLabel]);

            L(retnLabel);
                dq(kHook_ExecuteLegendarySkill_Ret.GetUIntPtr());
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        ExecuteLegendarySkill_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        g_branchTrampoline.Write6Branch(kHook_ExecuteLegendarySkill_Ent.GetUIntPtr(), uintptr_t(code.getCode()));
    }

    {
        struct CheckConditionForLegendarySkill_Code : Xbyak::CodeGenerator
        {
            CheckConditionForLegendarySkill_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;

                mov(edx, eax);
                lea(rcx, ptr[rdi + 0xB0]);
                call((void*)&CheckConditionForLegendarySkill_Hook);
                cmp(al, 1);

                jmp(ptr[rip + retnLabel]);

            L(retnLabel);
                dq(kHook_CheckConditionForLegendarySkill_Ret.GetUIntPtr());
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        CheckConditionForLegendarySkill_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        g_branchTrampoline.Write6Branch(kHook_CheckConditionForLegendarySkill_Ent.GetUIntPtr(), uintptr_t(code.getCode()));
    }

    {
        struct HideLegendaryButton_Code : Xbyak::CodeGenerator
        {
            HideLegendaryButton_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                mov(ecx, esi);
                call((void*)&HideLegendaryButton_Hook);
                cmp(al, 1);

                jmp(ptr[rip + retnLabel]);

            L(retnLabel);
                dq(kHook_HideLegendaryButton_Ret.GetUIntPtr());
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        HideLegendaryButton_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        g_branchTrampoline.Write6Branch(kHook_HideLegendaryButton_Ent.GetUIntPtr(), uintptr_t(code.getCode()));

    }
#endif
}
