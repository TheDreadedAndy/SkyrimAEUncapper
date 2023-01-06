/**
 * @file Signatures.h
 * @author Kasplat
 * @brief Code signatures for the assembly the skill functions hook in to.
 * @bug No known bugs.
 *
 * Note that these signatures and comments were originally written by vadfromnu.
 */

#ifndef __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__
#define __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__

#include <cstddef>

/// @brief The byte size of the branches which are patched into the code.
const size_t kBranchPatchSize = 5;

/**
 * @brief The signature and offset used to redirect to the code which alters
 *        the real skill cap.
 *
 * The offset into this signature overwrites a movess instruction and instead
 * redirects to our handler. Note that the last four bytes of this instruction
 * must be overwritten with 0x90 (NOP), at the request of the author of the
 * eXPerience mod (17751).
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
///@}

#endif /* __SKYRIM_SSE_SKILL_UNCAPPER_SIGNATURES_H__ */
