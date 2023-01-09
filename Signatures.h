/**
 * @file Signatures.h
 * @author Andrew Spaulding (Kasplat)
 * @brief Code signatures for the assembly the skill functions hook in to.
 * @bug No known bugs.
 *
 * Note that these signatures and comments were originally written by vadfromnu.
 * This file is more just a reorganization of his work.
 */

#ifndef __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__
#define __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__

#include <cstddef>

#include "RelocFn.h"

/**
 * @brief The signature and offset used to hook into the perk pool modification
 *        routine.
 *
 * The assembly for this signature is as follows:
 * 48 85 c0        TEST       RAX,RAX
 * 74 34           JZ         LAB_1408f678f
 * LAB_1408f675b   XREF[2]:     1435b64c0(*), 1435b64c8(*)
 * 66 0f 6e c7     MOVD       XMM0,EDI
 * 0f 5b c0        CVTDQ2PS   XMM0,XMM0
 * f3 0f 58        ADDSS      XMM0,dword ptr [RAX + 0x34]
 * 40 34
 * f3 0f 11        MOVSS      dword ptr [RAX + 0x34],XMM0
 * 40 34
 * 48 83 c4 20     ADD        RSP,0x20
 * 5f              POP        RDI
 * c3              RET
 * LAB_1408f6772   XREF[1]:     1408f671f(j)
 * ### kHook_ModifyPerkPool (entry) ###
 * 48 8b 15        MOV        RDX,qword ptr [DAT_142fc19c8]
 * 4f b2 6c 02
 * 0f b6 8a        MOVZX      ECX,byte ptr [RDX + 0xb01]
 * 01 0b 00 00
 * 8b c1           MOV        EAX,ECX
 * 03 c7           ADD        EAX,EDI
 * 78 09           JS         LAB_1408f678f
 * 40 02 cf        ADD        CL,DIL
 * 88 8a 01        MOV        byte ptr [RDX + 0xb01],CL
 * 0b 00 00
 * LAB_1408f678f   XREF[2]:     1408f6759(j), 1408f6784(j)
 * 48 83 c4 20     ADD        RSP,0x20
 * ### kHook_ModifyPerkPool (return) ###
 * 5f              POP        RDI
 * c3              RET
 */
const FunctionSignature kHook_ModifyPerkPoolSig(
    /* name */        "kHook_ModifyPerkPool",
    /* hook_type */   HookType::Branch5,
    /* sig */         "48 85 C0 74 ? 66 0F 6E ? 0F 5B C0 F3 0F 58 40 34 F3 0F "
                      "11 40 34 48 83 C4 20 ? C3 48 8B 15 ? ? ? ? 0F B6 8A 09 "
                      "0B 00 00 8B C1 03 ? 78",
    /* patch_size */  0x1D,
    /* hook_offset */ 0x1C
);

/**
 * @brief The signature and offset used to redirect to the code which alters
 *        the real skill cap.
 *
 * The offset into this signature overwrites a movess instruction and instead
 * redirects to our handler. Note that the last four bytes of this instruction
 * must be overwritten with 0x90 (NOP), at the request of the author of the
 * eXPerience mod (17751). This is handled by the RelocFn interface.
 *
 * This signature hooks into the following function:
 * void FUN_14070ec10(
 *     undefined8 param_1,
 *     undefined8 param_2,
 *     float param_3,
 *     float **param_4,
 *     int param_5_00,
 *     undefined8 param_6_00,
 *     undefined8 param_7_00,
 *     undefined4 param_5,
 *     char param_6,
 *     char param_7
 * ) {
 * 48 81 c1        ADD        param_1,0xb0
 * b0 00 00 00
 * 48 8b 01        MOV        RAX,qword ptr [param_1]
 * ff 50 18        CALL       qword ptr [RAX + 0x18]
 * 44 0f 28 c0     MOVAPS     XMM8,XMM0
 * ### kHook_SkillCapPatch_Ent ###
 * f3 44 0f        MOVSS      XMM10,dword ptr [DAT_14161af50] = 42C80000h 100.0
 * 10 15
 * ### kHook_SkillCapPatch_Ret ###
 * c1 c2 f0 00
 * 41 0f 2f c2     COMISS     XMM0,XMM10
 * 0f 83 d8        JNC        LAB_14070ef71
 * 02 00 00
 * f3 0f 10        MOVSS      XMM7,dword ptr [DAT_14161d3b8]  = 3F800000h 1.0
 * 3d 17 e7
 * f0 00
 * c7 44 24        MOV        dword ptr [RSP + local_178],0x3f800000
 * 30 00 00
 * 80 3f
 * }
 *
 * Note that the code being patched expects the current skill level in XMM0 and
 * the maximum skill level in XMM10.
 */
const FunctionSignature kHook_SkillCapPatchSig(
    /* name */        "kHook_SkillCapPatch",
    /* hook_type */   HookType::Branch5,
    /* sig */         "48 8B 0D ? ? ? ? 48 81 C1 B8 00 00 00 48 8B 01 FF 50 "
                      "18 44 0F 28 C0",
    /* patch_size */  9,
    /* hook_offset */ 0x18
);

#if 0
    kHook_ExecuteLegendarySkill_Ent            = RVAScan<uintptr_t *>(GET_RVA(kHook_ExecuteLegendarySkill_Ent), "0F 82 85 00 00 00 48 8B 0D ? ? ? ? 48 81 C1 B0 00 00 00 48 8B 01 F3 0F 10 15 ? ? ? ? 8B 56 1C FF 50 20 48 8B 05 ? ? ? ? 8B 56 1C 48 8B 88 B0 09 00 00");
    kHook_ExecuteLegendarySkill_Ret            = kHook_ExecuteLegendarySkill_Ent;
    kHook_ExecuteLegendarySkill_Ret            += 6;

    kHook_CheckConditionForLegendarySkill_Ent = RVAScan<uintptr_t *>(GET_RVA(kHook_CheckConditionForLegendarySkill_Ent), "8B D0 48 8D 8F B0 00 00 00 FF 53 18 0F 2F 05 ? ? ? ? 0F 82 10 0A 00 00 45 33 FF 4C 89 7D 80 44 89 7D 88 45 33 C0 48 8B 15 ? ? ? ? 48 8D 4D 80");
    kHook_CheckConditionForLegendarySkill_Ret = kHook_CheckConditionForLegendarySkill_Ent;
    kHook_CheckConditionForLegendarySkill_Ret += 0x13;

    kHook_HideLegendaryButton_Ent            = RVAScan<uintptr_t *>(GET_RVA(kHook_HideLegendaryButton_Ent), "48 8B 0D ? ? ? ? 48 81 C1 B0 00 00 00 48 8B 01 8B D6 FF 50 18 0F 2F 05 ? ? ? ? 72 64 48 8D 05 ? ? ? ? 48 89 85 C0 00 00 00 4C 89 64 24 20");
    kHook_HideLegendaryButton_Ret            = kHook_HideLegendaryButton_Ent;
    kHook_HideLegendaryButton_Ret            += 0x1D;
#endif

/**
 * @brief TODO
 *
 * FIXME:
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
const FunctionSignature kHook_ImproveSkillLevelSig(
    /* name */        "kHook_ImproveSkillLevel",
    /* hook_type */   HookType::DirectCall,
    /* sig */         "F3 0F 10 54 9F 10 41 3B F4 F3 0F 5C 54 9F 0C 0F 92 C0 "
                      "8B D5 88 44 24 30 45 33 C9 44 88 6C 24 28 49 8B CF 44 "
                      "89 6C 24 20 E8 73 FB FF FF FF C6 41 3B F6 72 CC",
    /* patch_size */  kDirectCallPatchSize,
    /* hook_offset */ (0x14070ee08 - 0x14070ede0)
);

/**
 * @brief TODO
 *
 * FIXME
 * kHook_SkillCapPatch_Ent is inside this function
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
const FunctionSignature kHook_ImprovePlayerSkillPointsSig(
    /* name */        "kHook_ImprovePlayerSkillPoints",
    /* hook_type */   HookType::Branch5,
    /* sig */         "48 8B C4 57 41 54 41 55 41 56 41 57 48 81 EC 80 01 00 "
                      "00 48 C7 44 24 48 FE FF FF FF",
    /* patch_size */  6
);

/**
 * @brief TODO
 *
 * FIXME: It would probably be more stable to give this its own signature.
 */
const FunctionSignature kHook_ImproveLevelExpBySkillLevelSig(
    /* name */        "kHook_ImproveLevelExpBySkillLevel",
    /* hook_type */   HookType::Branch6,
    /* sig */         kHook_ImprovePlayerSkillPointsSig.sig,
    /* patch_size */  8,
    /* hook_offset */ 0x2D7
);

/**
 * @brief TODO
 *
 * FIXME
 *
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
const FunctionSignature kHook_ImproveAttributeWhenLevelUpSig(
    /* name */        "ImproveAttributeWhenLevelUp",
    /* hook_type */   HookType::Branch6,
    /* sig */         "0F B6 DA 48 8B F9 48 8B 15 ? ? ? ? 48 81 C2 28 01 00 "
                      "00 48 8B 0D ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 BA 00 00 "
                      "00 84 DB 0F 85 AA 00 00 00",
    /* patch_size */  6,
    /* hook_offset */ -0x1E
);

#if 0
    GetSkillCoefficients                    = RVAScan<_GetSkillCoefficients>(GET_RVA(GetSkillCoefficients), "81 F9 A3 00 00 00 77 52 48 8B 05 ? ? ? ? 48 63 C9 4C 8B 54 C8 08 4D 85 D2 74 3E 49 8B 82 08 01 00 00 48 85 C0 74 32");
#endif

/**
 * @brief TODO
 */
const FunctionSignature GetLevelSig(
    /* name */            "GetLevel",
    /* hook_type */       HookType::None,
    /* sig */             "84 C0 74 24 48 8B CB E8 ? ? ? ? 0F B7 C8 48 8B D7 "
                          "E8 ? ? ? ? 84 C0 74 0D B0 01 48 8B 5C 24 30 48 83 "
                          "C4 20 5F C3",
    /* patch_size */      0,
    /* hook_offset */     7,
    /* indirect_offset */ 1,
    /* instr_size */      5
);

#if 0
    ImproveLevelExpBySkillLevel_Original    = RVAScan<_ImproveLevelExpBySkillLevel>(GET_RVA(ImproveLevelExpBySkillLevel_Original), "F3 0F 58 D3 0F 28 E0 0F 29 34 24 0F 57 F6 0F 28 CA F3 0F 58 CB F3 0F 59 CA F3 0F 59 CD F3 0F 2C C1 66 0F 6E C0 0F 5B C0 F3 0F 5C C8 0F 2F CE 73 04 F3 0F 5C C3", -0x17);
/*
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
#endif

#if 0
    //GetCurrentActorValue                    = RVAScan<_GetCurrentActorValue>(GET_RVA(GetCurrentActorValue), "4C 8B 44 F8 08 41 8B 40 60 C1 E8 12 A8 01 74 38 48 8B 49 40 48 85 C9 74 2F 48 83 79 50 00 74 28 40 B5 01 0F 57 C0 F3 0F 11 44 24 78 4C 8D 44 24 78 8B D7", -0x2C);
/*
ulonglong UndefinedFunction_140620d60
                    (undefined8 param_1,undefined8 param_2,longlong param_3,int param_4,
                    undefined8 param_5,undefined8 param_6)
After AE
ulonglong FUN_1406462a0(undefined8 param_1,undefined8 param_2,longlong param_3,int param_4)
params count changed or wrong function?
*/
#endif

/**
 * @brief TODO
 *
 * FIXME
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
const FunctionSignature GetBaseActorValueSig(
    /* name */       "GetBaseActorValue",
    /* hook_type */  HookType::None,
    /* sig */        "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 30 48 63 FA "
                     "48 8D B1 50 01 00 00 48 8B D9 4C 8D 44 24 40 0F 57 C0 "
                     "8B D7 48 8B CE",
    /* patch_size */ 0
);

// FIXME: Not sure why this was disabled. Broken? See nexus notes.
#if 0
    //CalculateChargePointsPerUse_Original    = RVAScan<_CalculateChargePointsPerUse>(GET_RVA(CalculateChargePointsPerUse_Original), "48 83 EC 48 0F 29 74 24 30 0F 29 7C 24 20 0F 28 F8 F3 0F 10 05 ? ? ? ? F3 0F 59 C1 F3 0F 10 0D ? ? ? ? E8 ? ? ? ? F3 0F 10 35 ? ? ? ? F3 0F 10 0D ? ? ? ?");
    CalculateChargePointsPerUse_Original    = RVAScan<_CalculateChargePointsPerUse>(GET_RVA(CalculateChargePointsPerUse_Original), "48 83 EC 48 0F 29 74 24 30 0F 29 7C 24 20 0F 28 F8 F3 0F 10 05");
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
#endif

/**
 * @brief TODO
 *
 * FIXME
 *
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
const FunctionSignature kHook_GetEffectiveSkillLevelSig(
    /* name */        "GetEffectiveSkillLevel",
    /* hook_type */   HookType::Branch6,
    /* sig */         "40 53 48 83 EC 20 48 8B 01 48 63 DA 8B D3 FF 50 08 48 "
                      "8B 05 ? ? ? ? 0F 28 C8 48 8B 4C D8 08 8B 51 60 8B C2 "
                      "C1 E8 04 A8 01 74 18 F3 0F 5D 0D",
    /* patch_size */  6,
    /* hook_offset */ 0x2C
);

#endif /* __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__ */
