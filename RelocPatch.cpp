/**
 * @file RelocPatch.cpp
 * @author Andrew Spaulding (Kasplat)
 * @brief Finds code signatures within the game and applies patches to them.
 * @bug No known bugs.
 *
 * This file contains code signatures which are used by the plugin to find
 * the game code which must be patched. These signatures are parsed by the
 * ApplyGamePatches() function, which will first find each location to be
 * patched and then apply the patches.
 *
 * Locations are resolved before patching to ensure that overlapping patches
 * do not prevent each other from being found. Additionally, this is likely
 * better from a performance standpoint, as the games binary will still
 * be in the cache (at least partially).
 *
 * Note that many of these signatures and the associated assembly code were
 * found by the other authors which worked on this mod.
 */

#include "RelocFn.h"
#include "RelocPatch.h"

#include <cstddef>
#include <cstdint>

#include "GameSettings.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"
#include "addr_lib/versionlibdb.h"

#include "Hook_Skill.h"
#include "HookWrappers.h"
#include "SafeMemSet.h"
#include "Settings.h"

/// @brief Encodes the various types of hooks which can be injected.
struct HookType {
    enum t {
        None,
        Jump5,
        Jump6,
        DirectJump,
        Call5,
        Call6,
        DirectCall,
        Nop
    };

    /// @brief Gets the patch size of the given hook type.
    static size_t
    Size(
        t type
    ) {
        size_t ret = 0;

        switch (type) {
            case None:
            case Nop:
                ret = 0;
                break;
            case Jump5:
            case Call5:
            case DirectCall:
            case DirectJump:
                ret = 5;
                break;
            case Jump6:
            case Call6:
                ret = 6;
                break;
            default:
                HALT("Cannot get the size of an invalid hook type.");
        }

        return ret;
    }
};

/// @brief Describes a patch to be applied by a RelocPatch<T>.
struct CodeSignature {
    const char* name;
    HookType::t hook_type;
    uintptr_t hook;
    unsigned long long id;
    size_t patch_size;
    ptrdiff_t offset;
    uintptr_t *return_trampoline;
    void **result;

    // Optional argument for finding new addresses.
#ifdef _DEBUG
    uintptr_t known_offset;
#endif

    /**
     * @brief Creates a new patch signature structure.
     * @param name The name of the patch signature.
     * @param hook_type The non-none type of hook to be inserted.
     * @param hook The hook to install with this patch.
     * @param sig The hex signature to search for.
     * @param patch_size The size of the code to be overwritten.
     * @param return_trampoline The trampoline to be filled with the patch
     *                          return address.
     * @param offset The offset from the signature to the hook address.
     * @param indirect_offset An indirection offset to use to dereference the
     *                        first hook address to get a second, or 0.
     * @param instr_size The size of the instruction being indirected through,
     *                   or 0.
     */
    CodeSignature(
        const char* name,
        HookType::t hook_type,
        uintptr_t hook,
        unsigned long long id,
        size_t patch_size,
        uintptr_t *return_trampoline = nullptr,
        ptrdiff_t offset = 0
#ifdef _DEBUG
        , uintptr_t known_offset = 0
#endif
    ) : name(name),
        hook_type(hook_type),
        hook(hook),
        id(id),
        patch_size(patch_size),
        offset(offset),
        return_trampoline(return_trampoline),
#ifdef _DEBUG
        known_offset(known_offset),
#endif
        result(nullptr)
    {}

    /**
     * @brief Creates a new code signature which links to a game object.
     * @param name The name of the code signature.
     * @param sig The signature to search for.
     * @param result The pointer to be filled with the found result.
     * @param offset The offset into the signature to inspect.
     * @param indirect_offset An indirection offset to use to dereference the
     *                        the first generated address.
     * @param instr_size The size of the instruction being indirected through,
     *                   or 0.
     */
    CodeSignature(
        const char *name,
        unsigned long long id,
        void **result
#ifdef _DEBUG
        , uintptr_t real_address = 0
#endif
    ) : name(name),
        hook_type(HookType::None),
        hook(0),
        id(id),
        patch_size(0),
        offset(0),
        return_trampoline(nullptr),
#ifdef _DEBUG
        known_offset(known_offset),
#endif
        result(result)
    {}
};

// FIXME
#if 0
typedef bool(*_GetSkillCoefficients)(UInt32, float*, float*, float*, float*);
typedef float(*_CalculateChargePointsPerUse)(float basePoints, float enchantingLevel);

static RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ent = 0x08C95FD;//get it when legendary skill by trace setbaseav.
static RelocAddr <uintptr_t *> kHook_ExecuteLegendarySkill_Ret = 0x08C95FD + 0x6;

static RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ent = 0x08BF655;
static RelocAddr <uintptr_t *> kHook_CheckConditionForLegendarySkill_Ret = 0x08BF655 + 0x13;

static RelocAddr <_GetSkillCoefficients> GetSkillCoefficients(0x03E37F0);

static RelocAddr <_CalculateChargePointsPerUse> CalculateChargePointsPerUse_Original(0x03C0F10);
#endif

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
    uintptr_t GetEffectiveSkillLevel_ReturnTrampoline;
    uintptr_t DisplayTrueSkillLevel_ReturnTrampoline;
    uintptr_t HideLegendaryButton_ReturnTrampoline;
}
///@}

/**
 * @brief Holds the function pointers which we use to call the game functions.
 */
///@{
static float (*GetBaseActorValue_Entry)(void *, UInt32);
static UInt16 (*GetLevel_Entry)(void*);
///@}

/**
 * @brief The signature used to find the games "GetLevel" function.
 */
static const CodeSignature kGetLevel_FunctionSig(
    /* name */   "GetLevel",
    /* id */     37334,
    /* result */ reinterpret_cast<void**>(&GetLevel_Entry)
);

/**
 * @brief The code signature used to find the games GetBaseActorValue() fn.
 *
 * The assembly for the signature is as follows:
 * 48 89 5c        MOV        qword ptr [RSP + local_res18],RBX
 * 24 18
 * 48 89 74        MOV        qword ptr [RSP + local_res20],RSI
 * 24 20
 * 57              PUSH       RDI
 * 48 83 ec 30     SUB        RSP,0x30
 * 48 63 fa        MOVSXD     RDI,EDX
 * 48 8d b1        LEA        RSI,[RCX + 0x150]
 * 50 01 00 00
 * 48 8b d9        MOV        RBX,RCX
 * 4c 8d 44        LEA        R8=>local_res8,[RSP + 0x40]
 * 24 40
 * 0f 57 c0        XORPS      XMM0,XMM0
 * 8b d7           MOV        EDX,EDI
 * 48 8b ce        MOV        RCX,RSI
 * ...
 */
static const CodeSignature kGetBaseActorValue_FunctionSig(
    /* name */   "GetBaseActorValue",
    /* id */     38464,
    /* result */ reinterpret_cast<void**>(&GetBaseActorValue_Entry)
);

/**
 * @brief The signature and offset used to hook into the perk pool modification
 *        routine.
 *
 * Upon entry into our hook, we run our function. We then reimplement the final
 * few instructions in the return path of the function we hooked into. This way,
 * we need only modify one instruction and can still use the RelocPatch interface.
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
 * ### kHook_ModifyPerkPool (redirect; does not return here) ###
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
 * 5f              POP        RDI
 * c3              RET
 */
static const CodeSignature kModifyPerkPool_PatchSig(
    /* name */       "ModifyPerkPool",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ModifyPerkPool_Wrapper),
    /* id */         52538,
    /* patch_size */ 7,
    /* trampoline */ nullptr,
    /* offset */     0x62
);

/**
 * @brief The signature and offset used to redirect to the code which alters
 *        the real skill cap.
 *
 * The offset into this signature overwrites a movess instruction and instead
 * redirects to our handler. Note that the last four bytes of this instruction
 * must be overwritten with 0x90 (NOP), at the request of the author of the
 * eXPerience mod (17751). This is handled by the RelocPatch interface.
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
static const CodeSignature kSkillCapPatch_PatchSig(
    /* name */       "SkillCapPatch",
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(SkillCapPatch_Wrapper),
    /* id */         41561,
    /* patch_size */ 9,
    /* trampoline */ nullptr,
    /* offset */     0x76
);

#if 0
    kHook_ExecuteLegendarySkill_Ent            = RVAScan<uintptr_t *>(GET_RVA(kHook_ExecuteLegendarySkill_Ent), "0F 82 85 00 00 00 48 8B 0D ? ? ? ? 48 81 C1 B0 00 00 00 48 8B 01 F3 0F 10 15 ? ? ? ? 8B 56 1C FF 50 20 48 8B 05 ? ? ? ? 8B 56 1C 48 8B 88 B0 09 00 00");
    kHook_ExecuteLegendarySkill_Ret            = kHook_ExecuteLegendarySkill_Ent;
    kHook_ExecuteLegendarySkill_Ret            += 6;

    kHook_CheckConditionForLegendarySkill_Ent = RVAScan<uintptr_t *>(GET_RVA(kHook_CheckConditionForLegendarySkill_Ent), "8B D0 48 8D 8F B0 00 00 00 FF 53 18 0F 2F 05 ? ? ? ? 0F 82 10 0A 00 00 45 33 FF 4C 89 7D 80 44 89 7D 88 45 33 C0 48 8B 15 ? ? ? ? 48 8D 4D 80");
    kHook_CheckConditionForLegendarySkill_Ret = kHook_CheckConditionForLegendarySkill_Ent;
    kHook_CheckConditionForLegendarySkill_Ret += 0x13;
#endif

/**
 * @brief Hooks into the legendary button display code to allow it to be hidden.
 *
 * The assembly for this hook is as follows:
 * 48 8b 0d 2e d4 6b 02 	mov    0x26bd42e(%rip),%rcx        # 0x142fc1b78
 * 48 81 c1 b8 00 00 00 	add    $0xb8,%rcx
 * 48 8b 01             	mov    (%rcx),%rax
 * 41 8b d7             	mov    %r15d,%edx
 * ff 50 18             	callq  *0x18(%rax)
 * 0f 2f 05 bf 57 d1 00 	comiss 0xd157bf(%rip),%xmm0        # 0x141619f20
 * 72 6b                	jb     0x1409047ce
 * 48 8d 05 d6 b9 e9 00 	lea    0xe9b9d6(%rip),%rax        # 0x1417a0140
 * 48 89 85 c0 00 00 00 	mov    %rax,0xc0(%rbp)
 * 48 8d 3d 58 99 c2 ff 	lea    -0x3d66a8(%rip),%rdi
*/
static const CodeSignature kHideLegendaryButton_PatchSig(
    /* name */       "HideLegendaryButton",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(HideLegendaryButton_Wrapper),
    /* id */         52527,
    /* patch_size */ 0x1e,
    /* trampoline */ &HideLegendaryButton_ReturnTrampoline,
    /* offset */     0x153
);

#if 0 // not updated code

    g_branchTrampoline.Write6Branch(CalculateChargePointsPerUse_Original.GetUIntPtr(), (uintptr_t)CalculateChargePointsPerUse_Hook);

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
static const CodeSignature kImproveSkillLevel_PatchSig(
    /* name */       "ImproveSkillLevel",
    /* hook_type */  HookType::Call5,
    /* hook */       reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Original),
    /* id */         41562,
    /* patch_size */ 5,
    /* trampoline */ nullptr,
    /* offset */     0x98
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
static const CodeSignature kImprovePlayerSkillPoints_PatchSig(
    /* name */       "ImprovePlayerSkillPoints",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Hook),
    /* id */         41561,
    /* patch_size */ 6,
    /* trampoline */ &ImprovePlayerSkillPoints_ReturnTrampoline
);

/**
 * @brief TODO
 *
 * FIXME: It would probably be more stable to give this its own signature.
 * FIXME: Should verify that this signature is correct against the one in the OG version.
 *
 * const unsigned char expectedCode[] = {0xf3, 0x0f, 0x58, 0x08, 0xf3, 0x0f, 0x11, 0x08};
 * ASSERT(memcmp((void*)(kHook_ImproveLevelExpBySkillLevel.GetUIntPtr()), expectedCode, sizeof(expectedCode)) == 0);
 */
static const CodeSignature kImproveLevelExpBySkillLevel_PatchSig(
    /* name */       "ImproveLevelExpBySkillLevel",
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveLevelExpBySkillLevel_Wrapper),
    /* id */         41561,
    /* patch_size */ 8,
    /* trampoline */ nullptr,
    /* offset */     0x2D7
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
static const CodeSignature kImproveAttributeWhenLevelUp_PatchSig(
    /* name */       "ImproveAttributeWhenLevelUp",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveAttributeWhenLevelUp_Hook),
    /* id */         51917,
    /* patch_size */ 6,
    /* trampoline */ &ImproveAttributeWhenLevelUp_ReturnTrampoline
);

/**
 * @brief Allows health and magicka level ups to improve carry weight.
 * 
 * This patch simply overwrites the branch instruction which would skip carry
 * weight improvement for health/magicka with NOPs.
 */
static const CodeSignature kAllowAllAttrImproveCarryWeight_PatchSig(
    /* name */       "AllowAllAttrImproveCarryWeight",
    /* hook_type */  HookType::Nop,
    /* hook */       0,
    /* id */         51917,
    /* patch_size */ 2,
    /* trampoline */ nullptr,
    /* offset */     0x9a
);

#if 0
    GetSkillCoefficients                    = RVAScan<_GetSkillCoefficients>(GET_RVA(GetSkillCoefficients), "81 F9 A3 00 00 00 77 52 48 8B 05 ? ? ? ? 48 63 C9 4C 8B 54 C8 08 4D 85 D2 74 3E 49 8B 82 08 01 00 00 48 85 C0 74 32");
#endif

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
 * @brief Caps the effective skill level in calculations by always returning
 *        a damaged result.
 *
 * This patch redirects to our hook, with an assembly wrapper allowing the
 * hook to call the original implementation. The assembly wrapper reimplements
 * the first 6 bytes, then jumps to the instruction after the hook.
 *
 * The assembly signature for the function is as follows:
 * 4C 8B DC             mov         r11,rsp
 * 55                   push        rbp
 * 56                   push        rsi
 * 57                   push        rdi
 * 41 56                push        r14
 * 41 57                push        r15
 * 48 83 EC 50          sub         rsp,50h
 * 49 C7 43 A8 FE FF FF mov         qword ptr [r11-58h],0FFFFFFFFFFFFFFFEh
 * FF
 * 49 89 5B 08          mov         qword ptr [r11+8],rbx
 * 0F 29 74 24 40       movaps      xmmword ptr [rsp+40h],xmm6
 * 0F 29 7C 24 30       movaps      xmmword ptr [rsp+30h],xmm7
 * 48 63 F2             movsxd      rsi,edx
 * 48 8B F9             mov         rdi,rcx
 * 48 8B 05 B7 FC 8F 01 mov         rax,qword ptr [7FF64BF28128h]
 * 4C 8B 44 F0 08       mov         r8,qword ptr [rax+rsi*8+8]
 * 41 8B 40 60          mov         eax,dword ptr [r8+60h]
 * C1 E8 12             shr         eax,12h
 * A8 01                test        al,1
 * 74 3F                je          00007FF64A6284C0
 * 48 8B 49 40          mov         rcx,qword ptr [rcx+40h]
 * 48 85 C9             test        rcx,rcx
 * 74 36                je          00007FF64A6284C0
 * 48 83 79 50 00       cmp         qword ptr [rcx+50h],0
 * 74 2F                je          00007FF64A6284C0
 * 40 B5 01             mov         bpl,1
 */
static const CodeSignature kGetEffectiveSkillLevel_PatchSig(
    /* name */       "GetEffectiveSkillLevel",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(GetEffectiveSkillLevel_Hook),
    /* id */         38462,
    /* patch_size */ 6,
    /* trampoline */ &GetEffectiveSkillLevel_ReturnTrampoline
);

/**
 * @brief Overwrites the skill display GetEffectiveSkillLevel() call to display
 *        the actual, non-damaged, skill level.
 *
 * The function that is overwritten by our GetEffectiveSkillLevel() hook is also
 * used to display the skill level in the skills menu.
 *
 * So as to not confuse players, this hook is used to force the skills menu to
 * show the actual skill level, not the damaged value.
 *
 * The assembly is as follows:
 * FF 50 08             call        qword ptr [rax+8]
 * F3 0F 2C C8          cvttss2si   ecx,xmm0
 * 81 F9 00 00 00 80    cmp         ecx,80000000h
 * 74 1E                je          00007FF6BD9C3BED
 * 66 0F 6E C9          movd        xmm1,ecx
 * 0F 5B C9             cvtdq2ps    xmm1,xmm1
 * 0F 2E C8             ucomiss     xmm1,xmm0
 * 74 12                je          00007FF6BD9C3BED
 * 0F 14 C0             unpcklps    xmm0,xmm0
 * 0F 50 C0             movmskps    eax,xmm0
 * 83 E0 01             and         eax,1
 * 2B C8                sub         ecx,eax
 * 66 0F 6E C1          movd        xmm0,ecx
 * 0F 5B C0             cvtdq2ps    xmm0,xmm0
 * 4D 8B CE             mov         r9,r14
 * 4C 89 74 24 40       mov         qword ptr [rsp+40h],r14
 * BA 03 00 00 00       mov         edx,3
 * 89 54 24 48          mov         dword ptr [rsp+48h],edx
 * F3 0F 5A C0          cvtss2sd    xmm0,xmm0
 * F2 0F 11 44 24 50    movsd       mmword ptr [rsp+50h],xmm0
 * 48 8D 0C 76          lea         rcx,[rsi+rsi*2]
 * 48 8D 9D 90 00 00 00 lea         rbx,[rbp+90h]
 * 48 8D 1C CB          lea         rbx,[rbx+rcx*8]
 * 48 8D 44 24 40       lea         rax,[rsp+40h]
 * 48 3B D8             cmp         rbx,rax
 */
static const CodeSignature kDisplayTrueSkillLevel_PatchSig(
    /* name */       "DisplayTrueSkillLevel",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(DisplayTrueSkillLevel_Hook),
    /* id */         52525,
    /* patch_size */ 7,
    /* trampoline */ &DisplayTrueSkillLevel_ReturnTrampoline,
    /* offset */     0x120
);

/// @brief The opcode for an x86 NOP.
static const uint8_t kNop = 0x90;

/**
 * @brief Lists all the code signatures to be resolved/applied
 *        by ApplyGamePatches().
 */
static const CodeSignature *const kGameSignatures[] = {
    &kGetLevel_FunctionSig,
    &kGetBaseActorValue_FunctionSig,
    &kModifyPerkPool_PatchSig,
    &kSkillCapPatch_PatchSig,
    &kHideLegendaryButton_PatchSig,
    &kImproveSkillLevel_PatchSig,
    &kImprovePlayerSkillPoints_PatchSig,
    &kImproveLevelExpBySkillLevel_PatchSig,
    &kImproveAttributeWhenLevelUp_PatchSig,
    &kGetEffectiveSkillLevel_PatchSig,
    &kDisplayTrueSkillLevel_PatchSig
};

/**
 * @brief Gets the base value of the attribute for the given actor.
 * @param actor The actor to get the base value of.
 * @param skill_id The ID of the skill to get the base level of.
 */
float
GetBaseActorValue(
    void *actor,
    UInt32 skill_id
) {
    ASSERT(GetBaseActorValue_Entry);
    return GetBaseActorValue_Entry(actor, skill_id);
}

/**
 * @brief Gets the level of the given actor.
 * @param actor The actor to get the level of.
 */
UInt16
GetLevel(
    void *actor
) {
    ASSERT(GetLevel_Entry);
    return GetLevel_Entry(actor);
}

/**
 * @brief Applies all of this plugins patches to the skyrim AE binary.
 */
void
ApplyGamePatches() {
    const size_t kNumSigs = sizeof(kGameSignatures) / sizeof(void*);

    _MESSAGE("Applying game patches...");

    auto db = VersionDb();
    db.Load();

    for (size_t i = 0; i < kNumSigs; i++) {
        auto sig = kGameSignatures[i];
        unsigned long long id = sig->id;

#ifdef _DEBUG
        if (sig->known_offset) {
            ASSERT(db.FindIdByOffset(sig->known_offset, id));
        }
#endif

        void *addr = db.FindAddressById(id);
        ASSERT(addr);
        uintptr_t real_address = reinterpret_cast<uintptr_t>(addr) + sig->offset;

        _MESSAGE(
            "Signature %s ([ID: %zu] + 0x%zx) is at offset 0x%zx",
            sig->name,
            id,
            sig->offset,
            real_address - RelocationManager::s_baseAddr
        );

        size_t hook_size = HookType::Size(sig->hook_type);
        size_t return_address = real_address + hook_size;

        ASSERT(hook_size <= sig->patch_size);
        ASSERT(!(sig->hook) == ((sig->hook_type == HookType::None)
                             || (sig->hook_type == HookType::Nop)));

        // Install the trampoline, if necessary.
        if (sig->return_trampoline) {
            ASSERT(sig->hook_type != HookType::None);
            ASSERT(sig->hook_type != HookType::Nop);
            *(sig->return_trampoline) = return_address;
        }

        // Install the hook/result.
        switch (sig->hook_type) {
            case HookType::None:
                ASSERT(sig->result);
                *(sig->result) = reinterpret_cast<void*>(real_address);
                break;
            case HookType::Jump5:
                ASSERT(g_branchTrampoline.Write5Branch(real_address, sig->hook));
                break;
            case HookType::Jump6:
                ASSERT(g_branchTrampoline.Write6Branch(real_address, sig->hook));
                break;
            case HookType::Call5:
                ASSERT(g_branchTrampoline.Write5Call(real_address, sig->hook));
                break;
            case HookType::Call6:
                ASSERT(g_branchTrampoline.Write6Call(real_address, sig->hook));
                break;
            case HookType::DirectCall:
                ASSERT(SafeWriteCall(real_address, sig->hook));
                break;
            case HookType::DirectJump:
                ASSERT(SafeWriteJump(real_address, sig->hook));
                break;
            case HookType::Nop:
                break;
            default:
                HALT("Cannot install a hook with an invalid type");
        }

        // Overwrite the rest of the instructions with NOPs. We do this with
        // every hook to ensure the best compatibility with other SKSE
        // plugins.
        SafeMemSet(return_address, kNop, sig->patch_size - hook_size);
    }

    _MESSAGE("Finished applying game patches!");
}
