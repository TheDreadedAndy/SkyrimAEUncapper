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
///@{
const char *const kModifyPerkPoolSig =
    "48 85 C0 74 ? 66 0F 6E ? 0F 5B C0 F3 0F 58 40 34 F3 0F 11 40 34 48 83 C4 "
    "20 ? C3 48 8B 15 ? ? ? ? 0F B6 8A 09 0B 00 00 8B C1 03 ? 78";
const size_t kModifyPerkPoolSigOffset = 0x1C;
const size_t kModifyPerkPoolPatchSize = 0x1D;
///@}

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
 * 14070ec75 48 81 c1        ADD        param_1,0xb0
 *           b0 00 00 00
 * 14070ec7c 48 8b 01        MOV        RAX,qword ptr [param_1]
 * 14070ec7f ff 50 18        CALL       qword ptr [RAX + 0x18]
 * 14070ec82 44 0f 28 c0     MOVAPS     XMM8,XMM0
 * 14070ec86 ### kHook_SkillCapPatch_Ent ###
 *           f3 44 0f        MOVSS      XMM10,dword ptr [DAT_14161af50] = 42C80000h 100.0
 *           10 15
 *           ### kHook_SkillCapPatch_Ret ###
 *           c1 c2 f0 00
 * 14070ec8f 41 0f 2f c2     COMISS     XMM0,XMM10
 * 14070ec93 0f 83 d8        JNC        LAB_14070ef71
 *           02 00 00
 * 14070ec99 f3 0f 10        MOVSS      XMM7,dword ptr [DAT_14161d3b8]  = 3F800000h 1.0
 *           3d 17 e7
 *           f0 00
 * 14070eca1 c7 44 24        MOV        dword ptr [RSP + local_178],0x3f800000
 *           30 00 00
 *           80 3f
 * }
 *
 * Note that the code being patched expects the current skill level in XMM0 and
 * the maximum skill level in XMM10.
 */
///@{
const char *const kSkillCapPatchSig =
    "48 8B 0D ? ? ? ? 48 81 C1 B8 00 00 00 48 8B 01 FF 50 18 44 0F 28 C0";
const size_t kSkillCapPatchSigOffset = 0x18;
const size_t kSkillCapPatchPatchSize = 9;
///@}

#endif /* __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__ */
