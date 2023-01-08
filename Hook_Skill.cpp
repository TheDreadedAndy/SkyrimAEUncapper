#include "common/IPrefix.h"
#include "GameAPI.h"
#include "GameSettings.h"
#include "Relocation.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"
#include "xbyak.h"

#include "Hook_Skill.h"
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

static RelocFn<uintptr_t*> kHook_ModifyPerkPool(
    "kHook_ModifyPerkPool",
    kModifyPerkPoolSig,
    kModifyPerkPoolSigOffset,
    kModifyPerkPoolPatchSize,
    HookType::Branch5
);

static RelocFn<uintptr_t*> kHook_SkillCapPatch(
    "kHook_SkillCapPatch",
    kSkillCapPatchSig,
    kSkillCapPatchSigOffset,
    kSkillCapPatchPatchSize,
    HookType::Branch5
);

#if 0
RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ent = 0x08C95FD;//get it when legendary skill by trace setbaseav.
RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ret = 0x08C95FD + 0x6;

RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ent = 0x08BF655;
RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ret = 0x08BF655 + 0x13;

RelocAddr <uintptr_t *> kHook_HideLegendaryButton_Ent = 0x08C1456;
RelocAddr <uintptr_t *> kHook_HideLegendaryButton_Ret = 0x08C1456 + 0x1D;
#endif

static RelocAddr <_ImproveSkillLevel> ImproveSkillLevel_Hook(0x06A0EE0);

static RelocAddr <_ImprovePlayerSkillPoints> ImprovePlayerSkillPoints(0x06E4B30);
static _ImprovePlayerSkillPoints ImprovePlayerSkillPoints_Original = nullptr;

static RelocAddr <_ImproveAttributeWhenLevelUp> ImproveAttributeWhenLevelUp(0x08925D0);
static _ImproveAttributeWhenLevelUp ImproveAttributeWhenLevelUp_Original = nullptr;

static RelocAddr <_GetSkillCoefficients> GetSkillCoefficients(0x03E37F0);

static RelocAddr <_GetLevel> GetLevel(0x05D4C40);

#if 0
RelocAddr <_ImproveLevelExpBySkillLevel> ImproveLevelExpBySkillLevel_Original = 0x06E5D90;

RelocAddr <_GetCurrentActorValue> GetCurrentActorValue = 0x061F6C0;
_GetCurrentActorValue GetCurrentActorValue_Original = nullptr;
#endif

static RelocAddr <_GetBaseActorValue> GetBaseActorValue(0x061F890);    //GetBaseActorValue

#if 0
RelocAddr <_CalculateChargePointsPerUse> CalculateChargePointsPerUse_Original(0x03C0F10);
#endif

class ActorValueOwner;
static RelocAddr <_GetEffectiveSkillLevel> GetEffectiveSkillLevel(0x03E5400);    //V1.5.3

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

void ImprovePlayerSkillPoints_Hook(PlayerSkills* skillData, UInt32 skillID, float exp, UInt64 unk1, UInt32 unk2, UInt8 unk3, bool unk4)
{
    exp *= settings.GetSkillExpGainMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
    ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

float ImproveLevelExpBySkillLevel_Hook(float exp, UInt32 skillID)
{
    return exp * settings.GetLevelSkillExpMult(skillID, GetPlayerBaseSkillLevel(skillID), GetPlayerLevel());
}

UInt64 ImproveAttributeWhenLevelUp_Hook(void* unk0, UInt8 unk1)
{

    Setting * iAVDhmsLevelUp = g_gameSettingCollection->Get("iAVDhmsLevelUp");
    Setting * fLevelUpCarryWeightMod = g_gameSettingCollection->Get("fLevelUpCarryWeightMod");

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

void ModifyPerkPool_Hook(SInt8 count)
{
    UInt8* points = &((g_thePlayer.GetPtr())->numPerkPoints);
    if (count > 0) { // Add perk points
        UInt32 sum = settings.GetPerkDelta(GetPlayerLevel()) + *points;
        *points = (sum > 0xFF) ? 0xFF : static_cast<UInt8>(sum);
    } else { // Remove perk points
        SInt32 sum = *points + count;
        *points = (sum < 0) ? 0 : static_cast<UInt8>(sum);
    }
}

float GetSkillCap_Hook(UInt32 skillID)
{
    return settings.GetSkillCap(skillID);
}

static float ClampSkillEffect(uint32_t skillID, const float val)
{
    float cap = settings.GetSkillFormulaCap(skillID);
    return (val <= 0) ? 0 : ((val >= cap) ? cap : val);
}

/*float GetCurrentActorValue_Hook(void* avo, UInt32 skillID)   //PC&NPC  //61F6C0
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
}*/

/*void LegendaryResetSkillLevel_Hook(float baseLevel, UInt32 skillID)
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
}*/

#undef GET_RVA
#define GET_RVA(n) #n
void InitRVA()
{
    //ImproveSkillLevel_Hook            = RVAScan<_ImproveSkillLevel>(GET_RVA(ImproveSkillLevel_Hook), "48 8B 89 B0 09 00 00 B8 01 00 00 00 44 3B C0 44 0F 42 C0 E9", (0x14070ee08-0x1406ca9b0));
    ImproveSkillLevel_Hook = RVAScan<_ImproveSkillLevel>(GET_RVA(ImproveSkillLevel_Hook), "F3 0F 10 54 9F 10 41 3B F4 F3 0F 5C 54 9F 0C 0F 92 C0 8B D5 88 44 24 30 45 33 C9 44 88 6C 24 28 49 8B CF 44 89 6C 24 20 E8 73 FB FF FF FF C6 41 3B F6 72 CC", (0x14070ee08-0x14070ede0));
    ASSERT(*(uint8_t*)ImproveSkillLevel_Hook.GetUIntPtr() == 0xE8); // CALL
/*
Calls ImprovePlayerSkillPoints offset:0x14070ee08-0x1406ca9b0=0x44458
       1406ca9b0 48 8b 89        MOV        param_1,qword ptr [param_1 + 0x9b0]
                 b0 09 00 00
       1406ca9b7 b8 01 00        MOV        EAX,0x1
                 00 00
       1406ca9bc 44 3b c0        CMP        param_3,EAX
       1406ca9bf 44 0f 42 c0     CMOVC      param_3,EAX
       1406ca9c3 e9 a8 43        JMP        LAB_14070ed70
                 04 00
...
       14070ee08 e8 73 fb        CALL       ImprovePlayerSkillPoints
                 ff ff
       14070ee0d ff c6           INC        ESI
       14070ee0f 41 3b f6        CMP        ESI,R14D
       14070ee12 72 cc           JC         LAB_14070ede0
       14070ee14 f3 0f 10        MOVSS      XMM0,dword ptr [RDI + RBX*0x4 + 0x10]
                 44 9f 10
       14070ee1a f3 0f 10        MOVSS      XMM1,dword ptr [RDI + RBX*0x4 + 0xc]
                 4c 9f 0c
       14070ee20 4c 8b 64        MOV        R12,qword ptr [RSP + local_20]
                 24 58
*/

    //ImprovePlayerSkillPoints                = RVAScan<_ImprovePlayerSkillPoints>(GET_RVA(ImprovePlayerSkillPoints), "4C 8B DC 55 56 41 56 48 81 EC 60 01 00 00 8D 42 FA 41 0F 29 7B C8 49 8B E9 0F 28 FA 8B F2 4C 8B F1 83 F8 11 0F 87 B9 02 00 00");
    ImprovePlayerSkillPoints                = RVAScan<_ImprovePlayerSkillPoints>(GET_RVA(ImprovePlayerSkillPoints), "48 8B C4 57 41 54 41 55 41 56 41 57 48 81 EC 80 01 00 00 48 C7 44 24 48 FE FF FF FF");
/* kHook_SkillCapPatch_Ent is inside this function
                             FUN_14070ec10                                   XREF[3]:     FUN_1406cac00:1406cac25(c),
                                                                                          FUN_1406cac40:14070f098(c),
                                                                                          14359bfd8(*)
       14070ec10 48 8b c4        MOV        RAX,RSP
       14070ec13 57              PUSH       RDI
       14070ec14 41 54           PUSH       R12
       14070ec16 41 55           PUSH       R13
       14070ec18 41 56           PUSH       R14
       14070ec1a 41 57           PUSH       R15
       14070ec1c 48 81 ec        SUB        RSP,0x180
                 80 01 00 00
       14070ec23 48 c7 44        MOV        qword ptr [RSP + local_160],-0x2
                 24 48 fe
                 ff ff ff
*/

    ImproveAttributeWhenLevelUp                = RVAScan<_ImproveAttributeWhenLevelUp>(GET_RVA(ImproveAttributeWhenLevelUp), "0F B6 DA 48 8B F9 48 8B 15 ? ? ? ? 48 81 C2 28 01 00 00 48 8B 0D ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 BA 00 00 00 84 DB 0F 85 AA 00 00 00", -0x1E);
/*
                             undefined ImproveAttributeWhenLevelUp()
             undefined         AL:1           <RETURN>
             undefined8        Stack[0x20]:8  local_res20                             XREF[2]:     1408c4719(W),
                                                                                                   1408c480a(R)
             undefined4        Stack[0x18]:4  local_res18                             XREF[1]:     1408c4769(W)
             undefined8        Stack[0x10]:8  local_res10                             XREF[2]:     1408c4714(W),
                                                                                                   1408c4805(R)
             undefined8        Stack[0x8]:8   local_res8                              XREF[2]:     1408c470f(W),
                                                                                                   1408c4800(R)
             undefined8        Stack[-0x18]:8 local_18                                XREF[1]:     1408c4706(W)
                             ImproveAttributeWhenLevelUp                     XREF[2]:     1417a7938(*), 1435b4908(*)
       1408c4700 40 57           PUSH       RDI
       1408c4702 48 83 ec 30     SUB        RSP,0x30
       1408c4706 48 c7 44        MOV        qword ptr [RSP + local_18],-0x2
                 24 20 fe
                 ff ff ff
       1408c470f 48 89 5c        MOV        qword ptr [RSP + local_res8],RBX
                 24 40
       1408c4714 48 89 6c        MOV        qword ptr [RSP + local_res10],RBP
                 24 48
       1408c4719 48 89 74        MOV        qword ptr [RSP + local_res20],RSI
                 24 58
       1408c471e 0f b6 da        MOVZX      EBX,DL
       1408c4721 48 8b f9        MOV        RDI,RCX
       1408c4724 48 8b 15        MOV        RDX,qword ptr [DAT_141f5b278]                    = ??
                 4d 6b 69 01
       1408c472b 48 81 c2        ADD        RDX,0x128
                 28 01 00 00
       1408c4732 48 8b 0d        MOV        RCX,qword ptr [DAT_141f59320]                    = ??
                 e7 4b 69 01
       1408c4739 e8 62 fd        CALL       FUN_140f044a0                                    undefined FUN_140f044a0()
                 63 00
       1408c473e 84 c0           TEST       AL,AL
       1408c4740 0f 84 ba        JZ         LAB_1408c4800
                 00 00 00
       1408c4746 84 db           TEST       BL,BL
       1408c4748 0f 85 aa        JNZ        LAB_1408c47f8
                 00 00 00
       1408c474e 8b 15 44        MOV        EDX,dword ptr [DAT_143531398]                    = ??
                 cc c6 02
       1408c4754 65 48 8b        MOV        RAX,qword ptr GS:[0x58]
                 04 25 58
                 00 00 00
       1408c475d bd 68 07        MOV        EBP,0x768
                 00 00
       1408c4762 48 8b 34 d0     MOV        RSI,qword ptr [RAX + RDX*0x8]
       1408c4766 8b 1c 2e        MOV        EBX,dword ptr [RSI + RBP*0x1]
       1408c4769 89 5c 24 50     MOV        dword ptr [RSP + local_res18],EBX
       1408c476d c7 04 2e        MOV        dword ptr [RSI + RBP*0x1],0x46
                 46 00 00 00
       1408c4774 48 8b 0d        MOV        RCX,qword ptr [DAT_142fc19c8]                    = ??
                 4d d2 6f 02
       1408c477b 48 81 c1        ADD        RCX,0xb0
                 b0 00 00 00
       1408c4782 48 8b 01        MOV        RAX,qword ptr [RCX]
       1408c4785 66 0f 6e        MOVD       XMM2,dword ptr [DAT_141e6a540]                   = 0000000Ah
                 15 b3 5d
                 5a 01
*/
    GetSkillCoefficients                    = RVAScan<_GetSkillCoefficients>(GET_RVA(GetSkillCoefficients), "81 F9 A3 00 00 00 77 52 48 8B 05 ? ? ? ? 48 63 C9 4C 8B 54 C8 08 4D 85 D2 74 3E 49 8B 82 08 01 00 00 48 85 C0 74 32");

    GetLevel                                = RVAScan<_GetLevel>(GET_RVA(GetLevel), "84 C0 74 24 48 8B CB E8 ? ? ? ? 0F B7 C8 48 8B D7 E8 ? ? ? ? 84 C0 74 0D B0 01 48 8B 5C 24 30 48 83 C4 20 5F C3", 7, 1, 5);

    /*ImproveLevelExpBySkillLevel_Original    = RVAScan<_ImproveLevelExpBySkillLevel>(GET_RVA(ImproveLevelExpBySkillLevel_Original), "F3 0F 58 D3 0F 28 E0 0F 29 34 24 0F 57 F6 0F 28 CA F3 0F 58 CB F3 0F 59 CA F3 0F 59 CD F3 0F 2C C1 66 0F 6E C0 0F 5B C0 F3 0F 5C C8 0F 2F CE 73 04 F3 0F 5C C3", -0x17);
    Before AE FUN_1406e7430
              pfVar1[uVar5 * 3 + 4] = fVar9;
              fVar8 = (float)FUN_1406e7430(fVar8); // Skill exp to PC level exp
              *pfVar7 = fVar8 + *pfVar7; // PC level exp increase
              fVar8 = fVar10;
              FUN_140694650(DAT_142f26ef8);
            } while (fVar8 < fVar11);
    After AE function inlined
              pfVar1[uVar5 * 3 + 4] = fVar9;
              fVar9 = (fVar8 + fVar12) * fVar8 * fVar15;
              fVar11 = (float)(int)fVar9;
              if (fVar9 - fVar11 < fVar13) {
                fVar11 = fVar11 - fVar12;
              }
              fVar10 = fVar8 * fVar10 * fVar15;
              fVar9 = (float)(int)fVar10;
              if (fVar10 - fVar9 < fVar13) {
                fVar9 = fVar9 - fVar12;
              }
              **param_4 = (fVar11 - fVar9) * _DAT_141e76790 + **param_4;
              FUN_1406bc2e0(DAT_142fc19c8);
            } while (fVar8 < fVar14);

                             LAB_14070ee83                                   XREF[1]:     14070ee7d(j)
       14070ee83 f3 0f 11        MOVSS      dword ptr [RBX + RDI*0x4 + 0x10],XMM0
                 44 bb 10
       14070ee89 0f 28 c6        MOVAPS     XMM0,XMM6
       14070ee8c f3 0f 58 c7     ADDSS      XMM0,XMM7
       14070ee90 f3 0f 59 c6     MULSS      XMM0,XMM6
       14070ee94 f3 41 0f        MULSS      XMM0,XMM11
                 59 c3
       14070ee99 f3 0f 2c c0     CVTTSS2SI  EAX,XMM0
       14070ee9d 66 0f 6e c8     MOVD       XMM1,EAX
       14070eea1 0f 5b c9        CVTDQ2PS   XMM1,XMM1
       14070eea4 f3 0f 5c c1     SUBSS      XMM0,XMM1
       14070eea8 41 0f 2f c1     COMISS     XMM0,XMM9
       14070eeac 73 04           JNC        LAB_14070eeb2
       14070eeae f3 0f 5c cf     SUBSS      XMM1,XMM7

*/

    //GetCurrentActorValue                    = RVAScan<_GetCurrentActorValue>(GET_RVA(GetCurrentActorValue), "4C 8B 44 F8 08 41 8B 40 60 C1 E8 12 A8 01 74 38 48 8B 49 40 48 85 C9 74 2F 48 83 79 50 00 74 28 40 B5 01 0F 57 C0 F3 0F 11 44 24 78 4C 8D 44 24 78 8B D7", -0x2C);
/*
ulonglong UndefinedFunction_140620d60
                    (undefined8 param_1,undefined8 param_2,longlong param_3,int param_4,
                    undefined8 param_5,undefined8 param_6)
After AE
ulonglong FUN_1406462a0(undefined8 param_1,undefined8 param_2,longlong param_3,int param_4)
params count changed or wrong function?
*/

    //GetBaseActorValue                        = RVAScan<_GetBaseActorValue>(GET_RVA(GetBaseActorValue), "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 30 48 63 DA 4C 8D 44 24 40 48 8B F9 0F 57 C0 8B D3 F3 0F 11 44 24 40 48 81 C1 50 01 00 00 E8 ? ? ? ? 84 C0 0F 85 DE 00 00 00");
    GetBaseActorValue                        = RVAScan<_GetBaseActorValue>(GET_RVA(GetBaseActorValue), "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 30 48 63 FA 48 8D B1 50 01 00 00 48 8B D9 4C 8D 44 24 40 0F 57 C0 8B D7 48 8B CE");
/*
ulonglong FUN_140620f30(undefined8 param_1,undefined8 param_2,longlong param_3,int param_4,
                       undefined8 param_5,undefined8 param_6)
After AE if not mistake
ulonglong FUN_140646370(undefined8 param_1,undefined8 param_2,longlong param_3,int param_4,
                       undefined8 param_5,undefined8 param_6)
       140646370 48 89 5c        MOV        qword ptr [RSP + local_res18],RBX
                 24 18
       140646375 48 89 74        MOV        qword ptr [RSP + local_res20],RSI
                 24 20
       14064637a 57              PUSH       RDI
       14064637b 48 83 ec 30     SUB        RSP,0x30
       14064637f 48 63 fa        MOVSXD     RDI,EDX
       140646382 48 8d b1        LEA        RSI,[RCX + 0x150]
                 50 01 00 00
       140646389 48 8b d9        MOV        RBX,RCX
       14064638c 4c 8d 44        LEA        R8=>local_res8,[RSP + 0x40]
                 24 40
       140646391 0f 57 c0        XORPS      XMM0,XMM0
       140646394 8b d7           MOV        EDX,EDI
       140646396 48 8b ce        MOV        RCX,RSI
       140646399 f3 0f 11        MOVSS      dword ptr [RSP + local_res8],XMM0
                 44 24 40
       14064639f e8 2c de        CALL       FUN_1406641d0                                    undefined FUN_1406641d0()
                 01 00
       1406463a4 84 c0           TEST       AL,AL
       1406463a6 0f 85 e3        JNZ        LAB_14064648f
                 00 00 00
       1406463ac 48 8d 54        LEA        RDX=>local_res8,[RSP + 0x40]
                 24 40
       1406463b1 8b cf           MOV        ECX,EDI
       1406463b3 e8 b8 3e        CALL       FUN_1403fa270                                    undefined FUN_1403fa270()
                 db ff
       1406463b8 84 c0           TEST       AL,AL
       1406463ba 0f 85 cf        JNZ        LAB_14064648f
                 00 00 00
       1406463c0 48 8b 4b 90     MOV        RCX,qword ptr [RBX + -0x70]
       1406463c4 8b d7           MOV        EDX,EDI
       1406463c6 48 81 c1        ADD        RCX,0xe8
                 e8 00 00 00
*/
    //CalculateChargePointsPerUse_Original    = RVAScan<_CalculateChargePointsPerUse>(GET_RVA(CalculateChargePointsPerUse_Original), "48 83 EC 48 0F 29 74 24 30 0F 29 7C 24 20 0F 28 F8 F3 0F 10 05 ? ? ? ? F3 0F 59 C1 F3 0F 10 0D ? ? ? ? E8 ? ? ? ? F3 0F 10 35 ? ? ? ? F3 0F 10 0D ? ? ? ?");
    //CalculateChargePointsPerUse_Original    = RVAScan<_CalculateChargePointsPerUse>(GET_RVA(CalculateChargePointsPerUse_Original), "48 83 EC 48 0F 29 74 24 30 0F 29 7C 24 20 0F 28 F8 F3 0F 10 05");
/*
Before AE
undefined8 FUN_1403c0e40(float param_1,float param_2)
After AE
FUN_1403d8870
       1403d8870 48 83 ec 48     SUB        RSP,0x48
       1403d8874 0f 29 74        MOVAPS     xmmword ptr [RSP + local_18[0]],XMM6
                 24 30
       1403d8879 0f 29 7c        MOVAPS     xmmword ptr [RSP + local_28[0]],XMM7
                 24 20
       1403d887e 0f 28 f8        MOVAPS     XMM7,XMM0
       1403d8881 f3 0f 10        MOVSS      XMM0,dword ptr [DAT_141e78500]                   = 3BA3D70Ah
                 05 77 fc
                 a9 01
       1403d8889 f3 0f 59 c1     MULSS      XMM0,XMM1
       1403d888d f3 0f 10        MOVSS      XMM1,dword ptr [DAT_141e78530]                   = 3F000000h
                 0d 9b fc
                 a9 01
       1403d8895 e8 0a ee        CALL       API-MS-WIN-CRT-MATH-L1-1-0.DLL::powf             float powf(float _X, float _Y)
                 09 01
*/

    //GetEffectiveSkillLevel                    = RVAScan<_GetEffectiveSkillLevel>(GET_RVA(GetEffectiveSkillLevel), "40 53 48 83 EC 20 48 8B 01 48 63 DA 8B D3 FF 50 08 48 8B 05 ? ? ? ? 48 8B 4C D8 08 8B 51 60 8B C2 C1 E8 04 A8 01 74 1E F3 0F 10 0D ? ? ? ? 0F 2F C1 73 44 0F 57 D2 0F 2F C2 73 3F");
    GetEffectiveSkillLevel                    = RVAScan<_GetEffectiveSkillLevel>(GET_RVA(GetEffectiveSkillLevel), "40 53 48 83 EC 20 48 8B 01 48 63 DA 8B D3 FF 50 08 48 8B 05 ? ? ? ? 0F 28 C8 48 8B 4C D8 08 8B 51 60 8B C2 C1 E8 04 A8 01 74 18 F3 0F 5D 0D");
/*
Before AE
undefined8 FUN_1403e5250(longlong *param_1,int param_2)
After AE
ulonglong FUN_1403fdf00(longlong *param_1,int param_2)
       1403fdf00 40 53           PUSH       RBX
       1403fdf02 48 83 ec 20     SUB        RSP,0x20
       1403fdf06 48 8b 01        MOV        RAX,qword ptr [RCX]
       1403fdf09 48 63 da        MOVSXD     RBX,EDX
       1403fdf0c 8b d3           MOV        EDX,EBX
       1403fdf0e ff 50 08        CALL       qword ptr [RAX + 0x8]
       1403fdf11 48 8b 05        MOV        RAX,qword ptr [DAT_141f58c18]                    = ??
                 00 ad b5 01
       1403fdf18 0f 28 c8        MOVAPS     XMM1,XMM0
       1403fdf1b 48 8b 4c        MOV        RCX,qword ptr [RAX + RBX*0x8 + 0x8]
                 d8 08
       1403fdf20 8b 51 60        MOV        EDX,dword ptr [RCX + 0x60]
       1403fdf23 8b c2           MOV        EAX,EDX
       1403fdf25 c1 e8 04        SHR        EAX,0x4
       1403fdf28 a8 01           TEST       AL,0x1
       1403fdf2a 74 18           JZ         LAB_1403fdf44
       1403fdf2c f3 0f 5d        MINSS      XMM1,dword ptr [DAT_14161af50]                   = 42C80000h
                 0d 1c d0
                 21 01
       1403fdf34 0f 57 c0        XORPS      XMM0,XMM0
       1403fdf37 f3 0f 5f c8     MAXSS      XMM1,XMM0
       1403fdf3b 0f 28 c1        MOVAPS     XMM0,XMM1
       1403fdf3e 48 83 c4 20     ADD        RSP,0x20
       1403fdf42 5b              POP        RBX
       1403fdf43 c3              RET
*/
    _MESSAGE("InitRVA complete");
}

void Hook_Skill_Commit()
{
    InitRVA();
    _MESSAGE("Do hooks");
    //Before AE SafeWrite16(GetEffectiveSkillLevel.GetUIntPtr() + 0x34, 0x9090);
    //unsigned char nop[] = {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    //SafeWriteBuf(GetEffectiveSkillLevel.GetUIntPtr() + 0x2C, &nop, sizeof(nop));
    {
        struct SkillEffectiveCapPatch_Code : Xbyak::CodeGenerator
        {
            SkillEffectiveCapPatch_Code(void* bufPtr) : Xbyak::CodeGenerator(4096, bufPtr)
            {
                mov(rcx, rbx); // pass id from rbx as first arg, xmm0 and xmm1 has current value
                call((void *)&ClampSkillEffect);
                add(rsp, 0x20); // Original ret code
                pop(rbx);
                ret();
            }
        };
        void* codeBuf = g_localTrampoline.StartAlloc();
        SkillEffectiveCapPatch_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());
        g_branchTrampoline.Write6Branch(GetEffectiveSkillLevel.GetUIntPtr() + 0x2C, uintptr_t(code.getCode()));
    }
/*
skill? range. min(100,val) and max(0,val). replacing MINSS with nop
       1403fdf2c f3 0f 5d        MINSS      XMM1,dword ptr [DAT_14161af50]                   = 42C80000h
                 0d 1c d0
                 21 01
       1403fdf34 0f 57 c0        XORPS      XMM0,XMM0
       1403fdf37 f3 0f 5f c8     MAXSS      XMM1,XMM0
*/

    {
        //fVar14 == XMM10 = 100.0(maximum)
        //fVar8 == XMM0 < 100.0(current)
        //ESI SkillId
        struct SkillCapPatch_Code : Xbyak::CodeGenerator
        {
            SkillCapPatch_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
            {
                Xbyak::Label retnLabel;
                movd(xmm10, ecx); // save ecx in xmm10 just in case. XMM6-XMM15 nonvolatile
                mov(ecx, esi); // pass SkillId in ecx to hook
                sub(rsp, 0x30);
                movss(ptr[rsp + 0x28], xmm0); // save current skill level
                call((void *)&GetSkillCap_Hook);
                movd(ecx, xmm10); // restore ecx
                movss(xmm10, xmm0); // replace maximum with function result
                movss(xmm0, ptr[rsp + 0x28]); // restore current skill level
                add(rsp, 0x30);
                jmp(ptr[rip + retnLabel]); // jump to cur and max cmp
            L(retnLabel);
                dq(kHook_SkillCapPatch.GetRetAddr());
            }
        };

        void * codeBuf = g_localTrampoline.StartAlloc();
        SkillCapPatch_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        kHook_SkillCapPatch.Hook(reinterpret_cast<uintptr_t>(code.getCode()));
    }

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
                dq(ImprovePlayerSkillPoints.GetUIntPtr() + 0x6);
            }
        };

        void * codeBuf = g_branchTrampoline.StartAlloc();
        ImprovePlayerSkillPoints_Code code(codeBuf);
        g_branchTrampoline.EndAlloc(code.getCurr());

        ImprovePlayerSkillPoints_Original = (_ImprovePlayerSkillPoints)codeBuf;

        g_branchTrampoline.Write5Branch(ImprovePlayerSkillPoints.GetUIntPtr(), (uintptr_t)ImprovePlayerSkillPoints_Hook);
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
                dq(ImproveAttributeWhenLevelUp.GetUIntPtr() + 0x6);
            }
        };
        void * codeBuf = g_localTrampoline.StartAlloc();
        ImproveAttributeWhenLevelUp_Code code(codeBuf);
        g_localTrampoline.EndAlloc(code.getCurr());

        ImproveAttributeWhenLevelUp_Original = (_ImproveAttributeWhenLevelUp)codeBuf;

        g_branchTrampoline.Write6Branch(ImproveAttributeWhenLevelUp.GetUIntPtr(), (uintptr_t)ImproveAttributeWhenLevelUp_Hook);

        SafeWrite8(ImproveAttributeWhenLevelUp.GetUIntPtr() + 0x9B, 0);
/*
       1408c4793 ff 50 28        CALL       qword ptr [RAX + 0x28]
       1408c4796 83 7f 18 1a     CMP        dword ptr [RDI + 0x18],0x1a
       1408c479a 75 22           JNZ        LAB_1408c47be
22 replaced with 0. To disable jump over increasing carry weight if not stamina(0x1a) selected?
*/
    }
    bool ok = SafeWriteCall(ImproveSkillLevel_Hook.GetUIntPtr(), (uintptr_t)ImprovePlayerSkillPoints_Original);
    ASSERT(ok);
    { // Patching skill exp to player level exp modification
        const unsigned entOffset = 0x2D7;
        const unsigned retOffset = entOffset + 8;
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
                    dq(ImprovePlayerSkillPoints.GetUIntPtr() + retOffset);
                }
            };
            void * codeBuf = g_localTrampoline.StartAlloc();
            ImproveLevelExpBySkillLevel_Code code(codeBuf);
            g_localTrampoline.EndAlloc(code.getCurr());
            const unsigned char expectedCode[] = {0xf3, 0x0f, 0x58, 0x08, 0xf3, 0x0f, 0x11, 0x08};
            ASSERT(memcmp((void*)(ImprovePlayerSkillPoints.GetUIntPtr() + entOffset), expectedCode, sizeof(expectedCode)) == 0);
            g_branchTrampoline.Write6Branch(ImprovePlayerSkillPoints.GetUIntPtr() + entOffset, uintptr_t(code.getCode()));
        }
    return;
    // not updated code

    //g_branchTrampoline.Write6Branch(CalculateChargePointsPerUse_Original.GetUIntPtr(), (uintptr_t)CalculateChargePointsPerUse_Hook);

    /*{
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

    }*/

    /*{
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

    }*/
}


