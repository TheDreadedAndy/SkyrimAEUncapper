#include "common/IPrefix.h"
#include "GameFormComponents.h"
#include "GameReferences.h"
#include "GameSettings.h"
#include "Relocation.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"
#include "xbyak.h"

#include "Hook_Skill.h"
#include "HookWrappers.h"
#include "Settings.h"
#include "Signatures.h"
#include "RelocFn.h"

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
static RelocFn<uintptr_t*> kHook_ModifyPerkPool(&kHook_ModifyPerkPoolSig);
static RelocFn<uintptr_t*> kHook_SkillCapPatch(&kHook_SkillCapPatchSig);
static RelocFn<_ImproveSkillLevel> kHook_ImproveSkillLevel(&kHook_ImproveSkillLevelSig);
static RelocFn<_ImprovePlayerSkillPoints> kHook_ImprovePlayerSkillPoints(&kHook_ImprovePlayerSkillPointsSig);
static RelocFn<void*> kHook_ImproveLevelExpBySkillLevel(&kHook_ImproveLevelExpBySkillLevelSig);
static RelocFn<_ImproveAttributeWhenLevelUp> kHook_ImproveAttributeWhenLevelUp(&kHook_ImproveAttributeWhenLevelUpSig);
static RelocFn<_GetEffectiveSkillLevel> kHook_GetEffectiveSkillLevel(&kHook_GetEffectiveSkillLevelSig);
///@}

/**
 * @brief Local hooks for the game functions that we call.
 */
///@{
static RelocFn<_GetLevel> GetLevel(&GetLevelSig);
static RelocFn<_GetBaseActorValue> GetBaseActorValue(&GetBaseActorValueSig);
static _ImprovePlayerSkillPoints ImprovePlayerSkillPoints_Original = nullptr;
static _ImproveAttributeWhenLevelUp ImproveAttributeWhenLevelUp_Original = nullptr;
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
        GetBaseActorValue((char*)(g_thePlayer.GetUIntPtr() + 0xB8), skill_id)
    );
}

/**
 * @brief Gets the current level of the player.
 */
static unsigned int
GetPlayerLevel() {
    return GetLevel(g_thePlayer.GetPtr());
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
    exp *= settings.GetSkillExpGainMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
    ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

static float ImproveLevelExpBySkillLevel_Hook(float exp, UInt32 skillID)
{
    return exp * settings.GetLevelSkillExpMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
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

static void ModifyPerkPool_Hook(SInt8 count)
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

extern "C" float ClampSkillEffect(uint32_t skillID, const float val)
{
    float cap = settings.GetSkillFormulaCap(skillID);
    return (val <= 0) ? 0 : ((val >= cap) ? cap : val);
}

#if 0
float GetCurrentActorValue_Hook(void* avo, UInt32 skillID)   //PC&NPC  //61F6C0
{
    float skillLevel = GetCurrentActorValue_Original(avo, skillID);
    if ((skillID >= 6) && (skillID <= 23))
    {
        UInt32 skillFormulaCap = settings.settingsSkillFormulaCaps[skillID - 6];
        skillLevel = (skillLevel > skillFormulaCap) ? skillFormulaCap : skillLevel;
#ifdef _DEBUG
        //_MESSAGE("function: %s, skillID: %d, skillLevel:%.2f, skillFormulaCap: %d, this:%p", __FUNCTION__, skillID, skillLevel, settings.settingsSkillFormulaCaps[skillID - 6], avo);
#endif
    }
    return skillLevel;
}

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

    GetLevel.Resolve();
    GetBaseActorValue.Resolve();

    // These overlap, so we do them before changing anything.
    kHook_ImprovePlayerSkillPoints.Resolve();
    kHook_ImproveLevelExpBySkillLevel.Resolve();

    kHook_GetEffectiveSkillLevel.Hook(SkillEffectiveCapPatch_Wrapper);
/*
skill? range. min(100,val) and max(0,val). replacing MINSS with nop
       1403fdf2c f3 0f 5d        MINSS      XMM1,dword ptr [DAT_14161af50]                   = 42C80000h
                 0d 1c d0
                 21 01
       1403fdf34 0f 57 c0        XORPS      XMM0,XMM0
       1403fdf37 f3 0f 5f c8     MAXSS      XMM1,XMM0
*/

#if 0
    {
        //fVar14 == XMM10 = 100.0(maximum)
        //fVar8 == XMM0 < 100.0(current)
        //ESI SkillId
        struct SkillCapPatch_Code : Xbyak::CodeGenerator
        {
            SkillCapPatch_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                movd(xmm10, ecx); // save ecx in xmm10 just in case. XMM6-XMM15 nonvolatile
                mov(ecx, esi); // pass SkillId in ecx to hook
                sub(rsp, 0x30);
                movss(ptr[rsp + 0x28], xmm0); // save current skill level
                call((void *)&GetSkillCap_Hook);
                movd(ecx, xmm10); // restore ecx
                movss(xmm10, xmm0); // replace maximum with function result
                movss(xmm0, ptr[rsp + 0x28]); // restore current skill level
                add(rsp, 0x30);
                ret();
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        SkillCapPatch_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        kHook_SkillCapPatch.Hook(reinterpret_cast<uintptr_t>(code.getCode()));
    }
#endif
    //kHook_SkillCapPatch.Hook(SkillCapPatch_Wrapper);

    {
        struct ModifyPerkPool_Code : Xbyak::CodeGenerator
        {
            ModifyPerkPool_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                sub(rsp, 0x20);
                mov(rcx, rdi); // AE changed rbx to rdi
                call((void *)&ModifyPerkPool_Hook);
                add(rsp, 0x20);
                jmp(ptr[rip + retnLabel]);
            L(retnLabel);
                dq(kHook_ModifyPerkPool.GetRetAddr());
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        ModifyPerkPool_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        kHook_ModifyPerkPool.Hook(reinterpret_cast<uintptr_t>(code.getCode()));
    }

    {
        struct ImprovePlayerSkillPoints_Code : Xbyak::CodeGenerator
        {
            ImprovePlayerSkillPoints_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                mov(rax, rsp); // 48 8b c4
                push(rdi); // 57
                push(r12); // 41 54
                jmp(ptr[rip + retnLabel]);
            L(retnLabel);
                dq(kHook_ImprovePlayerSkillPoints.GetRetAddr());
            }
        };

        void * codeBuf = g_branchTrampoline.StartAlloc();
        ImprovePlayerSkillPoints_Code code(codeBuf);
        g_branchTrampoline.EndAlloc(code.getCurr());

        ImprovePlayerSkillPoints_Original = (_ImprovePlayerSkillPoints)codeBuf;
        kHook_ImprovePlayerSkillPoints.Hook(reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Hook));
    }

    {
        struct ImproveAttributeWhenLevelUp_Code : Xbyak::CodeGenerator
        {
            ImproveAttributeWhenLevelUp_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                push(rdi);
                sub(rsp, 0x30);
                jmp(ptr[rip + retnLabel]);
            L(retnLabel);
                dq(kHook_ImproveAttributeWhenLevelUp.GetRetAddr());
            }
        };
        void * codeBuf = g_localTrampoline.StartAlloc();
        ImproveAttributeWhenLevelUp_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        ImproveAttributeWhenLevelUp_Original = (_ImproveAttributeWhenLevelUp)codeBuf;
        kHook_ImproveAttributeWhenLevelUp.Hook(reinterpret_cast<uintptr_t>(ImproveAttributeWhenLevelUp_Hook));

        // FIXME: This should probably have its own signature.
        SafeWrite8(kHook_ImproveAttributeWhenLevelUp.GetUIntPtr() + 0x9B, 0);
/*
       1408c4793 ff 50 28        CALL       qword ptr [RAX + 0x28]
       1408c4796 83 7f 18 1a     CMP        dword ptr [RDI + 0x18],0x1a
       1408c479a 75 22           JNZ        LAB_1408c47be
22 replaced with 0. To disable jump over increasing carry weight if not stamina(0x1a) selected?
*/
    }

    kHook_ImproveSkillLevel.Hook(reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Original));

    { // Patching skill exp to player level exp modification
        struct ImproveLevelExpBySkillLevel_Code : Xbyak::CodeGenerator
        {
            ImproveLevelExpBySkillLevel_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                push(rax);
                push(rdx);
                movss(xmm0, xmm1); // xmm1 level exp
                mov(rdx, rsi);
                sub(rsp, 0x20);
                call((void *)&ImproveLevelExpBySkillLevel_Hook);
                add(rsp, 0x20);
                pop(rdx);
                pop(rax);
                addss(xmm0, ptr[rax]); // replaced code 1, but xmm0 instead of xmm1
                movss(ptr[rax], xmm0); // replaced code 2
                jmp(ptr[rip + retnLabel]);
            L(retnLabel);
                dq(kHook_ImproveLevelExpBySkillLevel.GetRetAddr());
            }
        };
        void * codeBuf = g_localTrampoline.StartAlloc();
        ImproveLevelExpBySkillLevel_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        const unsigned char expectedCode[] = {0xf3, 0x0f, 0x58, 0x08, 0xf3, 0x0f, 0x11, 0x08};
        ASSERT(memcmp((void*)(kHook_ImproveLevelExpBySkillLevel.GetUIntPtr()), expectedCode, sizeof(expectedCode)) == 0);

        kHook_ImproveLevelExpBySkillLevel.Hook(reinterpret_cast<uintptr_t>(code.getCode()));
    }

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
